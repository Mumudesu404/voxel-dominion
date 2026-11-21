#include "VoxelWar.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Components/InstancedStaticMeshComponent.h"

namespace { const FIntPoint TrainingGuard(-3,2); }

void AVoxelBattle::InitializeTutorialLayout()
{
    auto Add=[&](const TCHAR* Name,FIntPoint Cell,int Team,EVoxelModule Module,float Troops,bool Tall=false)
    {FVoxelTower T;T.Name=Name;T.Cell=Cell;T.Team=Team;T.Module=Module;T.Troops=Troops;T.Tall=Tall;Towers.Add(T);};
    Add(TEXT("教学主城"),FIntPoint(-7,0),1,EVoxelModule::Command,120);
    Add(TEXT("训练箭楼"),FIntPoint(-5,-3),1,EVoxelModule::Arrow,60,true);
    Add(TEXT("溪畔村庄"),FIntPoint(-4,0),0,EVoxelModule::Village,0);
    Add(TEXT("敌军训练营"),FIntPoint(-7,6),2,EVoxelModule::Cannon,24);
    Add(TEXT("东岸炮堡"),FIntPoint(4,0),2,EVoxelModule::Cannon,25);
    Add(TEXT("敌方指挥城"),FIntPoint(7,0),2,EVoxelModule::Command,30);
}
void AVoxelBattle::RebuildTerrain()
{
    if(Batches.Num()<39)return;
    for(int I=13;I<26;++I)Batches[I]->ClearInstances();
    MakeMap();LastTowerSignature=MAX_uint32;
}
bool AVoxelBattle::TutorialAllowsAction(int32 Step,int32 Tower)
{
    if(!Tutorial)return true;
    if(!TutorialObjectiveComplete&&TutorialStep==Step&&(Tower==INDEX_NONE||Selected==Tower))return true;
    Message(TEXT("请先完成左侧当前教学目标；完成后点击「下一课」。"));return false;
}
bool AVoxelBattle::TutorialAllowsOrder(int32 Team,int32 Target,FIntPoint Destination)
{
    if(!Tutorial||Team!=1)return true;
    bool Allowed=!TutorialObjectiveComplete&&
        ((TutorialStep==1&&Target==2)||
         ((TutorialStep==5||TutorialStep==6)&&Target==INDEX_NONE&&Destination==TrainingGuard)||
         (TutorialStep==10&&(Target==INDEX_NONE||Target==3||Target==4||(Towers.IsValidIndex(Target)&&Towers[Target].Team==1)))||
         (TutorialStep==11&&(Target==INDEX_NONE||Target==5||(Towers.IsValidIndex(Target)&&Towers[Target].Team==1))));
    if(!Allowed)Message(TEXT("本课请按金色标记操作。需要帮助可点击「定位目标」或「重试本课」。"));
    return Allowed;
}
void AVoxelBattle::BeginTutorialLesson(int32 Step)
{
    TutorialStep=FMath::Clamp(Step,0,TutorialLessonCount-1);TutorialObjectiveComplete=false;TutorialFastForward=false;
    TutorialEncounterSeen=false;TutorialBlueHit=false;TutorialRedHit=false;CancelCommand();
    if(Step==6)
    {
        if(DispatchTo(3,CellPosition(TrainingGuard),.5f))TutorialRaidID=Squads.Last().ID;
    }
    if(Step==8)
    {
        MeteorReady=0;
        for(int Team=1;Team<=2;++Team)
        {
            FVoxelSquad S;S.ID=NextSquadID++;S.Team=Team;S.Target=INDEX_NONE;S.Troops=50;
            S.Destination=FIntPoint(Team==1?-1:1,TutorialBridgeRow);S.Position=CellPosition(S.Destination)+FVector(0,0,27);
            S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;Squads.Add(S);
            if(Team==1)TutorialBlueDemoID=S.ID;else TutorialRedDemoID=S.ID;
        }
    }
    TutorialCheckpoint.Towers=Towers;TutorialCheckpoint.Squads=Squads;TutorialCheckpoint.Bridges=Bridges;TutorialCheckpoint.Effects=Effects;
    for(int I=0;I<3;++I)TutorialCheckpoint.Gold[I]=Gold[I];
    TutorialCheckpoint.Time=Time;TutorialCheckpoint.MeteorReady=MeteorReady;TutorialCheckpoint.NextID=NextSquadID;
    TutorialCheckpoint.RaidID=TutorialRaidID;TutorialCheckpoint.BlueID=TutorialBlueDemoID;TutorialCheckpoint.RedID=TutorialRedDemoID;TutorialCheckpoint.BridgeRow=TutorialBridgeRow;
    FocusTutorialObjective();Message(FString::Printf(TEXT("新手教学 %d / %d：%s"),Step+1,TutorialLessonCount,*TutorialTitle()));
}
void AVoxelBattle::NextTutorialLesson()
{
    if(!Tutorial||MenuPaused||Paused||Winner||!TutorialObjectiveComplete||TutorialStep>=TutorialLessonCount-1)return;
    BeginTutorialLesson(TutorialStep+1);
}
void AVoxelBattle::RetryTutorialLesson()
{
    if(!Tutorial||MenuPaused||Paused||Winner)return;
    StopBattleAudio();LastRangeSignature=MAX_uint32;
    Towers=TutorialCheckpoint.Towers;Squads=TutorialCheckpoint.Squads;Bridges=TutorialCheckpoint.Bridges;Effects=TutorialCheckpoint.Effects;MeteorBursts.Empty();WeaponShots.Empty();
    for(auto& S:Squads)S.HasPreviousPose=false;
    for(auto& T:Towers){T.ShotAge=100;T.SpinSpeed=0;}
    for(int I=0;I<3;++I)Gold[I]=TutorialCheckpoint.Gold[I];
    Time=TutorialCheckpoint.Time;MeteorReady=TutorialCheckpoint.MeteorReady;NextSquadID=TutorialCheckpoint.NextID;AISince=0;Accumulator=0;AIOrders.Empty();
    TutorialRaidID=TutorialCheckpoint.RaidID;TutorialBlueDemoID=TutorialCheckpoint.BlueID;TutorialRedDemoID=TutorialCheckpoint.RedID;TutorialBridgeRow=TutorialCheckpoint.BridgeRow;
    TutorialObjectiveComplete=false;TutorialFastForward=false;TutorialEncounterSeen=false;TutorialBlueHit=false;TutorialRedHit=false;
    CancelCommand();LastTowerSignature=MAX_uint32;FocusTutorialObjective();Message(TEXT("已恢复本课开始时的兵力、金币、技能和桥梁，可以重新尝试。"));
}
bool AVoxelBattle::TutorialCanFastForward() const
{int I=BridgeIndex(TutorialBridgeRow);return Tutorial&&!TutorialObjectiveComplete&&(TutorialStep==7||TutorialStep==9)&&I!=INDEX_NONE&&Bridges[I].Remaining>0;}
void AVoxelBattle::ToggleTutorialFastForward()
{
    if(MenuPaused||Paused||Winner||!TutorialCanFastForward())return;
    TutorialFastForward=!TutorialFastForward;CompleteCommand();BridgeMode=false;MeteorMode=false;
}
void AVoxelBattle::UpdateTutorial()
{
    if(!Tutorial||MenuPaused||Paused||TutorialObjectiveComplete||Towers.Num()!=6)return;
    bool Done=false;
    switch(TutorialStep)
    {
    case 0:Done=Selected==0;break;
    case 1:Done=Towers[2].Team==1;break;
    case 2:Done=Towers[2].Team==1&&Towers[2].Level>=2;break;
    case 3:Done=Towers[1].Level>=2;break;
    case 4:Done=Towers[1].PlayerBound&&Towers[1].Arrow!=EVoxelArrow::Unbound;break;
    case 5:Done=Squads.ContainsByPredicate([&](const FVoxelSquad& S){return S.Team==1&&S.Troops>=25&&S.Target==INDEX_NONE&&S.Holding&&S.Destination==TrainingGuard&&FVector::Dist2D(S.Position,CellPosition(TrainingGuard))<80;});break;
    case 6:Done=TutorialEncounterSeen&&SquadIndex(TutorialRaidID)==INDEX_NONE;break;
    case 7:case 9:{int I=BridgeIndex(TutorialBridgeRow);Done=I!=INDEX_NONE&&Bridges[I].Remaining<=0;break;}
    case 8:Done=TutorialBlueHit&&TutorialRedHit&&BridgeIndex(TutorialBridgeRow)==INDEX_NONE&&MeteorBursts.IsEmpty();break;
    case 10:Done=Towers[3].Team==1&&Towers[4].Team==1;break;
    case 11:Done=Winner==1;break;
    }
    if(!Done)return;
    TutorialObjectiveComplete=true;TutorialFastForward=false;MeteorMode=false;BridgeMode=false;
    if(TutorialStep==11)
    {if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))P->MarkTutorialCompleted();}
    else Message(TEXT("本课目标完成！战场已停下，阅读提示后点击「下一课」。"));
}
FVector AVoxelBattle::TutorialGoalPosition() const
{
    int Tower=INDEX_NONE;
    switch(TutorialStep)
    {
    case 0:Tower=0;break;case 1:case 2:Tower=2;break;case 3:case 4:Tower=1;break;
    case 5:case 6:return CellPosition(TrainingGuard);
    case 7:case 8:case 9:return CellPosition(FIntPoint(0,TutorialBridgeRow));
    case 10:Tower=Towers.IsValidIndex(3)&&Towers[3].Team==2?3:4;break;
    case 11:Tower=5;break;
    }
    return Towers.IsValidIndex(Tower)?CellPosition(Towers[Tower].Cell):FVector::ZeroVector;
}
void AVoxelBattle::FocusTutorialObjective()
{
    if(!Tutorial)return;
    if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))
    {P->Focus=TutorialGoalPosition()+FVector(-400,485,0);P->Focus.Z=0;P->Zoom=6200;}
}
FString AVoxelBattle::TutorialTitle() const
{
    const TCHAR* Titles[]={TEXT("认识你的主城"),TEXT("派兵占领村庄"),TEXT("发展村庄经济"),TEXT("升级防御塔"),TEXT("改造弓箭塔"),TEXT("空地部署军队"),TEXT("拦截与遭遇战"),TEXT("修建过河桥梁"),TEXT("陨石：伤害与毁桥"),TEXT("重建战场通路"),TEXT("夺取敌方全部分城"),TEXT("攻占主塔，完成训练")};
    return Titles[FMath::Clamp(TutorialStep,0,TutorialLessonCount-1)];
}
TArray<FString> AVoxelBattle::TutorialInstructions() const
{
    switch(TutorialStep)
    {
    case 0:return {TEXT("左键点击金圈内的「教学主城」。"),TEXT("蓝旗是我方，红旗是敌军。"),TEXT("数字表示驻军；选中后可查看右侧信息。"),TEXT("选中单位会进入 0.2 倍思考时间。")};
    case 1:return {TEXT("先左键主城，再右键「溪畔村庄」。"),TEXT("右键用于出兵，部队会沿陆地前进。"),TEXT("兵力严格大于守军才能占领据点。"),TEXT("村庄为主城产兵和金币提供持续加成。")};
    case 2:return {TEXT("左键选中已占领的「溪畔村庄」。"),TEXT("按 U，或点击右侧「发展」按钮。"),TEXT("升到 2 级需 40 金，提高兵力与金币补给。"),TEXT("本关有教学物资；正式战役从零金币开始。")};
    case 3:return {TEXT("左键选中「训练箭楼」，按 U 升至 2 级。"),TEXT("也可点击右侧升级按钮，花费 80 金。"),TEXT("军事塔升级增加 50 驻军与驻军上限。"),TEXT("箭炮塔每级射程 +10%，伤害与射速也提高。")};
    case 4:return {TEXT("选中「训练箭楼」，按 1 或 2 选择箭种。"),TEXT("毒箭：持续伤害落点附近的敌军。"),TEXT("穿透箭：直线重击，攻击间隔较长。"),TEXT("箭种一旦选定便会保留，包括被敌军夺回。")};
    case 5:return {TEXT("选中主城，右键金圈内的空地。"),TEXT("部署至少 25 兵；Tab 可切换出兵比例。"),TEXT("等待部队到达，出现「驻守待命」。"),TEXT("左键兵力牌可重选部队，友军靠近会合并。")};
    case 6:return {TEXT("敌方训练营已派出一支真实的袭击部队。"),TEXT("让驻守部队等待，自动拦截附近敌军。"),TEXT("双方接触后停下交战，兵力会实际减少。"),TEXT("击败来敌即可；失误可点击「重试本课」。")};
    case 7:return {TEXT("按 B，或点击下方建桥按钮。"),TEXT("左键河道选址，花费 75 金。"),TEXT("施工 60 战场秒后双方都可以通行。"),TEXT("施工中可点「教学快进」，仍按战场秒计时。")};
    case 8:return {TEXT("已布置蓝、红各 50 兵的示范队。"),TEXT("按 M，左键金圈内的桥梁中心。"),TEXT("陨石伤害范围内双方部队，并摧毁桥梁。"),FString::Printf(TEXT("观察两队各损失 38 兵；冷却为 %d 秒。"),FMath::RoundToInt(MeteorCooldown))};
    case 9:return {TEXT("桥梁被毁后，河道再次阻断。"),TEXT("按 B 重新选址建桥，仍需 75 金和 60 秒。"),TEXT("已有目标的部队会在重建后重新寻路。"),TEXT("可用教学快进，完成后继续进攻训练。")};
    case 10:return {TEXT("先派兵攻占「敌军训练营」。"),TEXT("再过桥攻占「东岸炮堡」，可用定位切换目标。"),TEXT("遇到守军较多时，用 Tab 增大出兵比例。"),TEXT("必须夺取全部敌方分城和村庄，才可攻主塔。")};
    default:return {TEXT("敌方全部分城失守，主塔保护已解除。"),TEXT("从己方据点向「敌方指挥城」派兵。"),TEXT("攻陷主塔立即结束对局。"),FString::Printf(TEXT("正式战役 %d 分钟到时先比据点，再比兵力。"),FMath::RoundToInt(BattleTimeLimit/60))};
    }
}
FString AVoxelBattle::TutorialProgress() const
{
    if(TutorialObjectiveComplete)return TEXT("目标已完成 · 点击下一课继续");
    if(TutorialCanFastForward()){int I=BridgeIndex(TutorialBridgeRow);return FString::Printf(TEXT("桥梁施工中 · 还需 %d 战场秒"),FMath::CeilToInt(Bridges[I].Remaining));}
    if(TutorialStep==6)return TutorialEncounterSeen?TEXT("已发生遭遇战 · 继续击败来敌"):TEXT("来敌接近中 · 保持部队驻守");
    if(TutorialStep==10)return FString::Printf(TEXT("已占领分城 %d / 2"),int(Towers[3].Team==1)+int(Towers[4].Team==1));
    if(TutorialStep==8&&TutorialBlueHit&&TutorialRedHit&&BridgeIndex(TutorialBridgeRow)==INDEX_NONE&&!MeteorBursts.IsEmpty())return TEXT("命中成功 · 观察冲击波、碎石和烟尘");
    if(TutorialStep==8)return FString::Printf(TEXT("蓝军受击 %s · 红军受击 %s · 桥梁摧毁 %s"),TutorialBlueHit?TEXT("✓"):TEXT("—"),TutorialRedHit?TEXT("✓"):TEXT("—"),BridgeIndex(TutorialBridgeRow)==INDEX_NONE?TEXT("✓"):TEXT("—"));
    return TEXT("请按照提示完成操作，金圈标记当前目标");
}

void AVoxelHUD::DrawTutorial(AVoxelBattle* B,AVoxelCommander* P,float W,float H)
{
    const FLinearColor Gold(.75f,.53f,.22f),Ink(.025f,.042f,.038f),White(.96f,.91f,.77f),Muted(.55f,.63f,.54f);
    float X=28,Y=145,CW=382,CH=320;
    // Remove battlefield labels under the teaching card; the unit panel remains interactive.
    Labels.RemoveAll([&](const FLabel& L){return L.X>X-20&&L.X<X+CW&&L.Y>Y-10&&L.Y<Y+CH;});
    HUDFrame(X,Y,CW,CH);
    Text(FString::Printf(TEXT("新手教学  %02d / %02d"),B->TutorialStep+1,AVoxelBattle::TutorialLessonCount),X+18,Y+16,12,Gold);
    Panel(X+18,Y+42,CW-36,3,Muted);Panel(X+18,Y+42,(CW-36)*(B->TutorialStep+1)/AVoxelBattle::TutorialLessonCount,3,Gold);
    HUDText(B->TutorialTitle(),X+18,Y+57,22,White);
    auto Lines=B->TutorialInstructions();for(int I=0;I<Lines.Num();++I)Text(Lines[I],X+18,Y+99+I*28,12,White);
    Text(B->TutorialProgress(),X+18,Y+220,11,B->TutorialObjectiveComplete?Gold:Muted);
    Button(TEXT("lessonFocus"),TEXT("定位目标"),X+18,Y+257,100);
    Button(TEXT("lessonRetry"),TEXT("重试本课"),X+130,Y+257,100,!B->Paused);
    if(B->TutorialCanFastForward())Button(TEXT("lessonFast"),B->TutorialFastForward?TEXT("结束快进"):TEXT("教学快进"),X+242,Y+257,122,!B->Paused);
    else Button(TEXT("lessonNext"),TEXT("下一课 →"),X+242,Y+257,122,B->TutorialObjectiveComplete&&!B->Paused);
    FVector2D Screen;P->ProjectWorldLocationToScreen(B->TutorialGoalPosition()+FVector(0,0,55),Screen);
    if(Screen.X>430*Scale&&Screen.X<(W-295)*Scale&&Screen.Y>140*Scale&&Screen.Y<(H-180)*Scale)
    {Panel(Screen.X/Scale-45,Screen.Y/Scale+24,90,25,Ink);Text(TEXT("当前教学目标"),Screen.X/Scale-37,Screen.Y/Scale+29,10,Gold);}
}
