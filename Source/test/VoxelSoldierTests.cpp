#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Components/InstancedStaticMeshComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelSoldierVisualTest,"VoxelDominion.Visuals.SwordAndShieldInfantry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelSoldierVisualTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);B->Towers.Empty();
    FVoxelSquad S;S.ID=950;S.Target=INDEX_NONE;S.Troops=48;S.LastLand=FIntPoint(-30,0);S.Position=B->CellPosition(S.LastLand)+FVector(0,0,27);
    S.HasDestination=true;S.Destination=FIntPoint(-27,0);S.Route.Add(B->CellPosition(S.Destination)+FVector(0,0,27));S.RetargetIn=100;B->Squads.Add(S);
    B->AdvanceBattle(.05f);TestTrue(TEXT("Actual marching advances the visual gait"),B->Squads[0].VisualMoving&&B->Squads[0].WalkPhase>0);
    float Phase=B->Squads[0].WalkPhase;FVector Position=B->Squads[0].Position;
    B->Paused=true;B->AdvanceBattle(.25f);TestEqual(TEXT("Pause freezes the gait"),B->Squads[0].WalkPhase,Phase);TestEqual(TEXT("Pause freezes movement"),B->Squads[0].Position,Position);
    B->Paused=false;B->SelectSquad(950);B->AdvanceBattle(.25f);
    TestTrue(TEXT("Thinking time advances only 0.05 battle seconds of walking"),FMath::IsNearlyEqual(float(FVector::Dist2D(B->Squads[0].Position,Position)),B->SoldierSpeed*.05f,.01f));
    B->CancelCommand();B->Squads[0].Holding=true;B->AdvanceBattle(.05f);TestFalse(TEXT("Waiting armies stop walking"),B->Squads[0].VisualMoving);
    S=B->Squads[0];S.ID=951;S.Team=2;S.Position+=FVector(0,80,0);S.Destination=B->ToCell(S.Position);B->Squads.Add(S);
    float Before=B->TotalArmy(1);B->AdvanceBattle(.05f);
    TestTrue(TEXT("Real encounters trigger sword and shield poses on both sides"),B->Squads[0].VisualMelee&&B->Squads[1].VisualMelee&&B->Squads[0].MeleePhase>0);
    TestTrue(TEXT("Existing encounter damage is unchanged"),FMath::IsNearlyEqual(Before-B->TotalArmy(1),18*.55f*.05f,.001f));
    B->MenuPaused=true;Phase=B->Squads[0].MeleePhase;B->AdvanceBattle(.25f);TestEqual(TEXT("Menus freeze sword attacks"),B->Squads[0].MeleePhase,Phase);B->MenuPaused=false;
    B->InitializeSoldierVisuals();TestEqual(TEXT("Reusable geometry is built once"),B->SoldierMeshes.Num(),3);
    for(auto Mesh:B->SoldierMeshes)
    {
        bool Valid=Mesh&&Mesh->GetRenderData()&&Mesh->GetBounds().SphereRadius>0;
        TestTrue(TEXT("Body, shield and sword have valid nonempty geometry"),Valid);if(!Valid)continue;
        const auto& LOD=Mesh->GetRenderData()->LODResources[0];auto Indices=LOD.IndexBuffer.GetArrayView();bool Outward=Indices.Num()>0;
        for(int I=0;I+2<Indices.Num();I+=3)
        {
            FVector A(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I]));
            FVector C(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+1]));
            FVector D(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(Indices[I+2]));
            FVector Front=FVector::CrossProduct(D-A,C-A).GetSafeNormal();
            FVector Normal(LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(Indices[I]));
            Outward&=FVector::DotProduct(Front,(A+C+D)/3)>0&&FVector::DotProduct(Front,Normal)>.99;
        }
        TestTrue(TEXT("All exterior triangles face outwards and agree with their lighting normals"),Outward);
    }
    B->DrawSoldierVisuals();int Instances=0;for(auto Batch:B->SoldierBatches)Instances+=Batch->GetInstanceCount();
    TestTrue(TEXT("Living squads draw the new model"),Instances>0);float After=B->TotalArmy(1);B->DrawSoldierVisuals();TestEqual(TEXT("Drawing sword swings does not deal extra damage"),B->TotalArmy(1),After);
    B->ResetBattle(true);B->DrawSoldierVisuals();Instances=0;for(auto Batch:B->SoldierBatches)Instances+=Batch->GetInstanceCount();TestEqual(TEXT("Restart removes all old soldier instances"),Instances,0);
    auto* P=World->SpawnActor<AVoxelCommander>();P->SetupSoldierPreview(2);
    TestTrue(TEXT("The encounter preview uses actual combat"),P->SoldierPreviewActive&&!B->Squads.IsEmpty()&&B->Squads[0].VisualMelee);
    P->ShowMenu();P->MenuAction(TEXT("start"));P->MenuAction(TEXT("confirmNew"));
    TestTrue(TEXT("Leaving preview restores the standard battle"),B->AIEnabled&&B->TimeLimitEnabled&&!P->SoldierPreviewActive&&B->Squads.IsEmpty());
    World->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelThinkingSmoothTest,"VoxelDominion.Visuals.SmoothThinkingTimeInfantry",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelThinkingSmoothTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);B->Towers.Empty();
    FVoxelSquad S;S.ID=970;S.Troops=40;S.Target=INDEX_NONE;S.LastLand=FIntPoint(-30,0);S.Position=B->CellPosition(S.LastLand)+FVector(0,0,27);
    S.HasDestination=true;S.Destination=FIntPoint(-24,0);S.Route.Add(B->CellPosition(S.Destination)+FVector(0,0,27));S.RetargetIn=100;B->Squads.Add(S);
    B->AdvanceBattle(.1f);B->SelectSquad(S.ID);
    TestEqual(TEXT("Thinking keeps the requested 0.2x battle speed"),B->GetBattleSpeed(),.2f);
    FVector Start=B->Squads[0].Position,LastRender=B->SoldierRenderPose(B->Squads[0]).Position,LastSimulation=Start;
    int RenderUpdates=0,SimulationUpdates=0;bool Bounded=true,LabelsFollow=true;
    for(int Frame=0;Frame<60;++Frame)
    {
        B->AdvanceBattle(1.f/60);const auto& Army=B->Squads[0];const auto Pose=B->SoldierRenderPose(Army);
        float Distance=FVector::Dist2D(LastRender,Pose.Position);
        RenderUpdates+=Distance>.001f;SimulationUpdates+=!Army.Position.Equals(LastSimulation,.001f);
        Bounded&=Distance<1.1f&&Pose.Position.X>=LastRender.X-.001f;
        LabelsFollow&=(B->SquadLabelPosition(Army)-FVector(0,0,116)).Equals(Pose.Position,.001f);
        LastRender=Pose.Position;LastSimulation=Army.Position;
    }
    TestTrue(TEXT("Every display frame advances smoothly although the simulation ticks four times"),RenderUpdates>=59&&SimulationUpdates==4);
    TestTrue(TEXT("No slow-motion frame introduces a movement jump or backward step"),Bounded);
    TestTrue(TEXT("Health labels and selection hit points follow the rendered army"),LabelsFollow);
    TestTrue(TEXT("Smoothing leaves actual 0.2x marching distance unchanged"),FMath::IsNearlyEqual(float(FVector::Dist2D(Start,B->Squads[0].Position)),B->SoldierSpeed*.2f,.01f));
    B->Paused=true;const auto Frozen=B->SoldierRenderPose(B->Squads[0]);B->AdvanceBattle(.25f);
    TestTrue(TEXT("Pause freezes the interpolated pose as well as the simulation"),Frozen.Position.Equals(B->SoldierRenderPose(B->Squads[0]).Position)&&Frozen.WalkPhase==B->SoldierRenderPose(B->Squads[0]).WalkPhase);
    B->Paused=false;B->AdvanceBattle(.125f);auto& Army=B->Squads[0];
    Army.PreviousPose.Yaw=179;Army.VisualYaw=-179;Army.PreviousPose.WalkPhase=2*PI-.1f;Army.WalkPhase=.1f;
    auto Wrapped=B->SoldierRenderPose(Army);
    TestTrue(TEXT("Turning across north follows the short two-degree arc"),FMath::Abs(FMath::FindDeltaAngleDegrees(Wrapped.Yaw,180))<.01f);
    TestTrue(TEXT("Looping footsteps do not rewind when the phase wraps"),FMath::Abs(FMath::FindDeltaAngleRadians(Wrapped.WalkPhase,0))<.01f);
    Army.Position+=FVector(10000,0,0);TestTrue(TEXT("Relocation snaps safely instead of interpolating across water"),B->SoldierRenderPose(Army).Position.Equals(Army.Position));
    World->DestroyWorld(false);return true;
}
#endif
