#include "VoxelWar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
enum { RockDark,RockLight,Molten,FlameOrange,FlameGold,Flash,Smoke,Ember,Shockwave,Dust,Soot };
const FVector Approach(-680,460,1900);
}

void AVoxelBattle::InitializeMeteorVisuals()
{
    UStaticMesh* Rock=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Voxel/Effects/SM_MeteorRock.SM_MeteorRock"));
    UStaticMesh* Ring=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Voxel/Effects/SM_MeteorRing.SM_MeteorRing"));
    UStaticMesh* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    UMaterialInterface* Lit=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_Voxel.M_Voxel"));
    UMaterialInterface* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorGlow.M_MeteorGlow"));
    UMaterialInterface* Fire=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorFire.M_MeteorFire"));
    UMaterialInterface* Fog=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorSmoke.M_MeteorSmoke"));
    if(!Rock)Rock=Sphere;if(!Ring)Ring=Sphere;if(!Glow)Glow=Lit;if(!Fire)Fire=Lit;if(!Fog)Fog=Lit;
    const FLinearColor Colors[]={FLinearColor(.055f,.040f,.029f),FLinearColor(.15f,.09f,.05f),FLinearColor(5,.38f,.012f),
        FLinearColor(5.5f,.65f,.015f),FLinearColor(6,2.3f,.04f),FLinearColor(9,6,2),FLinearColor(.08f,.072f,.06f),
        FLinearColor(7,.5f,.008f),FLinearColor(4,1.7f,.15f),FLinearColor(.30f,.245f,.18f),FLinearColor(.018f,.015f,.012f)};
    for(int I=0;I<UE_ARRAY_COUNT(Colors);++I)
    {
        auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);
        Batch->SetupAttachment(RootComponent);Batch->SetMobility(EComponentMobility::Movable);
        Batch->SetStaticMesh(I<=Molten?Rock:I==Ember?Cube.Get():I==Shockwave?Ring:Sphere);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);Batch->SetCastShadow(I<=RockLight);Batch->bReceivesDecals=false;
        Batch->SetNumCustomDataFloats(1);
        auto* MID=UMaterialInstanceDynamic::Create(I<=RockLight?Lit:I==Molten?Glow:I==Smoke||I==Dust||I==Soot?Fog:Fire,this);
        MID->SetVectorParameterValue(TEXT("Tint"),Colors[I]);Batch->SetMaterial(0,MID);Batch->RegisterComponent();
        MeteorMaterials.Add(MID);MeteorBatches.Add(Batch);
    }
}
void AVoxelBattle::MeteorParticle(int32 Slot,FVector P,FVector Dimensions,FQuat Rotation,float Fade)
{
    if(!MeteorBatches.IsValidIndex(Slot)||Fade<=.001f||Dimensions.GetMin()<=.01f)return;
    auto* Batch=MeteorBatches[Slot].Get();int I=Batch->AddInstance(FTransform(Rotation,P,Dimensions/100.f));
    Batch->SetCustomDataValue(I,0,FMath::Clamp(Fade,0.f,1.f),false);
}
void AVoxelBattle::DrawMeteorVisuals()
{
    if(MeteorBatches.IsEmpty())return;
    for(const auto& Batch:MeteorBatches)Batch->ClearInstances();
    const FVector Tail=Approach.GetSafeNormal(),Side=FVector::CrossProduct(Tail,FVector::UpVector).GetSafeNormal(),Across=FVector::CrossProduct(Tail,Side).GetSafeNormal();
    const FQuat FlameRotation=FQuat::FindBetweenNormals(FVector::UpVector,Tail);
    const float Interpolation=Accumulator;
    for(const auto& E:Effects)if(E.Meteor)
    {
        float Remaining=FMath::Max(0.f,E.Remaining-Interpolation),Age=FMath::Clamp(1.5f-Remaining,0.f,1.5f);
        FVector Core=VoxelRules::MeteorPosition(E.Position,Remaining);
        FQuat Spin=FRotator(Age*107,Age*74,Age*59).Quaternion();
        // The opaque faceted boulder remains visible between its hot seams and flames.
        MeteorParticle(RockDark,Core,FVector(183,156,170),Spin);
        FRandomStream Random(615);
        for(int I=0;I<6;++I)
        {
            FVector N=Random.VRand();float Size=Random.FRandRange(53,84);
            MeteorParticle(I%2?RockDark:RockLight,Core+Spin.RotateVector(N*57),FVector(Size,Size*.85f,Size*.9f),Spin*Random.VRand().Rotation().Quaternion());
        }
        for(int I=0;I<15;++I)
        {
            FVector N=Random.VRand();FVector Surface=Spin.RotateVector(N*80);
            MeteorParticle(Molten,Core+Surface,FVector(13,34+I%3*9,11),Spin*N.Rotation().Quaternion());
        }
        for(int I=0;I<5;++I)
        {
            float Angle=I*2*PI/5+Age*3;
            FVector Offset=Tail*48+(Side*FMath::Cos(Angle)+Across*FMath::Sin(Angle))*68;
            MeteorParticle(FlameOrange,Core+Offset,FVector(78,70,155+FMath::Sin(Age*24+I)*24),FlameRotation,.42f);
        }
        // Sample the previous flight path for a continuous tail rather than an attached cube.
        for(int I=1;I<=23;++I)
        {
            float Delay=I*.021f;if(Delay>Age)break;
            FVector Trail=VoxelRules::MeteorPosition(E.Position,FMath::Min(1.5f,Remaining+Delay));
            Trail+=Side*FMath::Sin(Age*27-I*.9f)*(8+I*.65f)+Across*FMath::Cos(Age*21+I)*10;
            float Size=FMath::Lerp(113.f,19.f,I/23.f),Fade=FMath::Square(1-I/25.f)*.70f;
            MeteorParticle(I<7?FlameGold:FlameOrange,Trail,FVector(Size,Size,Size*1.5f),FlameRotation,Fade);
        }
        for(int I=0;I<13;++I)
        {
            float Delay=.12f+I*.071f;if(Delay>Age)break;
            FVector Trail=VoxelRules::MeteorPosition(E.Position,FMath::Min(1.5f,Remaining+Delay));
            Trail+=Side*FMath::Sin(I*2.3f)*32+FVector(Delay*28,Delay*13,Delay*70);
            float Size=85+Delay*100;
            MeteorParticle(Smoke,Trail,FVector(Size,Size*.9f,Size*1.3f),FQuat::Identity,(1-I/15.f)*.15f);
        }
        for(int I=0;I<25;++I)
        {
            float Life=FMath::Fmod(Age+I*.037f,.65f),Angle=I*2.39996f;
            FVector Drift=(Side*FMath::Cos(Angle)+Across*FMath::Sin(Angle))*(15+Life*(90+I*3));
            MeteorParticle(Ember,Core+Tail*(Life*470)+Drift,FVector(4,4,13),FlameRotation,(1-Life/.65f)*.8f);
        }
        MeteorParticle(Shockwave,E.Position+FVector(0,0,9),FVector(E.Radius*2,E.Radius*2,1),FQuat::Identity,.34f+.16f*FMath::Sin(Age*13));
        MeteorParticle(Soot,E.Position+FVector(0,0,5),FVector(180,160,4),FQuat::Identity,Age/1.5f*.26f);
    }
    for(const auto& Burst:MeteorBursts)
    {
        float Age=FMath::Min(MeteorBurstLifetime,Burst.Age+Interpolation),Life=1-Age/MeteorBurstLifetime;
        FVector Origin=Burst.Position;FRandomStream Random(927);
        if(Age<.13f)
        {
            float Scale=1+Age*3;
            MeteorParticle(Flash,Origin+FVector(0,0,75),FVector(420*Scale,420*Scale,230*Scale),FQuat::Identity,(1-Age/.13f)*.7f);
        }
        if(Age<.85f)
        {
            float Radius=Burst.Radius*(.15f+1.1f*FMath::Pow(Age/.85f,.65f));
            MeteorParticle(Shockwave,Origin+FVector(0,0,18),FVector(Radius*2,Radius*2,1),FQuat::Identity,1-Age/.85f);
            for(int I=0;I<8;++I)
            {
                float A=I*2*PI/8,R=40+Age*160,Size=100+FMath::Sin(FMath::Clamp(Age/.85f,0.f,1.f)*PI)*125;
                FVector Offset(FMath::Cos(A)*R,FMath::Sin(A)*R,60+Age*115);
                MeteorParticle(I%2?FlameOrange:FlameGold,Origin+Offset,FVector(Size,Size,Size*1.25f),FQuat::Identity,FMath::Square(1-Age/.85f)*.48f);
            }
        }
        for(int I=0;I<28;++I)
        {
            float Angle=Random.FRandRange(0,2*PI),Speed=Random.FRandRange(160,410),Up=Random.FRandRange(230,650),Size=Random.FRandRange(12,37);
            float Flight=FMath::Min(Age,(Up+FMath::Sqrt(Up*Up+2*900*40))/900);
            FVector P=Origin+FVector(FMath::Cos(Angle)*Speed*Flight,FMath::Sin(Angle)*Speed*Flight,40+Up*Flight-450*Flight*Flight);
            P.Z=FMath::Max(P.Z,Height(ToCell(P))+Size*.5f+2);
            float Shrink=FMath::Clamp(Life/.27f,0.f,1.f);
            MeteorParticle(I%3?RockDark:RockLight,P,FVector(Size*Shrink),FRotator(Age*I*17,Angle*57,Age*143).Quaternion());
            if(Age<.60f)MeteorParticle(Ember,P+FVector(0,0,6),FVector(9,9,15),FQuat::Identity,(1-Age/.60f)*.8f);
        }
        for(int I=0;I<15;++I)
        {
            float Angle=I*2.39996f,R=(55+Age*97)*(1+(I%3)*.12f),Size=80+Age*(68+I%4*14);
            FVector P=Origin+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,65+Age*(92+I%5*17));
            float Fade=FMath::Min(1.f,Age/.18f)*Life;
            MeteorParticle(Smoke,P,FVector(Size,Size*.9f,Size*1.15f),FQuat::Identity,Fade*.18f);
        }
        if(Age<1.8f)for(int I=0;I<18;++I)
        {
            float Angle=I*2*PI/18,R=70+Age*300,Size=60+Age*80;
            FVector P=Origin+FVector(FMath::Cos(Angle)*R,FMath::Sin(Angle)*R,26+Age*24);
            P.Z=Height(ToCell(P))+25+Age*16;
            MeteorParticle(Dust,P,FVector(Size,Size,Size*.55f),FQuat::Identity,(1-Age/1.8f)*.13f);
        }
        MeteorParticle(Soot,Origin+FVector(0,0,4),FVector(340,310,5),FQuat::Identity,Life*.30f);
    }
    for(const auto& Batch:MeteorBatches)Batch->MarkRenderStateDirty();
}

void AVoxelCommander::SetupMeteorPreview(float Age)
{
#if !UE_BUILD_SHIPPING
    if(auto* B=Battle())
    {
        SelectedMode=1;StartBattle();SelectedMode=0;MeteorPreviewActive=true;Focus=FVector(-400,500,0);Zoom=5400;
        FVoxelBridge Bridge;Bridge.Remaining=0;B->Bridges.Add(Bridge);
        for(int Team=1;Team<=2;++Team)
        {
            FVoxelSquad S;S.ID=100+Team;S.Team=Team;S.Troops=50;S.Target=INDEX_NONE;
            S.Destination=FIntPoint(Team==1?-1:1,0);S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;B->Squads.Add(S);
        }
        B->Gold[1]=55;B->Meteor(B->CellPosition(FIntPoint(0,0)));
        float Target=FMath::Clamp(Age,.05f,4.2f);
        for(float Advanced=0;Advanced+SMALL_NUMBER<Target;Advanced+=.05f)B->AdvanceBattle(.05f);
        B->Paused=true;B->Message(TEXT("陨石视觉预览：空格播放 / 暂停，F8 切换关键帧，主菜单返回正常游戏。"));
    }
#endif
}
void AVoxelCommander::PreviewMeteorKey()
{
#if !UE_BUILD_SHIPPING
    float Requested=0;if(!FParse::Value(FCommandLine::Get(),TEXT("MeteorPreview="),Requested))return;
    if(auto* B=Battle())SetupMeteorPreview(B->Time<1.5f?1.65f:B->Time<1.9f?2.15f:B->Time<3.f?3.6f:.65f);
#endif
}
