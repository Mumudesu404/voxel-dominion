#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

static void ConnectRiverRegions(AVoxelBattle* B)
{
    for(int Y=-B->MapHalfHeight();Y<=B->MapHalfHeight();++Y)for(int X=-B->MapHalfWidth();X<=B->MapHalfWidth();++X)
    {
        FVoxelBridge Site;FIntPoint C(X,Y);
        if(!B->BridgeSiteAt(C,Site)||B->CanReach(B->BridgeBank(Site,false),B->BridgeBank(Site,true)))continue;
        B->Gold[1]=999;if(B->BuildBridgeAt(C))B->Bridges.Last().Remaining=0;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCaptureTest,"VoxelDominion.Rules.Capture",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelCaptureTest::RunTest(const FString&)
{
    float Defense=20;TestTrue(TEXT("Superior force captures"),VoxelRules::ResolveAttack(35,Defense));TestEqual(TEXT("Only surviving attackers remain"),Defense,15.f);
    Defense=20;TestFalse(TEXT("Equal force does not capture"),VoxelRules::ResolveAttack(20,Defense));TestEqual(TEXT("Equal force clears defenders"),Defense,0.f);
    Defense=20;TestFalse(TEXT("Inferior force fails"),VoxelRules::ResolveAttack(12,Defense));TestEqual(TEXT("Defenders lose attackers' strength"),Defense,8.f);
    TestTrue(TEXT("High ground increases range"),VoxelRules::Range(true,120)>VoxelRules::Range(true,0));
    TestTrue(TEXT("Tall base outranges wide base"),VoxelRules::Range(true,0)>VoxelRules::Range(false,0));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRouteTest,"VoxelDominion.Rules.RiverRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelRouteTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* Battle=World->SpawnActor<AVoxelBattle>();
    TestFalse(TEXT("River blocks direct crossing"),Battle->Walkable(FIntPoint(0,0)));
    TestTrue(TEXT("No route crosses the river at start"),Battle->FindRoute(FIntPoint(-3,0),FIntPoint(3,0),1,false).IsEmpty());
    Battle->Gold[1]=75;Battle->BuildBridge(4);TestFalse(TEXT("Construction is not walkable"),Battle->Walkable(FIntPoint(0,4)));Battle->Bridges[0].Remaining=0;
    auto Long=Battle->FindRoute(FIntPoint(-3,0),FIntPoint(3,0),1,false);
    auto Length=[&](const TArray<FVector>& Route){float Sum=0;FVector P=Battle->CellPosition(FIntPoint(-3,0));for(FVector Q:Route){Sum+=FVector::Dist2D(P,Q);P=Q;}return Sum;};
    TestTrue(TEXT("Army detours through the completed bridge at the chosen row"),Long.Num()>1&&Length(Long)>6*Battle->Tile);
    for(auto P:Long)TestTrue(TEXT("Route stays on walkable terrain"),Battle->Walkable(Battle->ToCell(P)));
    Battle->Gold[1]=75;Battle->BuildBridge(0);Battle->Bridges.Last().Remaining=0;auto Short=Battle->FindRoute(FIntPoint(-3,0),FIntPoint(3,0),1,false);
    TestEqual(TEXT("A direct crossing needs only the actual destination"),Short.Num(),1);
    TestTrue(TEXT("Direct bridge yields the geometric shortest route"),FMath::IsNearlyEqual(Length(Short),6*Battle->Tile,.01f));
    TestTrue(TEXT("Bridge shortens the real travel distance"),Length(Short)<Length(Long));
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSimulationTest,"VoxelDominion.Simulation.CaptureEconomyAndMeteor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelSimulationTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Team=2;T.Module=EVoxelModule::Arrow;T.Troops=10;T.Cell=FIntPoint(3,0);T.Arrow=EVoxelArrow::Poison;B->Towers.Add(T);
    FVoxelSquad S;S.Team=1;S.Target=0;S.Troops=26;B->Arrive(S);
    TestEqual(TEXT("Capture transfers allegiance"),B->Towers[0].Team,1);
    TestEqual(TEXT("Capture keeps survivors"),B->Towers[0].Troops,16.f);
    TestTrue(TEXT("First player capture unlocks arrow choice"),B->Towers[0].Arrow==EVoxelArrow::Unbound);
    B->Selected=0;B->BindArrow(EVoxelArrow::Pierce);B->BindArrow(EVoxelArrow::Poison);
    TestTrue(TEXT("Binding is permanent"),B->Towers[0].Arrow==EVoxelArrow::Pierce);
    S.Team=2;S.Troops=30;B->Arrive(S);
    TestTrue(TEXT("Enemy inherits player doctrine"),B->Towers[0].Arrow==EVoxelArrow::Pierce);
    S.Team=1;S.Troops=30;B->Arrive(S);
    TestTrue(TEXT("Player recapture preserves doctrine"),B->Towers[0].Arrow==EVoxelArrow::Pierce);
    B->Towers.Empty();T.Module=EVoxelModule::Village;T.Team=1;T.Troops=10;B->Towers.Add(T);T.Team=2;B->Towers.Add(T);
    B->SoldierSpeed=0;
    for(int Team=1;Team<=2;++Team){FVoxelSquad Army;Army.Team=Team;Army.Troops=50;Army.Position=FVector(Team==1?-150:150,0,100);Army.Route.Add(Army.Position+FVector(500,0,0));B->Squads.Add(Army);}
    FVoxelEffect E;E.Position=FVector(0,0,100);E.Radius=340;E.Remaining=.01f;E.Meteor=true;B->Effects.Add(E);
    B->Simulate(.05f);
    TestEqual(TEXT("Meteor damages friendly troops"),B->Squads[0].Troops,12.f);
    TestEqual(TEXT("Meteor damages enemy troops equally"),B->Squads[1].Troops,12.f);
    TestTrue(TEXT("Village increases income"),FMath::IsNearlyEqual(B->Gold[1],.07f,.001f));
    TestTrue(TEXT("Village retains its own base production"),FMath::IsNearlyEqual(B->Towers[0].Troops,10.006f,.001f));
    B->Gold[1]=74;TestFalse(TEXT("Cannot build without gold"),B->BuildBridge(0));
    B->Gold[1]=75;TestTrue(TEXT("Bridge construction begins with sufficient gold"),B->BuildBridge(0));TestEqual(TEXT("Bridge pays exact cost"),B->Gold[1],0.f);
    TestFalse(TEXT("Cannot build duplicate bridge"),B->BuildBridge(0));TestEqual(TEXT("Duplicate bridge never charges twice"),B->Gold[1],0.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelCombatTest,"VoxelDominion.Simulation.AIHeatAndVictory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelCombatTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Team=0;T.Module=EVoxelModule::Arrow;T.Troops=10;T.Cell=FIntPoint(3,0);B->Towers.Add(T);
    FVoxelSquad Attack;Attack.Team=2;Attack.Target=0;Attack.Troops=15;B->Arrive(Attack);
    TestTrue(TEXT("AI capture activates developer's arrow doctrine"),B->Towers[0].Arrow==EVoxelArrow::Poison);
    B->Towers[0].Module=EVoxelModule::Cannon;B->Towers[0].Heat=100;B->Towers[0].Overheated=true;
    FVoxelSquad Army;Army.Team=1;Army.Target=0;Army.Troops=100;Army.Position=B->CellPosition(T.Cell)+FVector(300,0,27);B->Squads.Add(Army);
    B->FireTowers(.1f);TestEqual(TEXT("Overheated cannon cannot shoot"),B->Squads[0].Troops,100.f);
    for(int I=0;I<40;++I)B->FireTowers(.1f);
    TestTrue(TEXT("Cannon resumes fire after cooling"),B->Squads[0].Troops<100.f);
    B->Towers.Empty();B->Squads.Empty();T.Team=2;T.Module=EVoxelModule::Command;T.Troops=100;T.Cell=FIntPoint(7,0);B->Towers.Add(T);
    T.Module=EVoxelModule::Cannon;T.Troops=4;T.Cell=FIntPoint(3,0);B->Towers.Add(T);
    Army.Target=1;Army.Troops=30;B->Squads.Add(Army);B->RunAI();
    TestEqual(TEXT("AI dispatches reinforcement to threatened stronghold"),B->Squads.Num(),2);
    if(B->Squads.Num()==2){TestEqual(TEXT("Reinforcement has enemy allegiance"),B->Squads[1].Team,2);TestEqual(TEXT("AI reinforces threatened target"),B->Squads[1].Target,1);}
    TestTrue(TEXT("AI pays troops from donor"),B->Towers[0].Troops<100.f);
    B->Towers.SetNum(1);B->Towers[0].Troops=10;B->Towers[0].Module=EVoxelModule::Command;
    Attack.Team=1;Attack.Troops=11;B->Arrive(Attack);TestEqual(TEXT("Enemy command capture wins battle"),B->Winner,1);
    TestFalse(TEXT("No commands accepted after victory"),B->Dispatch(0,0,.5f));
    B->Winner=0;Attack.Team=2;Attack.Troops=11;B->Arrive(Attack);TestEqual(TEXT("Player command capture loses battle"),B->Winner,2);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelLargeMapTest,"VoxelDominion.Map.LargeBattlefield",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelLargeMapTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();
    TestEqual(TEXT("Forty capturable strongholds"),B->Towers.Num(),40);
    int Villages=0,Arrows=0,Cannons=0,Commands=0,Blue=0,Red=0;TSet<FIntPoint> Cells;
    for(int I=0;I<B->Towers.Num();++I)
    {
        const auto& T=B->Towers[I];Villages+=T.Module==EVoxelModule::Village;Arrows+=T.Module==EVoxelModule::Arrow;Cannons+=T.Module==EVoxelModule::Cannon;Commands+=T.Module==EVoxelModule::Command;
        Blue+=T.Team==1;Red+=T.Team==2;
        TestFalse(TEXT("No overlapping tower sites"),Cells.Contains(T.Cell));Cells.Add(T.Cell);
        TestTrue(TEXT("Every site is on walkable terrain"),B->Walkable(T.Cell));
        if(I!=0){auto Route=B->FindRoute(B->Towers[0].Cell,T.Cell,1,false);TestEqual(TEXT("Routes respect actual river connectivity"),!Route.IsEmpty(),B->CanReach(B->Towers[0].Cell,T.Cell));for(auto P:Route)TestTrue(TEXT("No army route crosses unbridged water"),B->Walkable(B->ToCell(P)));}
        bool Mirror=false;for(const auto& Other:B->Towers)if(Other.Cell==FIntPoint(-T.Cell.X,-T.Cell.Y)&&Other.Module==T.Module&&Other.Troops==T.Troops)Mirror=true;
        TestTrue(TEXT("Balanced rotationally symmetric sites"),Mirror);
    }
    TestEqual(TEXT("Ten resource villages"),Villages,10);TestEqual(TEXT("Fourteen arrow towers"),Arrows,14);TestEqual(TEXT("Fourteen cannon towers"),Cannons,14);TestEqual(TEXT("Two headquarters"),Commands,2);
    TestEqual(TEXT("Three blue initial holdings"),Blue,3);TestEqual(TEXT("Three red initial holdings"),Red,3);
    for(int Y=-B->H/2;Y<=B->H/2;++Y)for(int X=-B->W/2;X<=B->W/2;++X)if(B->IsRiver(FIntPoint(X,Y)))TestFalse(TEXT("No permanent crossings remain"),B->Walkable(FIntPoint(X,Y)));
    TestEqual(TEXT("Player starts with zero gold"),B->Gold[1],0.f);TestEqual(TEXT("Enemy starts with zero gold"),B->Gold[2],0.f);
    ConnectRiverRegions(B);
    for(int I=1;I<B->Towers.Num();++I)TestFalse(TEXT("All sites become reachable after the river network is bridged"),B->FindRoute(B->Towers[0].Cell,B->Towers[I].Cell,1,false).IsEmpty());
    B->InitializeLayout();TestEqual(TEXT("Initialization does not duplicate sites"),B->Towers.Num(),40);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTacticalTest,"VoxelDominion.Simulation.TacticalTime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelTacticalTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Setup=[&]()
    {
        auto* B=World->SpawnActor<AVoxelBattle>();
        FVoxelBridge Crossing;Crossing.Remaining=0;B->Bridges.Add(Crossing);
        FVoxelTower T;T.Team=1;T.Cell=FIntPoint(-8,0);T.Troops=10;T.Module=EVoxelModule::Village;B->Towers.Add(T);
        T.Team=2;T.Cell=FIntPoint(8,0);B->Towers.Add(T);
        FVoxelSquad S;S.Team=1;S.Target=1;S.Troops=5;S.Position=FVector(-1000,800,97);S.Route.Add(FVector(1000,800,97));B->Squads.Add(S);
        FVoxelEffect E;E.Remaining=5;E.Position=FVector(10000,10000,0);E.Radius=100;B->Effects.Add(E);
        B->MeteorReady=10;return B;
    };
    auto* Normal=Setup();auto* Slow=Setup();Slow->SelectTower(0);
    TestEqual(TEXT("Selecting a tower enables 0.2 speed"),Slow->GetBattleSpeed(),.2f);
    for(int I=0;I<20;++I){Normal->AdvanceBattle(.05f);Slow->AdvanceBattle(.05f);}
    TestTrue(TEXT("Normal battle advances one second"),FMath::IsNearlyEqual(Normal->Time,1.f,.0001f));
    TestTrue(TEXT("Thinking battle advances one fifth of a second"),FMath::IsNearlyEqual(Slow->Time,.2f,.0001f));
    TestTrue(TEXT("Troop movement scales to one fifth"),FMath::IsNearlyEqual(Slow->Squads[0].Position.X+1000,44.f,.001f));
    TestTrue(TEXT("Normal troop moves full distance"),FMath::IsNearlyEqual(Normal->Squads[0].Position.X+1000,220.f,.001f));
    TestTrue(TEXT("Friendly income slows equally"),FMath::IsNearlyEqual(Slow->Gold[1],.28f,.001f));
    TestTrue(TEXT("Enemy income slows equally"),FMath::IsNearlyEqual(Slow->Gold[2],.28f,.001f));
    TestTrue(TEXT("Garrison production uses slow clock"),FMath::IsNearlyEqual(Slow->Towers[0].Troops,10.024f,.001f));
    TestTrue(TEXT("Cooldown uses slow clock"),FMath::IsNearlyEqual(Slow->MeteorReady,9.8f,.001f));
    TestTrue(TEXT("Effects use slow clock"),FMath::IsNearlyEqual(Slow->Effects[0].Remaining,4.8f,.001f));
    TestTrue(TEXT("AI decision timer uses slow clock"),FMath::IsNearlyEqual(Slow->AISince,.2f,.001f));
    Slow->Paused=true;float Frozen=Slow->Time;Slow->AdvanceBattle(.2f);
    TestEqual(TEXT("Manual pause overrides thinking"),Slow->GetBattleSpeed(),0.f);TestEqual(TEXT("Pause freezes simulation"),Slow->Time,Frozen);
    Slow->Paused=false;TestEqual(TEXT("Resume restores thinking speed"),Slow->GetBattleSpeed(),.2f);
    TestFalse(TEXT("Invalid command fails"),Slow->Dispatch(0,0,.5f));TestEqual(TEXT("Failed command preserves thinking"),Slow->GetBattleSpeed(),.2f);
    TestTrue(TEXT("AI can issue an order during thinking"),Slow->Dispatch(1,0,.5f));TestEqual(TEXT("AI order cannot end player's thinking"),Slow->GetBattleSpeed(),.2f);
    TestTrue(TEXT("Player dispatch succeeds"),Slow->Dispatch(0,1,.5f));TestEqual(TEXT("Player order restores normal speed"),Slow->GetBattleSpeed(),1.f);
    Slow->SelectTower(0);Slow->Towers[0].Module=EVoxelModule::Arrow;Slow->BindArrow(EVoxelArrow::Poison);
    TestEqual(TEXT("Binding arrows restores normal speed"),Slow->GetBattleSpeed(),1.f);
    Slow->SelectTower(0);Slow->CancelCommand();TestEqual(TEXT("Cancel clears selection"),Slow->Selected,INDEX_NONE);TestEqual(TEXT("Cancel restores normal speed"),Slow->GetBattleSpeed(),1.f);
    Slow->SelectTower(1);TestEqual(TEXT("Inspecting enemy also permits thinking"),Slow->GetBattleSpeed(),.2f);
    Slow->SelectTower(INDEX_NONE);TestEqual(TEXT("Empty ground restores normal speed"),Slow->GetBattleSpeed(),1.f);
    Slow->SelectTower(0);Slow->Winner=2;TestEqual(TEXT("Battle end freezes time"),Slow->GetBattleSpeed(),0.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelVictoryTest,"VoxelDominion.Simulation.HeadquartersOnlyVictory",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelVictoryTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Duel=[&]()
    {
        auto* B=World->SpawnActor<AVoxelBattle>();FVoxelTower T;T.Module=EVoxelModule::Command;T.Troops=250;
        T.Team=1;T.Cell=FIntPoint(-7,0);B->Towers.Add(T);T.Team=2;T.Cell=FIntPoint(7,0);B->Towers.Add(T);return B;
    };
    auto* B=Duel();B->AdvanceBattle(.1f);
    TestEqual(TEXT("Two intact headquarters do not end the game"),B->Winner,0);
    TestTrue(TEXT("Battle clock continues with only headquarters"),B->Time>0);
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Cannon;T.Troops=1;B->Towers.Add(T);
    FVoxelSquad Attack;Attack.Team=2;Attack.Target=2;Attack.Troops=5;B->Arrive(Attack);
    TestEqual(TEXT("Loss of the last ordinary tower does not cause defeat"),B->Winner,0);
    Attack.Target=0;Attack.Troops=250;B->Arrive(Attack);
    TestEqual(TEXT("Equal force does not capture headquarters"),B->Towers[0].Team,1);
    TestEqual(TEXT("Empty but uncaptured headquarters does not end the game"),B->Winner,0);
    Attack.Troops=1;B->Arrive(Attack);
    TestEqual(TEXT("Actual player headquarters capture causes defeat"),B->Winner,2);
    float Frozen=B->Time,Gold=B->Gold[1],Troops=B->Towers[1].Troops;
    B->AdvanceBattle(.25f);B->Simulate(1);B->RunAI();
    TestEqual(TEXT("Time stops immediately after defeat"),B->Time,Frozen);
    TestEqual(TEXT("Income stops after defeat"),B->Gold[1],Gold);
    TestEqual(TEXT("Production stops after defeat"),B->Towers[1].Troops,Troops);
    Attack.Team=1;Attack.Target=1;Attack.Troops=300;B->Arrive(Attack);
    TestEqual(TEXT("Late arrivals cannot overwrite the result"),B->Winner,2);
    TestEqual(TEXT("Late arrivals cannot change tower ownership"),B->Towers[1].Team,2);
    auto* Win=Duel();Win->Towers[1].Troops=10;
    FVoxelSquad S;S.Team=1;S.Target=1;S.Troops=11;S.Position=Win->CellPosition(Win->Towers[1].Cell)+FVector(0,0,27);S.Route.Add(S.Position);Win->Squads.Add(S);
    Win->AdvanceBattle(.05f);
    TestEqual(TEXT("Capturing enemy headquarters through normal simulation wins"),Win->Winner,1);
    TestEqual(TEXT("Captured enemy headquarters belongs to player"),Win->Towers[1].Team,1);
    Frozen=Win->Time;Win->AdvanceBattle(.25f);TestEqual(TEXT("Victory freezes the battle clock"),Win->Time,Frozen);
    auto* Restart=Duel();TestEqual(TEXT("New battle clears the result"),Restart->Winner,0);TestEqual(TEXT("New battle runs at normal speed"),Restart->GetBattleSpeed(),1.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSiegeTest,"VoxelDominion.Simulation.CappedHeadquartersAssault",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelSiegeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();
    // Reproduce the screenshot: one blue headquarters, 39 red holdings, all at the 250 cap.
    for(int32 I=0;I<B->Towers.Num();++I){B->Towers[I].Team=I==0?1:2;B->Towers[I].Troops=250;}
    B->AdvanceBattle(.1f);TestEqual(TEXT("One versus 39 holdings is not a defeat"),B->Winner,0);
    TestTrue(TEXT("Capped garrisons do not stop the clock"),B->Time>0);
    B->SelectTower(0);B->RunAI();
    TestEqual(TEXT("AI dispatches only one tower per decision"),B->Squads.Num(),1);
    TestEqual(TEXT("AI assault preserves player's tactical time"),B->GetBattleSpeed(),.2f);
    B->CancelCommand();for(int I=0;I<17;++I)B->AdvanceBattle(.25f);B->RunAI();
    TestTrue(TEXT("Joint assault gathers subsequent waves over time"),B->Squads.Num()>=2);
    float Attackers=0,Latest=0,Earliest=FLT_MAX,Spent=0;
    for(const auto& T:B->Towers)if(T.Team==2)Spent+=250-T.Troops;
    for(const auto& S:B->Squads)
    {
        TestEqual(TEXT("Joint assault targets the surviving player headquarters"),S.Target,0);
        TestEqual(TEXT("Assault troops have enemy allegiance"),S.Team,2);Attackers+=S.Troops;
        float Length=0;FVector P=S.Position;for(const auto& Q:S.Route){Length+=FVector::Dist(P,Q);P=Q;}
        float Arrival=S.MarchDelay+Length/B->SoldierSpeed;Latest=FMath::Max(Latest,Arrival);Earliest=FMath::Min(Earliest,Arrival);
    }
    TestTrue(TEXT("Combined attack exceeds a capped garrison"),Attackers>250);
    TestTrue(TEXT("Attacking soldiers are deducted from source garrisons"),Spent>Attackers-30&&Spent<=Attackers);
    TestTrue(TEXT("Different routes are synchronized without teleporting"),Latest-Earliest<.001f);
    B->RunAI();TestEqual(TEXT("No duplicate assault while troops are en route"),B->Incoming(0,2),Attackers);
    B->CancelCommand();
    for(int32 I=0;I<2400&&!B->Winner;++I)B->AdvanceBattle(.05f);
    TestEqual(TEXT("Joint assault can capture a fully defended headquarters"),B->Winner,2);
    TestEqual(TEXT("Defeat requires actual headquarters ownership transfer"),B->Towers[0].Team,2);
    TestEqual(TEXT("Headquarters capture stops the battle"),B->GetBattleSpeed(),0.f);
    auto* Upgraded=World->SpawnActor<AVoxelBattle>();Upgraded->InitializeLayout();
    for(int32 I=0;I<Upgraded->Towers.Num();++I){Upgraded->Towers[I].Team=I==0?1:2;Upgraded->Towers[I].Troops=250;}
    Upgraded->Towers[0].Level=5;Upgraded->Towers[0].Troops=450;Upgraded->RunAI();
    TestEqual(TEXT("Upgraded HQ assault also begins with a single order"),Upgraded->Squads.Num(),1);
    for(int Wave=0;Wave<2;++Wave){for(int I=0;I<17;++I)Upgraded->AdvanceBattle(.25f);Upgraded->RunAI();}
    TestTrue(TEXT("Joint assault accounts for upgraded headquarters capacity"),Upgraded->Incoming(0,2)>450);
    for(int32 I=0;I<2400&&!Upgraded->Winner;++I)Upgraded->AdvanceBattle(.05f);
    TestEqual(TEXT("Upgraded headquarters does not reintroduce capped AI stalemate"),Upgraded->Winner,2);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelUpgradeTest,"VoxelDominion.Simulation.TowerUpgrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelUpgradeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Arrow;T.Tall=true;T.Troops=250;T.Cell=FIntPoint(-8,0);B->Towers.Add(T);
    T.Team=2;T.Module=EVoxelModule::Cannon;T.Tall=false;T.Troops=20;T.Cell=FIntPoint(8,0);B->Towers.Add(T);
    T.Team=1;T.Module=EVoxelModule::Village;T.Cell=FIntPoint(-8,6);B->Towers.Add(T);
    T.Module=EVoxelModule::Command;T.Cell=FIntPoint(-14,0);B->Towers.Add(T);
    B->SelectTower(0);B->Gold[1]=79;TestFalse(TEXT("Cannot upgrade without enough gold"),B->UpgradeTower(0));
    TestEqual(TEXT("Failed upgrade keeps level"),B->Towers[0].Level,1);TestEqual(TEXT("Failed upgrade does not spend gold"),B->Gold[1],79.f);
    TestEqual(TEXT("Failed upgrade preserves thinking"),B->GetBattleSpeed(),.2f);
    B->Gold[1]=80;B->Towers[0].FireTimer=2.8f;TestTrue(TEXT("First upgrade costs exactly 80 gold"),B->UpgradeTower(0));
    TestEqual(TEXT("Gold deducted once"),B->Gold[1],0.f);TestEqual(TEXT("Tower reaches level two"),B->Towers[0].Level,2);
    TestEqual(TEXT("Full garrison still receives all 50 upgrade troops"),B->Towers[0].Troops,300.f);
    TestEqual(TEXT("Capacity increases with level"),B->TroopCapacity(0),300.f);
    TestEqual(TEXT("Successful upgrade exits thinking"),B->GetBattleSpeed(),1.f);
    TestTrue(TEXT("Existing reload gains the faster speed"),FMath::IsNearlyEqual(B->Towers[0].FireTimer,2.8f/1.15f,.001f));
    B->AdvanceBattle(.05f);TestEqual(TEXT("Upgraded troops are not truncated back to 250"),B->Towers[0].Troops,300.f);
    FVoxelSquad Reinforce;Reinforce.Team=1;Reinforce.Target=0;Reinforce.Troops=20;B->Arrive(Reinforce);
    TestEqual(TEXT("Reinforcement respects upgraded capacity"),B->Towers[0].Troops,300.f);
    B->SelectTower(0);B->BindArrow(EVoxelArrow::Poison);
    FVoxelSquad Army;Army.Team=2;Army.Target=0;Army.Troops=100;Army.Position=B->CellPosition(B->Towers[0].Cell)+FVector(300,0,27);B->Squads.Add(Army);
    B->Towers[0].FireTimer=0;B->FireTowers(.01f);
    TestTrue(TEXT("Upgraded poison arrow deals 20 percent more impact damage"),FMath::IsNearlyEqual(100-B->Squads[0].Troops,2.3f*1.2f,.001f));
    TestTrue(TEXT("Poison arrow fires 15 percent faster"),FMath::IsNearlyEqual(B->Towers[0].FireTimer,2.8f/1.15f,.001f));
    TestTrue(TEXT("Poison effect was created"),!B->Effects.IsEmpty());
    if(!B->Effects.IsEmpty())TestTrue(TEXT("Upgrade also increases poison damage over time"),FMath::IsNearlyEqual(B->Effects[0].DamagePerSecond,2.2f*1.2f,.001f));
    B->Towers[0].Arrow=EVoxelArrow::Pierce;B->Towers[0].FireTimer=0;B->Squads[0].Troops=100;B->FireTowers(.01f);
    TestTrue(TEXT("Piercing arrow damage scales too"),FMath::IsNearlyEqual(100-B->Squads[0].Troops,2.3f*1.2f*2.8f,.001f));
    TestTrue(TEXT("Piercing reload scales too"),FMath::IsNearlyEqual(B->Towers[0].FireTimer,3.5f/1.15f,.001f));
    B->Towers[0].Module=EVoxelModule::Cannon;B->Towers[0].Tall=false;B->Towers[0].FireTimer=0;B->Squads[0].Troops=100;B->FireTowers(.01f);
    TestTrue(TEXT("Cannon damage scales too"),FMath::IsNearlyEqual(100-B->Squads[0].Troops,3.5f*1.2f,.001f));
    TestTrue(TEXT("Cannon reload scales too"),FMath::IsNearlyEqual(B->Towers[0].FireTimer,.38f/1.15f,.001f));
    B->Squads.Empty();B->Effects.Empty();B->Towers[0].Module=EVoxelModule::Arrow;
    B->Gold[1]=999;
    for(int32 Level=2;Level<5;++Level)
    {
        float Before=B->Gold[1],Troops=B->Towers[0].Troops;int32 Cost=B->UpgradeCost(0);
        TestEqual(TEXT("Escalating upgrade prices"),Cost,80+40*(Level-1));B->UpgradeTower(0);
        TestEqual(TEXT("Each upgrade adds exactly 50 troops"),B->Towers[0].Troops,Troops+50);
        TestEqual(TEXT("Each upgrade pays its displayed cost"),B->Gold[1],Before-Cost);
    }
    TestEqual(TEXT("Maximum level capacity is 450"),B->TroopCapacity(0),450.f);
    float Before=B->Gold[1];TestFalse(TEXT("Cannot upgrade beyond level five"),B->UpgradeTower(0));TestEqual(TEXT("Max level attempt is free"),B->Gold[1],Before);
    TestFalse(TEXT("Cannot upgrade enemy tower using player gold"),B->UpgradeTower(1));TestFalse(TEXT("Invalid selection cannot upgrade"),B->UpgradeTower(INDEX_NONE));
    Reinforce.Team=2;Reinforce.Target=0;Reinforce.Troops=451;B->Arrive(Reinforce);TestEqual(TEXT("Capture keeps the upgraded level"),B->Towers[0].Level,5);
    TestEqual(TEXT("Capture does not grant upgrade troops again"),B->Towers[0].Troops,1.f);
    Reinforce.Team=1;Reinforce.Troops=10;B->Arrive(Reinforce);TestEqual(TEXT("Recapture keeps the upgraded level"),B->Towers[0].Level,5);
    B->SelectTower(0);B->Gold[2]=80;TestTrue(TEXT("Enemy upgrades pay enemy gold"),B->UpgradeTower(1,2));TestEqual(TEXT("Enemy pays exact cost"),B->Gold[2],0.f);
    TestEqual(TEXT("Enemy upgrade does not end player's thinking"),B->GetBattleSpeed(),.2f);
    B->Paused=true;TestFalse(TEXT("Cannot upgrade during pause"),B->UpgradeTower(3));B->Paused=false;
    B->Winner=1;TestFalse(TEXT("Cannot upgrade after battle ends"),B->UpgradeTower(3));B->Winner=0;
    float HQBefore=B->Towers[3].Troops;B->Gold[1]=80;TestTrue(TEXT("Headquarters can also be upgraded"),B->UpgradeTower(3));TestEqual(TEXT("Headquarters receives 50 troops"),B->Towers[3].Troops,HQBefore+50);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelVillageTest,"VoxelDominion.Simulation.VillageHeadquartersBonus",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelVillageTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Module=EVoxelModule::Command;T.Troops=10;T.Team=1;T.Cell=FIntPoint(-8,0);B->Towers.Add(T);T.Team=2;T.Cell=FIntPoint(8,0);B->Towers.Add(T);
    T.Module=EVoxelModule::Village;T.Troops=0;T.Team=1;T.Cell=FIntPoint(-8,6);B->Towers.Add(T);T.Cell=FIntPoint(-12,-6);B->Towers.Add(T);
    T.Team=2;T.Cell=FIntPoint(8,6);B->Towers.Add(T);T.Team=0;T.Cell=FIntPoint(4,-6);B->Towers.Add(T);
    TestEqual(TEXT("Blue receives two village bonuses"),B->CommandRecovery(1),2.65f);TestEqual(TEXT("Red receives its own village bonus"),B->CommandRecovery(2),1.65f);
    for(int32 I=0;I<4;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Blue headquarters gains base plus one troop per village per second"),FMath::IsNearlyEqual(B->Towers[0].Troops,12.65f,.001f));
    TestTrue(TEXT("Red headquarters follows the same rule"),FMath::IsNearlyEqual(B->Towers[1].Troops,11.65f,.001f));
    TestTrue(TEXT("Two villages increase gold income"),FMath::IsNearlyEqual(B->Gold[1],1.8f,.001f));TestTrue(TEXT("Enemy village gold is independent"),FMath::IsNearlyEqual(B->Gold[2],1.4f,.001f));
    TestTrue(TEXT("Neutral village gives no troops or income to either side"),B->Towers[5].Troops==0);
    FVoxelSquad S;S.Team=2;S.Target=2;S.Troops=5;B->Arrive(S);
    TestEqual(TEXT("Losing a village removes one troop per second"),B->CommandRecovery(1),1.65f);TestEqual(TEXT("Captor gains one troop per second"),B->CommandRecovery(2),2.65f);
    TestTrue(TEXT("Lost village income transfers to captor"),FMath::IsNearlyEqual(B->GoldRecovery(1),1.4f)&&FMath::IsNearlyEqual(B->GoldRecovery(2),1.8f));
    S.Team=1;S.Target=5;S.Troops=1;B->Arrive(S);TestEqual(TEXT("Capturing neutral village activates bonus immediately"),B->CommandRecovery(1),2.65f);
    B->SelectTower(0);float Troops=B->Towers[0].Troops,Gold=B->Gold[1];for(int32 I=0;I<20;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Village troop bonus respects tactical slow motion"),FMath::IsNearlyEqual(B->Towers[0].Troops-Troops,.53f,.001f));
    TestTrue(TEXT("Village gold bonus respects tactical slow motion"),FMath::IsNearlyEqual(B->Gold[1]-Gold,.36f,.001f));
    B->Paused=true;Troops=B->Towers[0].Troops;B->AdvanceBattle(.25f);TestEqual(TEXT("Pause freezes village production"),B->Towers[0].Troops,Troops);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelBridgeTest,"VoxelDominion.Simulation.BridgeConstructionAndDestruction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelBridgeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Setup=[&](int32 Row)
    {
        auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;
        FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Village;T.Troops=100;T.Cell=FIntPoint(-3,Row);B->Towers.Add(T);
        T.Team=2;T.Cell=FIntPoint(3,Row);B->Towers.Add(T);return B;
    };
    auto* B=Setup(3);
    TestEqual(TEXT("Construction takes one minute"),B->BridgeBuildTime,60.f);
    TestFalse(TEXT("Zero starting gold cannot build"),B->BuildBridge(3));B->Gold[1]=75;
    TestFalse(TEXT("Out-of-bounds construction is rejected"),B->BuildBridge(B->H));
    TestEqual(TEXT("Invalid construction does not spend gold"),B->Gold[1],75.f);
    B->BridgeMode=true;TestTrue(TEXT("Player can freely choose a river row"),B->BuildBridge(3));
    TestEqual(TEXT("Construction charges exact cost"),B->Gold[1],0.f);TestEqual(TEXT("Full construction timer starts"),B->Bridges[0].Remaining,60.f);
    TestFalse(TEXT("In-progress bridge blocks movement"),B->Walkable(FIntPoint(0,3)));
    float Garrison=B->Towers[0].Troops;TestFalse(TEXT("Cannot dispatch through unfinished bridge"),B->Dispatch(0,1,.5f));TestEqual(TEXT("Blocked dispatch preserves garrison"),B->Towers[0].Troops,Garrison);
    TestFalse(TEXT("Duplicate construction is rejected"),B->BuildBridge(3));
    B->SelectTower(0);B->Gold[2]=75;TestTrue(TEXT("Enemy builds using its own gold"),B->BuildBridge(-7,2));
    FIntPoint EnemyCrossing(B->Bridges.Last().Column,B->Bridges.Last().Row);
    TestEqual(TEXT("Enemy pays construction cost"),B->Gold[2],0.f);TestEqual(TEXT("Enemy construction does not interrupt thinking"),B->GetBattleSpeed(),.2f);
    B->Paused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Pause freezes construction"),B->Bridges[0].Remaining,60.f);B->Paused=false;
    for(int I=0;I<20;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Construction respects tactical time"),FMath::IsNearlyEqual(B->Bridges[0].Remaining,59.8f,.002f));
    B->CancelCommand();for(int I=0;I<240;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Chosen row opens after one minute"),B->Walkable(FIntPoint(0,3)));
    TestFalse(TEXT("Adjacent river rows stay blocked"),B->Walkable(FIntPoint(0,4)));
    TestTrue(TEXT("Both sides can route over the same bridge"),!B->FindRoute(FIntPoint(-3,3),FIntPoint(3,3),1).IsEmpty()&&!B->FindRoute(FIntPoint(3,3),FIntPoint(-3,3),2).IsEmpty());
    TestTrue(TEXT("Armies can now cross"),B->Dispatch(0,1,.5f));
    B->Squads[0].Position=B->CellPosition(FIntPoint(0,3))+FVector(0,0,27);B->Squads[0].LastLand=FIntPoint(-1,3);B->SoldierSpeed=0;
    float Before=B->Squads[0].Troops;B->Gold[1]=55;B->Meteor(B->CellPosition(FIntPoint(0,3)));
    for(int I=0;I<5;++I)B->AdvanceBattle(.25f);TestTrue(TEXT("Meteor warning does not destroy bridge early"),B->BridgeIndex(3)!=INDEX_NONE);
    B->AdvanceBattle(.25f);TestEqual(TEXT("Impact destroys completed bridge"),B->BridgeIndex(3),INDEX_NONE);
    TestFalse(TEXT("Destroyed bridge cannot be crossed"),B->Walkable(FIntPoint(0,3)));
    TestTrue(TEXT("Distant alternate bridge survives"),B->Walkable(EnemyCrossing));
    TestTrue(TEXT("Survivors on the bridge evacuate toward their original bank"),!B->Squads.IsEmpty()&&B->Squads[0].Evacuating&&B->Squads[0].Route[0].X<0);
    if(!B->Squads.IsEmpty())TestTrue(TEXT("Meteor also damages friendly troops on bridge"),FMath::IsNearlyEqual(B->Squads[0].Troops,Before-38,.001f));
    B->SoldierSpeed=220;for(int I=0;I<5;++I)B->AdvanceBattle(.25f);
    TestFalse(TEXT("Survivors physically reach the bank"),B->Squads[0].Evacuating);
    for(const auto& P:B->Squads[0].Route)TestTrue(TEXT("Replacement route uses only surviving crossings"),B->Walkable(B->ToCell(P)));
    auto* Blocked=Setup(0);FVoxelBridge Ready;Ready.Remaining=0;Blocked->Bridges.Add(Ready);Blocked->Dispatch(0,1,.5f);
    FVector Location=Blocked->Squads[0].Position;Blocked->DestroyBridges(FVector::ZeroVector,340);
    TestTrue(TEXT("Armies without alternate crossings wait instead of walking on water"),Blocked->Squads[0].Blocked&&Blocked->Squads[0].Holding);
    for(int I=0;I<4;++I)Blocked->AdvanceBattle(.25f);TestTrue(TEXT("Blocked army stays on land"),Blocked->Squads[0].Position.Equals(Location));
    Blocked->Gold[1]=75;Blocked->BuildBridge(4);for(int I=0;I<240;++I)Blocked->AdvanceBattle(.25f);
    TestFalse(TEXT("Waiting armies resume after replacement bridge completes"),Blocked->Squads[0].Blocked);
    TestEqual(TEXT("Waiting armies retain their original target"),Blocked->Squads[0].Target,1);
    auto* Site=Setup(-4);Site->Gold[1]=75;Site->BuildBridge(-4);Site->DestroyBridges(Site->CellPosition(FIntPoint(0,-4)),340);
    TestTrue(TEXT("Meteor also destroys unfinished construction"),Site->Bridges.IsEmpty());TestEqual(TEXT("Destroyed construction does not refund gold"),Site->Gold[1],0.f);
    Site->Gold[1]=75;TestTrue(TEXT("Destroyed location can be rebuilt"),Site->BuildBridge(-4));TestEqual(TEXT("Rebuilding requires the full minute again"),Site->Bridges[0].Remaining,60.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelFieldTest,"VoxelDominion.Simulation.FieldOrdersAndInterception",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelFieldTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Village;T.Cell=FIntPoint(-8,0);T.Troops=100;B->Towers.Add(T);
    T.Team=2;T.Cell=FIntPoint(-8,5);T.Troops=20;B->Towers.Add(T);
    FVector Anchor=B->CellPosition(FIntPoint(-6,0));B->SelectTower(0);
    TestTrue(TEXT("Can dispatch soldiers to empty land"),B->DispatchTo(0,Anchor,.5f));int32 Guard=B->Squads[0].ID;
    TestEqual(TEXT("Field dispatch pays troops from tower"),B->Towers[0].Troops,50.f);TestEqual(TEXT("Field orders restore normal speed"),B->GetBattleSpeed(),1.f);
    for(int I=0;I<9;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Army stays alive and holds its empty destination"),B->SquadIndex(Guard)!=INDEX_NONE&&B->Squads[B->SquadIndex(Guard)].Holding);
    TestTrue(TEXT("Army reaches the chosen cell"),FVector::Dist2D(B->Squads[B->SquadIndex(Guard)].Position,Anchor)<1);
    B->SelectSquad(Guard);TestEqual(TEXT("Selecting field army enables thinking"),B->GetBattleSpeed(),.2f);
    TestFalse(TEXT("Field armies cannot be ordered across unbridged river"),B->OrderSquad(Guard,INDEX_NONE,B->CellPosition(FIntPoint(6,0))));
    TestEqual(TEXT("Failed army order preserves thinking"),B->GetBattleSpeed(),.2f);B->CompleteCommand();
    TestTrue(TEXT("Enemy can send a real army toward the guard"),B->DispatchTo(1,B->CellPosition(FIntPoint(-6,1)),.5f));int32 Enemy=B->Squads.Last().ID;
    bool Intercepted=false;
    for(int I=0;I<240;++I){B->AdvanceBattle(.05f);int32 G=B->SquadIndex(Guard);if(G!=INDEX_NONE)Intercepted|=B->Squads[G].InterceptTarget==Enemy;}
    TestTrue(TEXT("Guard leaves its anchor to intercept a nearby enemy"),Intercepted);
    TestEqual(TEXT("Intercepted enemy can be defeated"),B->SquadIndex(Enemy),INDEX_NONE);
    int32 G=B->SquadIndex(Guard);TestTrue(TEXT("Stronger guard survives interception"),G!=INDEX_NONE);
    if(G!=INDEX_NONE)
    {
        TestTrue(TEXT("Encounter causes real casualties"),B->Squads[G].Troops<50);
        TestTrue(TEXT("Guard returns to its original anchor after combat"),B->Squads[G].Holding&&FVector::Dist2D(B->Squads[G].Position,Anchor)<1);
        TestEqual(TEXT("Selection survives removal of another army"),B->SelectedSquad,Guard);
        B->SelectSquad(Guard);TestTrue(TEXT("Existing army can move to another empty destination"),B->OrderSquad(Guard,INDEX_NONE,B->CellPosition(FIntPoint(-5,-2))));
        TestEqual(TEXT("Reordering does not duplicate army"),B->Squads.Num(),1);
        for(int I=0;I<16;++I)B->AdvanceBattle(.25f);
        TestTrue(TEXT("Army holds its new destination"),B->Squads[B->SquadIndex(Guard)].Holding&&B->Squads[B->SquadIndex(Guard)].Destination==FIntPoint(-5,-2));
        TestTrue(TEXT("Field army can be redirected to attack a tower"),B->OrderSquad(Guard,1,FVector::ZeroVector));
        for(int I=0;I<52;++I)B->AdvanceBattle(.25f);
        TestEqual(TEXT("Redirected army captures the enemy site"),B->Towers[1].Team,1);
        TestEqual(TEXT("Arriving army becomes garrison rather than duplicating troops"),B->SquadIndex(Guard),INDEX_NONE);
    }
    TestFalse(TEXT("Removed army cannot receive orders"),B->OrderSquad(Guard,0,FVector::ZeroVector));
    B->Paused=true;float Before=B->Towers[0].Troops;TestFalse(TEXT("Pause blocks field dispatch"),B->DispatchTo(0,Anchor,.5f));TestEqual(TEXT("Rejected dispatch never spends troops"),B->Towers[0].Troops,Before);
    for(int Team : {1,2})
    {
        auto* Overflow=World->SpawnActor<AVoxelBattle>();Overflow->AIEnabled=false;
        FVoxelTower Post;Post.Team=Team;Post.Module=EVoxelModule::Village;Post.Cell=FIntPoint(-8,0);Post.Troops=245;
        Overflow->Towers.Add(Post);
        const FVector Center=Overflow->CellPosition(Post.Cell);
        FVoxelSquad Reinforcement;Reinforcement.Team=Team;Reinforcement.Target=0;Reinforcement.Troops=40;
        Reinforcement.Position=Center+FVector(-321,0,27);
        Reinforcement.Route.Add(Center+FVector(0,0,27));
        Overflow->Squads.Add(Reinforcement);
        Overflow->Simulate(.05f);
        TestEqual(TEXT("Reinforcement fills remaining slots"),Overflow->Towers[0].Troops,250.f);
        if(TestEqual(TEXT("Excess reinforcement stays alive"),Overflow->Squads.Num(),1))
        {
            const FVoxelSquad& GuardSquad=Overflow->Squads[0];
            TestTrue(TEXT("No part of the reinforcement vanishes"),FMath::IsNearlyEqual(Overflow->TotalArmy(Team),285.006f,.001f));
            TestTrue(TEXT("Overflow holds outside without entering the tower"),GuardSquad.Holding&&GuardSquad.Target==INDEX_NONE&&
                FVector::Dist2D(GuardSquad.Position,Center)>=320);
            const int32 GuardID=GuardSquad.ID;
            FVoxelSquad EnemyArmy;EnemyArmy.Team=3-Team;EnemyArmy.Target=INDEX_NONE;EnemyArmy.Troops=5;
            EnemyArmy.Holding=true;EnemyArmy.Position=GuardSquad.Position+FVector(0,400,0);
            Overflow->Squads.Add(EnemyArmy);
            bool Chased=false;
            for(int I=0;I<125;++I)
            {
                Overflow->Simulate(.05f);
                const int32 GuardPosition=Overflow->SquadIndex(GuardID);
                if(GuardPosition!=INDEX_NONE)Chased|=Overflow->Squads[GuardPosition].InterceptTarget!=INDEX_NONE;
            }
            TestTrue(TEXT("Both teams pursue enemies inside patrol radius"),Chased);
            TestTrue(TEXT("Overflow guard defeats enemy and returns to its post"),Overflow->Squads.Num()==1&&Overflow->Squads[0].Holding&&
                FVector::Dist2D(Overflow->Squads[0].Position,Center+FVector(-321,0,0))<1);
        }
        FVoxelSquad OverCapture;OverCapture.Team=3-Team;OverCapture.Target=0;OverCapture.Troops=525;OverCapture.Position=Center;
        Overflow->Arrive(OverCapture);
        TestEqual(TEXT("Capture caps the new garrison"),Overflow->Towers[0].Troops,250.f);
        TestEqual(TEXT("Surplus capture survivors receive an exterior post"),OverCapture.Troops,25.f);
        TestEqual(TEXT("Capture surplus is a field order"),OverCapture.Target,INDEX_NONE);
    }
    auto* BridgeDefense=World->SpawnActor<AVoxelBattle>();
    FVoxelTower Fort;Fort.Module=EVoxelModule::Village;Fort.Team=1;Fort.Cell=FIntPoint(-6,0);Fort.Troops=100;BridgeDefense->Towers.Add(Fort);
    Fort.Team=2;Fort.Cell=FIntPoint(6,0);BridgeDefense->Towers.Add(Fort);
    FVoxelBridge Crossing;Crossing.Remaining=1;BridgeDefense->Bridges.Add(Crossing);
    TestFalse(TEXT("AI does not post guards at unfinished crossings"),BridgeDefense->DeployAIBridgeGuard());
    BridgeDefense->Bridges[0].Remaining=0;
    BridgeDefense->RunAI();
    if(TestEqual(TEXT("AI uses one action to send one bridge guard"),BridgeDefense->Squads.Num(),1))
    {
        const FVoxelSquad& RedGuard=BridgeDefense->Squads[0];
        TestTrue(TEXT("Red bridge guard deploys to open ground"),RedGuard.Team==2&&RedGuard.Target==INDEX_NONE&&RedGuard.Destination.X>0);
        TestEqual(TEXT("Bridge guard is paid from the red garrison"),BridgeDefense->Towers[1].Troops+RedGuard.Troops,100.f);
        TestFalse(TEXT("AI does not repeatedly dispatch to the same guarded bank"),BridgeDefense->DeployAIBridgeGuard());
        BridgeDefense->AIEnabled=false;
        for(int I=0;I<100;++I)BridgeDefense->Simulate(.05f);
        TestTrue(TEXT("Red guard reaches the bank and holds"),BridgeDefense->Squads.Num()==1&&BridgeDefense->Squads[0].Holding);
    }
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPacingTest,"VoxelDominion.Simulation.AIPacingAndBridgeEconomy",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPacingTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();
    TestEqual(TEXT("AI decision interval is four seconds"),B->AIInterval,4.f);B->RunAI();int32 First=B->Squads.Num();
    TestEqual(TEXT("AI issues a single order"),First,1);
    for(int I=0;I<20;++I)B->RunAI();TestEqual(TEXT("Repeated calls cannot bypass command cooldown"),B->Squads.Num(),First);
    B->Time=3.9f;B->RunAI();TestEqual(TEXT("AI cannot bypass its four-second interval"),B->Squads.Num(),First);
    B->Time=4.01f;B->RunAI();TestEqual(TEXT("Next decision dispatches at most one army"),B->Squads.Num(),First+1);
    B->Time=45;B->Gold[2]=75;int32 Before=B->Squads.Num();B->RunAI();
    TestEqual(TEXT("AI can choose and fund bridge construction"),B->Bridges.Num(),1);
    TestEqual(TEXT("AI construction consumes the decision instead of also dispatching"),B->Squads.Num(),Before);
    TestEqual(TEXT("AI bridge costs enemy gold"),B->Gold[2],0.f);TestEqual(TEXT("AI bridge has same one-minute build time"),B->Bridges[0].Remaining,60.f);
    TestEqual(TEXT("AI never spends player's gold"),B->Gold[1],0.f);TestFalse(TEXT("AI does not get instant river access"),B->HasCrossing());
    for(int I=0;I<240;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Enemy-funded bridge eventually opens for everyone"),B->HasCrossing());
    if(!B->Bridges.IsEmpty())B->DestroyBridges(B->BridgeCenter(B->Bridges[0]),340);
    B->Time+=B->AIInterval+.1f;B->Gold[2]=75;B->RunAI();TestEqual(TEXT("AI can rebuild after its crossing is destroyed"),B->Bridges.Num(),1);
    TestEqual(TEXT("Rebuilding is also paid for"),B->Gold[2],0.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelVillageUpgradeTest,"VoxelDominion.Simulation.VillageUpgrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelVillageUpgradeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Village;T.Troops=42;T.Cell=FIntPoint(-8,0);B->Towers.Add(T);
    T.Team=2;T.Cell=FIntPoint(8,0);B->Towers.Add(T);
    B->SelectTower(0);B->Gold[1]=39;TestFalse(TEXT("Village needs full upgrade cost"),B->UpgradeTower(0));
    TestEqual(TEXT("Failed development keeps thinking"),B->GetBattleSpeed(),.2f);
    B->Gold[1]=999;
    for(int Level=1;Level<5;++Level)
    {
        int Cost=20*(Level+1);float Before=B->Gold[1];TestEqual(TEXT("Village price scales by 20"),B->UpgradeCost(0),Cost);
        TestTrue(TEXT("Village develops successfully"),B->UpgradeTower(0));TestEqual(TEXT("Development pays exact price"),B->Gold[1],Before-Cost);
        TestEqual(TEXT("Development never creates free garrison troops"),B->Towers[0].Troops,42.f);
        TestEqual(TEXT("Village garrison capacity stays 250"),B->TroopCapacity(0),250.f);
        TestEqual(TEXT("Village troop supply scales with level"),B->VillageTroopSupply(0),1+.25f*Level);
        TestTrue(TEXT("Village gold supply scales with level"),FMath::IsNearlyEqual(B->VillageGoldSupply(0),.4f+.1f*Level,.001f));
    }
    TestEqual(TEXT("Successful development restores normal time"),B->GetBattleSpeed(),1.f);
    TestFalse(TEXT("Village maximum level is five"),B->UpgradeTower(0));TestFalse(TEXT("Cannot develop enemy village"),B->UpgradeTower(1));
    B->SelectTower(0);B->Gold[2]=40;TestTrue(TEXT("AI can develop its own village"),B->UpgradeTower(1,2));
    TestEqual(TEXT("AI development uses enemy money"),B->Gold[2],0.f);TestEqual(TEXT("AI development preserves player's thinking"),B->GetBattleSpeed(),.2f);
    TestTrue(TEXT("Each side sums its own village levels"),FMath::IsNearlyEqual(B->CommandRecovery(1),2.65f)&&FMath::IsNearlyEqual(B->CommandRecovery(2),1.9f));
    B->Paused=true;TestFalse(TEXT("Pause prevents village development"),B->UpgradeTower(1,2));B->Paused=false;
    B->Winner=3;TestFalse(TEXT("Ended battle prevents village development"),B->UpgradeTower(1,2));
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelProtectionTest,"VoxelDominion.Simulation.HeadquartersProtection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelProtectionTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;
    auto Add=[&](int Team,EVoxelModule Module,int X,int Y,float Troops){FVoxelTower T;T.Team=Team;T.Module=Module;T.Cell=FIntPoint(X,Y);T.Troops=Troops;B->Towers.Add(T);};
    Add(1,EVoxelModule::Command,-10,0,100);Add(2,EVoxelModule::Command,-6,0,10);
    Add(1,EVoxelModule::Arrow,-10,8,100);Add(2,EVoxelModule::Cannon,-6,8,1);Add(2,EVoxelModule::Village,-6,-8,0);Add(0,EVoxelModule::Village,-15,-8,0);
    TestEqual(TEXT("Enemy village and cannon both protect HQ"),B->OutpostsRemaining(2),2);
    TestFalse(TEXT("Enemy HQ initially protected"),B->CanAttackTower(1,1));TestFalse(TEXT("Player HQ follows identical protection"),B->CanAttackTower(0,2));
    B->SelectTower(0);TestFalse(TEXT("Cannot dispatch toward protected HQ"),B->Dispatch(0,1,.5f));
    TestEqual(TEXT("Rejected HQ attack spends no troops"),B->Towers[0].Troops,100.f);TestEqual(TEXT("Rejected attack retains thinking time"),B->GetBattleSpeed(),.2f);
    TestTrue(TEXT("Friendly HQ reinforcement remains allowed"),B->Dispatch(2,0,.1f));B->Squads.Empty();
    FVoxelSquad S;S.ID=40;S.Team=1;S.Target=INDEX_NONE;S.Troops=50;S.Position=B->CellPosition(FIntPoint(-12,0))+FVector(0,0,27);S.HasDestination=true;S.Destination=FIntPoint(-12,0);S.Holding=true;B->Squads.Add(S);
    B->SelectSquad(40);TestFalse(TEXT("Field army cannot bypass HQ protection"),B->OrderSquad(40,1,FVector::ZeroVector));
    TestTrue(TEXT("Rejected retarget retains original holding order"),B->Squads[0].Holding&&B->Squads[0].Target==INDEX_NONE);
    FVoxelSquad Attack;Attack.Team=1;Attack.Target=3;Attack.Troops=10;B->Arrive(Attack);
    TestFalse(TEXT("Remaining village keeps HQ protected"),B->CanAttackTower(1,1));
    B->Towers[4].Level=5;Attack.Target=4;Attack.Troops=10;B->Arrive(Attack);
    TestTrue(TEXT("Last village capture unlocks HQ despite neutral villages"),B->CanAttackTower(1,1));TestEqual(TEXT("Village level survives capture"),B->Towers[4].Level,5);
    TestTrue(TEXT("High-level village income transfers to new owner"),FMath::IsNearlyEqual(B->GoldRecovery(1),1.8f));
    TestTrue(TEXT("Unlocked HQ accepts field army order"),B->OrderSquad(40,1,FVector::ZeroVector));
    Attack.Team=2;Attack.Target=4;Attack.Troops=20;B->Arrive(Attack);
    TestFalse(TEXT("Recapturing village restores HQ protection"),B->CanAttackTower(1,1));TestEqual(TEXT("Recapture keeps village level"),B->Towers[4].Level,5);
    TestTrue(TEXT("Village economic bonus immediately follows recapture"),FMath::IsNearlyEqual(B->GoldRecovery(2),1.8f));
    B->Squads[0].Position=B->CellPosition(B->Towers[1].Cell)+FVector(0,0,27);float Defense=B->Towers[1].Troops;B->Arrive(B->Squads[0]);
    TestEqual(TEXT("In-flight attack cannot damage relocked HQ"),B->Towers[1].Troops,Defense);
    TestTrue(TEXT("Relocked arrival preserves army in holding mode"),B->Squads[0].Troops==50&&B->Squads[0].Holding&&B->Squads[0].Target==INDEX_NONE);TestEqual(TEXT("Protected HQ never ends battle"),B->Winner,0);
    B->Squads.Empty();B->CancelCommand();TestTrue(TEXT("Ground orders may still reach HQ surroundings"),B->DispatchTo(0,B->CellPosition(B->Towers[1].Cell),.2f));
    B->Squads[0].Position=B->CellPosition(B->Towers[1].Cell)+FVector(0,0,27);B->Squads[0].Route.Empty();B->Simulate(.05f);
    TestEqual(TEXT("Ground destination does not capture HQ"),B->Towers[1].Team,2);TestEqual(TEXT("Ground destination does not end game"),B->Winner,0);
    B->Towers[4].Team=1;B->Squads.Empty();Attack.Team=1;Attack.Target=1;Attack.Troops=100;B->Arrive(Attack);
    TestEqual(TEXT("Unlocked HQ capture produces victory"),B->Winner,1);
    auto* AI=World->SpawnActor<AVoxelBattle>();AI->InitializeLayout();
    for(auto& T:AI->Towers){T.Team=2;T.Troops=250;}AI->Towers[0].Team=1;AI->Towers[1].Team=1;AI->Towers[1].Troops=0;
    AI->RunAI();for(const auto& Army:AI->Squads)TestTrue(TEXT("AI never selects protected HQ"),Army.Target!=0);
    AI->Towers[1].Team=2;AI->Squads.Empty();AI->AIOrders.Empty();AI->Time=AI->AIInterval+.1f;AI->RunAI();
    TestTrue(TEXT("AI can assault unlocked HQ"),AI->Incoming(0,2)>0);
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTimeLimitTest,"VoxelDominion.Simulation.TwentyMinuteSettlement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelTimeLimitTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Duel=[&]()
    {
        auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;B->CommandRegen=0;
        FVoxelTower T;T.Module=EVoxelModule::Command;T.Troops=20;T.Team=1;T.Cell=FIntPoint(-10,0);B->Towers.Add(T);T.Team=2;T.Cell=FIntPoint(10,0);B->Towers.Add(T);return B;
    };
    auto* OldDeadline=Duel();TestEqual(TEXT("Standard battles last twenty minutes"),OldDeadline->BattleTimeLimit,1200.f);
    OldDeadline->Time=899.98f;OldDeadline->AdvanceBattle(.25f);TestTrue(TEXT("Passing the former fifteen-minute limit does not end the battle"),OldDeadline->Time>900&&!OldDeadline->Winner&&!OldDeadline->TimeLimitReached);
    auto* More=Duel();FVoxelTower Village;Village.Module=EVoxelModule::Village;Village.Team=1;Village.Cell=FIntPoint(-15,8);Village.Troops=1;More->Towers.Add(Village);
    More->Towers[1].Troops=250;More->Time=1199.98f;More->AdvanceBattle(.25f);
    TestEqual(TEXT("Time stops exactly at twenty battle minutes"),More->Time,1200.f);TestEqual(TEXT("More holdings wins before troop comparison"),More->Winner,1);
    TestEqual(TEXT("Village is included in final holding score"),More->FinalHoldings[1],2);TestTrue(TEXT("Timed result is identified"),More->TimeLimitReached);
    float Gold=More->Gold[1],Troops=More->FinalTroops[1];FVoxelBridge Bridge;Bridge.Remaining=30;More->Bridges.Add(Bridge);
    FVoxelEffect Effect;Effect.Remaining=2;More->Effects.Add(Effect);More->AdvanceBattle(.25f);More->Simulate(1);More->RunAI();
    TestEqual(TEXT("Settlement freezes economy"),More->Gold[1],Gold);TestEqual(TEXT("Settlement freezes bridges"),More->Bridges[0].Remaining,30.f);TestEqual(TEXT("Settlement freezes effects"),More->Effects[0].Remaining,2.f);
    TestEqual(TEXT("Settlement scores remain fixed"),More->FinalTroops[1],Troops);TestFalse(TEXT("No dispatch after settlement"),More->Dispatch(0,1,1));
    auto* Army=Duel();Army->Time=1200;
    FVoxelSquad S;S.Team=1;S.Troops=30;S.Holding=true;S.Position=FVector(-2500,1200,100);Army->Squads.Add(S);S.Team=2;S.Troops=40;Army->Squads.Add(S);S.Team=1;S.Troops=-100;Army->Squads.Add(S);
    Village.Team=0;Village.Troops=250;Army->Towers.Add(Village);Army->AdvanceBattle(.05f);
    TestEqual(TEXT("Equal holdings compares field armies plus garrisons"),Army->Winner,2);TestEqual(TEXT("Dead armies contribute no negative score"),Army->FinalTroops[1],50.f);TestEqual(TEXT("All living enemy troops count"),Army->FinalTroops[2],60.f);
    TestEqual(TEXT("Neutral holdings do not count"),Army->FinalHoldings[1],1);
    auto* Draw=Duel();Draw->Time=1200;Draw->SelectTower(0);Draw->Simulate(.05f);
    TestEqual(TEXT("Identical holdings and troop totals produce a draw"),Draw->Winner,3);TestEqual(TEXT("Draw freezes thinking and battle time"),Draw->GetBattleSpeed(),0.f);
    auto* Clock=Duel();Clock->Time=1199;Clock->Paused=true;Clock->AdvanceBattle(.25f);TestEqual(TEXT("Manual pause stops deadline clock"),Clock->Time,1199.f);
    Clock->Paused=false;Clock->SelectTower(0);for(int I=0;I<4;++I)Clock->AdvanceBattle(.25f);
    TestTrue(TEXT("Thinking time slows the deadline equally"),FMath::IsNearlyEqual(Clock->Time,1199.2f,.001f));TestEqual(TEXT("Thinking does not trigger premature settlement"),Clock->Winner,0);
    auto* Capture=Duel();Capture->Time=1199.98f;Capture->Towers[1].Troops=1;
    S=FVoxelSquad();S.Team=1;S.Target=1;S.Troops=10;S.Position=Capture->CellPosition(Capture->Towers[1].Cell)+FVector(0,0,27);S.Route.Add(S.Position);Capture->Squads.Add(S);Capture->AdvanceBattle(.05f);
    TestEqual(TEXT("HQ capture on last step is decisive"),Capture->Winner,1);TestFalse(TEXT("HQ capture result takes priority over score settlement"),Capture->TimeLimitReached);
    TestEqual(TEXT("Captured army is not counted twice in final troops"),Capture->FinalTroops[1],29.f);
    auto* Restart=Duel();TestEqual(TEXT("New battle resets deadline"),Restart->Time,0.f);TestEqual(TEXT("New battle resets result"),Restart->Winner,0);TestFalse(TEXT("New battle clears timed ending"),Restart->TimeLimitReached);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMergeTest,"VoxelDominion.Simulation.FriendlyArmyMerging",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelMergeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Battle=[&](){auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;return B;};
    auto Add=[&](AVoxelBattle* B,int32 ID,int32 Team,float Troops,FVector Position)
    {
        FVoxelSquad S;S.ID=ID;S.Team=Team;S.Target=INDEX_NONE;S.Troops=Troops;S.Position=Position;
        S.HasDestination=true;S.Destination=B->ToCell(Position);S.Holding=true;S.RetargetIn=1000;B->Squads.Add(S);
    };
    FVector Base(-1440,0,97);
    auto* B=Battle();Add(B,1,1,80,Base);Add(B,2,1,20,Base+FVector(60,0,0));Add(B,3,2,40,Base);Add(B,4,2,10,Base+FVector(60,0,0));
    B->Squads[2].InterceptTarget=2;B->Squads[0].Destination=FIntPoint(-12,4);B->Squads[0].Route.Add(FVector(-2000,800,97));B->Squads[0].MarchDelay=3;
    float BlueBefore=B->TotalArmy(1),RedBefore=B->TotalArmy(2);B->MergeNearbySquads();
    TestEqual(TEXT("Both teams merge their nearby allies"),B->Squads.Num(),2);
    TestEqual(TEXT("Player troops are conserved exactly"),B->TotalArmy(1),BlueBefore);TestEqual(TEXT("AI troops are conserved exactly"),B->TotalArmy(2),RedBefore);
    int32 Host=B->SquadIndex(1),Enemy=B->SquadIndex(3);TestTrue(TEXT("Larger army retains stable identity"),Host!=INDEX_NONE&&Enemy!=INDEX_NONE);
    if(Host!=INDEX_NONE){TestEqual(TEXT("Larger army keeps its destination"),B->Squads[Host].Destination,FIntPoint(-12,4));TestEqual(TEXT("Merge preserves marching delay"),B->Squads[Host].MarchDelay,3.f);TestEqual(TEXT("Merge preserves route"),B->Squads[Host].Route.Num(),1);TestEqual(TEXT("Merge does not move the host"),B->Squads[Host].Position,Base);}
    if(Enemy!=INDEX_NONE)TestEqual(TEXT("Interceptors follow the merged army's surviving ID"),B->Squads[Enemy].InterceptTarget,1);
    TestEqual(TEXT("Absorbed IDs are removed"),B->SquadIndex(2),INDEX_NONE);B->MergeNearbySquads();TestEqual(TEXT("Repeated merge never duplicates troops"),B->TotalArmy(1),BlueBefore);
    auto* Selected=Battle();Add(Selected,10,1,800,Base);Add(Selected,11,1,5,Base+FVector(30,0,0));Selected->Squads[1].Destination=FIntPoint(-3,7);Selected->SelectSquad(11);
    Selected->MergeNearbySquads();TestEqual(TEXT("Selected smaller army retains its ID"),Selected->SelectedSquad,11);TestEqual(TEXT("Merged army is not limited by garrison capacity"),Selected->Squads[0].Troops,805.f);
    TestEqual(TEXT("Selected command takes precedence"),Selected->Squads[0].Destination,FIntPoint(-3,7));TestEqual(TEXT("Merging preserves tactical thinking"),Selected->GetBattleSpeed(),.2f);
    TestTrue(TEXT("Merged army can receive a new order"),Selected->OrderSquad(11,INDEX_NONE,Selected->CellPosition(FIntPoint(-4,2))));
    auto* Separate=Battle();Add(Separate,20,1,10,Base);Add(Separate,21,1,20,Base+FVector(121,0,0));Add(Separate,22,1,0,Base);Add(Separate,23,2,10,Base);
    Separate->MergeNearbySquads();TestEqual(TEXT("Far allies, enemies and dead armies never merge"),Separate->Squads.Num(),4);TestEqual(TEXT("Dead troops cannot be absorbed"),Separate->TotalArmy(1),30.f);
    Separate->Squads[1].Position=Base+FVector(119,0,0);Separate->Paused=true;Separate->MergeNearbySquads();TestEqual(TEXT("Pause prevents merging"),Separate->Squads.Num(),4);
    Separate->Paused=false;Separate->Winner=3;Separate->MergeNearbySquads();TestEqual(TEXT("Ended battle prevents merging"),Separate->Squads.Num(),4);
    Separate->Winner=0;Separate->MergeNearbySquads();TestEqual(TEXT("Merging resumes with the simulation"),Separate->TotalArmy(1),30.f);TestEqual(TEXT("Only one living player army remains"),Separate->Squads.FilterByPredicate([](const FVoxelSquad& S){return S.Team==1&&S.Troops>0;}).Num(),1);
    auto* River=Battle();River->SquadMergeRadius=500;Add(River,30,1,10,FVector(-180,0,97));Add(River,31,1,20,FVector(180,0,97));River->MergeNearbySquads();
    TestEqual(TEXT("No merging through an unbridged river even with a large radius"),River->Squads.Num(),2);
    FVoxelBridge Crossing;Crossing.Remaining=60;River->Bridges.Add(Crossing);River->MergeNearbySquads();TestEqual(TEXT("Unfinished bridge does not permit merging"),River->Squads.Num(),2);
    River->Bridges[0].Remaining=0;River->Squads[0].Evacuating=true;River->MergeNearbySquads();TestEqual(TEXT("Evacuating armies finish retreat before merging"),River->Squads.Num(),2);
    River->Squads[0].Evacuating=false;
    for(int I=0;I<20;++I)River->Simulate(.05f);
    TestEqual(TEXT("Completed walkable crossing permits armies to close the gap and merge"),River->Squads.Num(),1);
    auto* Moving=Battle();Add(Moving,40,1,20,Base);Add(Moving,41,1,30,Base+FVector(140,0,0));Moving->Squads[0].Holding=false;Moving->Squads[0].Route.Add(Moving->Squads[1].Position);
    for(int I=0;I<3;++I)Moving->Simulate(.05f);
    TestEqual(TEXT("Normal simulation automatically merges converging armies"),Moving->Squads.Num(),1);TestEqual(TEXT("Normal merge preserves total troops"),Moving->TotalArmy(1),50.f);
    auto* Slope=Battle();FVector Lower=Base+FVector(119,0,0),Upper=Base;
    Lower.Z=Slope->Height(Slope->ToCell(Lower))+27;Upper.Z=Slope->Height(Slope->ToCell(Upper))+27;
    Add(Slope,50,1,20,Upper);Add(Slope,51,1,30,Lower);Slope->MergeNearbySquads();
    TestEqual(TEXT("Nearby armies on a connected slope merge by ground separation"),Slope->Squads.Num(),1);
    for(int Team : {1,2})
    {
        auto* Rapid=Battle();
        FVoxelTower Tower;Tower.Team=Team;Tower.Module=EVoxelModule::Village;Tower.Cell=FIntPoint(-10,0);Tower.Troops=100;Rapid->Towers.Add(Tower);
        const FVector Post=Rapid->CellPosition(FIntPoint(-5,0));
        TestTrue(TEXT("First wave leaves tower"),Rapid->DispatchTo(0,Post,.5f));
        for(int I=0;I<21;++I)Rapid->Simulate(.05f);
        TestTrue(TEXT("Second wave can take the same route"),Rapid->DispatchTo(0,Post,.5f));
        if(Rapid->Squads.Num()==2)
        {
            const double Gap=FVector::Dist2D(Rapid->Squads[0].Position,Rapid->Squads[1].Position);
            Rapid->Simulate(.05f);
            TestTrue(TEXT("Following wave closes the gap before merging"),Rapid->Squads.Num()==2&&
                FVector::Dist2D(Rapid->Squads[0].Position,Rapid->Squads[1].Position)<Gap);
        }
        for(int I=0;I<28;++I)Rapid->Simulate(.05f);
        TestEqual(TEXT("Both waves merge while still marching"),Rapid->Squads.Num(),1);
        if(Rapid->Squads.Num()==1)TestEqual(TEXT("No soldiers are lost when successive waves merge"),Rapid->Squads[0].Troops,75.f);
    }
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeteorCooldownTest,"VoxelDominion.Simulation.ThreeMinuteMeteorCooldown",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelMeteorCooldownTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;
    TestEqual(TEXT("Meteor default cooldown is 180 battle seconds"),B->MeteorCooldown,180.f);B->Gold[1]=999;B->Meteor(FVector(0,0,70));
    TestEqual(TEXT("Casting starts full cooldown"),B->MeteorReady,180.f);float Gold=B->Gold[1];B->Meteor(FVector(0,0,70));TestEqual(TEXT("Repeated cast cannot spend gold during cooldown"),B->Gold[1],Gold);
    TestEqual(TEXT("Repeated cast cannot create another meteor"),B->Effects.Num(),1);
    for(int I=0;I<1200;++I)B->Simulate(.05f);
    TestTrue(TEXT("Meteor has two minutes remaining when a one-minute bridge completes"),B->MeteorReady>119&&B->MeteorReady<121);
    B->Meteor(FVector(0,0,70));TestEqual(TEXT("Cannot cast again at the old bridge build deadline"),B->Effects.Num(),0);
    for(int I=0;I<1200;++I)B->Simulate(.05f);
    Gold=B->Gold[1];B->Meteor(FVector(0,0,70));TestTrue(TEXT("Meteor remains unavailable at the former two-minute cooldown"),B->MeteorReady>59&&B->MeteorReady<61&&B->Effects.IsEmpty());TestEqual(TEXT("Casting at the old cooldown spends no gold"),B->Gold[1],Gold);
    B->Paused=true;float Ready=B->MeteorReady;B->AdvanceBattle(.25f);TestEqual(TEXT("Pause freezes meteor cooldown"),B->MeteorReady,Ready);B->Paused=false;
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Village;T.Cell=FIntPoint(-8,0);B->Towers.Add(T);B->SelectTower(0);
    for(int I=0;I<20;++I)B->AdvanceBattle(.05f);TestTrue(TEXT("Meteor cooldown follows 0.2 thinking time"),FMath::IsNearlyEqual(Ready-B->MeteorReady,.2f,.002f));
    B->CancelCommand();for(int I=0;I<1200;++I)B->Simulate(.05f);TestEqual(TEXT("Cooldown reaches zero after three battle minutes"),B->MeteorReady,0.f);B->Gold[1]=55;B->Meteor(FVector(0,0,70));
    TestEqual(TEXT("New cast is allowed after cooldown"),B->Gold[1],0.f);TestEqual(TEXT("New cast resets full three-minute cooldown"),B->MeteorReady,180.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMenuFlowTest,"VoxelDominion.Interface.MainMenuAndBattleFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelMenuFlowTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();auto* P=World->SpawnActor<AVoxelCommander>();
    TestTrue(TEXT("Controller starts at the title menu"),P->MenuPage==EVoxelMenuPage::Main);TestFalse(TEXT("No battle has started on launch"),P->BattleStarted);
    P->ShowMenu();TestTrue(TEXT("Main menu freezes the preview battle"),B->MenuPaused);B->AdvanceBattle(.25f);B->RunAI();
    TestEqual(TEXT("Preview does not earn gold"),B->Gold[1],0.f);TestEqual(TEXT("Preview does not advance time"),B->Time,0.f);TestEqual(TEXT("Preview AI cannot dispatch"),B->Squads.Num(),0);
    B->Gold[1]=999;TestFalse(TEXT("Menu prevents direct upgrade commands"),B->UpgradeTower(0));TestFalse(TEXT("Menu prevents direct dispatch commands"),B->DispatchTo(0,B->CellPosition(FIntPoint(-20,0)),.5f));
    P->BridgeKey();P->MeteorKey();P->PauseKey();P->RestartKey();P->FractionKey();P->UpgradeKey();TestFalse(TEXT("Battle hotkeys cannot activate behind menus"),B->BridgeMode||B->MeteorMode||B->Paused);TestEqual(TEXT("Restart hotkey cannot bypass title menu"),B->Gold[1],999.f);
    P->MenuAction(TEXT("setup"));TestTrue(TEXT("Setup page can be opened"),P->MenuPage==EVoxelMenuPage::Setup);P->MenuAction(TEXT("practice"));P->ConfirmKey();
    TestFalse(TEXT("Starting a battle closes the menu"),P->IsMenuOpen());TestTrue(TEXT("Starting enables later resume"),P->BattleStarted);TestFalse(TEXT("Practice disables active AI"),B->AIEnabled);TestFalse(TEXT("Practice removes deadline"),B->TimeLimitEnabled);
    TestEqual(TEXT("Practice still starts at zero gold"),B->Gold[1],0.f);B->Time=1201;B->Simulate(.05f);B->RunAI();TestEqual(TEXT("Practice continues beyond twenty minutes"),B->Winner,0);TestTrue(TEXT("Practice clock continues"),B->Time>1201);TestEqual(TEXT("Practice AI never issues an attack"),B->Squads.Num(),0);
    B->Gold[1]=123;B->Time=42;FVoxelBridge Bridge;Bridge.Remaining=30;B->Bridges.Add(Bridge);FVoxelEffect E;E.Remaining=5;B->Effects.Add(E);
    FVoxelSquad S;S.Team=1;S.Troops=20;S.Target=INDEX_NONE;S.Holding=true;S.Position=FVector(-3000,1000,97);S.ID=9;B->Squads.Add(S);B->SelectSquad(9);
    P->ShowMenu();float Before=B->Time,ArmyBefore=B->TotalArmy(1);B->AdvanceBattle(.25f);B->Simulate(1);B->RunAI();
    TestEqual(TEXT("Returning to menu freezes active battle time"),B->Time,Before);TestEqual(TEXT("Menu preserves active battle money"),B->Gold[1],123.f);TestEqual(TEXT("Menu freezes bridge construction"),B->Bridges[0].Remaining,30.f);TestEqual(TEXT("Menu freezes effects"),B->Effects[0].Remaining,5.f);
    P->CancelKey();TestFalse(TEXT("Escape from menu can resume an existing battle"),P->IsMenuOpen());TestEqual(TEXT("Resuming preserves selected army"),B->SelectedSquad,9);TestEqual(TEXT("Resuming preserves active troops"),B->TotalArmy(1),ArmyBefore);
    P->ShowMenu();P->MenuAction(TEXT("start"));TestTrue(TEXT("Replacing active battle requires an in-game confirmation"),P->MenuPage==EVoxelMenuPage::ConfirmNew);
    P->MenuAction(TEXT("back"));TestEqual(TEXT("Cancelling a new battle keeps progress"),B->Gold[1],123.f);
    P->MenuAction(TEXT("setup"));P->MenuAction(TEXT("standard"));P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    TestTrue(TEXT("New standard battle restores AI and deadline"),B->AIEnabled&&B->TimeLimitEnabled);TestEqual(TEXT("New battle resets elapsed time"),B->Time,0.f);TestEqual(TEXT("New battle resets money"),B->Gold[1],0.f);
    TestTrue(TEXT("New battle clears armies, effects and bridges"),B->Squads.IsEmpty()&&B->Effects.IsEmpty()&&B->Bridges.IsEmpty());TestEqual(TEXT("New battle restores forty sites"),B->Towers.Num(),40);
    TestEqual(TEXT("New battle clears selection"),B->SelectedSquad,INDEX_NONE);TestEqual(TEXT("New battle clears AI timers"),B->NextAIAction,0.f);TestEqual(TEXT("New battle starts at normal speed"),B->GetBattleSpeed(),1.f);
    P->CancelKey();TestTrue(TEXT("Escape on unselected battlefield opens menu"),P->IsMenuOpen());P->MenuAction(TEXT("settings"));P->MenuAction(TEXT("defaults"));
    TestEqual(TEXT("Default quality draft is high"),P->DraftQuality,2);TestEqual(TEXT("Default frame cap draft is sixty"),P->DraftFrameLimit,1);P->MenuAction(TEXT("camera"));TestEqual(TEXT("Camera speed setting cycles"),P->DraftCameraSpeed,2);TestEqual(TEXT("Unapplied settings leave camera unchanged"),P->CameraSpeed,1.f);
    P->MenuAction(TEXT("vsync"));TestTrue(TEXT("VSync setting cycles"),P->DraftVSync);P->CancelKey();TestTrue(TEXT("Escape from settings returns to main menu"),P->MenuPage==EVoxelMenuPage::Main);
    P->MenuAction(TEXT("quit"));TestTrue(TEXT("Quit opens an in-game confirmation"),P->MenuPage==EVoxelMenuPage::ConfirmQuit);P->CancelKey();TestTrue(TEXT("Quit can be cancelled"),P->MenuPage==EVoxelMenuPage::Main);
    B->Winner=3;P->ResumeBattle();TestTrue(TEXT("Ended battle cannot resume"),P->IsMenuOpen());P->MenuAction(TEXT("start"));TestEqual(TEXT("Starting after a result clears winner"),B->Winner,0);TestEqual(TEXT("Restart after a result clears time"),B->Time,0.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTutorialTest,"VoxelDominion.Interface.InteractiveTutorial",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelTutorialTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();auto* P=World->SpawnActor<AVoxelCommander>();
    P->MenuAction(TEXT("tutorial"));
    TestTrue(TEXT("Main menu starts tutorial mode"),B->Tutorial&&!P->IsMenuOpen());TestEqual(TEXT("Training map has six sites"),B->Towers.Num(),6);
    TestEqual(TEXT("Training starts with explicit teaching funds"),B->Gold[1],250.f);TestFalse(TEXT("Tutorial has no free-running AI or deadline"),B->AIEnabled||B->TimeLimitEnabled);
    TestTrue(TEXT("Training land is smaller than the main battlefield"),B->Walkable(FIntPoint(10,7))&&!B->Walkable(FIntPoint(11,0))&&!B->Walkable(FIntPoint(0,0)));
    B->NextTutorialLesson();TestEqual(TEXT("Uncompleted lessons cannot be skipped"),B->TutorialStep,0);
    TestFalse(TEXT("Early construction cannot skip the lesson sequence"),B->BuildBridge(0));B->Meteor(FVector::ZeroVector);TestEqual(TEXT("Blocked teaching actions spend nothing"),B->Gold[1],250.f);
    B->SelectTower(0);B->UpdateTutorial();TestTrue(TEXT("Selecting own HQ completes first objective"),B->TutorialObjectiveComplete);
    B->AdvanceBattle(.25f);TestEqual(TEXT("Completed lessons freeze the battlefield for reading"),B->Time,0.f);B->NextTutorialLesson();
    auto RequireProgress=[&](){if(B->TutorialObjectiveComplete)return true;AddError(FString::Printf(TEXT("Tutorial stuck at lesson %d"),B->TutorialStep+1));World->DestroyWorld(false);return false;};
    auto Wait=[&](int Frames=1000){for(int I=0;I<Frames&&!B->TutorialObjectiveComplete&&!B->Winner;++I)B->AdvanceBattle(.25f);};
    TestTrue(TEXT("Tutorial uses real dispatch to capture village"),B->Dispatch(0,2,.5f));Wait();if(!RequireProgress())return false;TestTrue(TEXT("Village capture advances only after arrival"),B->TutorialObjectiveComplete&&B->Towers[2].Team==1);B->NextTutorialLesson();
    B->SelectTower(2);TestTrue(TEXT("Village development uses the actual upgrade mechanism"),B->UpgradeTower(2));B->UpdateTutorial();TestTrue(TEXT("Developing village completes economic lesson"),B->TutorialObjectiveComplete);B->NextTutorialLesson();
    B->SelectTower(1);TestFalse(TEXT("Tutorial cannot upgrade a different military tower"),B->UpgradeTower(0));TestTrue(TEXT("Arrow tower can be upgraded in correct lesson"),B->UpgradeTower(1));B->UpdateTutorial();B->NextTutorialLesson();
    B->SelectTower(1);B->BindArrow(EVoxelArrow::Poison);B->UpdateTutorial();TestTrue(TEXT("Permanent arrow choice satisfies modification lesson"),B->TutorialObjectiveComplete&&B->Towers[1].PlayerBound);B->NextTutorialLesson();
    FVector Guard=B->CellPosition(FIntPoint(-3,2));TestFalse(TEXT("Deployment requires enough force for the drill"),B->DispatchTo(0,Guard,.1f));
    TestTrue(TEXT("Player can deploy a real holding army"),B->DispatchTo(0,Guard,.75f));Wait();if(!RequireProgress())return false;TestTrue(TEXT("Deployment waits for stationary troops at the marked area"),B->TutorialObjectiveComplete);
    B->NextTutorialLesson();TestTrue(TEXT("Encounter lesson dispatches an actual paid enemy army"),B->TutorialRaidID!=INDEX_NONE&&B->Towers[3].Troops==12);
    Wait();if(!RequireProgress())return false;TestTrue(TEXT("Encounter lesson requires real combat and defeat of the raid"),B->TutorialObjectiveComplete&&B->TutorialEncounterSeen&&B->SquadIndex(B->TutorialRaidID)==INDEX_NONE);B->NextTutorialLesson();
    TestTrue(TEXT("Bridge can be placed at a freely chosen training row"),B->BuildBridge(2));TestEqual(TEXT("Tutorial keeps real one-minute build time"),B->Bridges[0].Remaining,60.f);
    B->ToggleTutorialFastForward();TestEqual(TEXT("Optional teaching fast forward runs at five times speed"),B->GetBattleSpeed(),5.f);
    B->MenuPaused=true;float BuildTime=B->Bridges[0].Remaining;B->AdvanceBattle(.25f);TestEqual(TEXT("Menu still freezes fast-forwarded construction"),B->Bridges[0].Remaining,BuildTime);B->MenuPaused=false;
    Wait();if(!RequireProgress())return false;TestTrue(TEXT("Construction lesson ends only at completion"),B->TutorialObjectiveComplete&&B->HasCrossing());B->NextTutorialLesson();
    TestEqual(TEXT("Meteor demonstration has fifty blue troops"),B->Squads[B->SquadIndex(B->TutorialBlueDemoID)].Troops,50.f);
    float SavedGold=B->Gold[1],SavedTime=B->Time;B->Meteor(B->CellPosition(FIntPoint(-9,-6)));for(int I=0;I<8;++I)B->AdvanceBattle(.25f);
    TestFalse(TEXT("Missing the demonstration targets cannot complete meteor lesson"),B->TutorialObjectiveComplete);B->RetryTutorialLesson();
    TestEqual(TEXT("Retry restores teaching funds"),B->Gold[1],SavedGold);TestEqual(TEXT("Retry restores battle clock"),B->Time,SavedTime);TestEqual(TEXT("Retry restores skill availability instead of forcing three-minute wait"),B->MeteorReady,0.f);
    TestTrue(TEXT("Retry restores the completed bridge"),B->HasCrossing());B->Meteor(B->CellPosition(FIntPoint(0,2)));Wait(20);if(!RequireProgress())return false;
    TestTrue(TEXT("Meteor must hit both factions and destroy the bridge"),B->TutorialObjectiveComplete&&B->TutorialBlueHit&&B->TutorialRedHit&&!B->HasCrossing());
    TestEqual(TEXT("Friendly demonstration really loses 38 troops"),B->Squads[B->SquadIndex(B->TutorialBlueDemoID)].Troops,12.f);
    TestEqual(TEXT("Enemy demonstration loses the same amount"),B->Squads[B->SquadIndex(B->TutorialRedDemoID)].Troops,12.f);
    TestTrue(TEXT("Meteor retains its normal 180-second cooldown"),B->MeteorReady>175&&B->MeteorReady<180);B->NextTutorialLesson();
    TestTrue(TEXT("Rebuilding is paid and starts a new construction site"),B->BuildBridge(2));B->ToggleTutorialFastForward();Wait();if(!RequireProgress())return false;TestTrue(TEXT("Rebuilding restores the crossing"),B->TutorialObjectiveComplete&&B->HasCrossing());B->NextTutorialLesson();
    TestFalse(TEXT("Enemy HQ still requires all enemy outposts"),B->CanAttackTower(5,1));
    TestTrue(TEXT("Player can attack the training camp"),B->Dispatch(0,3,.75f));for(int I=0;I<500&&B->Towers[3].Team==2;++I)B->AdvanceBattle(.25f);
    TestEqual(TEXT("Camp is captured through normal combat"),B->Towers[3].Team,1);
    TestTrue(TEXT("Player can march over bridge to eastern outpost"),B->Dispatch(1,4,.9f));Wait();if(!RequireProgress())return false;TestTrue(TEXT("Clearing all enemy outposts unlocks the last lesson"),B->TutorialObjectiveComplete&&B->CanAttackTower(5,1));B->NextTutorialLesson();
    TestTrue(TEXT("Final lesson uses real HQ attack"),B->Dispatch(4,5,.95f));Wait();if(!RequireProgress())return false;TestTrue(TEXT("Capturing HQ completes the tutorial and freezes combat"),B->Winner==1&&B->TutorialObjectiveComplete&&B->GetBattleSpeed()==0);
    P->SelectedMode=0;P->StartBattle();TestFalse(TEXT("Starting a standard match clears tutorial restrictions"),B->Tutorial);TestEqual(TEXT("Standard map returns to forty sites"),B->Towers.Num(),40);
    TestTrue(TEXT("Standard map boundaries and AI are restored"),B->Walkable(FIntPoint(24,17))&&B->AIEnabled&&B->TimeLimitEnabled);TestEqual(TEXT("Teaching funds never carry into normal matches"),B->Gold[1],0.f);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelMeteorVisualTest,"VoxelDominion.Visuals.MeteorFlightAndSingleImpact",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelMeteorVisualTest::RunTest(const FString&)
{
    FVector Target(0,0,70),Start=VoxelRules::MeteorPosition(Target,1.5f),Mid=VoxelRules::MeteorPosition(Target,.75f),End=VoxelRules::MeteorPosition(Target,0);
    TestTrue(TEXT("Meteor descends visibly from the sky"),Start.Z>Mid.Z&&Mid.Z>End.Z&&Start.Z-End.Z>1500);
    TestTrue(TEXT("Meteor flight has lateral motion"),FVector::Dist2D(Start,End)>500);TestTrue(TEXT("Flight ends directly over the marked impact point"),FVector::Dist2D(End,Target)<.001f);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->AIInterval=10000;
    FVoxelBridge Bridge;Bridge.Remaining=0;B->Bridges.Add(Bridge);
    for(int Team=1;Team<=2;++Team){FVoxelSquad S;S.ID=40+Team;S.Team=Team;S.Target=INDEX_NONE;S.Troops=50;S.Destination=FIntPoint(Team==1?-1:1,0);S.HasDestination=true;S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;S.Holding=true;S.RetargetIn=100;B->Squads.Add(S);}
    B->Gold[1]=55;B->Meteor(Target);for(int I=0;I<29;++I)B->AdvanceBattle(.05f);
    TestEqual(TEXT("No blast before the 1.5-second warning ends"),B->MeteorBursts.Num(),0);TestEqual(TEXT("Flight deals no early friendly damage"),B->Squads[0].Troops,50.f);TestTrue(TEXT("Bridge survives until impact"),B->HasCrossing());
    B->AdvanceBattle(.05f);TestEqual(TEXT("Impact creates exactly one visual burst"),B->MeteorBursts.Num(),1);TestTrue(TEXT("Gameplay effect is consumed at impact"),B->Effects.IsEmpty());TestFalse(TEXT("Impact still destroys the bridge"),B->HasCrossing());
    TestEqual(TEXT("Friendly damage remains exactly 38"),B->Squads[0].Troops,12.f);TestEqual(TEXT("Enemy damage remains exactly 38"),B->Squads[1].Troops,12.f);
    for(int I=0;I<10;++I)B->AdvanceBattle(.05f);TestEqual(TEXT("Lingering fire and debris cannot apply damage a second time"),B->Squads[0].Troops,12.f);
    if(B->MeteorBursts.IsEmpty()){AddError(TEXT("Explosion vanished before its lifetime"));World->DestroyWorld(false);return false;}
    B->SelectSquad(41);float Age=B->MeteorBursts[0].Age;for(int I=0;I<20;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Explosion animation shares tactical slow time"),FMath::IsNearlyEqual(B->MeteorBursts[0].Age-Age,.2f,.001f));
    B->MenuPaused=true;Age=B->MeteorBursts[0].Age;B->AdvanceBattle(.25f);B->Simulate(1);TestEqual(TEXT("Menu freezes meteor particles"),B->MeteorBursts[0].Age,Age);B->MenuPaused=false;
    B->Paused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Manual pause freezes meteor particles"),B->MeteorBursts[0].Age,Age);B->Paused=false;
    B->CancelCommand();for(int I=0;I<70;++I)B->AdvanceBattle(.05f);TestTrue(TEXT("Explosion visuals are removed after fading"),B->MeteorBursts.IsEmpty());TestEqual(TEXT("Entire explosion never repeats damage"),B->Squads[0].Troops,12.f);
    FVoxelMeteorBurst Burst;B->MeteorBursts.Add(Burst);B->ResetBattle(true);TestTrue(TEXT("Restart clears lingering meteor visuals"),B->MeteorBursts.IsEmpty());
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAIReachableTest,"VoxelDominion.AI.ReachableExpansion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelAIReachableTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();
    FVoxelTower T;T.Team=2;T.Module=EVoxelModule::Command;T.Cell=FIntPoint(2,0);T.Troops=100;B->Towers.Add(T);
    // Unreachable villages outrank the reachable tower. They must not crowd it out.
    for(int Y=-3;Y<=3;++Y){T.Team=0;T.Module=EVoxelModule::Village;T.Cell=FIntPoint(-2,Y);T.Troops=0;B->Towers.Add(T);}
    T.Module=EVoxelModule::Cannon;T.Cell=FIntPoint(8,0);B->Towers.Add(T);int32 Target=B->Towers.Num()-1;
    B->Time=20;B->RunAI();TestEqual(TEXT("AI still expands on its bank when top-ranked villages are across unbridged water"),B->Squads.Num(),1);
    if(!B->Squads.IsEmpty())TestEqual(TEXT("AI chooses the reachable tower"),B->Squads[0].Target,Target);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAIFortificationTest,"VoxelDominion.AI.FortificationUpgrades",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelAIFortificationTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    for(EVoxelModule Module:{EVoxelModule::Arrow,EVoxelModule::Cannon})
    {
        auto* B=World->SpawnActor<AVoxelBattle>();
        FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Command;T.Cell=FIntPoint(-4,0);T.Troops=250;B->Towers.Add(T);
        T.Team=2;T.Module=Module;T.Cell=FIntPoint(4,0);T.Troops=0;B->Towers.Add(T);
        T.Module=EVoxelModule::Village;T.Cell=FIntPoint(2,0);B->Towers.Add(T);
        FVoxelBridge Bridge;Bridge.Remaining=60;B->Bridges.Add(Bridge);
        B->Time=60;B->Gold[1]=77;B->Gold[2]=140;B->SelectTower(0);
        float Damage=B->TowerDamage(1),Interval=B->TowerInterval(1);B->RunAI();
        TestEqual(TEXT("AI upgrades a defensive tower during bridge construction"),B->Towers[1].Level,2);
        TestEqual(TEXT("Defensive tower takes priority over a closer village"),B->Towers[2].Level,1);
        TestEqual(TEXT("AI pays eighty gold and retains its reserve"),B->Gold[2],60.f);
        TestEqual(TEXT("AI upgrades never spend player gold"),B->Gold[1],77.f);
        TestEqual(TEXT("AI military upgrade grants fifty troops"),B->Towers[1].Troops,50.f);
        TestTrue(TEXT("AI gains the same damage and firing-rate improvements"),B->TowerDamage(1)>Damage&&B->TowerInterval(1)<Interval);
        TestTrue(TEXT("Upgrade uses this action without also dispatching troops"),B->Squads.IsEmpty());
        TestEqual(TEXT("AI upgrade preserves player thinking time"),B->GetBattleSpeed(),.2f);
        B->Time=64.1f;B->Gold[2]=200;B->RunAI();TestEqual(TEXT("A four-second action cannot bypass the thirty-second upgrade interval"),B->Towers[1].Level,2);
        B->Time=91;B->Gold[2]=180;B->RunAI();TestEqual(TEXT("AI can later buy another level at its increased price"),B->Towers[1].Level,3);TestEqual(TEXT("Second upgrade costs one hundred twenty gold"),B->Gold[2],60.f);
        B->Time=130;B->Gold[2]=999;B->Paused=true;B->RunAI();TestEqual(TEXT("Pause prevents AI upgrades"),B->Towers[1].Level,3);B->Paused=false;
        B->MenuPaused=true;B->RunAI();TestEqual(TEXT("Menu prevents AI upgrades"),B->Towers[1].Level,3);B->MenuPaused=false;
        B->Towers[1].Level=5;B->RunAI();TestEqual(TEXT("AI cannot upgrade military towers past five"),B->Towers[1].Level,5);
    }
    auto* BridgeBudget=World->SpawnActor<AVoxelBattle>();BridgeBudget->InitializeLayout();BridgeBudget->Time=60;BridgeBudget->Gold[2]=155;BridgeBudget->RunAI();
    TestEqual(TEXT("An absent crossing is funded before upgrades"),BridgeBudget->Bridges.Num(),1);TestEqual(TEXT("Construction pays its normal price"),BridgeBudget->Gold[2],80.f);
    for(const auto& T:BridgeBudget->Towers)TestEqual(TEXT("Building and upgrading cannot share one AI action"),T.Level,1);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAIPreviewModeTest,"VoxelDominion.AI.PreviewExitRestoresBattleMode",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelAIPreviewModeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();auto* P=World->SpawnActor<AVoxelCommander>();
    P->SetupMeteorPreview(.65f);TestFalse(TEXT("The effect demonstration itself remains static"),B->AIEnabled);
    TestEqual(TEXT("Preview does not leave practice selected for the next battle"),P->SelectedMode,0);
    P->ShowMenu();P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    TestTrue(TEXT("Starting a battle from the preview menu enables the opponent and deadline"),B->AIEnabled&&B->TimeLimitEnabled);
    for(int I=0;I<60;++I)B->AdvanceBattle(.25f);TestTrue(TEXT("The restored standard opponent actually dispatches troops"),!B->Squads.IsEmpty()||B->HoldingCount(2)>3);
    P->ShowMenu();P->MenuAction(TEXT("setup"));P->MenuAction(TEXT("practice"));P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    for(int I=0;I<60;++I)B->AdvanceBattle(.25f);TestFalse(TEXT("Intentionally selected practice still disables AI"),B->AIEnabled);TestTrue(TEXT("Practice does not secretly dispatch enemies"),B->Squads.IsEmpty());
    P->SetupPoisonPreview(1.4f);TestTrue(TEXT("Poison preview uses an actual arrow effect"),!B->Effects.IsEmpty()&&P->PoisonPreviewActive);
    TestEqual(TEXT("Poison preview does not leave practice selected"),P->SelectedMode,0);
    P->ShowMenu();P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    TestTrue(TEXT("Leaving poison preview restores a normal opponent and clears the preview label"),B->AIEnabled&&B->TimeLimitEnabled&&!P->PoisonPreviewActive&&B->Effects.IsEmpty());
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPoisonClockTest,"VoxelDominion.Combat.PoisonLifetimeAndClock",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPoisonClockTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);
    FVector Center=B->CellPosition(FIntPoint(-4,0));
    for(int I=0;I<3;++I)
    {
        FVoxelSquad S;S.ID=101+I;S.Team=I==0?1:2;S.Troops=50;S.Target=INDEX_NONE;
        S.Position=Center+FVector(I==0?-90:I==1?90:0,I==2?400:0,27);S.Destination=B->ToCell(S.Position);
        S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;S.RetargetIn=100;B->Squads.Add(S);
    }
    FVoxelEffect E;E.Position=Center;E.Team=1;E.Radius=190;E.Remaining=4;B->Effects.Add(E);
    for(int I=0;I<4;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Poison retains its existing damage rate"),FMath::IsNearlyEqual(B->Squads[1].Troops,47.8f,.002f));
    TestEqual(TEXT("Allies inside the visible poison remain unharmed"),B->Squads[0].Troops,50.f);
    TestEqual(TEXT("Enemies outside the poison remain unharmed"),B->Squads[2].Troops,50.f);
    B->SelectTower(0);float Before=B->Effects[0].Remaining;
    for(int I=0;I<4;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Poison animation lifetime follows 0.2 tactical time"),FMath::IsNearlyEqual(Before-B->Effects[0].Remaining,.2f,.001f));
    Before=B->Effects[0].Remaining;B->Paused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Pause freezes poison lifetime"),B->Effects[0].Remaining,Before);
    B->Paused=false;B->MenuPaused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Menus freeze poison lifetime"),B->Effects[0].Remaining,Before);
    B->MenuPaused=false;B->CancelCommand();for(int I=0;I<56;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Poison expires after four battle seconds"),B->Effects.IsEmpty());
    TestTrue(TEXT("Fading poison does not change total damage"),FMath::IsNearlyEqual(B->Squads[1].Troops,41.2f,.003f));
    float Troops=B->Squads[1].Troops;B->AdvanceBattle(.25f);TestEqual(TEXT("No lingering damage after the cloud disappears"),B->Squads[1].Troops,Troops);
    B->Effects.Add(E);B->ResetBattle(true);TestTrue(TEXT("Restart clears active poison"),B->Effects.IsEmpty());
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAIProgressTest,"VoxelDominion.AI.StandardBattleProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelAIProgressTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle();
    bool Built=false,Opened=false,Upgraded=false,Dispatched=false;int32 PeakHoldings=3;
    for(int I=0;I<960&&!B->Winner;++I)
    {
        B->AdvanceBattle(.25f);Built|=!B->Bridges.IsEmpty();Opened|=B->HasCrossing();PeakHoldings=FMath::Max(PeakHoldings,B->HoldingCount(2));
        for(const auto& S:B->Squads)Dispatched|=S.Team==2;
        for(const auto& T:B->Towers)Upgraded|=T.Team==2&&(T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon)&&T.Level>1;
    }
    TestTrue(TEXT("Standard AI sends real armies without player input"),Dispatched);TestTrue(TEXT("AI expands beyond its initial three holdings"),PeakHoldings>=8);
    TestTrue(TEXT("AI funds and completes a crossing"),Built&&Opened);TestTrue(TEXT("AI upgrades its own defensive towers in normal play"),Upgraded);
    AddInfo(FString::Printf(TEXT("Four-minute standard battle: peak enemy holdings %d, bridge %d, military upgrade %d, bridges %d, villages %d, enemy gold %.1f"),PeakHoldings,Opened,Upgraded,B->Bridges.Num(),B->VillageCount(2),B->Gold[2]));
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelIrregularMapTest,"VoxelDominion.Map.IrregularBoundaryAndNavigation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelIrregularMapTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();
    int Land=0;for(int Y=-B->MapHalfHeight();Y<=B->MapHalfHeight();++Y)for(int X=-B->MapHalfWidth();X<=B->MapHalfWidth();++X)
    {
        FIntPoint C(X,Y);if(!B->ContainsCell(C)){TestFalse(TEXT("Clipped coastline cannot be walked on"),B->Walkable(C));continue;}
        TestTrue(TEXT("Irregular footprint treats both factions symmetrically"),B->ContainsCell(FIntPoint(-X,-Y)));
        Land+=!B->IsRiver(C);
    }
    TestTrue(TEXT("Rivers retain a substantially larger playable battlefield"),Land>1680*1.6f&&Land<1680*2.3f);
    for(int X:{-B->MapHalfWidth(),B->MapHalfWidth()})for(int Y:{-B->MapHalfHeight(),B->MapHalfHeight()})TestFalse(TEXT("Bounding-box corners are outside the actual battlefield"),B->ContainsCell(FIntPoint(X,Y)));
    float Minimum=FLT_MAX;
    for(int I=0;I<B->Towers.Num();++I)
    {
        for(int J=I+1;J<B->Towers.Num();++J)Minimum=FMath::Min(Minimum,FVector::Dist2D(B->CellPosition(B->Towers[I].Cell),B->CellPosition(B->Towers[J].Cell)));
        FVector Position=B->CellPosition(B->Towers[I].Cell),Focus=B->FocusFromMapUV(B->MapUV(Position));
        TestTrue(TEXT("Minimap and world agree at every stronghold"),FVector::Dist2D(Position,Focus)<.01f);
        if(B->Towers[I].Team==0)TestEqual(TEXT("Larger map keeps neutral buildings unoccupied"),B->Towers[I].Troops,0.f);
    }
    TestTrue(TEXT("Resource guards retain at least nine metres of spacing"),Minimum>=900);
    const FIntPoint Dirs[]={FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)};
    int Connected=0;
    for(int Source:{0,3})
    {
        TArray<FIntPoint> Queue;TSet<FIntPoint> Visited;Queue.Add(B->Towers[Source].Cell);Visited.Add(Queue[0]);
        for(int Head=0;Head<Queue.Num();++Head)for(FIntPoint D:Dirs)
        {FIntPoint C=Queue[Head]+D;if(B->Walkable(C)&&!Visited.Contains(C)){Visited.Add(C);Queue.Add(C);}}
        Connected+=Visited.Num();
    }
    TestTrue(TEXT("Tributaries separate land beyond the two headquarters' starting regions"),Connected>0&&Connected<Land);
    for(FVector2D UV:{FVector2D(0,0),FVector2D(1,0),FVector2D(0,1),FVector2D(1,1)})TestTrue(TEXT("Clicking minimap sea clamps the camera to the real coast"),B->ContainsCell(B->ToCell(B->FocusFromMapUV(UV))));
    B->Gold[1]=150;TestFalse(TEXT("Cannot build bridges beyond the river mouth"),B->BuildBridge(B->MapHalfHeight()));TestEqual(TEXT("Invalid coastal construction spends no money"),B->Gold[1],150.f);
    TestTrue(TEXT("Central river crossing remains buildable"),B->BuildBridge(0));B->Bridges[0].Remaining=0;
    ConnectRiverRegions(B);
    auto Route=B->FindRoute(B->Towers[0].Cell,B->Towers[3].Cell,1,false);TestFalse(TEXT("Opposite headquarters remain reachable after bridging connected regions"),Route.IsEmpty());
    for(const auto& P:Route)TestTrue(TEXT("Long routes remain inside the irregular battlefield"),B->ContainsCell(B->ToCell(P)));
    float Troops=B->Towers[0].Troops;TestFalse(TEXT("Field deployment rejects sea beyond the coast"),B->DispatchTo(0,FVector(B->MapHalfWidth()*B->Tile,B->MapHalfHeight()*B->Tile,0),.5f));TestEqual(TEXT("Rejected coastal deployment keeps its garrison"),B->Towers[0].Troops,Troops);
    AddInfo(FString::Printf(TEXT("Irregular map: %d usable land tiles (%.2fx), closest sites %.2f m"),Land,Land/1680.f,Minimum/100));
    B->ResetBattle(false,true);TestTrue(TEXT("Tutorial retains its compact rectangular training boundary"),B->ContainsCell(FIntPoint(10,7)));TestFalse(TEXT("Training routes cannot escape into the standard map"),B->ContainsCell(FIntPoint(11,0)));
    B->ResetBattle();TestFalse(TEXT("Returning from training restores the irregular border"),B->ContainsCell(FIntPoint(B->MapHalfWidth(),B->MapHalfHeight())));
    World->DestroyWorld(false);return true;
}
#endif
