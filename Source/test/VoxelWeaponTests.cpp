#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelWeaponVisualTest,"VoxelDominion.Visuals.TowerWeaponsAndHeat",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelWeaponVisualTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);B->Towers.Empty();
    for(int I=0;I<2;++I)
    {
        FVoxelTower T;T.Team=1;T.Module=I==0?EVoxelModule::Arrow:EVoxelModule::Cannon;T.Tall=I==0;T.Cell=FIntPoint(I==0?-30:-24,0);T.Arrow=EVoxelArrow::Pierce;T.Troops=50;B->Towers.Add(T);
        FVoxelSquad S;S.ID=600+I;S.Team=2;S.Troops=250;S.Target=INDEX_NONE;S.Destination=T.Cell+FIntPoint(0,-3);
        S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;S.RetargetIn=100;B->Squads.Add(S);
    }
    B->AdvanceBattle(.05f);TestEqual(TEXT("Each real shot records one bounded visual event"),B->WeaponShots.Num(),2);
    if(B->WeaponShots.Num()!=2){World->DestroyWorld(false);return false;}
    TestTrue(TEXT("Arrow and cannon have distinct flight visuals"),B->WeaponShots[0].Module==EVoxelModule::Arrow&&B->WeaponShots[1].Module==EVoxelModule::Cannon);
    TestTrue(TEXT("Projectiles originate above their stone towers"),B->WeaponShots[0].From.Z>B->CellPosition(B->Towers[0].Cell).Z+200&&B->WeaponShots[1].From.Z>B->CellPosition(B->Towers[1].Cell).Z+200);
    TestTrue(TEXT("Aim tracks the real enemy selected by the weapon"),B->Towers[0].AimSquad==600&&B->Towers[1].AimSquad==601);
    for(auto& T:B->Towers)T.FireTimer=100;
    float ArrowTroops=B->Squads[0].Troops,CannonTroops=B->Squads[1].Troops;
    B->SelectTower(0);for(int I=0;I<4;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Flight and recoil share tactical slow time"),FMath::IsNearlyEqual(B->WeaponShots[0].Age,.2f,.001f)&&FMath::IsNearlyEqual(B->Towers[0].ShotAge,.2f,.001f));
    B->Paused=true;float Age=B->WeaponShots[0].Age,Spin=B->Towers[1].BarrelAngle;B->AdvanceBattle(.25f);
    TestEqual(TEXT("Pause freezes projectile flight"),B->WeaponShots[0].Age,Age);TestEqual(TEXT("Pause freezes barrel rotation"),B->Towers[1].BarrelAngle,Spin);
    B->Paused=false;B->MenuPaused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Menus freeze weapon animation"),B->WeaponShots[0].Age,Age);
    B->MenuPaused=false;B->CancelCommand();for(int I=0;I<5;++I)B->AdvanceBattle(.25f);
    TestTrue(TEXT("Arrows, tracers, casings and impact visuals expire"),B->WeaponShots.IsEmpty());
    TestEqual(TEXT("Arrow visuals cannot apply gameplay damage again"),B->Squads[0].Troops,ArrowTroops);TestEqual(TEXT("Decorative cannon bursts never multiply damage"),B->Squads[1].Troops,CannonTroops);
    B->Towers[1].Heat=100;B->Towers[1].Overheated=true;B->Towers[1].FireTimer=0;B->AdvanceBattle(.25f);
    TestTrue(TEXT("Overheated cannon emits no new shots"),B->WeaponShots.IsEmpty());TestEqual(TEXT("Overheated visuals do not damage enemies"),B->Squads[1].Troops,CannonTroops);
    for(int I=0;I<80&&B->Towers[1].Overheated;++I)B->AdvanceBattle(.05f);
    TestTrue(TEXT("Cooling permits normal firing again"),!B->Towers[1].Overheated&&!B->WeaponShots.IsEmpty()&&B->Squads[1].Troops<CannonTroops);
    B->ResetBattle(true);TestTrue(TEXT("Restart clears all flying ammunition"),B->WeaponShots.IsEmpty());
    auto* P=World->SpawnActor<AVoxelCommander>();P->SetupWeaponPreview(1);
    TestTrue(TEXT("Weapon preview exercises real firing"),P->WeaponPreviewActive&&!B->WeaponShots.IsEmpty());TestEqual(TEXT("Preview leaves standard mode selected"),P->SelectedMode,0);
    P->ShowMenu();P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    TestTrue(TEXT("Leaving preview restores normal AI and clears visuals"),B->AIEnabled&&B->TimeLimitEnabled&&!P->WeaponPreviewActive&&B->WeaponShots.IsEmpty());
    B->ResetBattle(false,true);B->WeaponShots.Add(FVoxelWeaponShot());B->RetryTutorialLesson();TestTrue(TEXT("Retrying a tutorial clears old ammunition and muzzle flashes"),B->WeaponShots.IsEmpty()&&B->Towers[1].ShotAge>1);
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelPierceVisualTest,"VoxelDominion.Visuals.PiercingBlueWhiteTrail",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelPierceVisualTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);B->Towers.Empty();
    FVoxelTower T;T.Team=1;T.Module=EVoxelModule::Arrow;T.Arrow=EVoxelArrow::Pierce;T.Cell=FIntPoint(-30,0);T.Troops=50;B->Towers.Add(T);
    auto Army=[&](int ID,int Team,FIntPoint Cell)
    {
        FVoxelSquad S;S.ID=ID;S.Team=Team;S.Troops=100;S.Target=INDEX_NONE;S.Holding=true;S.HasDestination=true;S.Destination=Cell;
        S.Position=B->CellPosition(Cell)+FVector(0,0,27);S.LastLand=Cell;S.RetargetIn=100;B->Squads.Add(S);
    };
    Army(970,2,FIntPoint(-27,0));Army(971,2,FIntPoint(-26,0));Army(972,2,FIntPoint(-27,2));Army(973,1,FIntPoint(-28,0));
    B->FireTowers(.01f);TestEqual(TEXT("One actual piercing attack produces one visual event"),B->WeaponShots.Num(),1);
    if(B->WeaponShots.IsEmpty()){World->DestroyWorld(false);return false;}
    float Damage=B->TowerDamage(0)*2.8f;
    TestTrue(TEXT("Both enemies on the attack line take the existing damage"),FMath::IsNearlyEqual(B->Squads[0].Troops,100-Damage,.001f)&&FMath::IsNearlyEqual(B->Squads[1].Troops,100-Damage,.001f));
    TestEqual(TEXT("Enemies outside the piercing corridor remain unharmed"),B->Squads[2].Troops,100.f);TestEqual(TEXT("Allies on the ray remain unharmed"),B->Squads[3].Troops,100.f);
    TestTrue(TEXT("The light travels beyond the farthest pierced enemy"),B->WeaponShots[0].To.X>B->Squads[1].Position.X);
    TestTrue(TEXT("The trace remains within the actual tower range"),FVector::Dist2D(B->CellPosition(T.Cell),B->WeaponShots[0].To)<=B->TowerRange(0,B->Squads[0].Position)+.01f);
    TestTrue(TEXT("Piercing keeps its normal long reload interval"),FMath::IsNearlyEqual(B->Towers[0].FireTimer,3.5f,.001f));
    B->InitializeWeaponVisuals();B->WeaponShots[0].Age=.12f;B->DrawWeaponVisuals();
    auto BlueInstances=[&]()
    {
        int Count=0;for(int I=0;I<B->WeaponMaterials.Num();++I)
        {auto C=B->WeaponMaterials[I]->K2_GetVectorParameterValue(TEXT("Tint"));if(C.B>1&&C.B>C.R*2&&C.G>.5f)Count+=B->WeaponBatches[I]->GetInstanceCount();}return Count;
    };
    TestTrue(TEXT("An active piercing shot draws a blue luminous trail"),BlueInstances()>0);
    bool WhiteCore=false,TransparentBlue=false;
    for(int I=0;I<B->WeaponMaterials.Num();++I)
    {
        auto C=B->WeaponMaterials[I]->K2_GetVectorParameterValue(TEXT("Tint"));
        if(C.R>2&&C.R<4&&C.G>2&&C.B>3&&B->WeaponBatches[I]->GetInstanceCount()>0)WhiteCore=true;
        if(C.R<.3f&&C.G>1&&C.B>2&&B->WeaponMaterials[I]->GetBlendMode()==BLEND_Additive)TransparentBlue=true;
    }
    TestTrue(TEXT("A white core is drawn inside an additive blue glow"),WhiteCore&&TransparentBlue);
    float Troops=B->TotalArmy(2),Age=B->WeaponShots[0].Age;int BlueCount=BlueInstances();B->Paused=true;B->AdvanceBattle(.25f);B->DrawWeaponVisuals();
    TestEqual(TEXT("Paused blue trails retain their phase"),B->WeaponShots[0].Age,Age);TestEqual(TEXT("Paused sparks keep the same geometry"),BlueInstances(),BlueCount);
    TestEqual(TEXT("Drawing a piercing ray never reapplies damage"),B->TotalArmy(2),Troops);
    B->Paused=false;B->Towers[0].FireTimer=100;B->SelectTower(0);B->AdvanceBattle(.25f);
    TestTrue(TEXT("Ray and spark age follow tactical slow time"),FMath::IsNearlyEqual(B->WeaponShots[0].Age,Age+.05f,.001f));
    B->CancelCommand();B->WeaponShots[0].Age=B->WeaponShots[0].FlightTime+.32f;B->AdvanceBattle(.05f);B->DrawWeaponVisuals();
    TestTrue(TEXT("The beam and its small fireworks disappear after the shot"),B->WeaponShots.IsEmpty()&&BlueInstances()==0);
    TestEqual(TEXT("Expiring light has no delayed gameplay damage"),B->TotalArmy(2),Troops);
    World->DestroyWorld(false);return true;
}
#endif
