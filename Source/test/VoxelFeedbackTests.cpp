#include "VoxelWar.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Sound/SoundWave.h"
#include "Engine/Texture2D.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelUIArtTest,"VoxelDominion.UI.TransparentFeedbackAssets",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelUIArtTest::RunTest(const FString&)
{
    const TCHAR* Names[]={TEXT("T_MoneyBag"),TEXT("T_SelectionBlade"),TEXT("T_HandCursor"),TEXT("T_GoldDigits")};
    for(int I=0;I<4;++I)
    {
        FString Path=FString::Printf(TEXT("/Game/Voxel/UI/%s.%s"),Names[I],Names[I]);
        auto* Texture=LoadObject<UTexture2D>(nullptr,*Path);if(!TestNotNull(*Path,Texture))continue;
        TestEqual(TEXT("UI textures retain crisp, uncompressed color and alpha"),Texture->CompressionSettings,TC_EditorIcon);
        TestEqual(TEXT("UI art uses the UI texture group"),Texture->LODGroup,TEXTUREGROUP_UI);
        TestTrue(TEXT("UI art never streams to a lower resolution"),bool(Texture->NeverStream));
        if(I==3){TestEqual(TEXT("All ten numeral cells are present"),Texture->Source.GetSizeX(),int64(3840));TestEqual(TEXT("High resolution numeral atlas height is intact"),Texture->Source.GetSizeY(),int64(512));}
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelTowerAudioTest,"VoxelDominion.Audio.WeaponAssetsAndZoom",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelTowerAudioTest::RunTest(const FString&)
{
    FVector Center(1000,2000,0);
    float Near=VoxelRules::WeaponAudioGain(3200,Center,Center),Normal=VoxelRules::WeaponAudioGain(8500,Center,Center);
    float Overview=VoxelRules::WeaponAudioGain(12800,Center,Center),Far=VoxelRules::WeaponAudioGain(23000,Center,Center);
    TestTrue(TEXT("Zooming in makes centered weapons louder"),Near>Normal&&Normal>Overview&&Overview>Far);
    TestTrue(TEXT("Overview remains audible and close-up gain stays bounded"),Far>.05f&&Near<=.9f);
    TestTrue(TEXT("Weapons near screen edges are softer"),VoxelRules::WeaponAudioGain(8500,Center,Center+FVector(3500,0,0))<Normal);
    TestEqual(TEXT("Distant off-screen battles do not consume audio voices"),VoxelRules::WeaponAudioGain(8500,Center,Center+FVector(14000,0,0)),0.f);
    TestTrue(TEXT("Invalid zoom cannot create infinite gain"),FMath::IsFinite(VoxelRules::WeaponAudioGain(0,Center,Center)));
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->InitializeBattleAudio();
    TestEqual(TEXT("Three variations for each of the two weapons are loaded"),B->TowerSounds.Num(),6);
    TestEqual(TEXT("Reusable voice budget is bounded"),B->ShotAudio.Num(),12);
    for(int I=0;I<B->TowerSounds.Num();++I)
    {
        auto* Sound=B->TowerSounds[I].Get();if(!TestNotNull(TEXT("Authored WAV imports exist"),Sound))continue;
        TestEqual(TEXT("Weapon effects are mono"),Sound->NumChannels,1);
        TestFalse(TEXT("One attack never starts an endless sound loop"),bool(Sound->bLooping));
        TestTrue(TEXT("Short burst/release duration is preserved"),Sound->Duration>(I<3?.23f:.34f)&&Sound->Duration<(I<3?.26f:.38f));
    }
    B->InitializeBattleAudio();TestEqual(TEXT("Repeated initialization cannot duplicate the pool"),B->ShotAudio.Num(),12);
    B->StopBattleAudio();B->ResetBattle(true);TestTrue(TEXT("Reset keeps the audio bank ready for the next battle"),B->TowerSounds.Num()==6&&B->ShotAudio.Num()==12);
    World->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVoxelRangeOverlayTest,"VoxelDominion.Visuals.CompleteRotatingAttackRange",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVoxelRangeOverlayTest::RunTest(const FString&)
{
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);auto* B=World->SpawnActor<AVoxelBattle>();B->ResetBattle(true);
    B->InitializeRangeOverlay();if(!TestNotNull(TEXT("Range overlay material and component exist"),B->RangeOverlay.Get())){World->DestroyWorld(false);return false;}
    TestFalse(TEXT("Range is hidden without a selection"),B->RangeOverlay->IsVisible());
    B->SelectTower(2);B->UpdateRangeOverlay();TestTrue(TEXT("Selecting an arrow tower displays its range"),B->RangeOverlay->IsVisible());
    UStaticMesh* FirstMesh=B->RangeOverlay->GetStaticMesh();TestNotNull(TEXT("A continuous tactical plane exists"),FirstMesh);
    if(FirstMesh&&FirstMesh->GetRenderData())
    {
        const auto& LOD=FirstMesh->GetRenderData()->LODResources[0];int Vertices=0;float PlaneZ=0;bool Flat=true;
        for(uint32 V=0;V<LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices();++V)
        {
            float Z=LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(V).Z;
            if(Vertices)Flat&=FMath::IsNearlyEqual(PlaneZ,Z,.01f);PlaneZ=Z;++Vertices;
        }
        TestTrue(TEXT("One flat plane keeps the outline and crest continuous"),Vertices>=4&&Vertices<=6&&Flat);
    }
    TestTrue(TEXT("Cliffs and shorelines cannot cut the tactical circle"),bool(B->RangeMaterial->GetMaterial()->bDisableDepthTest));
    TestTrue(TEXT("Circle fill is translucent but clearly visible"),B->RangeMaterial->K2_GetScalarParameterValue(TEXT("FillOpacity"))>=.15f&&B->RangeMaterial->K2_GetScalarParameterValue(TEXT("FillOpacity"))<=.25f);
    auto FillTint=B->RangeMaterial->K2_GetVectorParameterValue(TEXT("FillTint"));
    TestTrue(TEXT("Interior fill is light blue"),FillTint.B>FillTint.G&&FillTint.G>FillTint.R);
    TestTrue(TEXT("Arrow dial uses blue light"),B->RangeMaterial->K2_GetVectorParameterValue(TEXT("Tint")).B>.9f);
    float Range=B->RangeMaterial->K2_GetScalarParameterValue(TEXT("BaseRange"));
    TestEqual(TEXT("Range material uses the combat base range"),Range,B->TowerBaseRange(2));
    auto Origin=B->RangeMaterial->K2_GetVectorParameterValue(TEXT("Origin"));
    TestTrue(TEXT("Height bonus uses the selected tower elevation"),FMath::IsNearlyEqual(Origin.B,B->CellPosition(B->Towers[2].Cell).Z,.01f));
    TestTrue(TEXT("Overlay does not block input or troops"),B->RangeOverlay->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    B->UpdateRangeOverlay();TestTrue(TEXT("Steady selection reuses its mesh instead of rebuilding each frame"),B->RangeOverlay->GetStaticMesh()==FirstMesh);
    B->Gold[1]=999;TestTrue(TEXT("Selected military tower can upgrade"),B->UpgradeTower(2));B->UpdateRangeOverlay();
    TestTrue(TEXT("Upgrade expands the visible range"),B->RangeMaterial->K2_GetScalarParameterValue(TEXT("BaseRange"))>Range);
    TestEqual(TEXT("Expanded display matches upgraded combat range"),B->RangeMaterial->K2_GetScalarParameterValue(TEXT("BaseRange")),B->TowerBaseRange(2));
    B->SelectTower(1);B->UpdateRangeOverlay();auto CannonTint=B->RangeMaterial->K2_GetVectorParameterValue(TEXT("Tint"));
    TestTrue(TEXT("Cannon dial uses amber instead of arrow blue"),CannonTint.R>.9f&&CannonTint.B<.05f);
    TestTrue(TEXT("Cannon keeps the requested blue translucent interior"),B->RangeMaterial->K2_GetVectorParameterValue(TEXT("FillTint"))==FillTint);
    B->CompleteCommand();B->Time=0;B->UpdateRangeOverlay();float Angle=B->RangeMaterial->K2_GetScalarParameterValue(TEXT("RotationAngle"));
    UStaticMesh* AnimationMesh=B->RangeOverlay->GetStaticMesh();
    for(int I=0;I<4;++I)B->AdvanceBattle(.25f);B->UpdateRangeOverlay();
    float Rotated=B->RangeMaterial->K2_GetScalarParameterValue(TEXT("RotationAngle"));
    TestTrue(TEXT("Dial rotates at four degrees per battle second"),FMath::IsNearlyEqual(Rotated-Angle,PI/45.f,.001f));
    TestTrue(TEXT("Rotation reuses the mesh"),B->RangeOverlay->GetStaticMesh()==AnimationMesh);
    B->SelectTower(1);Angle=Rotated;for(int I=0;I<4;++I)B->AdvanceBattle(.25f);B->UpdateRangeOverlay();
    Rotated=B->RangeMaterial->K2_GetScalarParameterValue(TEXT("RotationAngle"));
    TestTrue(TEXT("Thinking slows dial rotation to one fifth"),FMath::IsNearlyEqual(Rotated-Angle,PI/225.f,.001f));
    B->CompleteCommand();B->SetPlaybackSpeed(2);Angle=Rotated;
    for(int I=0;I<4;++I)B->AdvanceBattle(.25f);B->UpdateRangeOverlay();Rotated=B->RangeMaterial->K2_GetScalarParameterValue(TEXT("RotationAngle"));
    TestTrue(TEXT("2x doubles dial rotation using the shared clock"),FMath::IsNearlyEqual(Rotated-Angle,2*PI/45.f,.001f));
    B->Paused=true;B->UpdateRangeOverlay();TestTrue(TEXT("Range remains readable while paused"),B->RangeOverlay->IsVisible());
    B->AdvanceBattle(.25f);B->UpdateRangeOverlay();TestEqual(TEXT("Pause freezes dial rotation"),B->RangeMaterial->K2_GetScalarParameterValue(TEXT("RotationAngle")),Rotated);
    B->MenuPaused=true;B->UpdateRangeOverlay();TestFalse(TEXT("Main menu hides the battle range"),B->RangeOverlay->IsVisible());
    B->MenuPaused=false;B->Paused=false;B->CancelCommand();B->UpdateRangeOverlay();TestFalse(TEXT("Cancelling selection removes the dial"),B->RangeOverlay->IsVisible());
    B->SelectTower(0);B->UpdateRangeOverlay();TestFalse(TEXT("HQ does not borrow an arrow/cannon range dial"),B->RangeOverlay->IsVisible());
    B->SelectTower(6);B->UpdateRangeOverlay();TestFalse(TEXT("Resource villages do not show fake weapon range"),B->RangeOverlay->IsVisible());
    B->SelectTower(2);B->Winner=1;B->UpdateRangeOverlay();TestFalse(TEXT("Battle results hide the dial"),B->RangeOverlay->IsVisible());B->Winner=0;
    // Reproduce a coastal tower whose radius extends well outside the playable footprint.
    for(int X=-B->MapHalfWidth();X<=0;++X)if(B->ContainsCell(FIntPoint(X,0))){B->Towers[2].Cell=FIntPoint(X,0);break;}
    B->SelectTower(2);B->UpdateRangeOverlay();
    FBox Bounds=B->RangeOverlay->GetStaticMesh()->GetBounds().GetBox();FVector Center=B->CellPosition(B->Towers[2].Cell);
    float Radius=B->TowerBaseRange(2);int Outside=0;bool Complete=true,CrestCovered=true,WalkableOutside=false;
    float CrestReach=Radius+18+B->RangeMaterial->K2_GetScalarParameterValue(TEXT("BadgeSize"))*1.5f;
    for(int K=0;K<360;++K)
    {
        float A=K*PI/180;FVector P=Center+FVector(FMath::Cos(A)*Radius,FMath::Sin(A)*Radius,0);
        Complete&=P.X>=Bounds.Min.X&&P.X<=Bounds.Max.X&&P.Y>=Bounds.Min.Y&&P.Y<=Bounds.Max.Y;
        FVector CrestEdge=Center+FVector(FMath::Cos(A)*CrestReach,FMath::Sin(A)*CrestReach,0);
        CrestCovered&=CrestEdge.X>=Bounds.Min.X&&CrestEdge.X<=Bounds.Max.X&&CrestEdge.Y>=Bounds.Min.Y&&CrestEdge.Y<=Bounds.Max.Y;
        if(!B->ContainsCell(B->ToCell(P))){++Outside;WalkableOutside|=B->Walkable(B->ToCell(P));}
    }
    TestTrue(TEXT("Coastal regression scene includes off-map circle points"),Outside>0);
    TestTrue(TEXT("All 360 degrees remain covered beyond the coastline"),Complete);
    TestTrue(TEXT("Rotating crest stays inside the drawing surface throughout its full orbit"),CrestCovered);
    TestFalse(TEXT("Off-map visual fill does not create walkable land"),WalkableOutside);
    B->ResetBattle(false,true);B->SelectTower(1);B->UpdateRangeOverlay();
    TestEqual(TEXT("Training map retains its actual smaller weapon ranges"),B->RangeMaterial->K2_GetScalarParameterValue(TEXT("BaseRange")),B->TowerBaseRange(1));
    World->DestroyWorld(false);return true;
}
#endif
