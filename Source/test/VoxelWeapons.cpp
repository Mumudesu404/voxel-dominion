#include "VoxelWar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
enum { Wood,WoodLight,Iron,Edge,Black,Brass,Blue,Red,Neutral,Feather,Poison,Hot,GoldGlow,WhiteGlow,Smoke,Spark,IronRound,EdgeRound,BrassRound,BlackRound,HotRound,ArrowTip,PierceBlue,PierceWhite,PierceHalo,PierceSpark };
FQuat WeaponAim(FVector Delta)
{
    FRotator Aim=Delta.Rotation();Aim.Pitch=FMath::Clamp(Aim.Pitch,-12.f,20.f);return Aim.Quaternion();
}
}
FVector AVoxelBattle::WeaponPivot(int32 I) const
{
    const auto& T=Towers[I];float StoneHeight=T.Tall?230.f:T.Module==EVoxelModule::Cannon?180.f:158.f;
    return CellPosition(T.Cell)+FVector(0,0,StoneHeight+72);
}
void AVoxelBattle::RecordWeaponShot(int32 I,FVector Target)
{
    const auto& T=Towers[I];FVector Pivot=WeaponPivot(I);FQuat Aim=WeaponAim(Target-Pivot);
    FVoxelWeaponShot Shot;Shot.Tower=I;Shot.Module=T.Module;Shot.Arrow=T.Arrow;
    Shot.From=Pivot+Aim.RotateVector(T.Module==EVoxelModule::Cannon?FVector(202,0,57):FVector(139,0,50));Shot.To=Target+FVector(0,0,15);
    bool Piercing=T.Module==EVoxelModule::Arrow&&T.Arrow==EVoxelArrow::Pierce;
    if(Piercing)
    {
        // Match the existing line attack: carry the visual past the last enemy
        // on that line, rather than stopping the light at the nearest target.
        FVector Origin=CellPosition(T.Cell),Direction=(Target-Origin).GetSafeNormal2D();
        float Furthest=FVector::Dist2D(Target,Origin),EndHeight=Target.Z+15;
        for(const auto& S:Squads)if(S.Team!=T.Team)
        {
            float Along=FVector::DotProduct(S.Position-Origin,Direction);
            if(Along>Furthest&&Along<TowerRange(I,S.Position)&&FVector::Dist2D(Origin+Direction*Along,S.Position)<75&&Visible(Origin,S.Position))
            {Furthest=Along;EndHeight=S.Position.Z+15;}
        }
        Shot.To=Origin+Direction*FMath::Min(Furthest+85,TowerRange(I,Target));Shot.To.Z=EndHeight;
    }
    Shot.FlightTime=FMath::Clamp(float(FVector::Distance(Shot.From,Shot.To)/(T.Module==EVoxelModule::Cannon||Piercing?4200:2100)),Piercing?.12f:.14f,Piercing?.34f:T.Module==EVoxelModule::Cannon?.28f:.65f);
    WeaponShots.Add(Shot);
    PlayTowerAudio(I);
}
void AVoxelBattle::InitializeWeaponVisuals()
{
    if(!Cube)Cube=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    auto* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    auto* Sphere=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    auto* Cone=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cone.Cone"));
    auto* Lit=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_Voxel.M_Voxel"));
    auto* Glow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorGlow.M_MeteorGlow"));
    auto* Fog=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorSmoke.M_MeteorSmoke"));
    auto* SoftGlow=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_MeteorFire.M_MeteorFire"));
    const FLinearColor Colors[]={
        {.29f,.12f,.04f},{.48f,.255f,.085f},{.105f,.12f,.12f},{.27f,.29f,.27f},{.018f,.022f,.019f},
        {.80f,.47f,.075f},{.025f,.28f,.76f},{.72f,.065f,.025f},{.63f,.44f,.12f},{.85f,.81f,.65f},{.20f,.70f,.055f},
        {3.8f,.23f,.008f},{6,2.1f,.055f},{8,6,2},{.58f,.60f,.56f},{6,1.3f,.04f},
        {.10f,.115f,.115f},{.27f,.29f,.27f},{.80f,.47f,.075f},{.008f,.012f,.01f},{1.25f,.042f,.004f},{.93f,.61f,.09f},
        {.18f,1.1f,3.f},{2.4f,2.8f,3.2f},{.10f,.65f,1.8f},{.55f,1.6f,2.8f}};
    for(int I=0;I<UE_ARRAY_COUNT(Colors);++I)
    {
        auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);Batch->SetupAttachment(RootComponent);Batch->SetMobility(EComponentMobility::Movable);
        bool Round=I>=IronRound&&I<=HotRound;
        Batch->SetStaticMesh(I==Smoke?Sphere:I==ArrowTip?Cone:Round?Cylinder:Cube.Get());
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);Batch->SetCastShadow(I<Hot||(Round&&I!=HotRound)||I==ArrowTip);Batch->bReceivesDecals=false;Batch->SetNumCustomDataFloats(1);
        bool Emissive=I==Hot||I==GoldGlow||I==WhiteGlow||I==Spark||I==HotRound||I>=PierceBlue;
        auto* MID=UMaterialInstanceDynamic::Create(I==Smoke?Fog:(I==PierceHalo||I==PierceBlue)?SoftGlow:Emissive?Glow:Lit,this);MID->SetVectorParameterValue(TEXT("Tint"),Colors[I]);
        Batch->SetMaterial(0,MID);Batch->RegisterComponent();WeaponMaterials.Add(MID);WeaponBatches.Add(Batch);
    }
}
void AVoxelBattle::DrawWeaponVisuals()
{
    if(WeaponBatches.IsEmpty())return;
    for(const auto& Batch:WeaponBatches)Batch->ClearInstances();
    auto Particle=[&](int Slot,FVector P,FVector Size,FQuat Q=FQuat::Identity,float Fade=1.f)
    {
        if(Fade<=.001f||Size.GetMin()<=.01f)return;
        auto* Batch=WeaponBatches[Slot].Get();int N=Batch->AddInstance(FTransform(Q,P,Size/100.f));Batch->SetCustomDataValue(N,0,FMath::Clamp(Fade,0.f,1.f),false);
    };
    auto Beam=[&](int Slot,FVector A,FVector End,float Width,float Depth=0.f,float Fade=1.f)
    {FVector D=End-A;Particle(Slot,(A+End)*.5f,FVector(Width,Depth>0?Depth:Width,D.Size()),FQuat::FindBetweenNormals(FVector::UpVector,D.GetSafeNormal()),Fade);};
    auto Bolt=[&](FVector Tip,FVector Direction,EVoxelArrow Type,float Scale=1.f)
    {
        FQuat Q=Direction.Rotation().Quaternion();FVector Back=Tip-Direction*(50*Scale);
        Particle(WoodLight,Back,FVector(88,4,4)*Scale,Q);
        Particle(Type==EVoxelArrow::Poison?Poison:Brass,Tip-Direction*12*Scale,FVector(23,10,10)*Scale,Q);
        Particle(ArrowTip,Tip-Direction*5*Scale,FVector(17,17,29)*Scale,FQuat::FindBetweenNormals(FVector::UpVector,Direction));
        for(int K=0;K<3;++K)
        {
            FQuat Fin=Q*FQuat(FVector::ForwardVector,K*2*PI/3);
            Particle(Feather,Tip+Q.RotateVector(FVector(-80,0,0)*Scale)+Fin.RotateVector(FVector(0,7,0)*Scale),FVector(26,15,2)*Scale,Fin);
        }
    };
    for(int I=0;I<Towers.Num();++I)
    {
        const auto& T=Towers[I];if(T.Module!=EVoxelModule::Arrow&&T.Module!=EVoxelModule::Cannon)continue;
        FVector Pivot=WeaponPivot(I),Target=T.AimTarget;int Tracked=SquadIndex(T.AimSquad);
        if(Tracked!=INDEX_NONE&&Squads[Tracked].Team!=T.Team)Target=Squads[Tracked].Position;
        FQuat Aim=T.HasAim?WeaponAim(Target-Pivot):FRotator(0,T.Team==2?145:-35,0).Quaternion();
        FQuat Yaw=FRotator(0,Aim.Rotator().Yaw,0).Quaternion();float ShotAge=T.ShotAge+Accumulator;
        int Team=T.Team==1?Blue:T.Team==2?Red:Neutral;
        auto Part=[&](int Slot,FVector P,FVector Size,FQuat Local=FQuat::Identity,bool Level=false,float Fade=1.f)
        {FQuat Q=Level?Yaw:Aim;Particle(Slot,Pivot+Q.RotateVector(P),Size,Q*Local,Fade);};
        auto Rod=[&](int Slot,FVector A,FVector End,float Width,float Depth=0.f)
        {Beam(Slot,Pivot+Aim.RotateVector(A),Pivot+Aim.RotateVector(End),Width,Depth);};
        Part(EdgeRound,FVector(0,0,-5),T.Module==EVoxelModule::Cannon?FVector(129,129,21):FVector(84,84,18),FQuat::Identity,true);
        if(T.Module==EVoxelModule::Arrow)
        {
            float Kick=ShotAge<.22f?FMath::Sin(ShotAge/.22f*PI)*10:0;
            Part(Wood,FVector(-Kick,0,20),FVector(28,31,44));
            Part(BrassRound,FVector(0,-19,22),FVector(15,15,8),FQuat(FVector::ForwardVector,PI*.5));
            Part(WoodLight,FVector(25-Kick,0,47),FVector(151,29,21));Part(Iron,FVector(-38-Kick,0,47),FVector(17,36,27));
            FVector L(39,-112,51),L1(64,-65,51),Center(69,0,51),R1(64,65,51),R(39,112,51);
            Rod(WoodLight,L,L1,18,17);Rod(Wood,L1,Center,19,18);Rod(Wood,Center,R1,19,18);Rod(WoodLight,R1,R,18,17);
            for(FVector P:{L,R})Part(Iron,P,FVector(27,23,25));
            float StringX=ShotAge<.08f?FMath::Lerp(-41.f,55.f,ShotAge/.08f):FMath::Lerp(55.f,-41.f,FMath::Clamp((ShotAge-.08f)/.35f,0.f,1.f));
            Rod(Feather,L,FVector(StringX,0,52),2.3f);Rod(Feather,FVector(StringX,0,52),R,2.3f);
            Rod(WoodLight,FVector(-30,0,31),FVector(48,0,62),7);
            if(ShotAge>.32f)Bolt(Pivot+Aim.RotateVector(FVector(141,0,55)),Aim.GetForwardVector(),T.Arrow,.92f);
        }
        else
        {
            float Kick=ShotAge<.16f?FMath::Sin(ShotAge/.16f*PI)*7:0;
            Part(Team,FVector(0,0,10),FVector(97,84,20),FQuat::Identity,true);
            Part(IronRound,FVector(0,0,27),FVector(80,80,17),FQuat::Identity,true);
            Part(Team,FVector(-18-Kick,0,53),FVector(80,83,77));Part(Team,FVector(-31-Kick,-63,42),FVector(52,46,57));
            Part(Edge,FVector(-31-Kick,-88,42),FVector(45,5,48));Part(Team,FVector(-31-Kick,-92,42),FVector(36,4,38));
            for(int S:{-1,1})
            {
                Part(Team,FVector(13-Kick,S*45,24),FVector(88,18,24));
                Part(BrassRound,FVector(-3-Kick,S*52,57),FVector(34,34,14),FQuat(FVector::ForwardVector,PI*.5));
            }
            for(int K=0;K<5;++K)Part(Edge,FVector(-61-Kick,0,37+K*12),FVector(13,72,6));
            Part(Brass,FVector(38,0,12),FVector(62,61,8));
            float Spin=T.BarrelAngle+T.SpinSpeed*Accumulator,Heat=T.Heat/100.f;
            FQuat Axis=FQuat::FindBetweenNormals(FVector::UpVector,FVector::ForwardVector);
            for(int K=0;K<6;++K)
            {
                float A=K*PI/3+Spin*PI/180;FVector Offset(0,FMath::Cos(A)*24,FMath::Sin(A)*24);
                int Barrel=T.Overheated?HotRound:IronRound;
                Part(Barrel,FVector(112-Kick,0,57)+Offset,FVector(15,15,169),Axis,false,T.Overheated?.55f+.45f*Heat:1.f);
                if(Heat>.45f)Part(Hot,FVector(156-Kick,0,57)+Offset+FVector(0,0,1),FVector(69,3,3),FQuat::Identity,false,(Heat-.45f)/.55f);
                Part(BlackRound,FVector(199-Kick,0,57)+Offset,FVector(11.5f,11.5f,2),Axis);
                for(int J=0;J<8;++J)
                {
                    float B=J*PI/4;FVector Ring(200-Kick,Offset.Y+FMath::Cos(B)*8,57+Offset.Z+FMath::Sin(B)*8);
                    Part(T.Overheated?Hot:Edge,Ring,FVector(5,4,7),FQuat(FVector::ForwardVector,B));
                }
            }
            for(float X:{89.f,185.f})for(int K=0;K<10;++K)
            {
                float A=K*PI/5+Spin*PI/180;
                Part(T.Overheated?Hot:Edge,FVector(X-Kick,FMath::Cos(A)*36,57+FMath::Sin(A)*36),FVector(15,12,25),FQuat(FVector::ForwardVector,A));
            }
            FVector Muzzle=Pivot+Aim.RotateVector(FVector(204-Kick,0,57));
            if(ShotAge<.15f)
            {
                float Pulse=.7f+.3f*FMath::Sin(ShotAge*135);
                Part(GoldGlow,FVector(228,0,57),FVector(55*Pulse,21,21));Part(WhiteGlow,FVector(217,0,57),FVector(29,11,11));
                Part(GoldGlow,FVector(214,0,57),FVector(8,43*Pulse,7));Part(GoldGlow,FVector(214,0,57),FVector(8,7,43*Pulse));
            }
            if(Heat>.4f)for(int K=0;K<8;++K)
            {
                float Life=FMath::Fmod((Time+Accumulator)*(.65f+K*.015f)+K*.139f,1.f),Size=24+Life*60;
                FVector P=Muzzle+FVector(FMath::Sin(K*2.4f+Life*2)*20,FMath::Cos(K+Life*3)*16,26+Life*170);
                Particle(Smoke,P,FVector(Size,Size,Size*1.5f),FQuat::Identity,(Heat-.4f)/.6f*(1-Life)*.34f);
            }
        }
    }
    for(const auto& Shot:WeaponShots)
    {
        float Age=Shot.Age+Accumulator;FVector Direction=(Shot.To-Shot.From).GetSafeNormal();
        if(Shot.Module==EVoxelModule::Arrow)
        {
            if(Shot.Arrow==EVoxelArrow::Pierce)
            {
                float Head=FMath::Clamp(Age/Shot.FlightTime,0.f,1.f),Tail=FMath::Clamp((Age-.16f)/Shot.FlightTime,0.f,1.f);
                float Fade=1-FMath::Clamp((Age-Shot.FlightTime)/.20f,0.f,1.f);
                FVector P=FMath::Lerp(Shot.From,Shot.To,Head),Back=FMath::Lerp(Shot.From,Shot.To,Tail);
                if(Fade>0&&Head>Tail)
                {
                    Beam(PierceHalo,Back,P,11,0,Fade*.23f);
                    Beam(PierceBlue,Back,P,4.8f,0,Fade*.85f);
                    Beam(PierceWhite,Back,P,1.7f,0,Fade);
                    Beam(PierceBlue,Shot.From,P,1.15f,0,Fade*.20f);
                    if(Head<1)Beam(PierceWhite,P-Direction*15,P+Direction*4,3,0,Fade);
                }
                FVector Side=FVector::CrossProduct(Direction,FVector::UpVector).GetSafeNormal();
                FVector Up=FVector::CrossProduct(Side,Direction).GetSafeNormal();
                // A few pinpricks drift off the wake; no large burst or cloud.
                for(int K=0;K<8;++K)
                {
                    float Birth=.015f+K*.025f,Life=Age-Birth;if(Life<0||Life>.24f||Birth>Shot.FlightTime)continue;
                    float Angle=K*2.4f+Shot.Tower*.73f,Decay=1-Life/.24f;
                    FVector Drift=Side*FMath::Cos(Angle)+Up*FMath::Sin(Angle);
                    FVector SparkPosition=FMath::Lerp(Shot.From,Shot.To,FMath::Clamp(Birth/Shot.FlightTime,0.f,1.f))+Drift*(3+Life*55)+FVector(0,0,-Life*Life*30);
                    float Size=(K%3==0?7.f:4.f)*Decay;
                    Beam(PierceSpark,SparkPosition-Drift*Size*.5f,SparkPosition+Drift*Size*.5f,.9f,0,Decay*.8f);
                    if(K%3==0)Beam(PierceWhite,SparkPosition-Up*Size*.35f,SparkPosition+Up*Size*.35f,.8f,0,Decay*.65f);
                }
                continue;
            }
            float T=FMath::Clamp(Age/Shot.FlightTime,0.f,1.f);
            if(T<1)
            {
                FVector P=FMath::Lerp(Shot.From,Shot.To,T)+FVector(0,0,160*T*(1-T));
                FVector D=(Shot.To-Shot.From+FVector(0,0,160*(1-2*T))).GetSafeNormal();Bolt(P,D,Shot.Arrow);
                Beam(Shot.Arrow==EVoxelArrow::Poison?Poison:Feather,P-D*135,P-D*67,2);
            }
        }
        else for(int K=0;K<3;++K)
        {
            float Flight=Age-K*.045f;if(Flight<0)continue;
            if(Flight<Shot.FlightTime)
            {
                FVector P=FMath::Lerp(Shot.From,Shot.To,Flight/Shot.FlightTime);
                Beam(GoldGlow,P-Direction*62,P,4);Beam(WhiteGlow,P-Direction*26,P,1.7f);
            }
            if(Flight<.48f)
            {
                FVector Side=FVector::CrossProduct(Direction,FVector::UpVector).GetSafeNormal();
                FVector P=Shot.From-Direction*185+Side*(42+Flight*(125+K*24))+FVector(0,0,35+Flight*125-380*Flight*Flight);
                float Scale=FMath::Clamp((.48f-Flight)/.1f,0.f,1.f);
                Particle(BrassRound,P,FVector(5,5,13)*Scale,FRotator(Flight*480,K*71+Flight*180,Flight*350).Quaternion());
            }
        }
        float Impact=Age-Shot.FlightTime;
        if(Impact>=0&&Impact<.16f)for(int K=0;K<4;++K)
        {
            FVector D=FVector(FMath::Cos(K*2.4f),FMath::Sin(K*2.4f),.5f).GetSafeNormal();
            Particle(Shot.Arrow==EVoxelArrow::Poison?Poison:Spark,Shot.To+D*(Impact*200),FVector(4,4,10)*(1-Impact/.16f),D.Rotation().Quaternion());
        }
    }
    for(const auto& Batch:WeaponBatches)Batch->MarkRenderStateDirty();
}

void AVoxelCommander::SetupWeaponPreview(int32 Stage)
{
#if !UE_BUILD_SHIPPING
    if(auto* B=Battle())
    {
        SelectedMode=1;StartBattle();SelectedMode=0;WeaponPreviewActive=true;WeaponPreviewStage=Stage%4;
        B->Towers.Empty();
        for(int I=0;I<2;++I)
        {
            FVoxelTower T;T.Name=I==0?TEXT("床弩箭塔"):TEXT("六管机炮塔");T.Team=I+1;T.Module=I==0?EVoxelModule::Arrow:EVoxelModule::Cannon;
            T.Cell=FIntPoint(I==0?-30:-24,0);T.Tall=I==0;T.Arrow=I==0?EVoxelArrow::Pierce:EVoxelArrow::Unbound;T.Troops=100;
            FVoxelSquad S;S.ID=801+I;S.Team=I==0?2:1;S.Target=INDEX_NONE;S.Troops=250;S.Destination=T.Cell+FIntPoint(0,-4);
            S.Position=B->CellPosition(S.Destination)+FVector(0,0,27);S.LastLand=S.Destination;S.HasDestination=true;S.Holding=true;S.RetargetIn=100;B->Squads.Add(S);
            T.HasAim=true;T.AimTarget=S.Position;T.AimSquad=S.ID;B->Towers.Add(T);
        }
        B->RebuildTerrain();Focus=B->CellPosition(FIntPoint(-27,-1))+FVector(-100,430,0);Zoom=2400;
        if(WeaponPreviewStage==1)for(int K=0;K<3;++K)B->AdvanceBattle(.05f);
        if(WeaponPreviewStage>=2)
        {
            for(int K=0;K<160&&!B->Towers[1].Overheated;++K)B->AdvanceBattle(.05f);
            if(WeaponPreviewStage==2)for(int K=0;K<7;++K)B->AdvanceBattle(.05f);
            else{for(auto& T:B->Towers)T.FireTimer=100;for(int K=0;K<90;++K)B->AdvanceBattle(.05f);}
        }
        B->Paused=true;B->Message(TEXT("武器预览：空格播放 / 暂停，F8 切换待机、射击、过热与冷却。"));
    }
#endif
}
void AVoxelCommander::PreviewWeaponKey()
{
#if !UE_BUILD_SHIPPING
    int Stage=0;if(FParse::Value(FCommandLine::Get(),TEXT("WeaponPreview="),Stage))SetupWeaponPreview((WeaponPreviewStage+1)%4);
#endif
}
