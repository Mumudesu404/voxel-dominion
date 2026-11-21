#include "VoxelWar.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/App.h"

namespace
{
const FLinearColor Cream(.98f,.94f,.81f),Gold(1.f,.73f,.24f),Navy(.018f,.045f,.10f);

// Position and color are interpolated together so the enamel and brass highlights
// stay aligned with the shield at every camera zoom.
void AddPlateLayer(TArray<FCanvasUVTri>& Mesh,TConstArrayView<FVector2D> Outline,
                   FVector2D Origin,FVector2D Size,FLinearColor Light,FLinearColor Shade,float SideLighting=.25f)
{
    auto ColorAt=[&](FVector2D UV)
    {
        const float Mix=FMath::Lerp(UV.Y,UV.X,SideLighting);
        return FMath::Lerp(Light,Shade,Mix);
    };
    for(int I=1;I+1<Outline.Num();++I)
    {
        FCanvasUVTri& Tri=Mesh.AddDefaulted_GetRef();
        Tri.V0_Pos=Origin+Size*Outline[0]; Tri.V0_Color=ColorAt(Outline[0]);
        Tri.V1_Pos=Origin+Size*Outline[I]; Tri.V1_Color=ColorAt(Outline[I]);
        Tri.V2_Pos=Origin+Size*Outline[I+1]; Tri.V2_Color=ColorAt(Outline[I+1]);
        Tri.V0_UV=Tri.V1_UV=Tri.V2_UV=FVector2D::ZeroVector;
    }
}
}

void AVoxelHUD::MenuPlate(float X,float Y,float W,float H,FLinearColor Top,FLinearColor Bottom)
{
    const float Cut=FMath::Min(9.f,FMath::Min(W,H)*.5f);
    const float U=Cut/W,V=Cut/H;
    const FVector2D Outline[]={{U,0},{1-U,0},{1,V},{1,1-V},{1-U,1},{U,1},{0,1-V},{0,V}};
    TArray<FCanvasUVTri> Mesh;
    Mesh.Reserve(6);
    AddPlateLayer(Mesh,Outline,FVector2D(X,Y)*Scale,FVector2D(W,H)*Scale,Top,Bottom,0);
    Canvas->K2_DrawTriangle(nullptr,MoveTemp(Mesh));
}

void AVoxelHUD::DrawTroopPlate(int32 Team,float Troops,FVector2D Anchor,float ZoomScale,
                              int32 Level,const FString& Caption,float CaptionWidth)
{
    static const FVector2D Shield[]={{.09,0},{.91,0},{1,.12},{1,.67},{.88,.80},{.5,1},{.12,.80},{0,.67},{0,.12}};
    static const FVector2D Ribbon[]={{.06,0},{.94,0},{1,.5},{.94,1},{.06,1},{0,.5}};
    static const FVector2D Lozenge[]={{.5,0},{1,.5},{.5,1},{0,.5}};
    static const FLinearColor Bronze=FColor(124,70,26), Brass=FColor(226,165,58), Glint=FColor(255,235,165);
    static const FLinearColor Keyline=FColor(36,29,34), Inset=FColor(42,39,43);
    struct FEnamel { FLinearColor Light,Dark; };
    static const FEnamel Teams[]={
        {FColor(102,106,113),FColor(43,45,55)},
        {FColor(26,114,191),FColor(9,34,77)},
        {FColor(178,57,66),FColor(75,18,36)}
    };
    const FEnamel& Face=Teams[FMath::Clamp(Team,0,2)];
    const float PixelScale=Scale*ZoomScale;
    const FVector2D PixelOrigin=Anchor*Scale;
    TArray<FCanvasUVTri> Mesh;
    Mesh.Reserve(64);
    auto Layer=[&](TConstArrayView<FVector2D> Shape,float X,float Y,float W,float H,FLinearColor Top,FLinearColor Bottom)
    {
        AddPlateLayer(Mesh,Shape,PixelOrigin+FVector2D(X,Y)*PixelScale,FVector2D(W,H)*PixelScale,Top,Bottom);
    };
    Layer(Shield,-25,-14,50,33,Keyline,Keyline);
    Layer(Shield,-24,-13,48,30,Glint,Bronze);
    Layer(Shield,-22,-11,44,26,Inset,Brass);
    Layer(Shield,-21,-10,42,24,Face.Light,Face.Dark);

    const int RankMarks=FMath::Clamp(Level-1,0,4);
    for(int I=0;I<RankMarks;++I)
        Layer(Lozenge,(I-(RankMarks-1)*.5f)*5-2,7,4,4,Glint,Brass);

    auto CenterText=[&](const FString& String,float Top,float Width,float Height,float Font,FLinearColor Color)
    {
        HUDText(String,Anchor.X-Width*.5f*ZoomScale,Anchor.Y+Top*ZoomScale,Font*ZoomScale,Color);
        Labels.Last().TextBox=FVector2D(Width,Height)*ZoomScale;
        Labels.Last().CenterHorizontally=true;
    };
    if(!Caption.IsEmpty())
    {
        Layer(Ribbon,-CaptionWidth*.5f,-34,CaptionWidth,21,Brass,Bronze);
        Layer(Ribbon,-CaptionWidth*.5f+2,-33,CaptionWidth-4,18,Navy,Keyline);
        CenterText(Caption,-33,CaptionWidth-11,18,11.5f,Cream);
    }
    Canvas->K2_DrawTriangle(nullptr,MoveTemp(Mesh));
    CenterText(FString::FromInt(FMath::CeilToInt(Troops)),RankMarks?-12:-11,39,21,16.5f,FLinearColor::White);
}

void AVoxelHUD::HUDText(const FString& S,float X,float Y,float Size,FLinearColor Color,bool OnPaper)
{
    Text(S,X,Y,Size,Color);Labels.Last().MenuBold=!OnPaper;Labels.Last().FlatBold=OnPaper;
}

void AVoxelHUD::HUDFrame(float X,float Y,float W,float H,bool Paper,FLinearColor Tint)
{
    static TStrongObjectPtr<UTexture2D> Blue(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_HUDBar.T_HUDBar")));
    static TStrongObjectPtr<UTexture2D> Parchment(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_HUDOrders.T_HUDOrders")));
    UTexture2D* Texture=Paper?Parchment.Get():Blue.Get();
    if(!Texture){Panel(X,Y,W,H,Navy);return;}
    // Nine slices retain sculpted corners at every control size. Only empty
    // surfaces stretch; no game data, text or minimap is part of this texture.
    FVector2D UVMin=Paper?FVector2D(20.f/887,11.f/1774):FVector2D(9.f/2172,223.f/724);
    FVector2D UVSize=Paper?FVector2D(845.f/887,1741.f/1774):FVector2D(2154.f/2172,254.f/724);
    const float L=Paper?13.f:FMath::Min(18.f,H*.27f),T=Paper?64.f:FMath::Min(8.f,H*.18f),B=Paper?16.f:T;
    const float PX[]={X,X+L,X+W-L,X+W},PY[]={Y,Y+T,Y+H-B,Y+H};
    const float SX[]={0,Paper?.065f:.04f,Paper?.935f:.96f,1},SY[]={0,Paper?.13f:.21f,Paper?.966f:.79f,1};
    for(int Row=0;Row<3;++Row)for(int Col=0;Col<3;++Col)
    {
        float Left=FMath::RoundToFloat(PX[Col]*Scale),Right=FMath::RoundToFloat(PX[Col+1]*Scale);
        float Top=FMath::RoundToFloat(PY[Row]*Scale),Bottom=FMath::RoundToFloat(PY[Row+1]*Scale);
        DrawTexture(Texture,Left,Top,Right-Left,Bottom-Top,UVMin.X+SX[Col]*UVSize.X,UVMin.Y+SY[Row]*UVSize.Y,
                    (SX[Col+1]-SX[Col])*UVSize.X,(SY[Row+1]-SY[Row])*UVSize.Y,Tint,BLEND_Translucent);
    }
}

void AVoxelHUD::HUDIcon(int Icon,float X,float Y,float W,float H)
{
    static TStrongObjectPtr<UTexture2D> Atlas(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_HUDReference.T_HUDReference")));
    // Only the small UI symbols are read from the reference, never its world.
    static const FVector4 Rects[]={
        FVector4(82,938,66,73),FVector4(438,940,76,70),FVector4(787,931,76,82),FVector4(1094,943,75,73),
        FVector4(1541,157,55,58),FVector4(1146,26,77,86),FVector4(1442,37,46,65),
        FVector4(1538,265,70,77),FVector4(1538,364,70,77),FVector4(1538,462,70,77)};
    if(Icon<0||Icon>=UE_ARRAY_COUNT(Rects))return;
    const FVector4 R=Rects[Icon];
    if(Icon==5)
    {
        // At the lower right of the original flag the score starts. Sample the
        // fabric above the digits and only the pole below it.
        constexpr float Upper=42.f,Pole=16.f;
        MenuImage(Atlas.Get(),X,Y,W,H*Upper/R.W,FVector2D(R.X/1920,R.Y/1080),FVector2D(R.Z/1920,Upper/1080));
        MenuImage(Atlas.Get(),X,Y+H*Upper/R.W,W*Pole/R.Z,H*(1-Upper/R.W),FVector2D(R.X/1920,(R.Y+Upper)/1080),FVector2D(Pole/1920,(R.W-Upper)/1080));
        return;
    }
    MenuImage(Atlas.Get(),X,Y,W,H,FVector2D(R.X/1920,R.Y/1080),FVector2D(R.Z/1920,R.W/1080));
}

void AVoxelHUD::Button(FName ID,const FString& Label,float X,float Y,float W,bool Active,float H,int Icon,bool Selected)
{
    auto* Commander=Cast<AVoxelCommander>(GetOwningPlayerController());
    const bool IsMenu=Commander&&Commander->IsMenuOpen();
    float MX=0,MY=0;
    const bool HasPointer=Commander&&Commander->GetMousePosition(MX,MY);
    const FBox2D Hit(FVector2D(X*Scale,Y*Scale),FVector2D((X+W)*Scale,(Y+H)*Scale));
    float& Hover=MenuHover.FindOrAdd(ID);
    Hover=FMath::FInterpTo(Hover,Active&&HasPointer&&Hit.IsInside(FVector2D(MX,MY))?1.f:0.f,FApp::GetDeltaTime(),14.f);
    if(IsMenu)
    {
        const float Grow=Hover*.025f;
        X-=W*Grow*.5f;
        Y-=H*Grow*.5f;
        W*=1+Grow;
        H*=1+Grow;
    }
    HUDFrame(X,Y,W,H,false,Active?FLinearColor::White:FLinearColor(.46f,.48f,.53f,1));
    if(Active&&(Selected||Hover>.01f))
        Panel(X+7,Y+7,W-14,H-14,FLinearColor(.02f,.42f,.95f,(Selected?.42f:0.f)+Hover*.27f));
    if(Hover>.01f)
    {
        static TStrongObjectPtr<UTexture2D> Glow(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuGoldGlow.T_MenuGoldGlow")));
        const float Brightness=Hover*(IsMenu?.85f:.5f);
        MenuImage(Glow.Get(),X-28,Y+H*.5f-15,56,30,FVector2D(.24,.12),FVector2D(.54,.76),Brightness);
        MenuImage(Glow.Get(),X+W-28,Y+H*.5f-15,56,30,FVector2D(.24,.12),FVector2D(.54,.76),Brightness);
    }
    const float Inset=Icon>=0?H+7:12,Available=W-Inset-12;
    if(Icon>=0)HUDIcon(Icon,X+10,Y+7,H-14,H-14);
    TArray<FString> Lines;Label.ParseIntoArrayLines(Lines,false);
    const float Footer=ID==TEXT("fraction")?16.f:5.f;
    const float RowHeight=(H-5-Footer)/FMath::Max(1,Lines.Num());
    for(int I=0;I<Lines.Num();++I)
    {
        const float Font=I>0?13.f:IsMenu?19.f:Icon>=0?17.f:16.f;
        HUDText(Lines[I],X+Inset,Y+5+I*RowHeight,Font,Active?Cream:FLinearColor(.67f,.70f,.74f));
        Labels.Last().TextBox=FVector2D(Available,RowHeight);
        Labels.Last().CenterHorizontally=Icon<0;
    }
    if(ID==TEXT("fraction"))
    {
        auto* B=Commander?Commander->Battle():nullptr;
        const float BW=FMath::Min(138.f,W-Inset-18);
        Panel(X+Inset,Y+H-14,BW,6,FLinearColor(.68f,.51f,.24f));Panel(X+Inset+1,Y+H-13,BW-2,4,Navy);
        if(B)Panel(X+Inset+1,Y+H-13,(BW-2)*B->DispatchFraction,4,FLinearColor(.15f,.66f,1));
        for(int K=1;K<4;++K)Panel(X+Inset+BW*K*.25f,Y+H-13,1,4,Navy);
    }
    FButton Button;Button.ID=ID;Button.Rect=Hit;Button.Active=Active;Buttons.Add(Button);
}

void AVoxelHUD::DrawBattleTop(AVoxelBattle* B,AVoxelCommander* P,float W)
{
    HUDFrame(8,8,W-182,78);Button(TEXT("pause"),B->Paused?TEXT("继续 [空格]"):TEXT("暂停 [空格]"),W-165,25,149,true,43);
    static TStrongObjectPtr<UTexture2D> Logo(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuLogo.T_MenuLogo")));
    MenuImage(Logo.Get(),22,12,230,50,FVector2D(.025,.08),FVector2D(.95,.77));
    const TCHAR* WeaponStages[]={TEXT("待机"),TEXT("射击"),TEXT("过热"),TEXT("冷却完成")};
    const TCHAR* SoldierStages[]={TEXT("多角度"),TEXT("行军"),TEXT("遭遇战"),TEXT("双方编队")};
    FString Sub=P->SoldierPreviewActive?FString::Printf(TEXT("士兵预览 · %s · F8 切换"),SoldierStages[FMath::Clamp(P->SoldierPreviewStage,0,3)]):P->WeaponPreviewActive?FString::Printf(TEXT("武器预览 · %s · F8 切换"),WeaponStages[FMath::Clamp(P->WeaponPreviewStage,0,3)]):P->PoisonPreviewActive?TEXT("毒箭效果预览"):P->MeteorPreviewActive?TEXT("陨石效果预览"):B->Tutorial?TEXT("领主训练场 / 新手教学"):B->AIEnabled?TEXT("河谷大战 / 标准战役 · AI 对抗"):TEXT("河谷大战 / 自由演练 · AI 关闭");
    HUDText(Sub,34,65,11,Cream);
    Button(TEXT("menu"),TEXT("主菜单 [Esc]"),356,25,148,true,43);
    HUDText(TEXT("战场速度"),540,12,12,Gold);
    bool CanChange=!B->Winner&&!(B->Tutorial&&B->TutorialObjectiveComplete);
    const bool FastLesson=B->TutorialFastForward&&B->TutorialCanFastForward();
    Button(TEXT("speed1"),TEXT("1×"),540,35,64,CanChange,35,-1,!FastLesson&&B->GetPlaybackSpeed()==1);
    Button(TEXT("speed2"),TEXT("2×"),610,35,64,CanChange,35,-1,!FastLesson&&B->GetPlaybackSpeed()==2);
    int Friendly=0,Enemy=0;for(const auto& Tower:B->Towers){Friendly+=Tower.Team==1;Enemy+=Tower.Team==2;}
    HUDIcon(5,W-621,20,45,54);HUDText(TEXT("我方 / 敌方据点"),W-566,14,12,Gold);
    HUDText(FString::Printf(TEXT("%02d"),Friendly),W-566,33,31,FLinearColor(.16f,.66f,1));
    HUDText(TEXT("/"),W-505,34,28,Cream);HUDText(FString::Printf(TEXT("%02d"),Enemy),W-481,33,31,FLinearColor(1,.20f,.14f));
    Panel(W-384,19,1,49,FLinearColor(.44f,.58f,.7f,.6f));HUDIcon(6,W-367,25,27,41);
    HUDText(B->Tutorial?TEXT("新手教学 · 无时限"):B->TimeLimitEnabled?FString::Printf(TEXT("%d 分钟时限"),FMath::RoundToInt(B->BattleTimeLimit/60)):TEXT("自由演练 · 无时限"),W-323,13,11,Gold);
    HUDText(FString::Printf(TEXT("%02d:%02d"),int(B->Time)/60,int(B->Time)%60),W-323,30,27,Cream);
    HUDText(B->Winner?TEXT("战役结束"):B->Paused?TEXT("已暂停"):B->TutorialObjectiveComplete?TEXT("本课完成"):B->IsThinking()?TEXT("思考时间 0.2×"):FastLesson?TEXT("教学快进 5×"):B->GetPlaybackSpeed()==2?TEXT("战场加速 2×"):TEXT("正常速度 1×"),W-322,64,10,B->IsThinking()?Gold:Cream);
}

void AVoxelHUD::DrawIdleOrders(float RX,float Y)
{
    const FLinearColor Ink(.025f,.065f,.13f),Muted(.12f,.16f,.20f);
    const TCHAR* Titles[]={TEXT("选择据点"),TEXT("调遣军队"),TEXT("清空分城")};
    const TCHAR* LineA[]={TEXT("左键点击蓝旗据点，"),TEXT("右键据点进攻，"),TEXT("占领敌方分城和村庄，")};
    const TCHAR* LineB[]={TEXT("查看驻军与防御属性。"),TEXT("右键空地部署驻守。"),TEXT("解除主塔保护后决战。")};
    for(int I=0;I<3;++I)
    {
        float RY=Y+I*94;
        Panel(RX+17,RY+83,226,1,FLinearColor(.71f,.54f,.26f,.55f));
        HUDIcon(7+I,RX+17,RY,48,53);HUDText(Titles[I],RX+72,RY+4,18,Ink,true);
        Text(LineA[I],RX+72,RY+37,11,Muted);Text(LineB[I],RX+72,RY+59,11,Muted);
    }
    float T=Y+302;
    HUDText(TEXT("B 选址建桥：75 金 / 60 秒"),RX+20,T,11,Ink,true);
    Text(TEXT("左键野战部队，可重新调遣。"),RX+20,T+26,11,Muted);
    HUDText(TEXT("陨石伤及双方部队，也能毁桥。"),RX+20,T+53,10,FLinearColor(.65f,.10f,.045f),true);
}
