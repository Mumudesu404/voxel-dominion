#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelAnyAngleRouteTest,"VoxelDominion.Navigation.ShortestGroundRoutes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelAnyAngleRouteTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    auto* B=World->SpawnActor<AVoxelBattle>();B->Tutorial=true;B->AIEnabled=false;B->TimeLimitEnabled=false;
    auto Length=[](FVector Start,const TArray<FVector>& Route)
    {float Total=0;for(FVector End:Route){Total+=FVector::Dist2D(Start,End);Start=End;}return Total;};
    // Independently sample the whole corridor, not just the returned waypoints.
    auto DryCorridor=[&](AVoxelBattle* Map,FVector Start,const TArray<FVector>& Route)
    {
        if(Route.IsEmpty())return false;
        for(FVector End:Route)
        {
            int Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/5));
            for(int I=0;I<=Steps;++I)for(FVector Offset:{FVector::ZeroVector,FVector(-20,-20,0),FVector(-20,20,0),FVector(20,-20,0),FVector(20,20,0)})
            {FVector P=FMath::Lerp(Start,End,float(I)/Steps)+Offset;if(!Map->Walkable(Map->ToCell(P)))return false;}
            Start=End;
        }
        return true;
    };
    FVector A=B->CellPosition(FIntPoint(-8,-4))+FVector(23,31,27),Goal=B->CellPosition(FIntPoint(-2,3))+FVector(-41,17,27);
    auto Open=B->FindGroundRoute(A,Goal);
    TestEqual(TEXT("Open ground is crossed with one straight segment"),Open.Num(),1);
    TestTrue(TEXT("Open-ground travel equals the straight-line distance"),FMath::IsNearlyEqual(Length(A,Open),float(FVector::Dist2D(A,Goal)),.01f));
    TestTrue(TEXT("Off-grid destinations retain the actual requested location"),!Open.IsEmpty()&&FVector::Dist2D(Open.Last(),Goal)<.01);
    TestTrue(TEXT("The full diagonal corridor stays dry"),DryCorridor(B,A,Open));
    A=B->CellPosition(FIntPoint(-3,0))+FVector(0,0,27);Goal=B->CellPosition(FIntPoint(3,0))+FVector(0,0,27);
    TestTrue(TEXT("A straight line does not bypass an unbridged river"),B->FindGroundRoute(A,Goal).IsEmpty());
    FVoxelBridge Bridge;Bridge.Column=0;Bridge.Row=3;Bridge.Elevation=82;Bridge.Remaining=60;B->Bridges.Add(Bridge);
    TestTrue(TEXT("An unfinished bridge is still unavailable"),B->FindGroundRoute(A,Goal).IsEmpty());
    B->Bridges[0].Remaining=0;auto Detour=B->FindGroundRoute(A,Goal);
    TestTrue(TEXT("A finished bridge produces a short route around the blocked river"),Detour.Num()>1&&Length(A,Detour)>6*B->Tile&&Length(A,Detour)<12*B->Tile);
    TestTrue(TEXT("Every centimetre-scale sample of the detour remains on land or a finished bridge"),DryCorridor(B,A,Detour));
    Bridge.Row=0;Bridge.Remaining=0;B->Bridges.Add(Bridge);auto Direct=B->FindGroundRoute(A,Goal);
    TestTrue(TEXT("A new direct bridge invalidates cached detours"),Direct.Num()==1&&FMath::IsNearlyEqual(Length(A,Direct),6*B->Tile,.01f));
    float HalfTile=B->Tile*.5f;FVector CornerA(HalfTile+22,HalfTile-25,97),CornerB(HalfTile+25,HalfTile-22,97);
    TestTrue(TEXT("Both ends of a tiny corner cut can be individually safe"),B->WalkableSegment(CornerA,CornerA)&&B->WalkableSegment(CornerB,CornerB));
    TestFalse(TEXT("Even a sub-centimetre clip of the river clearance is rejected"),B->WalkableSegment(CornerA,CornerB));
    B->Bridges.RemoveAt(1);auto Rerouted=B->FindGroundRoute(A,Goal);
    TestTrue(TEXT("Losing the shortcut restores the remaining valid detour"),Rerouted.Num()>1&&DryCorridor(B,A,Rerouted)&&FMath::IsNearlyEqual(Length(A,Rerouted),Length(A,Detour),.01f));
    B->Bridges.Empty();TestTrue(TEXT("Losing every bridge cannot reuse stale straight segments"),B->FindGroundRoute(A,Goal).IsEmpty());

    auto* Field=World->SpawnActor<AVoxelBattle>();Field->ResetBattle(true);Field->Towers.Empty();
    FVoxelTower Tower;Tower.Team=1;Tower.Module=EVoxelModule::Village;Tower.Cell=FIntPoint(-30,0);Tower.Troops=100;Field->Towers.Add(Tower);
    FVector Target=Field->CellPosition(FIntPoint(-28,3))+FVector(47,-36,0);
    TestTrue(TEXT("A player can deploy to an arbitrary point on open ground"),Field->DispatchTo(0,Target,.5f));
    if(Field->Squads.IsEmpty()){World->DestroyWorld(false);return false;}
    int ID=Field->Squads[0].ID;FVector Start=Field->Squads[0].Position;
    TestTrue(TEXT("The real marching order has one direct destination"),Field->Squads[0].Route.Num()==1&&Field->Squads[0].HasGroundDestination);
    Field->AdvanceBattle(.25f);FVector Moved=Field->Squads[0].Position-Start,Direction=(Target-Start).GetSafeNormal2D();
    TestTrue(TEXT("Soldiers advance diagonally instead of following grid axes"),FMath::Abs(FVector::CrossProduct(Moved,Direction).Z)<.02&&FVector::DotProduct(Moved,Direction)>50);
    TestTrue(TEXT("Long straight movement follows the terrain height"),FMath::IsNearlyEqual(float(Field->Squads[0].Position.Z),Field->Height(Field->ToCell(Field->Squads[0].Position))+27,.01f));
    for(int I=0;I<30;++I)Field->AdvanceBattle(.25f);
    TestTrue(TEXT("Soldiers hold at the clicked point rather than the cell centre"),Field->Squads[0].Holding&&FVector::Dist2D(Field->Squads[0].Position,Target)<.02);
    FVector Anchor=Field->Squads[0].GroundDestination;
    FVoxelSquad Enemy;Enemy.ID=1002;Enemy.Team=2;Enemy.Target=INDEX_NONE;Enemy.Troops=10;Enemy.Position=Anchor+FVector(245,25,0);
    Enemy.Destination=Field->ToCell(Enemy.Position);Enemy.Holding=true;Enemy.HasDestination=true;Enemy.RetargetIn=100;Field->Squads.Add(Enemy);
    bool Intercepted=false;
    for(int I=0;I<240;++I)
    {
        Field->AdvanceBattle(.05f);int G=Field->SquadIndex(ID);
        if(G!=INDEX_NONE)Intercepted|=Field->Squads[G].InterceptTarget==1002;
    }
    int Guard=Field->SquadIndex(ID);TestTrue(TEXT("A guard still intercepts enemies using a straight pursuit"),Intercepted);
    TestTrue(TEXT("After combat the guard returns to its exact off-grid anchor"),Guard!=INDEX_NONE&&Field->SquadIndex(1002)==INDEX_NONE&&Field->Squads[Guard].Holding&&FVector::Dist2D(Field->Squads[Guard].Position,Anchor)<.02);
    if(Guard!=INDEX_NONE)
    {
        FVector Next=Field->CellPosition(FIntPoint(-27,2))+FVector(-37,24,0);
        TestTrue(TEXT("A marching army can receive another precise ground order"),Field->OrderSquad(ID,INDEX_NONE,Next));Field->AdvanceBattle(.25f);
        Field->RepathSquad(Field->Squads[Guard]);
        TestTrue(TEXT("Replanning mid-stride does not return to a grid centre"),Field->Squads[Guard].Route.Num()==1&&FVector::Dist2D(Field->Squads[Guard].Route[0],Next)<.02);
    }
    AddInfo(FString::Printf(TEXT("Bridge detour %.1f cm, direct %.1f cm; %d cached shoreline corners"),Length(A,Detour),Length(A,Direct),Field->NavigationCorners.Num()));
    World->DestroyWorld(false);return true;
}
#endif
