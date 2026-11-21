#include "VoxelWar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "DrawDebugHelpers.h"
#include "Algo/Reverse.h"

namespace
{
enum ESoldierTint
{
    BlueArmor,BlueLight,BlueShade,RedArmor,RedLight,RedShade,Skin,SkinShade,Hair,
    Cloth,Iron,Steel,SteelLight,Linen,Brass,ShieldRim,BlueShield,RedShield,SwordBlade,TintCount
};

// Shared low-poly primitives, built once. Bevels catch the light without textures.
UStaticMesh* MakeSoldierPrimitive(UObject* Outer,UMaterialInterface* Material,int Shape)
{
    FMeshDescription Description;FStaticMeshAttributes Attributes(Description);Attributes.Register();
    auto Positions=Attributes.GetVertexPositions();auto Normals=Attributes.GetVertexInstanceNormals();
    auto Tangents=Attributes.GetVertexInstanceTangents();auto Signs=Attributes.GetVertexInstanceBinormalSigns();
    auto UVs=Attributes.GetVertexInstanceUVs();UVs.SetNumChannels(1);
    auto Colors=Attributes.GetVertexInstanceColors();auto Group=Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Soldier");
    auto Face=[&](TArray<FVector> Points)
    {
        FVector Center=FVector::ZeroVector;for(FVector P:Points)Center+=P;Center/=Points.Num();
        // Unreal's front faces use clockwise winding.
        FVector Normal=FVector::CrossProduct(Points[2]-Points[0],Points[1]-Points[0]).GetSafeNormal();
        if(FVector::DotProduct(Normal,Center)<0){Algo::Reverse(Points);Normal=-Normal;}
        FVector Tangent=(Points[1]-Points[0]).GetSafeNormal();TArray<FVertexInstanceID> Corners;
        for(FVector P:Points)
        {
            auto V=Description.CreateVertex();Positions[V]=FVector3f(P);auto VI=Description.CreateVertexInstance(V);Corners.Add(VI);
            Normals[VI]=FVector3f(Normal);Tangents[VI]=FVector3f(Tangent);Signs[VI]=1;Colors[VI]=FVector4f(1,1,1,1);
            UVs.Set(VI,0,FVector2f(float(P.Y/100+.5),float(P.Z/100+.5)));
        }
        Description.CreatePolygon(Group,Corners);
    };
    if(Shape==0)
    {
        constexpr double A=50,B=45;
        for(int Axis=0;Axis<3;++Axis)for(int Sign:{-1,1})
        {
            TArray<FVector> P;for(FVector2D Corner:{FVector2D(-1,-1),FVector2D(1,-1),FVector2D(1,1),FVector2D(-1,1)})
            {FVector V=FVector::ZeroVector;V[Axis]=Sign*A;V[(Axis+1)%3]=Corner.X*B;V[(Axis+2)%3]=Corner.Y*B;P.Add(V);}Face(P);
        }
        for(int Axis=0;Axis<3;++Axis)for(int S:{-1,1})for(int T:{-1,1})
        {
            int J=(Axis+1)%3,K=(Axis+2)%3;TArray<FVector> P;
            for(int End:{-1,1}){FVector V=FVector::ZeroVector;V[Axis]=S*A;V[J]=T*B;V[K]=End*B;P.Add(V);}
            for(int End:{1,-1}){FVector V=FVector::ZeroVector;V[Axis]=S*B;V[J]=T*A;V[K]=End*B;P.Add(V);}Face(P);
        }
        for(int X:{-1,1})for(int Y:{-1,1})for(int Z:{-1,1})Face({FVector(X*A,Y*B,Z*B),FVector(X*B,Y*A,Z*B),FVector(X*B,Y*B,Z*A)});
    }
    else
    {
        TArray<FVector2D> Outline=Shape==1?
            TArray<FVector2D>{{-34,50},{34,50},{50,34},{50,-25},{30,-44},{0,-50},{-30,-44},{-50,-25},{-50,34}}:
            TArray<FVector2D>{{-50,50},{50,50},{42,-29},{0,-50},{-42,-29}};
        for(int Sign:{-1,1}){TArray<FVector> P;for(auto V:Outline)P.Add(FVector(Sign*50,V.X,V.Y));Face(P);}
        for(int I=0;I<Outline.Num();++I)
        {auto A=Outline[I],B=Outline[(I+1)%Outline.Num()];Face({FVector(-50,A.X,A.Y),FVector(50,A.X,A.Y),FVector(50,B.X,B.Y),FVector(-50,B.X,B.Y)});}
    }
    auto* Mesh=NewObject<UStaticMesh>(Outer,NAME_None,RF_Transient);Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,TEXT("Soldier")));
    UStaticMesh::FBuildMeshDescriptionsParams Params;Params.bFastBuild=true;Params.bBuildSimpleCollision=false;
    Params.bCommitMeshDescription=false;Params.bMarkPackageDirty=false;Params.bUseHashAsGuid=true;Params.bAllowCpuAccess=true;
    if(!Mesh->BuildFromMeshDescriptions({&Description},Params))return nullptr;
    return Mesh;
}
}

void AVoxelBattle::InitializeSoldierVisuals()
{
    if(!SoldierBatches.IsEmpty())return;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_Voxel.M_Voxel"));
    if(!Material)Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    for(int Shape=0;Shape<3;++Shape)SoldierMeshes.Add(MakeSoldierPrimitive(this,Material,Shape));
    const FLinearColor Colors[]={
        {.025f,.235f,.61f},{.055f,.33f,.80f},{.018f,.12f,.30f},
        {.63f,.065f,.028f},{.83f,.13f,.055f},{.30f,.025f,.018f},
        {.91f,.52f,.25f},{.64f,.30f,.115f},{.055f,.038f,.027f},
        {.115f,.12f,.12f},{.185f,.195f,.185f},{.46f,.49f,.49f},{.71f,.73f,.70f},
        {.79f,.66f,.43f},{.91f,.51f,.055f},{.135f,.15f,.15f},
        {.025f,.24f,.63f},{.65f,.067f,.025f},{.56f,.59f,.59f}};
    static_assert(UE_ARRAY_COUNT(Colors)==TintCount);
    for(int I=0;I<TintCount;++I)
    {
        auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);Batch->SetupAttachment(RootComponent);Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetStaticMesh(SoldierMeshes[I==SwordBlade?2:I>=ShieldRim?1:0]);Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Batch->SetCanEverAffectNavigation(false);Batch->SetCastShadow(true);Batch->bReceivesDecals=false;
        auto* MID=UMaterialInstanceDynamic::Create(Material,this);MID->SetVectorParameterValue(TEXT("Tint"),Colors[I]);
        Batch->SetMaterial(0,MID);Batch->RegisterComponent();SoldierMaterials.Add(MID);SoldierBatches.Add(Batch);
    }
}

void AVoxelBattle::UpdateSoldierPose(FVoxelSquad& S,FVector Previous,FVector FacingTarget,bool Fighting,float Dt)
{
    FVector Delta=S.Position-Previous;Delta.Z=0;float Distance=Delta.Size();
    S.VisualMoving=Distance>.01f;S.VisualMelee=Fighting&&S.Troops>0;
    if(S.VisualMoving)S.WalkPhase=FMath::Fmod(S.WalkPhase+Distance*(2*PI/72),2*PI);
    if(S.VisualMelee)S.MeleePhase=FMath::Fmod(S.MeleePhase+Dt*(2*PI/.85f),2*PI);else S.MeleePhase=0;
    FVector Direction=S.VisualMelee?FacingTarget-S.Position:Delta;Direction.Z=0;
    if(!Direction.IsNearlyZero())S.VisualYaw=FMath::FixedTurn(S.VisualYaw,Direction.Rotation().Yaw,540*Dt);
}

FVoxelSoldierPose AVoxelBattle::SoldierRenderPose(const FVoxelSquad& S) const
{
    FVoxelSoldierPose Current{S.Position,S.VisualYaw,S.WalkPhase,S.MeleePhase,float(S.VisualMoving),float(S.VisualMelee)};
    if(!S.HasPreviousPose||Winner||(Tutorial&&TutorialObjectiveComplete)||
        FVector::DistSquared2D(S.PreviousPose.Position,S.Position)>FMath::Square(SoldierSpeed*.05f+SquadMergeRadius+1))return Current;
    float Alpha=FMath::Clamp(Accumulator/.05f,0.f,1.f);const auto& Previous=S.PreviousPose;
    Current.Position=FMath::Lerp(Previous.Position,Current.Position,Alpha);
    Current.Yaw=Previous.Yaw+FMath::FindDeltaAngleDegrees(Previous.Yaw,Current.Yaw)*Alpha;
    Current.WalkPhase=Previous.WalkPhase+FMath::FindDeltaAngleRadians(Previous.WalkPhase,Current.WalkPhase)*Alpha;
    Current.MeleePhase=Previous.MeleePhase+FMath::FindDeltaAngleRadians(Previous.MeleePhase,Current.MeleePhase)*Alpha;
    Current.Moving=FMath::Lerp(Previous.Moving,Current.Moving,Alpha);
    Current.Melee=FMath::Lerp(Previous.Melee,Current.Melee,Alpha);
    return Current;
}

FVector AVoxelBattle::SquadLabelPosition(const FVoxelSquad& S) const
{return SoldierRenderPose(S).Position+FVector(0,0,116);}

void AVoxelBattle::DrawSoldierVisuals()
{
    if(SoldierBatches.IsEmpty())return;
    TArray<FTransform> Instances[TintCount];auto* Commander=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController());
    float Zoom=Commander?Commander->Zoom:8500;int LOD=Zoom<5000?0:Zoom<11500?1:2;
    for(const auto& S:Squads)
    {
        if(S.Troops<=0)continue;
        const auto Pose=SoldierRenderPose(S);
        if(Commander&&!Commander->IsMenuOpen()&&FVector::DistSquared2D(Pose.Position,Commander->Focus)>FMath::Square(Zoom*.95f+300))continue;
        int Armor=S.Team==2?RedArmor:BlueArmor,Light=Armor+1,Shade=Armor+2;
        int ShieldColor=S.Team==2?RedShield:BlueShield;
        if(S.ID==SelectedSquad)DrawDebugCircle(GetWorld(),Pose.Position-FVector(0,0,23),95,32,FColor(245,208,94),false,0,0,3,FVector(1,0,0),FVector(0,1,0),false);
        FQuat Facing=FRotator(0,Pose.Yaw,0).Quaternion();int Count=FMath::Clamp(FMath::CeilToInt(S.Troops/4),1,12),Columns=FMath::Min(3,Count),Rows=FMath::DivideAndRoundUp(Count,Columns);
        for(int I=0;I<Count;++I)
        {
            int Row=I/Columns,RowCount=FMath::Min(Columns,Count-Row*Columns);
            FVector Offset((Rows-1)*19.f-Row*38.f,((I%Columns)-(RowCount-1)*.5f)*46,0);
            // Keep each army behind its own front rank while fighting. The
            // simulation anchors can be close without interleaving the models.
            if(Pose.Melee>.5f)Offset.X=10-Row*38.f;
            FVector Root=Pose.Position-FVector(0,0,27)+Facing.RotateVector(Offset);
            FIntPoint Cell=ToCell(Root);
            if(!Walkable(Cell))Root=Pose.Position-FVector(0,0,27)+Facing.RotateVector(Offset*.25f);
            else Root.Z=FMath::Max(Root.Z,Height(Cell));
            float Phase=Pose.WalkPhase+I*1.73f+S.ID*.39f;
            float Stride=Pose.Moving*FMath::Sin(Phase),Attack=Pose.Melee*FMath::Sin(Pose.MeleePhase+I*.57f);
            float Bob=Pose.Moving*FMath::Abs(FMath::Sin(Phase))*1.5f;
            auto Part=[&](int Tint,FVector P,FVector Size,FQuat Local=FQuat::Identity)
            {Instances[Tint].Add(FTransform(Facing*Local,Root+Facing.RotateVector(P),Size/100.f));};
            auto Pitch=[](float Angle){return FQuat(FVector::RightVector,FMath::DegreesToRadians(Angle));};
            FVector Torso(0,0,Bob);
            Part(Armor,Torso+FVector(0,0,51),FVector(19,28,24));
            if(LOD<2)
            {
                Part(Light,Torso+FVector(10,0,53),FVector(3,23,19));
                Part(Shade,Torso+FVector(0,0,33),FVector(19,24,10));
                Part(Hair,Torso+FVector(0,0,65),FVector(12,15,8));
            }
            Part(Linen,Torso+FVector(0,0,39),FVector(22,32,6));
            if(LOD<2){Part(Brass,Torso+FVector(12,0,39),FVector(3,9,9));Part(Shade,Torso+FVector(13.7,0,39),FVector(1,4.5,4.5));}
            Part(Skin,Torso+FVector(0,0,78),FVector(25,31,27));
            Part(Armor,Torso+FVector(-1,0,91),FVector(33,38,10));
            if(LOD<2)
            {
                Part(Armor,Torso+FVector(-3,0,98),FVector(28,33,6));
                Part(Light,Torso+FVector(-3,0,103),FVector(20,25,4));
                Part(Hair,Torso+FVector(-12.5,0,74),FVector(3,29,22));
                Part(Shade,Torso+FVector(-6,0,85),FVector(27,37,9));
                for(int Side:{-1,1})
                {
                    Part(Armor,Torso+FVector(-4,Side*17,79),FVector(23,5,19));
                    Part(SkinShade,Torso+FVector(0,Side*16.5,77),FVector(9,3,10));
                    Part(Hair,Torso+FVector(12.9,Side*6.7,77),FVector(1.8,4.4,6.8));
                    Part(Hair,Torso+FVector(13,Side*6.7,83.5),FVector(2,9.5,3.5),FQuat(FVector::ForwardVector,FMath::DegreesToRadians(Side*12.f)));
                }
            }
            for(int Side:{-1,1})
            {
                float Swing=Side*Stride*24,FootLift=FMath::Max(0.f,Side*Stride)*3;
                FQuat Leg=Pitch(Swing);FVector Hip(0,Side*7.5f,31+FootLift);
                Part(Cloth,Hip+Leg.RotateVector(FVector(0,0,-11)),FVector(12,12,23),Leg);
                Part(Cloth,Hip+Leg.RotateVector(FVector(3,0,-26)),FVector(19,16,9),Leg);
                if(LOD==0)Part(Iron,Hip+Leg.RotateVector(FVector(.4,0,-18)),FVector(15,14,6),Leg);
                float ArmAngle=Side==1?FMath::Lerp(Stride*-15-5,-60-Attack*37,Pose.Melee):Stride*10+4;
                FQuat Arm=Pitch(ArmAngle);FVector Shoulder=Torso+FVector(0,Side*18.f,60);
                Part(Light,Shoulder+Arm.RotateVector(FVector(0,Side*1.5f,-4)),FVector(21,13,13),Arm);
                Part(Shade,Shoulder+Arm.RotateVector(FVector(0,0,-14)),FVector(11,11,13),Arm);
                FVector Hand=Shoulder+Arm.RotateVector(FVector(0,0,-26));
                if(LOD<2)Part(Skin,Hand,FVector(12,11,12),Arm);
                if(Side==1)
                {
                    FQuat Sword=Arm*Pitch(-30);FVector Hilt=Hand+Arm.RotateVector(FVector(0,0,-3));
                    Part(SwordBlade,Hilt+Sword.RotateVector(FVector(0,0,-21)),FVector(2.6,7.8,34),Sword);
                    Part(Brass,Hilt,FVector(5,15,4),Sword);
                    if(LOD<2)
                    {
                        Part(Hair,Hilt+Sword.RotateVector(FVector(0,0,5)),FVector(5,5,8),Sword);
                        Part(Brass,Hilt+Sword.RotateVector(FVector(0,0,10)),FVector(6,6,3),Sword);
                        Part(SteelLight,Hilt+Sword.RotateVector(FVector(1.5,0,-17)),FVector(.9,2,23),Sword);
                    }
                }
                else
                {
                    FQuat Shield=Pitch(-17*Pose.Melee);FVector Center=Torso+FVector(FMath::Lerp(5.f,12.f,Pose.Melee),-24,39+Stride*.6f);
                    Part(ShieldRim,Center,FVector(6,28,36),Shield);
                    Part(ShieldColor,Center+Shield.RotateVector(FVector(4,0,0)),FVector(2.2,22,29.5),Shield);
                    if(LOD<2)Part(Linen,Center+Shield.RotateVector(FVector(5.6,0,1)),FVector(1.7,6.5,12),Shield);
                    if(LOD==0){Part(Hair,Center+Shield.RotateVector(FVector(-4,0,0)),FVector(3,4,18),Shield);Part(Iron,Center+Shield.RotateVector(FVector(-5,0,0)),FVector(2,15,4),Shield);}
                }
            }
        }
    }
    for(int I=0;I<TintCount;++I){SoldierBatches[I]->ClearInstances();SoldierBatches[I]->AddInstances(Instances[I],false,false,false);}
}

void AVoxelCommander::SetupSoldierPreview(int32 Stage)
{
#if !UE_BUILD_SHIPPING
    if(auto* B=Battle())
    {
        SelectedMode=1;StartBattle();SelectedMode=0;SoldierPreviewActive=true;SoldierPreviewStage=Stage%4;
        B->Towers.Empty();FVector Anchor=B->CellPosition(FIntPoint(-30,0));
        FVector Forward=FVector(3300,4000,0).GetSafeNormal(),Right(-Forward.Y,Forward.X,0);float FrontYaw=(-Forward).Rotation().Yaw;
        auto Army=[&](int Team,FVector Offset,float Yaw,float Troops)
        {
            FVoxelSquad S;S.ID=901+B->Squads.Num();S.Team=Team;S.Target=INDEX_NONE;S.Troops=Troops;
            S.Position=Anchor+Offset+FVector(0,0,27);S.Position.Z=B->Height(B->ToCell(S.Position))+27;
            S.VisualYaw=Yaw;S.Destination=B->ToCell(S.Position);S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;S.RetargetIn=100;
            B->Squads.Add(S);
        };
        if(SoldierPreviewStage==0)
        {
            Army(1,Right*-140+Forward*125,FrontYaw+20,4);Army(1,Right*140+Forward*125,FrontYaw+180,4);
            Army(2,Right*-140-Forward*125,FrontYaw-25,4);Army(2,Right*140-Forward*125,FrontYaw+100,4);
        }
        else if(SoldierPreviewStage==1)
        {
            Army(1,Right*-200-Forward*130,FrontYaw,48);Army(2,Right*200-Forward*130,FrontYaw,48);
            for(auto& S:B->Squads){S.Holding=false;S.Route.Add(S.Position-Forward*850);S.Destination=B->ToCell(S.Route.Last());}
            for(int I=0;I<4;++I)B->AdvanceBattle(.05f);
        }
        else if(SoldierPreviewStage==2)
        {
            Army(1,Right*-45,Right.Rotation().Yaw,48);Army(2,Right*45,(-Right).Rotation().Yaw,48);
            for(int I=0;I<5;++I)B->AdvanceBattle(.05f);
        }
        else{Army(1,Right*-200,FrontYaw+15,48);Army(2,Right*200,FrontYaw-15,48);}
        Focus=Anchor+Right*95+FVector(0,0,40);Zoom=SoldierPreviewStage==0?850:1050;
        B->RebuildTerrain();B->Paused=true;B->Message(TEXT("士兵预览：F8 查看多角度、行军、遭遇战与编队；空格播放 / 暂停，Esc 返回菜单。"));
    }
#endif
}
void AVoxelCommander::PreviewSoldierKey()
{
#if !UE_BUILD_SHIPPING
    int Stage=0;if(FParse::Value(FCommandLine::Get(),TEXT("SoldierPreview="),Stage))SetupSoldierPreview((SoldierPreviewStage+1)%4);
#endif
}
