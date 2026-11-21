#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRiverNetworkTest,"VoxelDominion.Map.BraidedRiversAndCrossings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelRiverNetworkTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeLayout();B->AIEnabled=false;
    TSet<FIntPoint> Visited;int Regions=0,Water=0,OffAxis=0;TArray<FVoxelBridge> Sites;
    const FIntPoint Dirs[]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int Y=-B->MapHalfHeight();Y<=B->MapHalfHeight();++Y)for(int X=-B->MapHalfWidth();X<=B->MapHalfWidth();++X)
    {
        FIntPoint C(X,Y);if(!B->ContainsCell(C))continue;
        TestEqual(TEXT("The complete water network is rotationally balanced"),B->IsRiver(C),B->IsRiver(FIntPoint(-C.X,-C.Y)));
        if(B->IsRiver(C))
        {
            ++Water;OffAxis+=X!=0;TestFalse(TEXT("No open water can be walked on"),B->Walkable(C));
            FVoxelBridge Site;if(B->BridgeSiteAt(C,Site)&&C==FIntPoint(Site.Column,Site.Row))Sites.Add(Site);
        }
        else if(!Visited.Contains(C))
        {
            ++Regions;TArray<FIntPoint> Queue;Queue.Add(C);Visited.Add(C);
            for(int Head=0;Head<Queue.Num();++Head)for(FIntPoint D:Dirs)
            {FIntPoint N=Queue[Head]+D;if(B->Walkable(N)&&!Visited.Contains(N)){Visited.Add(N);Queue.Add(N);}}
        }
    }
    TestEqual(TEXT("Main river and tributaries create six land regions"),Regions,6);
    TestTrue(TEXT("Rivers meander beyond the old central column"),OffAxis>200&&Water>250);
    for(const auto& T:B->Towers)
    {
        TestTrue(TEXT("Every stronghold is safely on land"),B->Walkable(T.Cell));
        for(int Y=-1;Y<=1;++Y)for(int X=-1;X<=1;++X)TestFalse(TEXT("Building footprints do not straddle a river"),B->IsRiver(T.Cell+FIntPoint(X,Y)));
    }
    TestTrue(TEXT("Blue opening towers share their headquarters region"),B->CanReach(B->Towers[0].Cell,B->Towers[1].Cell)&&B->CanReach(B->Towers[0].Cell,B->Towers[2].Cell));
    TestTrue(TEXT("Red opening towers share their headquarters region"),B->CanReach(B->Towers[3].Cell,B->Towers[4].Cell)&&B->CanReach(B->Towers[3].Cell,B->Towers[5].Cell));
    TestFalse(TEXT("No initial bridge connects the headquarters"),B->CanReach(B->Towers[0].Cell,B->Towers[3].Cell));
    FVoxelBridge Chosen;bool Found=false,Wide=false;
    for(const auto& Site:Sites)
    {
        Wide|=Site.Length>=3;
        if(Site.Vertical&&Site.Column!=0&&!B->CanReach(B->BridgeBank(Site,false),B->BridgeBank(Site,true))){Chosen=Site;Found=true;}
    }
    TestTrue(TEXT("Wide meanders require multi-tile bridge decks"),Wide);
    TestTrue(TEXT("Tributaries support north-south crossings away from the old river"),Found);
    if(!Found){World->DestroyWorld(false);return false;}
    FIntPoint Cell(Chosen.Column,Chosen.Row),A=B->BridgeBank(Chosen,false),C=B->BridgeBank(Chosen,true);
    TestTrue(TEXT("A tributary separates banks with the same X-side"),A.X*C.X>0);
    B->Gold[1]=75;TestTrue(TEXT("A tributary bridge can be freely placed"),B->BuildBridgeAt(Cell));TestEqual(TEXT("Bridge costs seventy-five gold"),B->Gold[1],0.f);
    TestEqual(TEXT("Bridge still takes sixty battle seconds"),B->Bridges[0].Remaining,60.f);TestFalse(TEXT("Construction cannot be used for crossing"),B->CanReach(A,C));
    B->Gold[1]=75;TestFalse(TEXT("Overlapping sites are rejected"),B->BuildBridgeAt(Cell));TestEqual(TEXT("Overlap does not charge again"),B->Gold[1],75.f);
    B->Bridges[0].Remaining=0;TestTrue(TEXT("Completing a bridge refreshes actual connectivity"),B->CanReach(A,C));
    TestFalse(TEXT("Both factions can path over a vertical bridge"),B->FindRoute(A,C,1,false).IsEmpty()||B->FindRoute(C,A,2,false).IsEmpty());
    FVoxelSquad S;S.ID=901;S.Team=1;S.Troops=100;S.Position=B->CellPosition(Cell)+FVector(0,0,27);S.LastLand=A;S.Destination=C;S.HasDestination=true;S.Holding=true;B->Squads.Add(S);
    FIntPoint Last=Cell+(Chosen.Vertical?FIntPoint(0,1):FIntPoint(1,0))*(Chosen.Length-1);
    B->DestroyBridges(B->CellPosition(Last),25);
    TestTrue(TEXT("A hit at either end destroys the complete deck"),B->Bridges.IsEmpty());TestFalse(TEXT("Destroying a tributary bridge disconnects the banks again"),B->CanReach(A,C));
    TestTrue(TEXT("Survivors retreat toward their actual previous bank"),B->Squads[0].Evacuating&&B->Squads[0].Route.Last().Equals(B->CellPosition(A)+FVector(0,0,27),.01f));
    for(int I=0;I<200&&B->Squads[0].Evacuating;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Retreat reaches dry land without teleporting across the channel"),!B->Squads[0].Evacuating&&B->ToCell(B->Squads[0].Position)==A);
    B->Squads.Empty();
    for(const auto& Site:Sites)
    {
        if(B->CanReach(B->BridgeBank(Site,false),B->BridgeBank(Site,true)))continue;
        B->Gold[1]=999;if(B->BuildBridgeAt(FIntPoint(Site.Column,Site.Row)))B->Bridges.Last().Remaining=0;
    }
    for(FIntPoint Land:Visited)TestTrue(TEXT("Every land tile can join the bridge network"),B->CanReach(B->Towers[0].Cell,Land));
    for(int I=0;I<B->Towers.Num();++I)
    {
        const auto& T=B->Towers[I];if(T.Module!=EVoxelModule::Arrow&&T.Module!=EVoxelModule::Cannon)continue;
        float Distance=FLT_MAX;FVector P=B->CellPosition(T.Cell);
        for(const auto& Site:Sites)Distance=FMath::Min(Distance,float(FVector::Dist2D(P,B->BridgeCenter(Site))));
        for(const auto& Village:B->Towers)if(Village.Module==EVoxelModule::Village)Distance=FMath::Min(Distance,float(FVector::Dist2D(P,B->CellPosition(Village.Cell))));
        TestTrue(*FString::Printf(TEXT("%s covers a crossing or nearby resource village"),*T.Name.ToString()),Distance<=B->TowerBaseRange(I));
    }
    AddInfo(FString::Printf(TEXT("River network: %d regions, %d water tiles, %d dry tiles, %d candidate crossings"),Regions,Water,Visited.Num(),Sites.Num()));
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRangeTest,"VoxelDominion.Combat.DefensiveRangeProgression",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelRangeTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
    for(int Team:{1,2})for(EVoxelModule Module:{EVoxelModule::Arrow,EVoxelModule::Cannon})for(bool Tall:{false,true})
    {
        auto* B=World->SpawnActor<AVoxelBattle>();FVoxelTower T;T.Team=Team;T.Cell=FIntPoint(Team==1?-30:30,0);T.Tall=Tall;T.Module=Module;T.Arrow=EVoxelArrow::Poison;T.Troops=30;B->Towers.Add(T);
        float Base=B->TowerBaseRange(0);TestTrue(TEXT("Base range gives defensive towers meaningful battlefield coverage"),Base>=1000);
        FVoxelSquad S;S.Team=Team==1?2:1;S.Troops=100;S.Position=B->CellPosition(T.Cell)+FVector((Team==1?1:-1)*(Base+40),0,0);B->Squads.Add(S);
        B->FireTowers(.05f);TestEqual(TEXT("Level one cannot hit beyond its actual range"),B->Squads[0].Troops,100.f);
        B->Gold[Team]=80;TestTrue(TEXT("Either faction can buy the range upgrade normally"),B->UpgradeTower(0,Team));
        TestTrue(TEXT("A level adds ten percent of original base range"),FMath::IsNearlyEqual(B->TowerBaseRange(0),Base*1.1f,.01f));
        B->Towers[0].FireTimer=0;B->FireTowers(.05f);TestTrue(TEXT("An upgraded tower really fires at newly covered enemies"),B->Squads[0].Troops<100);
        B->Towers[0].Level=5;TestTrue(TEXT("Five levels yield forty percent total extra base range"),FMath::IsNearlyEqual(B->TowerBaseRange(0),Base*1.4f,.01f));
        FVector Target=B->CellPosition(T.Cell)+FVector(0,0,-120);
        TestTrue(TEXT("Elevation still extends the upgraded range"),B->TowerRange(0,Target)>B->TowerBaseRange(0));
    }
    World->DestroyWorld(false);return true;
}
#endif
