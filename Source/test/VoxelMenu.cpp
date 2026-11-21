#include "VoxelWar.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "UObject/StrongObjectPtr.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
const FLinearColor MenuInk(.025f,.042f,.038f,1), MenuWhite(.96f,.91f,.77f,1);
const TCHAR* PreferencesSection=TEXT("/Script/test.VoxelPreferences");
constexpr float FrameLimits[]={30,60,120,0};
constexpr float CameraSpeeds[]={.7f,1.f,1.4f};
}

void AVoxelBattle::ResetBattle(bool Practice,bool ForTutorial)
{
    StopBattleAudio();LastRangeSignature=MAX_uint32;
    bool MapChanged=Tutorial!=ForTutorial;Tutorial=ForTutorial;
    TutorialStep=0;TutorialBridgeRow=0;TutorialObjectiveComplete=false;TutorialFastForward=false;TutorialRaidID=TutorialBlueDemoID=TutorialRedDemoID=INDEX_NONE;
    CancelCommand();Towers.Empty();Squads.Empty();Effects.Empty();MeteorBursts.Empty();WeaponShots.Empty();Bridges.Empty();AIOrders.Empty();
    for(int Team=0;Team<3;++Team){Gold[Team]=0;FinalHoldings[Team]=0;FinalTroops[Team]=0;}
    Time=0;MeteorReady=0;AISince=0;Accumulator=0;NextAIAction=0;NextAIUpgrade=45;NextAIBridge=45;NextSquadID=1;
    DispatchFraction=.5f;PlaybackSpeed=1;Winner=0;TimeLimitReached=false;Paused=false;MenuPaused=false;
    AIEnabled=!(Practice||Tutorial);TimeLimitEnabled=!(Practice||Tutorial);LastTowerSignature=MAX_uint32;
    if(Tutorial){InitializeTutorialLayout();Gold[1]=250;BeginTutorialLesson(0);}else InitializeLayout();
    if(MapChanged)RebuildTerrain();
    if(Tutorial)return;
    Message(Practice?TEXT("自由演练：敌方不会主动调兵，无对局时限。可练习占领、建桥、升级与技能。"):TEXT("战役开始：先占村庄发展，筹建桥梁；夺取敌方全部分城后进攻主塔。"));
}

void AVoxelCommander::LoadPreferences()
{
    if(GConfig)GConfig->GetBool(PreferencesSection,TEXT("TutorialCompleted"),TutorialCompleted,GGameUserSettingsIni);
    float SavedSpeed=1;
    if(GConfig)GConfig->GetFloat(PreferencesSection,TEXT("CameraSpeed"),SavedSpeed,GGameUserSettingsIni);
    CameraSpeed=FMath::Clamp(SavedSpeed,.7f,1.4f);
}
void AVoxelCommander::MarkTutorialCompleted()
{
    TutorialCompleted=true;
    if(GConfig&&GetWorld()->HasBegunPlay())
    {GConfig->SetBool(PreferencesSection,TEXT("TutorialCompleted"),true,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
}
void AVoxelCommander::LoadSettingsDraft()
{
    if(auto* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
    {
        DraftQuality=FMath::Clamp(Settings->GetOverallScalabilityLevel(),-1,3);
        float Limit=Settings->GetFrameRateLimit();DraftFrameLimit=3;
        for(int I=0;I<3;++I)if(FMath::IsNearlyEqual(Limit,FrameLimits[I],1.f)){DraftFrameLimit=I;break;}
        DraftVSync=Settings->IsVSyncEnabled();
    }
    DraftCameraSpeed=CameraSpeed<.85f?0:CameraSpeed>1.2f?2:1;
}
void AVoxelCommander::ApplySettingsDraft()
{
    DraftQuality=FMath::Clamp(DraftQuality,-1,3);DraftFrameLimit=FMath::Clamp(DraftFrameLimit,0,3);DraftCameraSpeed=FMath::Clamp(DraftCameraSpeed,0,2);
    if(auto* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
    {
        if(DraftQuality>=0)Settings->SetOverallScalabilityLevel(DraftQuality);
        Settings->SetFrameRateLimit(FrameLimits[DraftFrameLimit]);Settings->SetVSyncEnabled(DraftVSync);
        Settings->ApplyNonResolutionSettings();Settings->SaveSettings();
    }
    CameraSpeed=CameraSpeeds[DraftCameraSpeed];
    if(GConfig){GConfig->SetFloat(PreferencesSection,TEXT("CameraSpeed"),CameraSpeed,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    MenuNotice=TEXT("设置已应用并保存，下次启动继续生效。");
}
void AVoxelCommander::ShowMenu(EVoxelMenuPage Page)
{
    MenuPage=Page;MenuNotice.Empty();
    if(auto* B=Battle())B->MenuPaused=IsMenuOpen();
    if(Page==EVoxelMenuPage::Settings)LoadSettingsDraft();
}
void AVoxelCommander::StartBattle()
{
    if(auto* B=Battle())
    {
        bool LeavingSoldierPreview=SoldierPreviewActive;SoldierPreviewActive=false;
        B->ResetBattle(SelectedMode==1,SelectedMode==2);BattleStarted=true;MeteorPreviewActive=false;PoisonPreviewActive=false;WeaponPreviewActive=false;SoldierPreviewActive=false;MenuPage=EVoxelMenuPage::None;MenuNotice.Empty();
        if(LeavingSoldierPreview)B->RebuildTerrain();
        Focus=FVector(B->Towers[0].Cell.X*B->Tile+900,B->Towers[0].Cell.Y*B->Tile+900,0);Zoom=8500;if(B->Tutorial)B->FocusTutorialObjective();
    }
}
void AVoxelCommander::ResumeBattle()
{
    if(auto* B=Battle();B&&BattleStarted&&!B->Winner)
    {B->MenuPaused=false;B->Paused=false;MenuPage=EVoxelMenuPage::None;MenuNotice.Empty();}
}
void AVoxelCommander::ConfirmKey()
{
    if(MenuPage==EVoxelMenuPage::Main){if(BattleStarted&&Battle()&&!Battle()->Winner)ResumeBattle();else MenuAction(TEXT("start"));}
    else if(MenuPage==EVoxelMenuPage::Setup)MenuAction(TEXT("start"));
    else if(MenuPage==EVoxelMenuPage::ConfirmNew)MenuAction(TEXT("confirmNew"));
    else if(MenuPage==EVoxelMenuPage::ConfirmQuit)MenuAction(TEXT("confirmQuit"));
}
void AVoxelCommander::MenuAction(FName Action)
{
    if(!IsMenuOpen())return;
    if(Action==TEXT("tutorial")){SelectedMode=2;MenuAction(TEXT("start"));return;}
    if(Action==TEXT("resume")){ResumeBattle();return;}
    if(Action==TEXT("start"))
    {
        if(auto* B=Battle();BattleStarted&&B&&!B->Winner)ShowMenu(EVoxelMenuPage::ConfirmNew);else StartBattle();return;
    }
    if(Action==TEXT("confirmNew")){if(MenuPage==EVoxelMenuPage::ConfirmNew)StartBattle();return;}
    if(Action==TEXT("setup")){ShowMenu(EVoxelMenuPage::Setup);return;}
    if(Action==TEXT("settings")){ShowMenu(EVoxelMenuPage::Settings);return;}
    if(Action==TEXT("guide")){ShowMenu(EVoxelMenuPage::Guide);return;}
    if(Action==TEXT("back")){ShowMenu();return;}
    if(Action==TEXT("quit")){ShowMenu(EVoxelMenuPage::ConfirmQuit);return;}
    if(Action==TEXT("confirmQuit")){if(MenuPage==EVoxelMenuPage::ConfirmQuit)UKismetSystemLibrary::QuitGame(this,this,EQuitPreference::Quit,false);return;}
    if(MenuPage==EVoxelMenuPage::Setup)
    {if(Action==TEXT("standard"))SelectedMode=0;else if(Action==TEXT("practice"))SelectedMode=1;else if(Action==TEXT("tutorialMode"))SelectedMode=2;return;}
    if(MenuPage!=EVoxelMenuPage::Settings)return;
    MenuNotice.Empty();
    if(Action==TEXT("quality"))DraftQuality=(DraftQuality+1)%4;
    else if(Action==TEXT("frames"))DraftFrameLimit=(DraftFrameLimit+1)%4;
    else if(Action==TEXT("vsync"))DraftVSync=!DraftVSync;
    else if(Action==TEXT("camera"))DraftCameraSpeed=(DraftCameraSpeed+1)%3;
    else if(Action==TEXT("defaults")){DraftQuality=2;DraftFrameLimit=1;DraftVSync=false;DraftCameraSpeed=1;MenuNotice=TEXT("默认选项已填入，点击「应用设置」生效。");}
    else if(Action==TEXT("apply"))ApplySettingsDraft();
}

void AVoxelHUD::MenuImage(UTexture2D* Texture,float X,float Y,float W,float H,FVector2D UVMin,FVector2D UVSize,float Alpha)
{
    if(Texture)DrawTexture(Texture,X*Scale,Y*Scale,W*Scale,H*Scale,UVMin.X,UVMin.Y,UVSize.X,UVSize.Y,FLinearColor(1,1,1,Alpha),BLEND_Translucent);
}
void AVoxelHUD::MenuButton(FName ID,const FString& Label,int IconRow,float X,float Y,float W,float H)
{
    float MX=0,MY=0;GetOwningPlayerController()->GetMousePosition(MX,MY);
    const FBox2D Hit(FVector2D(X*Scale,Y*Scale),FVector2D((X+W)*Scale,(Y+H)*Scale));
    float& Blend=MenuHover.FindOrAdd(ID);
    Blend=FMath::FInterpTo(Blend,Hit.IsInside(FVector2D(MX,MY))?1.f:0.f,FApp::GetDeltaTime(),12.f);
    const float Grow=Blend*.035f;X-=W*Grow*.5f;Y-=H*Grow*.5f;W*=1+Grow;H*=1+Grow;
    static TStrongObjectPtr<UTexture2D> Resume(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuResume.T_MenuResume")));
    static TStrongObjectPtr<UTexture2D> Glow(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuGoldGlow.T_MenuGoldGlow")));
    static TStrongObjectPtr<UMaterialInterface> ArtMaterial(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/UI/M_MenuButtonArt.M_MenuButtonArt")));
    static TMap<FName,TStrongObjectPtr<UMaterialInstanceDynamic>> Faces,Edges;

    // Preserve the actual reference lettering, icon, facets and carved border.
    // Clip to its silhouette in UV space instead of carrying over landscape pixels.
    static const float RowY[]={440,524,607,690,774,858};
    const int Row=FMath::Clamp(IconRow,0,5);
    FVector2D UVMin(105.f/1920,RowY[Row]/1080),UVSize(495.f/1920,(Row==5?77.f:74.f)/1080);
    UTexture2D* Source=MenuReference;
    if(IconRow<0){Source=Resume.Get();UVMin=FVector2D(83.f/2109,199.f/745);UVSize=FVector2D(1944.f/2109,347.f/745);}

    auto DrawArt=[&](float PX,float PY,float PW,float PH,UMaterialInterface* Material)
    {
        static const FVector2D Outline[]={{.040f,.010f},{.947f,.010f},{.997f,.225f},{.997f,.760f},{.963f,.985f},{.036f,.985f},{.003f,.780f},{.003f,.235f}};
        TArray<FCanvasUVTri> Triangles;Triangles.Reserve(8);
        auto Pos=[&](FVector2D P){return (FVector2D(PX,PY)+P*FVector2D(PW,PH))*Scale;};
        auto UV=[&](FVector2D P){return UVMin+P*UVSize;};
        for(int K=0;K<8;++K)
        {
            FCanvasUVTri T;T.V0_Pos=Pos(FVector2D(.5,.5));T.V0_UV=UV(FVector2D(.5,.5));
            T.V1_Pos=Pos(Outline[K]);T.V1_UV=UV(Outline[K]);T.V2_Pos=Pos(Outline[(K+1)%8]);T.V2_UV=UV(Outline[(K+1)%8]);
            T.V0_Color=T.V1_Color=T.V2_Color=FLinearColor::White;Triangles.Add(T);
        }
        if(Material)Canvas->K2_DrawMaterialTriangle(Material,MoveTemp(Triangles));else Canvas->K2_DrawTriangle(Source,MoveTemp(Triangles));
    };
    UMaterialInstanceDynamic* Face=nullptr;UMaterialInstanceDynamic* Edge=nullptr;
    if(ArtMaterial.IsValid())
    {
        if(!Faces.Contains(ID))Faces.Add(ID,TStrongObjectPtr<UMaterialInstanceDynamic>(UMaterialInstanceDynamic::Create(ArtMaterial.Get(),GetTransientPackage())));
        if(!Edges.Contains(ID))Edges.Add(ID,TStrongObjectPtr<UMaterialInstanceDynamic>(UMaterialInstanceDynamic::Create(ArtMaterial.Get(),GetTransientPackage())));
        Face=Faces[ID].Get();Edge=Edges[ID].Get();
        Face->SetTextureParameterValue(TEXT("Art"),Source);Face->SetScalarParameterValue(TEXT("Highlight"),Blend);Face->SetScalarParameterValue(TEXT("BorderOnly"),0);
        Edge->SetTextureParameterValue(TEXT("Art"),Source);Edge->SetScalarParameterValue(TEXT("Highlight"),Blend);Edge->SetScalarParameterValue(TEXT("BorderOnly"),1);
    }
    if(Blend>.005f)
    {
        // Luminous copy of the original sculpted rim; the face and lettering stay sharp.
        if(Edge)DrawArt(X-2.5f,Y-2.5f,W+5,H+5,Edge);
        const float Pulse=.91f+.09f*FMath::Sin(GetWorld()->GetRealTimeSeconds()*2.7f);
        for(int Side=0;Side<2;++Side)
            MenuImage(Glow.Get(),X+(Side?W:0)-58,Y+H*.5f-30,116,60,FVector2D(.24,.12),FVector2D(.54,.76),Blend*Pulse*.88f);
    }
    // The normal state uses the same texture path as the original UI art,
    // retaining its exact navy, cream and gold colors. Hover is a separate layer.
    DrawArt(X,Y,W,H,nullptr);
    if(Face&&Blend>.005f)DrawArt(X,Y,W,H,Face);
    if(Blend>.005f)
        for(int Side=0;Side<2;++Side)
            MenuImage(Glow.Get(),X+(Side?W:0)-31,Y+H*.5f-16,62,32,FVector2D(.24,.12),FVector2D(.54,.76),Blend*.72f);

    // Alternate selected modes retain live labels with the same heavy cream/ink treatment.
    auto* Commander=Cast<AVoxelCommander>(GetOwningPlayerController());
    if(ID==TEXT("start")&&Commander&&Commander->SelectedMode!=0)
    {
        // The far-right blank face patch contains no lettering or border.
        MenuImage(MenuReference,X+W*.239f,Y+H*.18f,W*.665f,H*.66f,FVector2D(545.f/1920,541.f/1080),FVector2D(20.f/1920,35.f/1080));
        Text(Label,X+W*.25f,Y+H*.22f,23,MenuWhite);Labels.Last().MenuBold=true;
    }
    FButton B;B.ID=ID;B.Rect=Hit;B.Active=true;Buttons.Add(B);
}

void AVoxelHUD::DrawFrontEnd(AVoxelCommander* P,float W,float H)
{
    if(MenuBackground)
    {
        float Aspect=float(MenuBackground->GetSizeX())/FMath::Max(1,MenuBackground->GetSizeY());
        float BW=FMath::Max(W,H*Aspect),BH=BW/Aspect;
        MenuImage(MenuBackground,(W-BW)*.5f,(H-BH)*.5f,BW,BH);
    }
    else Panel(0,0,W,H,FLinearColor(.012f,.035f,.065f,.85f));
    for(int I=0;I<180;++I)
    {
        float Left=FMath::RoundToFloat(I*W*.58f/180*Scale),Right=FMath::RoundToFloat((I+1)*W*.58f/180*Scale);
        DrawRect(FLinearColor(.003f,.013f,.025f,.68f*FMath::Pow(1-I/180.f,1.7f)),Left,0,Right-Left,ViewportSize.Y);
    }
    Panel(0,H-54,W,54,FLinearColor(.005f,.012f,.02f,.65f));
    Text(TEXT("单机策略 · 可玩原型"),W-218,30,12,MenuWhite);
    Text(TEXT("以城为疆，以谋定胜。"),46,H-37,13,MenuWhite);
    Text(TEXT("VOXEL DOMINION  ·  DEMO"),W-268,H-37,11,FLinearColor(.42f,.68f,.94f));
    if(P->MenuPage==EVoxelMenuPage::Main)
    {
        static TStrongObjectPtr<UTexture2D> Logo(LoadObject<UTexture2D>(nullptr,TEXT("/Game/Voxel/UI/T_MenuLogo.T_MenuLogo")));
        MenuImage(Logo.Get(),53,13,489,220);
        Text(TEXT("集结军队，跨越河谷，夺取每一座城。"),82,218,17,MenuWhite);
        const bool CanResume=P->BattleStarted&&P->Battle()&&!P->Battle()->Winner;
        const int Count=CanResume?7:6;
        const float Gap=9,ButtonH=FMath::Min(56.f,(H-344-(Count-1)*Gap)/Count);
        float Y=264;
        if(CanResume){MenuButton(TEXT("resume"),TEXT("继续战役"),-1,82,Y,370,ButtonH);Y+=ButtonH+Gap;}
        MenuButton(TEXT("tutorial"),P->TutorialCompleted?TEXT("新手教程 · 重温训练"):TEXT("新手教程 · 首次推荐"),0,82,Y,370,ButtonH);Y+=ButtonH+Gap;
        MenuButton(TEXT("start"),P->SelectedMode==2?TEXT("开始新手教程"):P->SelectedMode==1?TEXT("开始自由演练"):TEXT("开始标准战役（AI 对抗）"),1,82,Y,370,ButtonH);Y+=ButtonH+Gap;
        MenuButton(TEXT("setup"),TEXT("模式与关卡"),2,82,Y,370,ButtonH);Y+=ButtonH+Gap;
        MenuButton(TEXT("settings"),TEXT("游戏设置"),3,82,Y,370,ButtonH);Y+=ButtonH+Gap;
        MenuButton(TEXT("guide"),TEXT("操作指南"),4,82,Y,370,ButtonH);Y+=ButtonH+Gap;
        MenuButton(TEXT("quit"),TEXT("退出游戏"),5,82,Y,370,ButtonH);
        Text(CanResume?TEXT("回车继续战役 · 菜单中战场时间停止"):TEXT("回车开始 · 先部署，再出征"),W*.42f,H-37,11,MenuWhite);
        const float CX=W-478,CY=H-294;
        MenuPlate(CX-7,CY-7,425,221,FLinearColor(.72f,.44f,.15f),FLinearColor(.23f,.10f,.035f));
        MenuPlate(CX-3,CY-3,417,213,FLinearColor(1,.77f,.31f),FLinearColor(.48f,.29f,.10f));
        MenuPlate(CX,CY,411,207,FLinearColor(.025f,.075f,.13f),FLinearColor(.008f,.024f,.055f));
        MenuPlate(CX+16,CY-10,200,32,FLinearColor(1,.80f,.35f),FLinearColor(.64f,.33f,.06f));
        Text(TEXT("已选战场 / 第一章"),CX+29,CY-4,15,MenuInk);
        Text(P->SelectedMode==2?TEXT("领主训练场"):TEXT("河谷大战"),CX+25,CY+39,34,MenuWhite);
        Text(P->SelectedMode==2?TEXT("新手教学 · 12 课 · 分步引导"):P->SelectedMode==0?TEXT("标准战役 · 双方交锋 · 20 分钟"):TEXT("自由演练 · 静态防守 · 无时限"),CX+27,CY+95,14,MenuWhite);
        Panel(CX+20,CY+134,371,1,FLinearColor(.58f,.44f,.21f));
        Text(P->SelectedMode==2?TEXT("6 座据点   /   基础机制   /   实操教学"):TEXT("40 座据点    /    10 座村庄    /    分叉水系"),CX+26,CY+159,12,MenuWhite);
        return;
    }

    const float X=74,Y=106,CardW=796,CardH=FMath::Min(688.f,H-197);
    const float L=X+35,InnerW=CardW-70,Footer=Y+CardH-79;
    const FLinearColor BrightGold(1.f,.75f,.28f);
    const FLinearColor Accent(.57f,.77f,.98f);
    const FLinearColor Body(.79f,.86f,.93f);
    const FLinearColor Well(.013f,.033f,.075f,.90f);
    const FLinearColor WellEdge(.20f,.40f,.65f,.55f);

    const bool Setup=P->MenuPage==EVoxelMenuPage::Setup;
    const bool Settings=P->MenuPage==EVoxelMenuPage::Settings;
    const bool Guide=P->MenuPage==EVoxelMenuPage::Guide;
    const bool Quitting=P->MenuPage==EVoxelMenuPage::ConfirmQuit;
    const TCHAR* Title=Setup?TEXT("模式与关卡"):Settings?TEXT("游戏设置"):Guide?TEXT("领主手册"):
        Quitting?TEXT("退出游戏"):TEXT("开始新战役");
    const TCHAR* Summary=Setup?TEXT("选择战役规则和出征地点。"):Settings?TEXT("点击右侧按钮切换选项，应用后保存到本机。"):
        Guide?TEXT("战场操作与快捷键"):Quitting?TEXT("退出后，当前战役进度将丢失。"):TEXT("新战役将替换当前进度。");

    // Keep the landscape visible around the panel and repeat the main menu's carved frame.
    HUDFrame(X,Y,CardW,CardH);
    MenuPlate(X+16,Y+109,CardW-32,CardH-130,FLinearColor(.024f,.067f,.133f),FLinearColor(.007f,.023f,.063f));
    Panel(L,Y+105,InnerW,1,FLinearColor(.65f,.42f,.18f,.85f));
    Panel(L+2,Y+106,InnerW-4,1,FLinearColor(.11f,.28f,.47f,.9f));
    if(Settings)
        MenuImage(MenuReference,L,Y+23,50,48,FVector2D(143.f/1920,700.f/1080),FVector2D(57.f/1920,56.f/1080));
    else
        HUDIcon(Setup?3:Guide?4:5,L,Y+23,48,48);
    HUDText(Title,L+67,Y+14,32,BrightGold);
    Text(Summary,L+68,Y+63,15,Body);

    auto Section=[&](const TCHAR* Name,float Top)
    {
        HUDText(Name,L,Top,15,BrightGold);
        Panel(L+118,Top+13,InnerW-118,1,WellEdge);
    };
    auto InsetRow=[&](float Top,float Height)
    {
        MenuPlate(L,Top,InnerW,Height,WellEdge,WellEdge);
        MenuPlate(L+1,Top+1,InnerW-2,Height-2,Well,Well);
    };
    Panel(L,Footer-17,InnerW,1,FLinearColor(.58f,.38f,.16f,.7f));
    Button(TEXT("back"),TEXT("返回主菜单"),L,Footer,205,true,49);
    if(Setup)
    {
        Section(TEXT("游戏模式"),Y+128);
        const TCHAR* ModeLabels[]={TEXT("标准战役"),TEXT("自由演练"),TEXT("新手教学")};
        const FName ModeIDs[]={TEXT("standard"),TEXT("practice"),TEXT("tutorialMode")};
        for(int I=0;I<3;++I)
        {
            const bool Chosen=P->SelectedMode==I;
            const FString Label=Chosen?FString(ModeLabels[I])+TEXT(" · 已选"):FString(ModeLabels[I]);
            Button(ModeIDs[I],Label,L+I*248,Y+161,230,true,51,-1,Chosen);
        }
        Text(P->SelectedMode==2?TEXT("12 课实操，学习派兵、升级、箭种、遭遇战、建桥与陨石。"):
            P->SelectedMode==0?TEXT("与 AI 争夺领地，20 分钟时按据点与兵力结算。"):
            TEXT("敌方据点保留防御，无时限，可练习建桥、升级和作战。"),L,Y+226,14,Body);

        Section(TEXT("战场关卡"),Y+283);
        const bool Tutorial=P->SelectedMode==2;
        Button(TEXT("riverlands"),Tutorial?TEXT("领主训练场 · 已选"):TEXT("河谷大战 · 已选"),L,Y+316,354,true,54,3,true);
        Button(TEXT("lockedMap"),TEXT("更多战场 · 后续开放"),L+374,Y+316,352,false,54);
        InsetRow(Y+392,96);
        HUDText(Tutorial?TEXT("训练场概况"):TEXT("河谷战场概况"),L+19,Y+403,15,Accent);
        Text(Tutorial?TEXT("21 × 15 格 · 6 座据点 · 250 金教学物资 · 可重试本课"):
            TEXT("分叉水系 · 六片陆地 · 40 座据点 · 自由选址修桥"),L+19,Y+431,13,Body);
        Text(Tutorial?TEXT("完成派兵、驻守与攻占据点等分步目标。"):
            TEXT("先夺取敌方全部分城与村庄，再进攻敌方主城。"),L+19,Y+456,13,Body);
        Button(TEXT("start"),Tutorial?TEXT("进入训练场  →"):TEXT("开始这场战役  →"),L+InnerW-270,Footer,270,true,49,-1,true);
    }
    else if(Settings)
    {
        const TCHAR* Quality[]={TEXT("自定义"),TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")};
        const TCHAR* Frames[]={TEXT("30 帧"),TEXT("60 帧"),TEXT("120 帧"),TEXT("不限")};
        const TCHAR* Speeds[]={TEXT("舒缓"),TEXT("标准"),TEXT("快速")};
        auto Row=[&](const TCHAR* Name,FName ID,const TCHAR* Value,float Top)
        {
            InsetRow(Top,64);
            HUDText(Name,L+19,Top+16,16,MenuWhite);
            Button(ID,FString(Value)+TEXT("   ›"),L+InnerW-264,Top+9,254,true,46);
        };
        Row(TEXT("画面品质"),TEXT("quality"),Quality[FMath::Clamp(P->DraftQuality,-1,3)+1],Y+135);
        Row(TEXT("帧率上限"),TEXT("frames"),Frames[FMath::Clamp(P->DraftFrameLimit,0,3)],Y+210);
        Row(TEXT("垂直同步"),TEXT("vsync"),P->DraftVSync?TEXT("开启"):TEXT("关闭"),Y+285);
        Row(TEXT("镜头移动速度"),TEXT("camera"),Speeds[FMath::Clamp(P->DraftCameraSpeed,0,2)],Y+360);
        InsetRow(Y+453,52);
        Text(P->MenuNotice.IsEmpty()?TEXT("开启垂直同步后，实际帧率受显示器刷新率限制。"):P->MenuNotice,L+19,Y+468,14,
            P->MenuNotice.IsEmpty()?Body:BrightGold);
        Button(TEXT("defaults"),TEXT("恢复默认选项"),L+225,Footer,234,true,49);
        Button(TEXT("apply"),TEXT("应用设置"),L+479,Footer,247,true,49,-1,true);
    }
    else if(Guide)
    {
        const TCHAR* Keys[]={TEXT("左键 / 右键"),TEXT("Tab / U / 1、2"),TEXT("B / M"),TEXT("WASD / 滚轮 / Home"),TEXT("空格 / Esc / R")};
        const TCHAR* Help[]={TEXT("选择据点或部队 / 出兵、移动、增援"),TEXT("切换出兵比例 / 升级 / 选择永久箭种"),
            TEXT("河道选址修桥 / 释放陨石"),TEXT("移动镜头 / 缩放 / 查看战场全图"),TEXT("暂停 / 取消选择或打开菜单 / 重开")};
        for(int I=0;I<5;++I)
        {
            const float Top=Y+128+I*56;
            InsetRow(Top,47);
            MenuPlate(L+9,Top+6,209,35,FLinearColor(.065f,.17f,.30f),FLinearColor(.02f,.06f,.13f));
            HUDText(Keys[I],L+20,Top+6,15,BrightGold);
            Labels.Last().TextBox=FVector2D(188,35);
            Labels.Last().CenterHorizontally=true;
            Text(Help[I],L+237,Top+11,15,Body);
        }
        Section(TEXT("战场规则"),Y+421);
        Text(TEXT("顶部 1× / 2× 切换战场速度，选中单位时进入 0.2× 思考时间。"),L,Y+454,14,Body);
        Text(TEXT("修桥需 75 金，工期 60 秒。陨石需 55 金，冷却 180 秒，可毁桥。"),L,Y+485,14,Body);
        Text(TEXT("右键空地部署驻守。守军自动拦截敌军，追击结束后返回驻守点。"),L,Y+516,14,Body);
    }
    else
    {
        InsetRow(Y+143,82);
        HUDText(Quitting?TEXT("设置保存在本机。"):TEXT("可返回主菜单，继续当前战役。"),L+19,Y+167,17,Body);
        Button(Quitting?TEXT("confirmQuit"):TEXT("confirmNew"),Title,L+InnerW-270,Footer,270,true,49,-1,true);
    }
}
