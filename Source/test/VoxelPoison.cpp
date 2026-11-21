#include "VoxelWar.h"
#include "Camera/CameraActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
enum { PoolDark,PoolVenom,MistDark,MistLime,Bubble,Spore,Skull,Socket };
constexpr float PoisonLifetime=4.f;
}

void AVoxelBattle::InitializePoisonVisuals()
{
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Fog=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorSmoke.M_MeteorSmoke"));
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorGlow.M_MeteorGlow"));
    const FLinearColor Colors[]={
        {.035f,.085f,.009f},{.29f,.58f,.018f},{.11f,.38f,.025f},{.43f,.78f,.035f},
        {.40f,.95f,.035f},{.62f,1.3f,.055f},{.70f,1.25f,.17f},{.009f,.025f,.003f}};
    for(int I=0;I<UE_ARRAY_COUNT(Colors);++I)
    {
        auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);
        Batch->SetupAttachment(RootComponent);Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetStaticMesh(I>=Spore?Cube.Get():Sphere);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);Batch->SetCastShadow(false);Batch->bReceivesDecals=false;
        Batch->SetNumCustomDataFloats(1);
        auto* MID=UMaterialInstanceDynamic::Create(I>=Spore?Glow:Fog,this);
        MID->SetVectorParameterValue(TEXT("Tint"),Colors[I]);Batch->SetMaterial(0,MID);Batch->RegisterComponent();
        PoisonMaterials.Add(MID);PoisonBatches.Add(Batch);
    }
}

void AVoxelBattle::DrawPoisonVisuals()
{
    if(PoisonBatches.IsEmpty())return;
    for(const auto& Batch:PoisonBatches)Batch->ClearInstances();
    auto Particle=[&](int Slot,FVector P,FVector Size,float Fade,FQuat Rotation=FQuat::Identity)
    {
        if(Fade<=.001f||Size.GetMin()<=.01f)return;
        auto* Batch=PoisonBatches[Slot].Get();int I=Batch->AddInstance(FTransform(Rotation,P,Size/100.f));
        Batch->SetCustomDataValue(I,0,FMath::Clamp(Fade,0.f,1.f),false);
    };
    FQuat View=FRotator(-45,50,0).Quaternion();
    if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController());P&&P->BattleCamera)View=P->BattleCamera->GetActorQuat();
    FVector Right=View.GetRightVector(),Up=View.GetUpVector(),Toward=-View.GetForwardVector();
    FQuat Billboard=FRotationMatrix::MakeFromXZ(Right,Up).ToQuat();
    for(const auto& E:Effects)if(!E.Meteor&&E.Remaining>0)
    {
        // Use the simulation clock and its fractional step, never wall-clock time.
        float Age=FMath::Clamp(PoisonLifetime-E.Remaining+Accumulator,0.f,PoisonLifetime);
        float Fade=FMath::Min(1.f,Age/.15f)*FMath::Clamp((PoisonLifetime-Age)/.65f,0.f,1.f);
        if(Fade<=.001f)continue;
        float Spread=FMath::Lerp(.55f,1.f,FMath::Clamp(Age/.28f,0.f,1.f));
        FVector Origin=E.Position;Origin.Z=Height(ToCell(Origin))+8;
        FRandomStream Random(HashCombineFast(GetTypeHash(E.Position.X),GetTypeHash(E.Position.Y)));
        // A broken, ground-hugging stain follows the terrain rather than a neon circle.
        Particle(PoolDark,Origin,FVector(E.Radius*1.65f,E.Radius*1.45f,9)*Spread,Fade*.60f);
        for(int I=0;I<9;++I)
        {
            float Angle=Random.FRandRange(0,2*PI),R=Random.FRandRange(.15f,.67f)*E.Radius*Spread;
            FVector P=Origin+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);P.Z=Height(ToCell(P))+10+I*.35f;
            float Size=Random.FRandRange(58,104)*Spread;
            Particle(I%3?PoolVenom:PoolDark,P,FVector(Size,Size*.75f,7),Fade*.40f,FRotator(0,Angle*180/PI,0).Quaternion());
        }
        // Separate low translucent lobes leave soldiers and their colors readable.
        for(int I=0;I<12;++I)
        {
            float Phase=Random.FRandRange(0,2*PI),Angle=I*2.39996f+FMath::Sin(Age*1.3f+Phase)*.20f;
            float R=Random.FRandRange(.10f,.65f)*E.Radius*Spread;
            FVector P=Origin+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);
            P.Z=Height(ToCell(P))+37+(I%3)*19+FMath::Sin(Age*2.4f+Phase)*12;
            float Size=(78+(I%4)*17)*(1+.13f*FMath::Sin(Age*2+Phase))*Spread;
            Particle(I%3?MistDark:MistLime,P,FVector(Size,Size*.86f,Size*.65f),Fade*(I%3?.17f:.13f));
        }
        for(int I=0;I<5;++I)for(int K=0;K<3;++K)
        {
            float Angle=I*2.39996f+FMath::Sin(Age*1.9f+K*.7f+I)*.3f,R=E.Radius*(.20f+I*.09f);
            FVector P=Origin+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,0);
            P.Z=Height(ToCell(P))+65+K*29+FMath::Sin(Age*2.2f+I)*9;
            Particle(MistLime,P,FVector(44-K*7,38-K*6,57),Fade*(.13f-K*.025f));
        }
        // Bubbles swell, rise, and pop; a few sharp motes read as caustic spray.
        for(int I=0;I<13;++I)
        {
            float Phase=Random.FRand(),Cycle=FMath::Fmod(Age*(.55f+(I%4)*.07f)+Phase,1.f);
            float Angle=Random.FRandRange(0,2*PI),R=Random.FRandRange(.18f,.73f)*E.Radius;
            FVector P=Origin+FVector(FMath::Cos(Angle)*R+FMath::Sin(Age*2+I)*8,FMath::Sin(Angle)*R,0);
            P.Z=Height(ToCell(P))+19+Cycle*(80+(I%4)*22);
            float Pop=FMath::Clamp((1-Cycle)/.20f,0.f,1.f),Size=(8+(I%4)*3)*(1+Cycle*.7f);
            Particle(Bubble,P,FVector(Size),Fade*Pop*.64f);
            Particle(Spore,P+Toward*Size*.28f+Up*Size*.22f,FVector(2.4f,2.4f,3.4f)*Fade*Pop,1);
        }
        // A small camera-facing voxel skull and crossbones make the hazard explicit.
        float IconScale=1.2f*FMath::Min(1.f,Fade*1.8f);
        FVector Icon=Origin+FVector(0,0,218+FMath::Sin(Age*2.8f)*6);
        auto Mark=[&](int Slot,FVector P,FVector Size)
        {Particle(Slot,Icon+(Right*P.X+Toward*P.Y+Up*P.Z)*IconScale,Size*IconScale,1,Billboard);};
        auto Bone=[&](FVector2D A,FVector2D B)
        {
            FVector Start=Icon+(Right*A.X+Up*A.Y)*IconScale,End=Icon+(Right*B.X+Up*B.Y)*IconScale,D=End-Start;
            Particle(Skull,(Start+End)*.5f,FVector(5*IconScale,5*IconScale,D.Size()),1,FQuat::FindBetweenNormals(FVector::UpVector,D.GetSafeNormal()));
            Mark(Skull,FVector(A.X,0,A.Y),FVector(8,6,8));Mark(Skull,FVector(B.X,0,B.Y),FVector(8,6,8));
        };
        Bone(FVector2D(-23,-11),FVector2D(23,-34));Bone(FVector2D(-23,-34),FVector2D(23,-11));
        Mark(Skull,FVector(0,0,8),FVector(38,10,29));Mark(Skull,FVector(0,0,-8),FVector(25,10,17));
        for(int S:{-1,1})Mark(Socket,FVector(S*9,6,10),FVector(10,3,10));
        Mark(Socket,FVector(0,6,-1),FVector(5,3,6));
        for(int X:{-7,0,7})Mark(Socket,FVector(X,6,-12),FVector(3,3,7));
    }
    for(const auto& Batch:PoisonBatches)Batch->MarkRenderStateDirty();
}

void AVoxelCommander::SetupPoisonPreview(float Age)
{
#if !UE_BUILD_SHIPPING
    if(auto* B=Battle())
    {
        SelectedMode=1;StartBattle();SelectedMode=0;PoisonPreviewActive=true;
        // Exercise a real poison-arrow hit, then isolate its four-second aftermath.
        auto& Tower=B->Towers[2];Tower.Cell=FIntPoint(-11,4);Tower.Arrow=EVoxelArrow::Poison;
        Focus=B->CellPosition(FIntPoint(-9,4))+FVector(-190,220,0);Zoom=3200;
        FVoxelSquad S;S.ID=102;S.Team=2;S.Troops=60;S.Target=INDEX_NONE;S.Destination=FIntPoint(-8,4);
        S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;S.RetargetIn=100;B->Squads.Add(S);
        B->AdvanceBattle(.05f);
        for(auto& T:B->Towers)T.FireTimer=100;
        S.ID=101;S.Team=1;S.Destination=FIntPoint(-8,3);S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;B->Squads.Add(S);
        float Target=FMath::Clamp(Age,.05f,4.2f);
        for(float Advanced=.05f;Advanced+SMALL_NUMBER<Target;Advanced+=.05f)B->AdvanceBattle(.05f);
        B->Paused=true;B->Message(TEXT("毒箭效果预览：空格播放 / 暂停，F8 切换阶段，Esc 返回菜单。"));
    }
#endif
}
void AVoxelCommander::PreviewPoisonKey()
{
#if !UE_BUILD_SHIPPING
    float Requested=0;if(!FParse::Value(FCommandLine::Get(),TEXT("PoisonPreview="),Requested))return;
    if(auto* B=Battle())SetupPoisonPreview(B->Time<.4f?1.4f:B->Time<2.f?3.6f:B->Time<4.f?4.15f:.15f);
#endif
}
