#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPlaybackSpeedTest,"VoxelDominion.Simulation.PlaybackSpeed",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPlaybackSpeedTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto Setup=[&]()
    {
        auto* B=World->SpawnActor<AVoxelBattle>();
        for(int Team=1;Team<=2;++Team)
        {
            FVoxelTower T;T.Team=Team;T.Troops=10;T.Module=EVoxelModule::Command;T.Cell=FIntPoint(Team==1?-26:26,0);B->Towers.Add(T);
            T.Module=EVoxelModule::Village;T.Cell.Y=3;B->Towers.Add(T);
        }
        FVoxelSquad S;S.Team=1;S.Target=INDEX_NONE;S.Troops=30;S.Position=B->CellPosition(FIntPoint(-18,0));S.Route.Add(B->CellPosition(FIntPoint(-10,0)));B->Squads.Add(S);
        FVoxelBridge Bridge;Bridge.Remaining=60;B->Bridges.Add(Bridge);
        FVoxelEffect Effect;Effect.Position=FVector(100000,0,0);Effect.Remaining=10;B->Effects.Add(Effect);
        FVoxelWeaponShot Shot;Shot.FlightTime=10;B->WeaponShots.Add(Shot);
        FVoxelMeteorBurst Burst;B->MeteorBursts.Add(Burst);
        B->MeteorReady=180;return B;
    };
    auto* Normal=Setup();auto* Fast=Setup();const FVector Start=Fast->Squads[0].Position;
    TestEqual(TEXT("New battle defaults to 1x"),Fast->GetPlaybackSpeed(),1);
    Fast->SetPlaybackSpeed(2);TestEqual(TEXT("2x is the active battle rate"),Fast->GetBattleSpeed(),2.f);
    for(int I=0;I<20;++I){Normal->AdvanceBattle(.05f);Fast->AdvanceBattle(.05f);}
    TestTrue(TEXT("1x advances one battle second per real second"),FMath::IsNearlyEqual(Normal->Time,1.f,.001f));
    TestTrue(TEXT("2x advances two battle seconds per real second"),FMath::IsNearlyEqual(Fast->Time,2.f,.001f));
    TestTrue(TEXT("2x marching covers twice the distance"),FMath::IsNearlyEqual(FVector::Dist2D(Fast->Squads[0].Position,Start),440.f,.01f));
    TestTrue(TEXT("1x keeps ordinary marching speed"),FMath::IsNearlyEqual(FVector::Dist2D(Normal->Squads[0].Position,Start),220.f,.01f));
    for(int Team=1;Team<=2;++Team)TestTrue(TEXT("Both teams receive twice the income"),FMath::IsNearlyEqual(Fast->Gold[Team],2*Normal->Gold[Team],.001f));
    for(int I=0;I<Fast->Towers.Num();++I)TestTrue(TEXT("HQ recovery and village production share the accelerated clock"),FMath::IsNearlyEqual(Fast->Towers[I].Troops-10,2*(Normal->Towers[I].Troops-10),.001f));
    TestTrue(TEXT("Construction uses accelerated time"),FMath::IsNearlyEqual(Fast->Bridges[0].Remaining,58.f,.001f));
    TestTrue(TEXT("Meteor cooldown uses accelerated time"),FMath::IsNearlyEqual(Fast->MeteorReady,178.f,.001f));
    TestTrue(TEXT("Poison lifetime uses accelerated time"),FMath::IsNearlyEqual(Fast->Effects[0].Remaining,8.f,.001f));
    TestTrue(TEXT("Weapon flights use accelerated time"),FMath::IsNearlyEqual(Fast->WeaponShots[0].Age,2.f,.001f));
    TestTrue(TEXT("Meteor bursts use accelerated time"),FMath::IsNearlyEqual(Fast->MeteorBursts[0].Age,2.f,.001f));
    TestTrue(TEXT("AI action cadence accelerates equally"),FMath::IsNearlyEqual(Fast->AISince,2.f,.001f));

    // Compare actual combat at equal battle time, including attacks and cooling.
    auto Combat=[&]()
    {
        auto* B=World->SpawnActor<AVoxelBattle>();B->AIEnabled=false;B->SoldierSpeed=0;
        FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Cannon;T.Cell=FIntPoint(-8,0);B->Towers.Add(T);
        FVoxelSquad S;S.Team=2;S.Target=INDEX_NONE;S.Troops=200;S.Holding=true;S.Position=B->CellPosition(T.Cell)+FVector(280,0,27);B->Squads.Add(S);return B;
    };
    auto* FastCombat=Combat();auto* ReferenceCombat=Combat();FastCombat->SetPlaybackSpeed(2);
    for(int I=0;I<20;++I)FastCombat->AdvanceBattle(.05f);
    for(int I=0;I<40;++I)ReferenceCombat->AdvanceBattle(.05f);
    TestTrue(TEXT("Accelerated cannon actually attacks"),FastCombat->Squads[0].Troops<200);
    TestEqual(TEXT("2x damage matches equal 1x battle time"),FastCombat->Squads[0].Troops,ReferenceCombat->Squads[0].Troops);
    TestEqual(TEXT("2x cannon heat matches equal 1x battle time"),FastCombat->Towers[0].Heat,ReferenceCombat->Towers[0].Heat);
    FastCombat->Towers[0].Heat=ReferenceCombat->Towers[0].Heat=100;FastCombat->Towers[0].Overheated=ReferenceCombat->Towers[0].Overheated=true;
    FastCombat->AdvanceBattle(.1f);ReferenceCombat->AdvanceBattle(.2f);
    TestEqual(TEXT("Overheat cooling accelerates with combat"),FastCombat->Towers[0].Heat,ReferenceCombat->Towers[0].Heat);

    Fast->SelectTower(0);TestEqual(TEXT("Thinking remains absolute 0.2x while 2x is selected"),Fast->GetBattleSpeed(),.2f);
    float Before=Fast->Time;for(int I=0;I<20;++I)Fast->AdvanceBattle(.05f);
    TestTrue(TEXT("Thinking advances only 0.2 seconds per real second"),FMath::IsNearlyEqual(Fast->Time-Before,.2f,.001f));
    Fast->SetPlaybackSpeed(1);TestEqual(TEXT("Changing base speed does not end thinking"),Fast->GetBattleSpeed(),.2f);
    Fast->SetPlaybackSpeed(2);TestTrue(TEXT("Dispatch succeeds during thinking"),Fast->DispatchTo(0,Fast->CellPosition(FIntPoint(-23,0)),.5f));
    TestEqual(TEXT("Successful command restores selected 2x"),Fast->GetBattleSpeed(),2.f);
    Fast->SelectSquad(Fast->Squads[0].ID);TestEqual(TEXT("Selecting an army also slows 2x"),Fast->GetBattleSpeed(),.2f);
    Fast->CancelCommand();TestEqual(TEXT("Cancelling selection restores 2x"),Fast->GetBattleSpeed(),2.f);
    Fast->BridgeMode=true;TestEqual(TEXT("Bridge placement keeps 0.2x"),Fast->GetBattleSpeed(),.2f);Fast->CancelCommand();
    Fast->Paused=true;Before=Fast->Time;float Funds=Fast->Gold[1],Build=Fast->Bridges[0].Remaining,Cooldown=Fast->MeteorReady;
    Fast->SetPlaybackSpeed(1);Fast->SetPlaybackSpeed(2);Fast->AdvanceBattle(.25f);
    TestEqual(TEXT("Speed buttons never unpause"),Fast->GetBattleSpeed(),0.f);TestEqual(TEXT("Pause freezes time at 2x"),Fast->Time,Before);
    TestEqual(TEXT("Pause freezes income"),Fast->Gold[1],Funds);TestEqual(TEXT("Pause freezes construction"),Fast->Bridges[0].Remaining,Build);TestEqual(TEXT("Pause freezes cooldown"),Fast->MeteorReady,Cooldown);
    Fast->Paused=false;TestEqual(TEXT("Resume restores 2x"),Fast->GetBattleSpeed(),2.f);
    Fast->MenuPaused=true;Fast->AdvanceBattle(.25f);Fast->SetPlaybackSpeed(1);
    TestEqual(TEXT("Menu freezes the 2x clock"),Fast->Time,Before);TestEqual(TEXT("Menu preserves selected speed"),Fast->GetPlaybackSpeed(),2);
    Fast->MenuPaused=false;TestEqual(TEXT("Leaving menu resumes 2x"),Fast->GetBattleSpeed(),2.f);
    Fast->SetPlaybackSpeed(0);Fast->SetPlaybackSpeed(3);TestEqual(TEXT("Only 1x and 2x are accepted"),Fast->GetPlaybackSpeed(),2);
    Fast->SetPlaybackSpeed(1);Before=Fast->Time;Fast->AdvanceBattle(.25f);TestTrue(TEXT("1x restores ordinary clock progression"),FMath::IsNearlyEqual(Fast->Time-Before,.25f,.001f));

    auto* Lesson=Setup();Lesson->AIEnabled=false;Lesson->Tutorial=true;Lesson->TutorialStep=7;Lesson->TutorialBridgeRow=0;
    Lesson->SetPlaybackSpeed(2);Lesson->ToggleTutorialFastForward();TestEqual(TEXT("Bridge lesson retains 5x fast forward"),Lesson->GetBattleSpeed(),5.f);
    Lesson->SelectTower(0);TestEqual(TEXT("Thinking overrides teaching fast forward"),Lesson->GetBattleSpeed(),.2f);Lesson->CancelCommand();
    Lesson->SetPlaybackSpeed(2);TestFalse(TEXT("Selecting a normal rate exits teaching fast forward"),Lesson->TutorialFastForward);TestEqual(TEXT("Lesson returns explicitly to 2x"),Lesson->GetBattleSpeed(),2.f);
    Lesson->TutorialObjectiveComplete=true;Before=Lesson->Time;Lesson->AdvanceBattle(.25f);Lesson->SetPlaybackSpeed(1);
    TestEqual(TEXT("Completed lesson stays frozen"),Lesson->Time,Before);TestEqual(TEXT("Speed button cannot bypass lesson completion"),Lesson->GetBattleSpeed(),0.f);
    auto* Ending=Setup();Ending->SetPlaybackSpeed(2);Ending->Time=Ending->BattleTimeLimit-.25f;Ending->AdvanceBattle(.25f);
    TestTrue(TEXT("2x reaches the existing 20-minute settlement"),Ending->Winner!=0);TestEqual(TEXT("Clock stops exactly at the battle limit"),Ending->Time,1200.f);
    Before=Ending->Time;Ending->SetPlaybackSpeed(1);Ending->AdvanceBattle(.25f);TestEqual(TEXT("Speed cannot restart a finished match"),Ending->Time,Before);
    Fast->SetPlaybackSpeed(2);Fast->ResetBattle(true);TestEqual(TEXT("New match resets to 1x"),Fast->GetPlaybackSpeed(),1);TestEqual(TEXT("New match clears time"),Fast->Time,0.f);
    World->DestroyWorld(false);return true;
}
#endif
