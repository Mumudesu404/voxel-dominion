#include "VoxelWar.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"

void AVoxelBattle::InitializeRangeOverlay()
{
    if(RangeOverlay)return;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_TowerRange.M_TowerRange"));
    if(!Material)return;
    RangeMaterial=UMaterialInstanceDynamic::Create(Material,this);
    RangeOverlay=NewObject<UStaticMeshComponent>(this);
    RangeOverlay->SetupAttachment(RootComponent);RangeOverlay->SetMobility(EComponentMobility::Movable);
    RangeOverlay->SetCollisionEnabled(ECollisionEnabled::NoCollision);RangeOverlay->SetCanEverAffectNavigation(false);
    RangeOverlay->SetCastShadow(false);RangeOverlay->bReceivesDecals=false;RangeOverlay->SetTranslucentSortPriority(1);
    RangeOverlay->SetMaterial(0,RangeMaterial);RangeOverlay->RegisterComponent();RangeOverlay->SetVisibility(false);
}

void AVoxelBattle::UpdateRangeOverlay()
{
    bool Show=!MenuPaused&&!Winner&&Towers.IsValidIndex(Selected)&&
        (Towers[Selected].Module==EVoxelModule::Arrow||Towers[Selected].Module==EVoxelModule::Cannon);
    if(!Show){if(RangeOverlay)RangeOverlay->SetVisibility(false);return;}
    InitializeRangeOverlay();if(!RangeOverlay||!RangeMaterial)return;
    RangeOverlay->SetVisibility(true);
    RangeMaterial->SetScalarParameterValue(TEXT("BattleClock"),Time+Accumulator);
    // Animate the dial in the shader; selection and time changes never rebuild its mesh.
    RangeMaterial->SetScalarParameterValue(TEXT("RotationAngle"),FMath::Fmod((Time+Accumulator)*(PI/45.f),2*PI));
    const auto& T=Towers[Selected];FVector Origin=CellPosition(T.Cell);
    uint32 Signature=HashCombineFast(GetTypeHash(Selected),GetTypeHash(T.Cell));
    for(uint32 Value:{uint32(T.Level),uint32(T.Module),uint32(T.Tall),uint32(T.Team),uint32(Tutorial)})Signature=HashCombineFast(Signature,Value);
    if(Signature==LastRangeSignature&&RangeOverlay->GetStaticMesh())return;
    LastRangeSignature=Signature;float Radius=TowerBaseRange(Selected);
    RangeMaterial->SetVectorParameterValue(TEXT("Origin"),FLinearColor(Origin.X,Origin.Y,Origin.Z,1));
    RangeMaterial->SetScalarParameterValue(TEXT("BaseRange"),Radius);
    RangeMaterial->SetScalarParameterValue(TEXT("FillOpacity"),.18f);
    RangeMaterial->SetVectorParameterValue(TEXT("FillTint"),FLinearColor(.16f,.65f,1.75f));
    RangeMaterial->SetVectorParameterValue(TEXT("Tint"),T.Module==EVoxelModule::Arrow?FLinearColor(.035f,.49f,1):FLinearColor(1,.43f,.018f));
    FVector Front=FVector(-3300,-4000,0).GetSafeNormal();
    FVector Badge=Origin+Front*(Radius+18);
    RangeMaterial->SetVectorParameterValue(TEXT("BadgePosition"),FLinearColor(Badge.X,Badge.Y,0,1));
    float BadgeSize=FMath::Clamp(Radius*.075f,65.f,100.f);
    RangeMaterial->SetScalarParameterValue(TEXT("BadgeSize"),BadgeSize);

    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
    auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
    auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    auto Colors=Attributes.GetVertexInstanceColors();auto Group=Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Range");
    // One continuous tactical plane includes the coast, river and off-map background.
    // Depth is disabled only for this translucent indicator, so cliffs cannot cut its outline.
    // The collider remains disabled and the combat range/terrain rules remain in the simulation.
    float Extent=Radius+240;
    TArray<FVertexInstanceID> Corners;
    for(FVector2D Offset:{FVector2D(-1,-1),FVector2D(-1,1),FVector2D(1,1),FVector2D(1,-1)})
    {
        auto V=Description.CreateVertex();Positions[V]=FVector3f(Origin.X+Offset.X*Extent,Origin.Y+Offset.Y*Extent,Origin.Z+8);
        auto VI=Description.CreateVertexInstance(V);Corners.Add(VI);
        Normals[VI]=FVector3f::UpVector;Tangents[VI]=FVector3f::ForwardVector;Signs[VI]=1;Colors[VI]=FVector4f(1,1,1,1);
        UVs.Set(VI,0,FVector2f((Offset.X+1)*.5,(Offset.Y+1)*.5));
    }
    Description.CreatePolygon(Group,Corners);
    auto* Mesh=NewObject<UStaticMesh>(this,NAME_None,RF_Transient);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(RangeMaterial,TEXT("Range")));
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=true;Params.bBuildSimpleCollision=false;
    Params.bCommitMeshDescription=false;Params.bMarkPackageDirty=false;Params.bUseHashAsGuid=true;Params.bAllowCpuAccess=true;
    if(Mesh->BuildFromMeshDescriptions({&Description},Params))RangeOverlay->SetStaticMesh(Mesh);
}
