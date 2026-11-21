#include "VoxelWar.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Fonts/SlateFontInfo.h"
#include "Fonts/FontMeasure.h"
#include "Misc/Paths.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

// Software cursor: the image's fingertip, rather than its bounding box, is the click point.
class SVoxelHandCursor : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SVoxelHandCursor){} SLATE_ARGUMENT(UTexture2D*,Texture) SLATE_END_ARGS()
    void Construct(const FArguments& Args)
    {Brush.SetResourceObject(Args._Texture);Brush.ImageSize=FVector2D(56,56);Brush.DrawAs=ESlateBrushDrawType::Image;SetVisibility(EVisibility::HitTestInvisible);}
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D(56,56);}
    virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle&,bool) const override
    {
        // Slate centers software cursors and applies its own DPI scale. Undo that
        // centering with the actual geometry size so the fingertip stays exact.
        const auto& Buttons=FSlateApplication::Get().GetPressedMouseButtons();
        const bool Pressed=Buttons.Contains(EKeys::LeftMouseButton)||Buttons.Contains(EKeys::RightMouseButton);
        // Slate's real frame time keeps click feedback responsive during thinking and pause.
        PressScale=FMath::FInterpTo(PressScale,Pressed?.90f:1.f,Args.GetDeltaTime(),40.f);
        const FVector2D Bounds=Geometry.GetLocalSize(),Size=Bounds*PressScale;
        const FVector2D Offset=Bounds*.5-Size*FVector2D(.135,.0825);
        FSlateDrawElement::MakeBox(Elements,Layer,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(Offset)),&Brush);
        return Layer;
    }
private:
    FSlateBrush Brush;
    mutable float PressScale=1;
};

namespace
{
// Project an extruded silhouette around the sword's lengthwise Y axis. The
// supplied artwork remains on its two faces; the colored rim gives it thickness.
void PaintAxialSword(FSlateWindowElementList& Elements,int32& Layer,const FGeometry& Geometry,
                    const FSlateBrush& Face,FVector2D Position,FVector2D Size,FBox2D UV,float Angle)
{
    const float C=FMath::Cos(Angle),S=FMath::Sin(Angle),Depth=Size.X*.075f;
    auto Project=[&](FVector2D Tex,float Z)
    {
        const float X=(Tex.X-.5f)/(UV.Max.X-UV.Min.X)*Size.X;
        const float Y=(Tex.Y-UV.Min.Y)/(UV.Max.Y-UV.Min.Y)*Size.Y;
        return FVector2f(Position+FVector2D(Size.X*.5+X*C+Z*S,Y));
    };
    TArray<FSlateVertex> Vertices;TArray<SlateIndex> Indices;
    auto Quad=[&](const FVector2f (&P)[4],const FVector2f (&Tex)[4],FColor Tint)
    {
        const SlateIndex Start=Vertices.Num();
        for(int K=0;K<4;++K)Vertices.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(Geometry.GetAccumulatedRenderTransform(),P[K],Tex[K],Tint));
        Indices.Append({Start,SlateIndex(Start+1),SlateIndex(Start+2),Start,SlateIndex(Start+2),SlateIndex(Start+3)});
    };
    // Outline in source-image pixels, including the dark contour of the artwork.
    static const FVector2D Outline[]={
        {225,27},{287,27},{307,47},{307,94},{294,105},{294,175},{324,175},{341,193},
        {341,219},{324,238},{310,238},{310,355},{300,384},{256,459},{212,384},{201,355},
        {201,238},{187,238},{170,219},{170,193},{187,175},{217,175},{217,105},{205,94},{205,47}};
    const FVector2f WhiteUV[4]={{.5f,.5f},{.5f,.5f},{.5f,.5f},{.5f,.5f}};
    for(int K=0;K<UE_ARRAY_COUNT(Outline);++K)
    {
        FVector2D A=Outline[K]/512.f,B=Outline[(K+1)%UE_ARRAY_COUNT(Outline)]/512.f;
        if(-(B.Y-A.Y)*S<=0)continue;
        const float Y=(A.Y+B.Y)*.5f;
        FLinearColor Tint=Y<.20f?FLinearColor(.95f,.56f,.06f):Y<.34f?FLinearColor(.03f,.27f,.70f):Y<.47f?FLinearColor(.95f,.56f,.06f):FLinearColor(.40f,.66f,.83f);
        Tint*=.65f+.35f*FMath::Abs(S);Tint.A=1;
        FVector2f P[4]={Project(A,-Depth),Project(B,-Depth),Project(B,Depth),Project(A,Depth)};Quad(P,WhiteUV,Tint.ToFColor(true));
    }
    auto Renderer=FSlateApplication::Get().GetRenderer();
    if(!Vertices.IsEmpty())FSlateDrawElement::MakeCustomVerts(Elements,++Layer,Renderer->GetResourceHandle(*FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"))),Vertices,Indices,nullptr,0,0);
    Vertices.Reset();Indices.Reset();
    if(FMath::Abs(C)>.001f)
    {
        const float Z=C>=0?Depth:-Depth;
        const FVector2D Tex[4]={UV.Min,FVector2D(UV.Max.X,UV.Min.Y),UV.Max,FVector2D(UV.Min.X,UV.Max.Y)};
        FVector2f P[4];FVector2f FaceUV[4];for(int K=0;K<4;++K){P[K]=Project(Tex[K],Z);FaceUV[K]=FVector2f(Tex[K]);}
        const float Brightness=.82f+.18f*FMath::Abs(C);Quad(P,FaceUV,FLinearColor(Brightness,Brightness,Brightness,1).ToFColor(true));
        FSlateDrawElement::MakeCustomVerts(Elements,++Layer,Renderer->GetResourceHandle(Face),Vertices,Indices,nullptr,0,0);
    }
}
}

class SVoxelLabels : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SVoxelLabels){} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        static TSharedPtr<const FCompositeFont> SharedFont=MakeShared<FCompositeFont>(TEXT("Regular"),FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")),TEXT("Fonts/msyh.ttc")),EFontHinting::Auto,EFontLoadingPolicy::LazyLoad);
        static TSharedPtr<const FCompositeFont> SharedBold=MakeShared<FCompositeFont>(TEXT("Regular"),FPaths::Combine(FPlatformMisc::GetEnvironmentVariable(TEXT("WINDIR")),TEXT("Fonts/msyhbd.ttc")),EFontHinting::Auto,EFontLoadingPolicy::LazyLoad);
        BoldFont=SharedBold;Font=SharedFont;SetVisibility(EVisibility::HitTestInvisible);
    }
    void Update(const TArray<AVoxelHUD::FLabel>& InLabels,const TArray<AVoxelHUD::FIcon>& InIcons,float InScale,FVector2D InPixels)
    { Entries=InLabels;Icons=InIcons;UIScale=InScale;Pixels=InPixels;Invalidate(EInvalidateWidgetReason::Paint); }
    virtual FVector2D ComputeDesiredSize(float) const override {return FVector2D(1,1);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& Geometry,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle&,bool) const override
    {
        if(!Font.IsValid()||Pixels.X<=0)return Layer;
        float Ratio=Geometry.GetLocalSize().X/Pixels.X;
        for(const auto& I:Icons)
        {
            if(!I.Texture)continue;
            FSlateBrush Brush;Brush.SetResourceObject(I.Texture);Brush.ImageSize=I.Size;Brush.DrawAs=ESlateBrushDrawType::Image;Brush.SetUVRegion(I.UV);
            FVector2D Size=I.Size*UIScale*Ratio,Position=I.Position*UIScale*Ratio;
            if(I.LongAxisSpin){PaintAxialSword(Elements,Layer,Geometry,Brush,Position,Size,I.UV,I.Angle);continue;}
            FSlateDrawElement::MakeRotatedBox(Elements,++Layer,Geometry.ToPaintGeometry(Size,FSlateLayoutTransform(Position)),&Brush,ESlateDrawEffect::None,I.Angle,TOptional<FVector2f>(),FSlateDrawElement::RelativeToElement,FLinearColor::White);
        }
        for(const auto& L:Entries)
        {
            FVector2D P(FMath::RoundToFloat(L.X*UIScale*Ratio),FMath::RoundToFloat(L.Y*UIScale*Ratio));
            FSlateFontInfo Info((L.MenuBold||L.FlatBold)?BoldFont:Font,FMath::Max(7.f,FMath::RoundToFloat(L.Size*UIScale*.9f*Ratio)),TEXT("Regular"));
            if(L.MenuBold){Info.OutlineSettings.OutlineSize=FMath::Max(1,FMath::RoundToInt((L.Size<17?1.f:1.5f)*UIScale*Ratio));Info.OutlineSettings.OutlineColor=FLinearColor(.013f,.009f,.006f,1);}
            if(L.TextBox.X>0)
            {
                const FVector2D Space=L.TextBox*UIScale*Ratio;
                const auto& Metrics=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
                FVector2D Extent=Metrics->Measure(FStringView(L.String),Info);
                if(Extent.X>Space.X)
                {
                    Info.Size=FMath::Max(1.f,FMath::FloorToFloat(Info.Size*Space.X/Extent.X));
                    Extent=Metrics->Measure(FStringView(L.String),Info);
                }
                if(L.CenterHorizontally)P.X+=FMath::RoundToFloat((Space.X-Extent.X)*.5f);
                P.Y+=FMath::RoundToFloat((Space.Y-Extent.Y)*.5f);
            }
            FSlateDrawElement::MakeText(Elements,++Layer,Geometry.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(P)),L.String,Info,ESlateDrawEffect::None,L.Color);
        }
        return Layer;
    }
private:
    TArray<AVoxelHUD::FLabel> Entries;
    TArray<AVoxelHUD::FIcon> Icons;
    TSharedPtr<const FCompositeFont> Font,BoldFont;
    float UIScale=1;
    FVector2D Pixels=FVector2D::ZeroVector;
};

namespace {
const FLinearColor Ink(.035f,.050f,.038f,1), Muted(.12f,.10f,.07f), Paper(.83f,.76f,.58f,1), White(.96f,.91f,.77f), Cyan(.06f,.23f,.48f), Gold(.75f,.53f,.22f);
FString ModuleName(EVoxelModule M){switch(M){case EVoxelModule::Command:return TEXT("指挥城堡");case EVoxelModule::Arrow:return TEXT("弓箭塔");case EVoxelModule::Village:return TEXT("资源村庄");default:return TEXT("机炮堡垒");}}
}
void AVoxelHUD::Panel(float X,float Y,float W,float H,FLinearColor C)
{
    DrawRect(C,FMath::RoundToFloat(X*Scale),FMath::RoundToFloat(Y*Scale),FMath::RoundToFloat(W*Scale),FMath::RoundToFloat(H*Scale));
}
void AVoxelHUD::Text(const FString& S,float X,float Y,float Size,FLinearColor Color)
{Labels.Add({S,X,Y,Size,Color});}
void AVoxelHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    if(GEngine&&GEngine->GameViewport)
    {
        if(LabelWidget.IsValid())GEngine->GameViewport->RemoveViewportWidgetContent(LabelWidget.ToSharedRef());
        if(CursorWidget.IsValid())GEngine->GameViewport->SetSoftwareCursorWidget(EMouseCursor::Default,TSharedPtr<SWidget>());
    }
    LabelWidget.Reset();CursorWidget.Reset();Super::EndPlay(Reason);
}
void AVoxelHUD::InitializeUIArt()
{
    if(MoneyBag)return;
    MenuBackground=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuBackground.T_MenuBackground"));
    MenuReference=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuReference.T_MenuReference"));
    MoneyBag=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MoneyBag.T_MoneyBag"));
    GoldDigits=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_GoldDigits.T_GoldDigits"));
    SelectionBlade=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_SelectionBlade.T_SelectionBlade"));
    HandCursor=LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_HandCursor.T_HandCursor"));
    if(HandCursor&&GEngine&&GEngine->GameViewport)
    {
        SAssignNew(CursorWidget,SVoxelHandCursor).Texture(HandCursor);
        GEngine->GameViewport->SetSoftwareCursorWidget(EMouseCursor::Default,StaticCastSharedPtr<SWidget>(CursorWidget));
        GEngine->GameViewport->SetUseSoftwareCursorWidgets(true);
        if(auto* P=GetOwningPlayerController())P->DefaultMouseCursor=EMouseCursor::Default;
    }
}
void AVoxelHUD::DrawTreasury(AVoxelBattle* B,float H)
{
    const FVector2D Position(24,H-314),Size(200,200);
    TreasuryRect=FBox2D(Position*Scale,(Position+Size)*Scale);
    if(MoneyBag)Icons.Add({MoneyBag,Position,Size});
    const FString Amount=FString::FromInt(FMath::Max(0,FMath::FloorToInt(B->Gold[1])));
    // Each numeral uses its own UV cell so it stays sharp and changes with the treasury.
    // Include the complete brown outline and downward extrusion, not only the face.
    const float InkWidth=(Amount.Len()-1)*75.f+76,InkHeight=106;
    float GlyphScale=FMath::Min(19.f/InkHeight,72.f/InkWidth);
    float Left=Position.X+Size.X*.505f-InkWidth*GlyphScale*.5f+4*GlyphScale;
    float Top=Position.Y+Size.Y*.648f-InkHeight*GlyphScale*.5f+4*GlyphScale;
    for(int I=0;I<Amount.Len();++I)
    {
        int Digit=Amount[I]-TCHAR('0');
        if(GoldDigits)Icons.Add({GoldDigits,FVector2D(Left+(I*75-16)*GlyphScale,Top-13*GlyphScale),FVector2D(96,128)*GlyphScale,FBox2D(FVector2D(Digit*.1,0),FVector2D((Digit+1)*.1,1))});
    }
    if(!GoldDigits)Text(Amount,Position.X+76,Position.Y+119,16,Gold);
    HUDText(B->Tutorial?TEXT("教学物资"):TEXT("战争金库"),238,H-213,19,FLinearColor(1,.76f,.28f));
    HUDText(FString::Printf(TEXT("%d 座村庄 · +%.1f 金 / 秒"),B->VillageCount(1),B->GoldRecovery(1)),238,H-184,12,White);
}
void AVoxelHUD::DrawSelectionBlade(AVoxelBattle* B,AVoxelCommander* P,float W,float H)
{
    if(!SelectionBlade||B->Winner)return;
    FVector Anchor;const int Unit=B->SquadIndex(B->SelectedSquad);
    if(Unit!=INDEX_NONE&&B->Squads[Unit].Troops>0)Anchor=B->SquadLabelPosition(B->Squads[Unit]);
    else if(B->Towers.IsValidIndex(B->Selected))Anchor=B->CellPosition(B->Towers[B->Selected].Cell)+FVector(0,0,B->TowerLabelHeight(B->Selected));
    else return;
    FVector2D Screen;if(!P->ProjectWorldLocationToScreen(Anchor,Screen))return;
    const FVector2D Center(Screen.X/Scale,Screen.Y/Scale-67);
    // An unscaled clock keeps a two-second turn during thinking, 2x playback and pause.
    const float Angle=FMath::Fmod(GetWorld()->GetRealTimeSeconds(),2.f)*PI;
    if(Center.X<38||Center.X>W-316||Center.Y<123||Center.Y>H-155)return;
    Icons.Add({SelectionBlade,Center-FVector2D(13,31),FVector2D(26,62),FBox2D(FVector2D(.32,.035),FVector2D(.68,.93)),Angle,true});
}
bool AVoxelHUD::HandleClick(float X,float Y)
{
    auto* P=Cast<AVoxelCommander>(GetOwningPlayerController());if(!P)return false;
    if(P->IsMenuOpen())
    {
        for(const auto& B:Buttons)if(B.Rect.IsInside(FVector2D(X,Y))){if(B.Active)P->MenuAction(B.ID);return true;}
        return true;
    }
    if(IsTreasuryHit(X,Y))return true;
    if(MinimapRect.IsInside(FVector2D(X,Y)))
    {
        FVector2D UV=(FVector2D(X,Y)-MinimapRect.Min)/(MinimapRect.Max-MinimapRect.Min);
        if(auto* Battle=P->Battle())P->Focus=Battle->FocusFromMapUV(UV);
        P->Zoom=7000;return true;
    }
    for(const auto& B:Buttons)if(B.Rect.IsInside(FVector2D(X,Y)))
    {
        if(!B.Active)return true;
        if(B.ID==TEXT("menu")){if(auto* Battle=P->Battle();Battle&&Battle->Tutorial&&Battle->Winner==1)P->SelectedMode=0;P->ShowMenu();return true;}
        if(B.ID==TEXT("graduate")){P->SelectedMode=0;P->StartBattle();return true;}
        if(B.ID==TEXT("speed1")||B.ID==TEXT("speed2"))
        {if(auto* Battle=P->Battle())Battle->SetPlaybackSpeed(B.ID==TEXT("speed2")?2:1);return true;}
        if(auto* Battle=P->Battle();Battle&&Battle->Tutorial)
        {
            if(B.ID==TEXT("lessonFocus")){Battle->FocusTutorialObjective();return true;}
            if(B.ID==TEXT("lessonRetry")){Battle->RetryTutorialLesson();return true;}
            if(B.ID==TEXT("lessonNext")){Battle->NextTutorialLesson();return true;}
            if(B.ID==TEXT("lessonFast")){Battle->ToggleTutorialFastForward();return true;}
        }
        if(B.ID==TEXT("bridge"))P->BridgeKey();else if(B.ID==TEXT("upgrade"))P->UpgradeKey();else if(B.ID==TEXT("meteor"))P->MeteorKey();else if(B.ID==TEXT("fraction"))P->FractionKey();else if(B.ID==TEXT("pause"))P->PauseKey();else if(B.ID==TEXT("poison"))P->PoisonKey();else if(B.ID==TEXT("pierce"))P->PierceKey();else if(B.ID==TEXT("restart"))P->RestartKey();else if(B.ID==TEXT("overview"))P->OverviewKey();return true;
    }
    if(auto* Battle=P->Battle();Battle&&Battle->Tutorial&&X>=28*Scale&&X<=410*Scale&&Y>=145*Scale&&Y<=465*Scale)return true;
    return Y<90*Scale||Y>ViewportSize.Y-104*Scale||(X>ViewportSize.X-282*Scale&&Y<ViewportSize.Y-104*Scale);
}
void AVoxelHUD::DrawHUD()
{
    Super::DrawHUD();Buttons.Empty();auto* P=Cast<AVoxelCommander>(GetOwningPlayerController());if(!P||!Canvas)return;
    Labels.Reset();Icons.Reset();TreasuryRect=FBox2D(ForceInit);InitializeUIArt();
    ViewportSize=FVector2D(Canvas->SizeX,Canvas->SizeY);auto* B=P->Battle();if(!B)return;
    Scale=FMath::Min(Canvas->SizeX/1440.f,Canvas->SizeY/900.f);float W=Canvas->SizeX/Scale,H=Canvas->SizeY/Scale;
    if(P->IsMenuOpen()){MinimapRect=FBox2D(ForceInit);DrawFrontEnd(P,W,H);PublishLabels();return;}
    DrawTreasury(B,H);
    DrawBattleTop(B,P,W);
    float RX=W-278;HUDFrame(RX,103,260,H-220,true);
    HUDIcon(4,RX+17,113,32,36);HUDText(TEXT("领主军令"),RX+56,111,22,FLinearColor(1,.74f,.25f));
    HUDText(TEXT("先占全部分城（含村庄），再攻主城"),RX+19,160,10,Ink,true);
    Panel(RX+18,184,224,1,FLinearColor(.65f,.45f,.19f,.6f));float Y=201;
    int32 SelectedUnit=B->SquadIndex(B->SelectedSquad);
    if(SelectedUnit!=INDEX_NONE)
    {
        const auto& S=B->Squads[SelectedUnit];
        HUDText(S.Team==1?TEXT("蓝旗野战部队"):TEXT("赤旗野战部队"),RX+20,Y,20,Ink,true);Y+=36;
        Text(FString::Printf(TEXT("编号 %d · %s"),S.ID,S.Evacuating?TEXT("撤回近岸"):S.Blocked?TEXT("道路阻断"):S.InterceptTarget!=INDEX_NONE?TEXT("拦截敌军"):S.Holding?TEXT("驻守待命"):TEXT("行军中")),RX+20,Y,13,Muted);Y+=42;
        Text(FString::Printf(TEXT("%d 兵"),FMath::CeilToInt(S.Troops)),RX+20,Y,32,S.Team==1?Cyan:Ink);Y+=64;
        Text(TEXT("右键空地：移动后驻守"),RX+20,Y,13,Ink);Y+=32;
        Text(TEXT("右键敌塔：进攻占领"),RX+20,Y,13,Ink);Y+=32;
        Text(TEXT("右键友塔：回城增援"),RX+20,Y,13,Ink);Y+=38;
        Text(FString::Printf(TEXT("驻守半径 %.1f 米 · 自动拦截"),B->GuardRadius/100.f),RX+20,Y,12,Muted);Y+=29;
        Text(TEXT("追击结束后返回驻守点"),RX+20,Y,12,Muted);Y+=29;
        Text(TEXT("友军靠近自动合并，兵力相加"),RX+20,Y,11,Muted);Y+=25;
        Text(TEXT("优先保留选中队伍的命令"),RX+20,Y,11,Muted);
    }
    else if(B->Towers.IsValidIndex(B->Selected))
    {
        const auto& T=B->Towers[B->Selected];HUDText(T.Name.ToString(),RX+20,Y,20,Ink,true);Y+=30;
        FString Faction=T.Team==1?TEXT("蓝旗盟军"):T.Team==2?TEXT("赤旗敌军"):TEXT("中立据点");
        Text(ModuleName(T.Module)+FString::Printf(TEXT(" · %d级 · "),T.Level)+Faction,RX+20,Y,12,T.Team==1?Cyan:T.Team==2?FLinearColor(.64f,.18f,.12f):Muted);Y+=29;
        Text(FString::Printf(TEXT("%d"),FMath::CeilToInt(T.Troops)),RX+20,Y,32,Ink);Text(FString::Printf(TEXT("/ %d 兵"),FMath::RoundToInt(B->TroopCapacity(B->Selected))),RX+107,Y+20,13,Muted);Y+=56;
        if(T.Module!=EVoxelModule::Village)
        {
            Text(FString::Printf(TEXT("伤害 %.1f · 攻击间隔 %.2f秒"),B->TowerDamage(B->Selected)*(T.Arrow==EVoxelArrow::Pierce?2.8f:1.f),B->TowerInterval(B->Selected)),RX+20,Y,12,Ink);Y+=24;
            Text(FString::Printf(TEXT("%s · 基础射程 %.1f 米"),T.Tall?TEXT("远射底座"):TEXT("重击底座"),B->TowerBaseRange(B->Selected)/100.f),RX+20,Y,11,Muted);Y+=24;
            if(T.Team==1)
            {
                int32 Cost=B->UpgradeCost(B->Selected);
                Button(TEXT("upgrade"),Cost>0?FString::Printf(TEXT("[U] 升至 %d 级 · %d 金"),T.Level+1,Cost):TEXT("已达最高等级 · 5 级"),RX+18,Y,224,Cost>0&&B->Gold[1]>=Cost&&!B->Paused&&!B->Winner&&(!B->Tutorial||(B->TutorialStep==3&&B->Selected==1&&T.Level<2&&!B->TutorialObjectiveComplete)));Y+=43;
                Text(Cost>0?TEXT("升级：驻军 +50 · 上限 +50"):TEXT("已达最高等级 · 驻军上限 450"),RX+20,Y,10,Muted);Y+=20;
                bool Defensive=T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon;
                Text(Defensive?(Cost>0?TEXT("伤害/射速/射程 +20%/+15%/+10%"):TEXT("伤害/射速/射程 +80%/+60%/+40%")):(Cost>0?TEXT("伤害 +20% · 射速 +15%"):TEXT("伤害 +80% · 射速 +60%")),RX+20,Y,10,Muted);Y+=23;
            }
        }
        if(T.Module==EVoxelModule::Cannon)
        {Text(T.Overheated?TEXT("炮管过热！冷却中"):TEXT("炮管热量"),RX+20,Y,13,T.Overheated?FLinearColor(.67f,.20f,.10f):Ink);Y+=24;Panel(RX+20,Y,220,9,Muted);Panel(RX+20,Y,220*T.Heat/100,9,Gold);Y+=25;}
        if(T.Module==EVoxelModule::Village)
        {
            Text(FString::Printf(TEXT("主塔恢复 +%.2f 兵 / 秒"),B->VillageTroopSupply(B->Selected)),RX+20,Y,13,Ink);Y+=28;
            Text(FString::Printf(TEXT("金币收入 +%.1f 金 / 秒"),B->VillageGoldSupply(B->Selected)),RX+20,Y,13,Ink);Y+=28;
            if(T.Team==1)
            {
                int32 Cost=B->UpgradeCost(B->Selected);Button(TEXT("upgrade"),Cost>0?FString::Printf(TEXT("[U] 发展至 %d级 · %d金"),T.Level+1,Cost):TEXT("村庄已达最高等级"),RX+18,Y,224,Cost>0&&B->Gold[1]>=Cost&&!B->Paused&&!B->Winner&&(!B->Tutorial||(B->TutorialStep==2&&B->Selected==2&&T.Level<2&&!B->TutorialObjectiveComplete)));Y+=45;
                if(Cost>0){Text(FString::Printf(TEXT("下级主塔补给 +%.2f兵/秒"),B->VillageTroopBonus*(1+.25f*T.Level)),RX+20,Y,11,Muted);Y+=24;Text(FString::Printf(TEXT("下级金币补给 +%.1f金/秒"),B->VillageGoldBonus*(1+.25f*T.Level)),RX+20,Y,11,Muted);Y+=26;}
                Text(TEXT("发展提高补给，不额外增加驻军"),RX+20,Y,10,Muted);Y+=26;
            }
            Text(TEXT("失守后等级与补给归占领方"),RX+20,Y,11,Muted);Y+=30;
        }
        if(T.Module==EVoxelModule::Command&&T.Team!=0)
        {
            int32 Posts=B->OutpostsRemaining(T.Team);Text(Posts>0?TEXT("主塔保护中"):TEXT("保护解除 · 可被敌军攻占"),RX+20,Y,12,Posts>0?Cyan:Ink);Y+=25;
            Text(FString::Printf(TEXT("本方分城 %d · 村庄 %d"),Posts,B->VillageCount(T.Team)),RX+20,Y,11,Muted);Y+=22;
            Text(FString::Printf(TEXT("主塔恢复 %.2f 兵 / 秒"),B->CommandRecovery(T.Team)),RX+20,Y,13,Ink);Y+=26;
            Text(FString::Printf(TEXT("金币收入 %.1f 金 / 秒"),B->GoldRecovery(T.Team)),RX+20,Y,13,Ink);Y+=30;
        }
        if(T.Module==EVoxelModule::Arrow)
        {
            if(T.Team==1&&!T.PlayerBound)
            {Text(TEXT("选择永久箭种"),RX+20,Y,13,Ink);Y+=23;Button(TEXT("poison"),TEXT("[1] 毒箭 · 持续范围伤害"),RX+18,Y,224,!B->Tutorial||(B->TutorialStep==4&&!B->TutorialObjectiveComplete));Y+=42;Button(TEXT("pierce"),TEXT("[2] 穿透箭 · 直线重击"),RX+18,Y,224,!B->Tutorial||(B->TutorialStep==4&&!B->TutorialObjectiveComplete));Y+=42;Text(TEXT("一旦选定，易主后仍然保留"),RX+20,Y,10,Muted);Y+=25;}
            else{Text(T.Arrow==EVoxelArrow::Poison?TEXT("毒箭 · 持续范围伤害"):T.Arrow==EVoxelArrow::Pierce?TEXT("穿透箭 · 直线重击"):TEXT("尚未选择箭种"),RX+20,Y,13,Ink);Y+=30;}
        }
        if(T.Team==1){Text(TEXT("右键派兵至据点或空地"),RX+20,Y,12,Ink);Y+=24;Text(FString::Printf(TEXT("本次出兵：%d 人"),FMath::FloorToInt(T.Troops*B->DispatchFraction)),RX+20,Y,13,Cyan);}
    }
    else
    {
        DrawIdleOrders(RX,Y);
    }
    {
        float MY=H-236;HUDText(TEXT("战场态势 · 点击移动镜头"),RX+20,MY-28,12,Ink,true);
        HUDFrame(RX+14,MY-4,232,111);Panel(RX+20,MY+2,220,99,FLinearColor(.10f,.31f,.43f));
        MinimapRect=FBox2D(FVector2D((RX+20)*Scale,(MY+2)*Scale),FVector2D((RX+240)*Scale,(MY+101)*Scale));
        float CW=220.f/(B->MapHalfWidth()*2+1),CH=99.f/(B->MapHalfHeight()*2+1);
        for(int Row=-B->MapHalfHeight();Row<=B->MapHalfHeight();++Row)
        {
            int Run=MIN_int32;
            for(int Col=-B->MapHalfWidth();Col<=B->MapHalfWidth()+1;++Col)
            {
                bool Land=Col<=B->MapHalfWidth()&&B->ContainsCell(FIntPoint(Col,Row))&&!B->IsRiver(FIntPoint(Col,Row));
                if(Land&&Run==MIN_int32)Run=Col;
                if(!Land&&Run!=MIN_int32){Panel(RX+20+(Run+B->MapHalfWidth())*CW,MY+2+(Row+B->MapHalfHeight())*CH,(Col-Run)*CW,CH,FLinearColor(.24f,.32f,.15f));Run=MIN_int32;}
            }
        }
        for(const auto& Bridge:B->Bridges)
        {
            FVector2D A=B->MapUV(B->CellPosition(B->BridgeBank(Bridge,false))),C=B->MapUV(B->CellPosition(B->BridgeBank(Bridge,true)));
            Panel(RX+20+FMath::Min(A.X,C.X)*220-1,MY+2+FMath::Min(A.Y,C.Y)*99-1,FMath::Abs(A.X-C.X)*220+2,FMath::Abs(A.Y-C.Y)*99+2,Bridge.Remaining>0?Gold:Paper);
        }
        for(const auto& T:B->Towers)
        {FVector2D UV=B->MapUV(FVector(T.Cell.X*B->Tile,T.Cell.Y*B->Tile,0));float MX=RX+20+UV.X*220,YY=MY+2+UV.Y*99;Panel(MX-2,YY-2,T.Module==EVoxelModule::Command?7:4,T.Module==EVoxelModule::Command?7:4,T.Team==1?FLinearColor(.15f,.48f,.85f):T.Team==2?FLinearColor(.8f,.15f,.08f):Paper);}
        FVector2D FocusUV=B->MapUV(P->Focus);float FX=RX+20+FocusUV.X*220,FY=MY+2+FocusUV.Y*99;
        Panel(FX-5,FY,11,1,White);Panel(FX,FY-5,1,11,White);
    }
    const bool ShowPause=B->Paused&&!B->Winner;
    const bool ShowNotice=!B->Paused&&(B->NoticeUntil>B->Time||B->Winner);
    const bool ShowThinking=B->IsThinking()&&!B->Paused&&!B->TutorialObjectiveComplete;
    auto LabelFits=[&](const FBox2D& Bounds)
    {
        const float Top=ShowPause?140.f:ShowNotice?135.f:90.f;
        if(Bounds.Min.Y<Top||Bounds.Max.Y>H-104||Bounds.Max.X>=RX)return false;
        if(Bounds.Intersect(FBox2D(TreasuryRect.Min/Scale,TreasuryRect.Max/Scale)))return false;
        if(Bounds.Intersect(FBox2D(FVector2D(238,H-213),FVector2D(420,H-156))))return false;
        return !ShowThinking||!Bounds.Intersect(FBox2D(FVector2D(238,H-150),FVector2D(W-312,H-115)));
    };
    const float PlateScale=FMath::GetMappedRangeValueClamped(FVector2D(8500,14000),FVector2D(1,.73),P->Zoom);
    TArray<FBox2D,TInlineAllocator<48>> PlateBounds;
    for(const FVoxelSquad& Squad:B->Squads)
    {
        const bool Selected=Squad.ID==B->SelectedSquad;
        if(Squad.Troops<=0||(P->Zoom>11000&&!Selected))continue;
        FVector2D Screen;
        if(!P->ProjectWorldLocationToScreen(B->SquadLabelPosition(Squad),Screen))continue;
        const FVector2D Anchor=Screen/Scale;
        const float BadgeScale=Selected?.85f:.76f*PlateScale;
        const FBox2D Bounds(Anchor-FVector2D(25,14)*BadgeScale,Anchor+FVector2D(25,19)*BadgeScale);
        if(!LabelFits(Bounds))continue;
        PlateBounds.Add(Bounds);
        DrawTroopPlate(Squad.Team,Squad.Troops,Anchor,BadgeScale);
    }
    for(int I=0;I<B->Towers.Num();++I)
    {
        const FVoxelTower& Tower=B->Towers[I];
        const FVector WorldAnchor=B->CellPosition(Tower.Cell)+FVector(0,0,B->TowerLabelHeight(I));
        FVector2D Screen;
        if(!P->ProjectWorldLocationToScreen(WorldAnchor,Screen))continue;
        const FVector2D Anchor=Screen/Scale;
        FString Caption;
        float CaptionWidth=0;
        if(Tower.Module==EVoxelModule::Village)
        {
            Caption=FString::Printf(TEXT("%d级村庄"),Tower.Level);
            CaptionWidth=68;
        }
        else if(Tower.Module==EVoxelModule::Command)
        {
            Caption=B->OutpostsRemaining(Tower.Team)>0?TEXT("主城·保护中"):TEXT("主城·可攻");
            CaptionWidth=84;
        }
        const float HalfWidth=FMath::Max(25.f,CaptionWidth*.5f)*PlateScale;
        const FBox2D Bounds(Anchor-FVector2D(HalfWidth,(Caption.IsEmpty()?14:34)*PlateScale),
                            Anchor+FVector2D(HalfWidth,19*PlateScale));
        if(!LabelFits(Bounds))continue;
        PlateBounds.Add(Bounds);
        DrawTroopPlate(Tower.Team,Tower.Troops,Anchor,PlateScale,Tower.Level,Caption,CaptionWidth);
    }
    for(const auto& Bridge:B->Bridges)if(Bridge.Remaining>0)
    {
        FVector2D Screen;
        if(!P->ProjectWorldLocationToScreen(B->BridgeCenter(Bridge)+FVector(0,0,240),Screen))continue;
        const FVector2D Origin=Screen/Scale-FVector2D(52,5);
        static const FVector2D Offsets[]={{0,0},{0,-32},{0,32},{-64,0},{64,0},{0,-60},{0,60}};
        for(FVector2D Offset:Offsets)
        {
            const FVector2D Pos=Origin+Offset;
            const FBox2D Bounds(Pos,Pos+FVector2D(104,24));
            if(!LabelFits(Bounds)||PlateBounds.ContainsByPredicate([&](const FBox2D& Plate){return Plate.Intersect(Bounds);}))continue;
            PlateBounds.Add(Bounds);
            Panel(Pos.X,Pos.Y,104,24,Ink);
            Text(FString::Printf(TEXT("%s施工 %d秒"),Bridge.Team==1?TEXT("蓝方"):TEXT("红方"),FMath::CeilToInt(Bridge.Remaining)),Pos.X+4,Pos.Y+5,10,Gold);
            break;
        }
    }
    HUDFrame(8,H-103,W-16,91);
    Button(TEXT("fraction"),FString::Printf(TEXT("[Tab] 出兵 %d%%"),FMath::RoundToInt(B->DispatchFraction*100)),34,H-90,258,true,56,0);
    Button(TEXT("bridge"),B->BridgeMode?TEXT("左键选址\n右键取消"):FString::Printf(TEXT("[B] 建桥\n%d 金 / %d 秒"),FMath::RoundToInt(B->BridgeCost),FMath::RoundToInt(B->BridgeBuildTime)),310,H-90,244,B->Gold[1]>=B->BridgeCost&&(!B->Tutorial||((B->TutorialStep==7||B->TutorialStep==9)&&B->Bridges.IsEmpty()&&!B->TutorialObjectiveComplete)),56,1);
    FString Meteor=B->MeteorReady>0?FString::Printf(TEXT("[M] 天降陨石\n冷却 %d 秒"),FMath::CeilToInt(B->MeteorReady)):B->MeteorMode?TEXT("左键落点\n右键取消"):FString::Printf(TEXT("[M] 天降陨石\n%d 金"),FMath::RoundToInt(B->MeteorCost));
    Button(TEXT("meteor"),Meteor,572,H-90,236,B->MeteorReady<=0&&B->Gold[1]>=B->MeteorCost&&(!B->Tutorial||(B->TutorialStep==8&&!B->TutorialObjectiveComplete)),56,2);
    Button(TEXT("overview"),TEXT("[Home] 战场全图"),826,H-90,242,true,56,3);
    HUDText(TEXT("WASD 移动   Shift 加速   滚轮 缩放"),W-350,H-78,12,White);
    HUDText(TEXT("空格 暂停 / 继续   R 重新开局"),W-350,H-52,12,White);
    Text(B->Tutorial?TEXT("领主训练场 / 按提示完成实操；可定位目标、重试本课"):TEXT("河谷大战 / 10 座村庄 · 14 座箭塔 · 14 座炮塔 · 2 座主城"),36,H-34,10,FLinearColor(.95f,.82f,.55f));
    if(ShowNotice){Panel(25,99,RX-48,35,Ink);Text(B->Notice,38,107,13,White);}
    if(ShowThinking)
    {Panel(238,H-150,W-550,35,Ink);Text(TEXT("战术思考 0.2×  ·  完成操作恢复所选速度  ·  Esc 取消选择"),252,H-142,13,Gold);}
    if(ShowPause)
    {
        Panel(25,99,RX-48,40,Ink);Text(TEXT("战事暂歇 · 按空格继续，可查看据点与战场"),40,110,16,Gold);
    }
    if(B->Tutorial&&!B->Winner)DrawTutorial(B,P,W,H);
    if(B->Winner)
    {
        float CX=(W-278)/2;Panel(CX-240,H/2-118,480,305,Gold);Panel(CX-237,H/2-115,474,299,Ink);Panel(CX-221,H/2-99,442,267,Paper);
        Labels.RemoveAll([&](const FLabel& L){return L.X>CX-240&&L.X<CX+240&&L.Y>H/2-118&&L.Y<H/2+187;});
        FString Title=B->Tutorial&&B->Winner==1?TEXT("训练完成 · 准备出征"):B->Winner==3?TEXT("势均力敌 · 平局"):B->TimeLimitReached?(B->Winner==1?TEXT("时限结算 · 胜利"):TEXT("时限结算 · 落败")):B->Winner==1?TEXT("敌方主城攻克 · 胜利"):TEXT("我方主城失守");
        Text(Title,CX-190,H/2-87,25,Ink);
        Text(B->Tutorial&&B->Winner==1?TEXT("你已掌握基础指挥，现在可以挑战正式战役。"):B->TimeLimitReached?FString::Printf(TEXT("%d分钟已到：先比据点，同数再比总兵力"),FMath::RoundToInt(B->BattleTimeLimit/60)):TEXT("主塔被攻陷，战役结束"),CX-190,H/2-46,12,Muted);
        Text(FString::Printf(TEXT("据点：我方 %d  /  敌方 %d"),B->FinalHoldings[1],B->FinalHoldings[2]),CX-190,H/2-14,15,Ink);
        Text(FString::Printf(TEXT("兵力：我方 %.1f  /  敌方 %.1f"),B->FinalTroops[1],B->FinalTroops[2]),CX-190,H/2+16,13,Ink);
        Button(B->Tutorial&&B->Winner==1?TEXT("graduate"):TEXT("restart"),B->Tutorial&&B->Winner==1?TEXT("开始标准战役"):TEXT("重新开局 [R]"),CX-190,H/2+67,380);
        Button(TEXT("menu"),TEXT("返回主菜单"),CX-190,H/2+112,380);
    }
    DrawSelectionBlade(B,P,W,H);PublishLabels();
}
void AVoxelHUD::PublishLabels()
{
    if(GEngine&&GEngine->GameViewport)
    {
        if(!LabelWidget.IsValid()){SAssignNew(LabelWidget,SVoxelLabels);GEngine->GameViewport->AddViewportWidgetContent(LabelWidget.ToSharedRef(),10);}
        LabelWidget->Update(Labels,Icons,Scale,ViewportSize);
    }
}
