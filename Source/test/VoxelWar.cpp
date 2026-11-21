#include "VoxelWar.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "CanvasItem.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

static const FLinearColor Blue(.07f,.28f,.62f), Red(.65f,.09f,.045f), Cream(.91f,.92f,.81f);
static int32 TeamColor(int32 Team) { return Team == 1 ? 0 : Team == 2 ? 1 : 2; }

AVoxelBattle::AVoxelBattle()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AVoxelBattle::BeginPlay()
{
    Super::BeginPlay();
    Cube = LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_Voxel.M_Voxel"));
    if (!Material) Material = LoadObject<UMaterialInterface>(nullptr,TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    const TArray<FLinearColor> Colors = { Blue, Red, FLinearColor(.53f,.46f,.33f), FLinearColor(.20f,.25f,.14f),
        FLinearColor(.28f,.42f,.13f), FLinearColor(.38f,.50f,.19f), FLinearColor(.24f,.12f,.045f),
        FLinearColor(.09f,.30f,.39f), FLinearColor(.53f,.25f,.095f), FLinearColor(.12f,.18f,.20f),
        FLinearColor(.78f,.69f,.48f), FLinearColor(.90f,.61f,.18f), FLinearColor(.30f,.84f,.45f) };
    for (int32 Pass=0;Pass<3;++Pass) for (int32 C=0;C<Colors.Num();++C)
    {
        auto* Batch = NewObject<UInstancedStaticMeshComponent>(this);
        Batch->SetupAttachment(RootComponent); Batch->SetStaticMesh(Cube);
        Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        auto* MID = UMaterialInstanceDynamic::Create(Material,this);
        MID->SetVectorParameterValue(TEXT("Tint"),Colors[C]);
        Batch->SetMaterial(0,MID); Batch->RegisterComponent();
        Palette.Add(MID); Batches.Add(Batch);
    }
    InitializeMeteorVisuals();
    InitializePoisonVisuals();
    InitializeWeaponVisuals();
    InitializeBattleAudio();
    InitializeRangeOverlay();
    InitializeSoldierVisuals();
    InitializeReferenceBuildings();
    InitializeLayout();
    if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))MenuPaused=P->IsMenuOpen();
    MakeMap(); DrawWorld();
    Message(TEXT("开局无桥、双方零金币。先占村庄发展，按 B 在河道选址建桥。"));
}

void AVoxelBattle::InitializeLayout()
{
    if(!Towers.IsEmpty())return;
    auto Add=[this](const TCHAR* Name,int X,int Y,int Team,EVoxelModule Module,float Troops,bool Tall=false)
    {
        FVoxelTower T;T.Name=Name;T.Cell=FIntPoint(X,Y);T.Team=Team;T.Module=Module;T.Troops=Troops;T.Tall=Tall;
        if(Module==EVoxelModule::Arrow&&Team==2)T.Arrow=EVoxelArrow::Poison;
        Towers.Add(T);
    };
    Add(TEXT("苍石主城"),-36,0,1,EVoxelModule::Command,95);
    Add(TEXT("西境炮堡"),-30,-6,1,EVoxelModule::Cannon,42);
    Add(TEXT("苍鹰箭楼"),-29,5,1,EVoxelModule::Arrow,38,true);
    Add(TEXT("赤岩主城"),36,0,2,EVoxelModule::Command,95);
    Add(TEXT("东境炮堡"),30,6,2,EVoxelModule::Cannon,42);
    Add(TEXT("赤羽箭楼"),29,-5,2,EVoxelModule::Arrow,38,true);
    const FIntPoint Villages[]={ {-26,-16},{-22,4},{-16,-19},{-10,19},{-7,-7} };
    const TCHAR* WestVillage[]={TEXT("松溪村"),TEXT("苔木村"),TEXT("牧野村"),TEXT("北岸粮仓"),TEXT("河湾村")};
    const TCHAR* EastVillage[]={TEXT("白桦村"),TEXT("麦穗村"),TEXT("石泉村"),TEXT("南岸粮仓"),TEXT("柳岸村")};
    for(int I=0;I<5;++I)
    {Add(WestVillage[I],Villages[I].X,Villages[I].Y,0,EVoxelModule::Village,0);Add(EastVillage[I],-Villages[I].X,-Villages[I].Y,0,EVoxelModule::Village,0);}
    const FIntPoint Arrows[]={ {-6,-16},{-5,-1},{-7,13},{-12,25},{-23,16},{-29,13} };
    for(int I=0;I<6;++I)
    {
        Add(*FString::Printf(TEXT("西线箭楼·%d"),I+1),Arrows[I].X,Arrows[I].Y,0,EVoxelModule::Arrow,0,true);
        Add(*FString::Printf(TEXT("东线箭楼·%d"),I+1),-Arrows[I].X,-Arrows[I].Y,0,EVoxelModule::Arrow,0,true);
    }
    const FIntPoint Cannons[]={ {-15,-14},{-23,-8},{-33,-11},{-11,7},{-16,14},{-2,21} };
    for(int I=0;I<6;++I)
    {
        Add(*FString::Printf(TEXT("西线炮堡·%d"),I+1),Cannons[I].X,Cannons[I].Y,0,EVoxelModule::Cannon,0);
        Add(*FString::Printf(TEXT("东线炮堡·%d"),I+1),-Cannons[I].X,-Cannons[I].Y,0,EVoxelModule::Cannon,0);
    }
}

bool AVoxelBattle::ContainsCell(FIntPoint C) const
{
    if(FMath::Abs(C.X)>MapHalfWidth()||FMath::Abs(C.Y)>MapHalfHeight())return false;
    if(Tutorial)return true;
    // One footprint is shared by terrain, routes, bridge sites and the minimap.
    // Cache it once so path searches do not repeatedly evaluate trigonometry.
    static const TArray<uint8> Footprint=[]()
    {
        TArray<uint8> Cells;Cells.SetNumZeroed(W*H);
        for(int Y=-H/2;Y<=H/2;++Y)for(int X=-W/2;X<=W/2;++X)
        {
            float NX=float(X)/(W/2),NY=float(Y)/(H/2),A=FMath::Atan2(NY,NX);
            float Edge=.90f+.045f*FMath::Sin(4*A+.7f)+.035f*FMath::Cos(6*A-.4f)+.020f*FMath::Sin(10*A+1.3f);
            Cells[(Y+H/2)*W+X+W/2]=NX*NX+NY*NY<=Edge*Edge;
        }
        return Cells;
    }();
    return Footprint[(C.Y+H/2)*W+C.X+W/2]!=0;
}
FVector2D AVoxelBattle::MapUV(FVector P) const
{return FVector2D(.5f+P.X/((MapHalfWidth()*2+1)*Tile),.5f+P.Y/((MapHalfHeight()*2+1)*Tile));}
FVector AVoxelBattle::FocusFromMapUV(FVector2D UV) const
{
    FVector P((FMath::Clamp(UV.X,0.,1.)-.5)*(MapHalfWidth()*2+1)*Tile,(FMath::Clamp(UV.Y,0.,1.)-.5)*(MapHalfHeight()*2+1)*Tile,0);
    if(ContainsCell(ToCell(P)))return P;
    float Best=FLT_MAX;FIntPoint Nearest=FIntPoint::ZeroValue;
    for(int Y=-MapHalfHeight();Y<=MapHalfHeight();++Y)for(int X=-MapHalfWidth();X<=MapHalfWidth();++X)
    {
        FIntPoint C(X,Y);if(!ContainsCell(C))continue;
        float D=FVector::DistSquared2D(P,FVector(X*Tile,Y*Tile,0));if(D<Best){Best=D;Nearest=C;}
    }
    return FVector(Nearest.X*Tile,Nearest.Y*Tile,0);
}
float AVoxelBattle::Height(FIntPoint C) const
{
    if(!ContainsCell(C))return -10;
    if(IsRiver(C)){int I=BridgeIndexAt(C);return I!=INDEX_NONE&&Bridges[I].Remaining<=0?Bridges[I].Elevation:70.f;}
    float Spread=Tutorial?1.f:1.7f,X=FMath::Abs(C.X)/Spread,Y=FMath::Abs(C.Y)/Spread;
    float Base=ContainsCell(C+FIntPoint(1,0))&&ContainsCell(C+FIntPoint(-1,0))&&ContainsCell(C+FIntPoint(0,1))&&ContainsCell(C+FIntPoint(0,-1))?70.f:46.f;
    float Z=Base+28*FMath::Max(0.f,4-FMath::Abs(X-7)-Y/3)+20*FMath::Max(0.f,3-FMath::Abs(X-16)-FMath::Abs(Y-10)/2);
    for(FIntPoint D:{FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})if(IsRiver(C+D))return FMath::Min(Z,82.f);
    return Z;
}
FVector AVoxelBattle::CellPosition(FIntPoint C) const { return FVector(C.X*Tile,C.Y*Tile,Height(C)); }
FIntPoint AVoxelBattle::ToCell(FVector P) const { return FIntPoint(FMath::RoundToInt(P.X/Tile),FMath::RoundToInt(P.Y/Tile)); }
bool AVoxelBattle::Walkable(FIntPoint C) const
{
    if(!ContainsCell(C))return false;
    if(!IsRiver(C))return true;
    int I=BridgeIndexAt(C);return I!=INDEX_NONE&&Bridges[I].Remaining<=0;
}
float AVoxelBattle::TowerBaseRange(int32 I) const
{
    const auto& T=Towers[I];bool Defensive=T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon;
    float Base=T.Module==EVoxelModule::Arrow?(T.Tall?1250.f:1050.f):T.Module==EVoxelModule::Cannon?(T.Tall?1150.f:1000.f):T.Tall?820.f:570.f;
    if(Tutorial)Base=T.Tall?820.f:570.f;
    return Base*(Defensive?1+.1f*(FMath::Clamp(T.Level,1,MaxTowerLevel)-1):1.f);
}
float AVoxelBattle::TowerRange(int32 I,FVector Target) const
{ return TowerBaseRange(I)+FMath::Clamp((CellPosition(Towers[I].Cell).Z-Target.Z)*.65f,-200.f,260.f); }
bool AVoxelBattle::Visible(FVector From,FVector To) const
{
    From.Z+=150; To.Z+=30;
    int32 Steps=FMath::CeilToInt(FVector::Dist2D(From,To)/80.f);
    for(int32 I=1;I<Steps;++I)
    { FVector P=FMath::Lerp(From,To,float(I)/Steps); if(Height(ToCell(P))>P.Z+10) return false; }
    return true;
}
void AVoxelBattle::Box(int32 Color,FVector P,FVector Size,bool World)
{
    Batches[Color+(World?13:bDrawingTowers?26:0)]->AddInstance(FTransform(FRotator::ZeroRotator,P,Size/100.f));
}
void AVoxelBattle::MakeMap()
{
    const auto* PreviewCommander=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController());
    for(int X=-MapHalfWidth();X<=MapHalfWidth();++X) for(int Y=-MapHalfHeight();Y<=MapHalfHeight();++Y)
    {
        FIntPoint C(X,Y);if(!ContainsCell(C))continue;float Z=Height(C);
        if(IsRiver(C))
        {
            Box(7,FVector(X*Tile,Y*Tile,-3),FVector(Tile,Tile,28),true);
            continue;
        }
        // Each land column continues below the water; no rectangular board underneath.
        Box(3,FVector(X*Tile,Y*Tile,(Z-195)/2),FVector(Tile,Tile,Z+155),true);
        bool Coast=!ContainsCell(C+FIntPoint(1,0))||!ContainsCell(C+FIntPoint(-1,0))||!ContainsCell(C+FIntPoint(0,1))||!ContainsCell(C+FIntPoint(0,-1));
        int Color=Coast&&(FMath::Abs(X*7+Y*13)%3==0)?2:FMath::PerlinNoise2D(FVector2D(X*.12f,Y*.12f))>.05f?5:4;
        Box(Color,FVector(X*Tile,Y*Tile,Z-10),FVector(Tile,Tile,20),true);
        bool Occupied=false; for(const auto& T:Towers) if(FMath::Abs(T.Cell.X-X)+FMath::Abs(T.Cell.Y-Y)<2) Occupied=true;
        bool ShowcaseClear=PreviewCommander&&PreviewCommander->SoldierPreviewActive&&FVector::DistSquared2D(CellPosition(C),PreviewCommander->Focus)<FMath::Square(1200.f);
        if(!Occupied && !ShowcaseClear && !Coast && FMath::Abs(X)>1 && ((X*X+Y*Y*3)%19==0))
        {
            Box(6,FVector(X*Tile,Y*Tile,Z+38),FVector(20,20,76),true);
            Box(3,FVector(X*Tile,Y*Tile,Z+110),FVector(115,110,85),true);
            Box(5,FVector(X*Tile,Y*Tile,Z+175),FVector(87,82,62),true);
            Box(4,FVector(X*Tile,Y*Tile,Z+220),FVector(55,52,35),true);
        }
    }
    // Keep the background quiet so the playable shoreline remains easy to read.
    Box(7,FVector(0,0,-50),FVector(60000,60000,80),true);
    // Stepping-stone approaches connect each side's command to its outposts.
    for(int Pair=0;Pair<4;++Pair)
    {
        int From=Pair<2?0:3,To=Pair<2?Pair+1:Pair+2;
        if(!Towers.IsValidIndex(From)||!Towers.IsValidIndex(To))continue;
        FVector Previous=CellPosition(Towers[From].Cell)+FVector(0,0,27);
        for(FVector Q:FindRoute(Towers[From].Cell,Towers[To].Cell,0,false))
        {
            int Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Previous,Q)/110));
            for(int S=1;S<=Steps;++S){FVector P=FMath::Lerp(Previous,Q,float(S)/Steps);P.Z=Height(ToCell(P))+3;Box(2,P,FVector(58,58,4),true);}
            Previous=Q;
        }
    }

}

void AVoxelBattle::DrawWorld()
{
    for(int I=0;I<13;++I) Batches[I]->ClearInstances();
    uint32 Signature=HashCombineFast(GetTypeHash(Selected),GetTypeHash(Towers.Num()));
    for(const auto& T:Towers)
    {
        Signature=HashCombineFast(Signature,GetTypeHash(T.Cell));
        Signature=HashCombineFast(Signature,GetTypeHash(T.Level));
        Signature=HashCombineFast(Signature,uint32(T.Team)+uint32(T.Arrow)*4+uint32(T.Module)*16+uint32(T.Tall)*128);
    }
    if(Signature!=LastTowerSignature)
    {
        for(int C=26;C<39;++C)Batches[C]->ClearInstances();
        for(const auto& Batch:BuildingBatches)Batch->ClearInstances();
        bDrawingTowers=true;
    for(int I=0;I<Towers.Num();++I)
    {
        const auto& T=Towers[I];FVector P=CellPosition(T.Cell);int C=TeamColor(T.Team);
        if(DrawReferenceBuilding(I))continue;
        auto B=[&](int Color,FVector Offset,FVector Size){Box(Color,P+Offset*1.15f,Size*1.15f);};
        auto Roof=[&](FVector Offset,float Width,float Depth,int Color)
        {for(int R=0;R<5;++R)B(Color,Offset+FVector(0,0,R*12),FVector(Width-R*Width/6,Depth,12));};
        auto Flag=[&](FVector Offset)
        {B(6,Offset+FVector(0,0,43),FVector(8,8,86));B(C,Offset+FVector(28,0,65),FVector(54,7,36));B(11,Offset+FVector(0,0,91),FVector(12,12,12));};
        B(3,FVector(0,0,8),FVector(175,175,16));B(10,FVector(0,0,21),FVector(157,157,12));
        B(C,FVector(0,-76,32),FVector(96,8,12));
        if(T.Module==EVoxelModule::Command)
        {
            // Four stone turrets, crenellated walls, central keep and gatehouse.
            B(2,FVector(0,0,87),FVector(135,135,120));
            for(int S=-1;S<=1;S+=2)for(int R=-1;R<=1;R+=2)
            {
                FVector O(S*63,R*63,0);B(2,O+FVector(0,0,107),FVector(46,46,162));B(10,O+FVector(0,0,185),FVector(54,54,12));
                for(int A=-1;A<=1;A+=2)for(int D=-1;D<=1;D+=2)B(10,O+FVector(A*17,D*17,202),FVector(17,17,24));
                B(9,O+FVector(0,-24,128),FVector(10,3,25));
            }
            B(10,FVector(0,0,166),FVector(75,78,116));Roof(FVector(0,0,228),99,100,C);
            B(9,FVector(0,-69,68),FVector(41,5,74));B(6,FVector(0,-73,66),FVector(31,6,62));
            for(int J=-1;J<=1;++J)B(11,FVector(J*10,-78,66),FVector(3,3,58));
            B(C,FVector(-38,-72,124),FVector(22,4,39));B(C,FVector(38,-72,124),FVector(22,4,39));
            Flag(FVector(0,0,282));
        }
        else if(T.Module==EVoxelModule::Village)
        {
            for(int S=-1;S<=1;S+=2)
            {
                FVector O(S*43,0,0);B(10,O+FVector(0,0,59),FVector(65,92,66));
                for(int J=-1;J<=1;J+=2)B(6,O+FVector(J*28,-47,59),FVector(7,5,66));
                B(6,O+FVector(0,-47,39),FVector(20,5,28));B(9,O+FVector(-17,-47,70),FVector(14,5,14));
                Roof(O+FVector(0,0,96),85,105,8);B(2,O+FVector(15,20,139),FVector(14,16,58));
            }
            Flag(FVector(-65,60,105));
            for(int K=0;K<5;++K)B(11,FVector(85, K*22-44,28),FVector(22,12,7));
        }
        else
        {
            float BH=T.Tall?183:102, BW=T.Tall?68:118;
            B(T.Module==EVoxelModule::Arrow?6:2,FVector(0,0,33+BH/2),FVector(BW,BW,BH));
            for(int L=0;L<3;++L)B(10,FVector(0,0,45+L*BH/3),FVector(BW+4,BW+4,8));
            B(9,FVector(0,-BW/2-1,BH*.7f),FVector(15,4,24));
            B(10,FVector(0,0,BH+38),FVector(BW+28,BW+28,16));
            for(int S=-1;S<=1;S+=2)for(int R=-1;R<=1;R+=2)
                B(2,FVector(S*(BW/2+4),R*(BW/2+4),BH+56),FVector(22,22,30));
            if(T.Module==EVoxelModule::Cannon)
            {
                B(C,FVector(0,0,BH+65),FVector(67,70,38));
                for(int S=-1;S<=1;S+=2){B(6,FVector(0,S*43,BH+63),FVector(34,12,34));B(T.Overheated?11:9,FVector(T.Team==2?-53:53,S*15,BH+76),FVector(100,19,19));}
                Flag(FVector(-52,42,BH+65));
            }
            else
            {
                B(6,FVector(0,0,BH+62),FVector(20,20,40));
                B(T.Arrow==EVoxelArrow::Poison?12:11,FVector(0,0,BH+91),FVector(113,14,14));
                B(9,FVector(0,0,BH+98),FVector(12,102,12));
                B(C,FVector(0,-BW/2-6,120),FVector(36,6,58));Flag(FVector(-42,38,BH+83));
            }
        }
        for(int L=1;L<T.Level;++L)
            B(11,FVector(-48+(L-1)*32,-83,46),FVector(21,9,18));
        if(I==Selected)for(int S=-1;S<=1;S+=2){B(11,FVector(S*97,0,13),FVector(7,199,9));B(11,FVector(0,S*97,13),FVector(199,7,9));}
    }
        bDrawingTowers=false;
        LastTowerSignature=Signature;
    }
    for(const auto& Bridge:Bridges)
    {
        FVector Center=BridgeCenter(Bridge),Axis=Bridge.Vertical?FVector(0,1,0):FVector(1,0,0),Side=Bridge.Vertical?FVector(1,0,0):FVector(0,1,0);
        auto Size=[&](float Along,float Across,float High){return Bridge.Vertical?FVector(Across,Along,High):FVector(Along,Across,High);};
        float Length=Bridge.Length*Tile+40,Progress=1-FMath::Clamp(Bridge.Remaining/FMath::Max(1.f,BridgeBuildTime),0.f,1.f);
        for(int S:{-1,1})Box(2,Center+Axis*(S*Length*.5f)-FVector(0,0,35),Size(25,Tile-20,90));
        int Planks=FMath::CeilToInt(Length/22);
        for(int P=0;P<FMath::CeilToInt(Planks*Progress);++P)Box(8,Center+Axis*(-Length*.5f+(P+.5f)*Length/Planks),Size(Length/Planks-1,Tile-20,22));
        for(int S:{-1,1})Box(Bridge.Remaining>0?TeamColor(Bridge.Team):10,Center+Side*(S*75)+FVector(0,0,31),Size(Length,8,12));
    }
    DrawSoldierVisuals();
    DrawWeaponVisuals();
    DrawPoisonVisuals();
    DrawMeteorVisuals();
}

void AVoxelBattle::Message(const FString& S) { Notice=S; NoticeUntil=Time+7; }
int32 AVoxelBattle::UpgradeCost(int32 I) const
{if(!Towers.IsValidIndex(I)||Towers[I].Level>=MaxTowerLevel)return 0;int32 Step=Towers[I].Module==EVoxelModule::Village?20:40;return Step*(FMath::Max(1,Towers[I].Level)+1);}
float AVoxelBattle::TroopCapacity(int32 I) const
{return 250.f+(Towers[I].Module==EVoxelModule::Village?0:50*(FMath::Clamp(Towers[I].Level,1,MaxTowerLevel)-1));}
float AVoxelBattle::TowerDamage(int32 I) const
{return (Towers[I].Tall?2.3f:3.5f)*(1+.2f*(FMath::Clamp(Towers[I].Level,1,MaxTowerLevel)-1));}
float AVoxelBattle::TowerInterval(int32 I) const
{
    const auto& T=Towers[I];float Base=T.Module==EVoxelModule::Cannon?.38f:T.Module==EVoxelModule::Arrow?(T.Arrow==EVoxelArrow::Pierce?3.5f:2.8f):1.3f;
    return Base/(1+.15f*(FMath::Clamp(T.Level,1,MaxTowerLevel)-1));
}
int32 AVoxelBattle::VillageCount(int32 Team) const
{int32 Count=0;for(const auto& T:Towers)Count+=T.Team==Team&&T.Module==EVoxelModule::Village;return Count;}
float AVoxelBattle::VillageTroopSupply(int32 I) const
{return Towers.IsValidIndex(I)&&Towers[I].Module==EVoxelModule::Village?VillageTroopBonus*(1+.25f*(FMath::Clamp(Towers[I].Level,1,MaxTowerLevel)-1)):0;}
float AVoxelBattle::VillageGoldSupply(int32 I) const
{return Towers.IsValidIndex(I)&&Towers[I].Module==EVoxelModule::Village?VillageGoldBonus*(1+.25f*(FMath::Clamp(Towers[I].Level,1,MaxTowerLevel)-1)):0;}
float AVoxelBattle::CommandRecovery(int32 Team) const
{float Rate=CommandRegen;for(int I=0;I<Towers.Num();++I)if(Towers[I].Team==Team)Rate+=VillageTroopSupply(I);return Rate;}
float AVoxelBattle::GoldRecovery(int32 Team) const
{float Rate=1;for(int I=0;I<Towers.Num();++I)if(Towers[I].Team==Team)Rate+=VillageGoldSupply(I);return Rate;}
int32 AVoxelBattle::HoldingCount(int32 Team) const
{int32 Count=0;for(const auto& T:Towers)Count+=T.Team==Team;return Count;}
int32 AVoxelBattle::OutpostsRemaining(int32 Team) const
{int32 Count=0;for(const auto& T:Towers)Count+=T.Team==Team&&T.Module!=EVoxelModule::Command;return Count;}
bool AVoxelBattle::CanAttackTower(int32 I,int32 Attacker) const
{return Towers.IsValidIndex(I)&&Attacker>=1&&Attacker<=2&&(Towers[I].Team==Attacker||Towers[I].Team==0||Towers[I].Module!=EVoxelModule::Command||OutpostsRemaining(Towers[I].Team)==0);}
float AVoxelBattle::TotalArmy(int32 Team) const
{float Troops=0;for(const auto& T:Towers)if(T.Team==Team)Troops+=FMath::Max(0.f,T.Troops);for(const auto& S:Squads)if(S.Team==Team)Troops+=FMath::Max(0.f,S.Troops);return Troops;}
void AVoxelBattle::FinishBattle(int32 Result,const FString& Summary)
{
    if(Winner)return;
    Winner=Result;Paused=false;CancelCommand();AIOrders.Empty();
    for(int Team=1;Team<=2;++Team){FinalHoldings[Team]=HoldingCount(Team);FinalTroops[Team]=TotalArmy(Team);}
    Message(Summary);
}
void AVoxelBattle::ResolveTimeLimit()
{
    if(Winner)return;
    TimeLimitReached=true;Time=BattleTimeLimit;
    int32 BlueHoldings=HoldingCount(1),RedHoldings=HoldingCount(2);float BlueArmy=TotalArmy(1),RedArmy=TotalArmy(2);
    int32 Result=BlueHoldings!=RedHoldings?(BlueHoldings>RedHoldings?1:2):FMath::IsNearlyEqual(BlueArmy,RedArmy,.001f)?3:BlueArmy>RedArmy?1:2;
    FString Reason=BlueHoldings!=RedHoldings?TEXT("据点数量"):FMath::IsNearlyEqual(BlueArmy,RedArmy,.001f)?TEXT("据点和兵力均相同"):TEXT("场上总兵力");
    FinishBattle(Result,FString::Printf(TEXT("%d 分钟时限已到：按%s结算，%s。"),FMath::RoundToInt(BattleTimeLimit/60),*Reason,Result==1?TEXT("我方获胜"):Result==2?TEXT("敌方获胜"):TEXT("双方平局")));
}
bool AVoxelBattle::UpgradeTower(int32 I,int32 Team)
{
    if(MenuPaused||Paused||Winner||Team<1||Team>2||!Towers.IsValidIndex(I))return false;
    auto& T=Towers[I];
    if(T.Team!=Team)return false;
    if(Tutorial&&Team==1&&(I!=(T.Module==EVoxelModule::Village?2:1)||!TutorialAllowsAction(T.Module==EVoxelModule::Village?2:3,T.Module==EVoxelModule::Village?2:1)||T.Level>=2))return false;
    int32 Cost=UpgradeCost(I);
    if(Cost<=0){if(Team==1)Message(TEXT("此塔已达到最高等级。"));return false;}
    if(Gold[Team]<Cost){if(Team==1)Message(FString::Printf(TEXT("升级需要 %d 金币，当前金币不足。"),Cost));return false;}
    float OldInterval=TowerInterval(I);Gold[Team]-=Cost;++T.Level;
    if(T.Module!=EVoxelModule::Village){T.Troops=FMath::Min(TroopCapacity(I),T.Troops+50);T.FireTimer=FMath::Max(0.f,T.FireTimer)*TowerInterval(I)/OldInterval;}
    if(Team==1){CompleteCommand();Message(T.Module==EVoxelModule::Village?FString::Printf(TEXT("%s发展至 %d 级：主塔补给 +%.2f 兵/秒，金币 +%.1f 金/秒。"),*T.Name.ToString(),T.Level,VillageTroopSupply(I),VillageGoldSupply(I)):FString::Printf(TEXT("%s已升至 %d 级！驻军 +50，%s。"),*T.Name.ToString(),T.Level,T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon?TEXT("伤害、射速与射程提升"):TEXT("伤害与射速提升")));}
    return true;
}
bool AVoxelBattle::Dispatch(int32 From,int32 To,float Fraction)
{
    if(!Towers.IsValidIndex(To)||From==To)return false;
    return SendTroops(From,To,Towers[To].Cell,Fraction);
}
bool AVoxelBattle::DispatchTo(int32 From,FVector P,float Fraction)
{return SendTroops(From,INDEX_NONE,ToCell(P),Fraction,&P);}
bool AVoxelBattle::SendTroops(int32 From,int32 Target,FIntPoint Destination,float Fraction,const FVector* GroundDestination)
{
    if(MenuPaused||Paused||Winner||!Towers.IsValidIndex(From)||Towers[From].Team==0)return false;
    if(Tutorial&&Towers[From].Team==1&&Target==INDEX_NONE&&(TutorialStep==5||TutorialStep==6)&&FMath::Abs(Destination.X+3)<=1&&FMath::Abs(Destination.Y-2)<=1)Destination=FIntPoint(-3,2);
    if(!TutorialAllowsOrder(Towers[From].Team,Target,Destination))return false;
    if(Target!=INDEX_NONE&&!CanAttackTower(Target,Towers[From].Team)){if(Towers[From].Team==1)Message(TEXT("主塔受保护：先占领敌方全部非主塔据点（含村庄）。"));return false;}
    if(!Walkable(Destination)){if(Towers[From].Team==1)Message(TEXT("目标不可通行，请先建桥或选择岸上的空地。"));return false;}
    int32 Count=FMath::FloorToInt(Towers[From].Troops*FMath::Clamp(Fraction,.1f,1.f));
    if(Tutorial&&Towers[From].Team==1&&TutorialStep==5&&Count<25){Message(TEXT("本课需要至少 25 兵。按 Tab 增大比例，或从教学主城出兵。"));return false;}
    if(Count<1) { if(Towers[From].Team==1) Message(TEXT("驻军不足，暂时无法出兵。")); return false; }
    FVoxelSquad S;S.Team=Towers[From].Team;S.Target=Target;S.Troops=Count;S.Destination=Destination;S.HasDestination=true;S.LastLand=Towers[From].Cell;
    if(GroundDestination&&Target==INDEX_NONE)
    {
        FVector Goal=Tutorial&&S.Team==1&&(TutorialStep==5||TutorialStep==6)?CellPosition(Destination):*GroundDestination;
        S.GroundDestination=ClampMoveDestination(Goal);S.HasGroundDestination=true;
    }
    S.Position=CellPosition(Towers[From].Cell)+FVector(0,0,27);
    if(!RepathSquad(S)){if(S.Team==1)Message(TEXT("暂无可达路线。河道需要已完工的桥梁。"));return false;}
    if(!S.Route.IsEmpty())S.VisualYaw=(S.Route[0]-S.Position).Rotation().Yaw;
    S.ID=NextSquadID++;
    Towers[From].Troops-=Count; Squads.Add(MoveTemp(S));
    if(Towers[From].Team==1) { CompleteCommand(); Message(Target==INDEX_NONE?FString::Printf(TEXT("已派出 %d 兵，抵达空地后驻守并拦截敌军。"),Count):FString::Printf(TEXT("已派出 %d 名士兵，前往%s。"),Count,*Towers[Target].Name.ToString())); }
    return true;
}
int32 AVoxelBattle::SquadIndex(int32 ID) const
{if(ID==INDEX_NONE)return INDEX_NONE;return Squads.IndexOfByPredicate([&](const FVoxelSquad& S){return S.ID==ID&&S.Troops>0;});}
bool AVoxelBattle::OrderSquad(int32 ID,int32 Target,FVector P)
{
    int32 I=SquadIndex(ID);if(MenuPaused||Paused||Winner||I==INDEX_NONE||Squads[I].Team!=1)return false;
    if(Towers.IsValidIndex(Target)&&!CanAttackTower(Target,1)){Message(TEXT("主塔受保护：先清空敌方分城与村庄。"));return false;}
    FIntPoint Goal=Towers.IsValidIndex(Target)?Towers[Target].Cell:ToCell(P);if(!Walkable(Goal)){Message(TEXT("目标不可通行。"));return false;}
    if(Tutorial&&Target==INDEX_NONE&&(TutorialStep==5||TutorialStep==6)&&FMath::Abs(Goal.X+3)<=1&&FMath::Abs(Goal.Y-2)<=1)Goal=FIntPoint(-3,2);
    if(!TutorialAllowsOrder(1,Target,Goal))return false;
    FVoxelSquad Planned=Squads[I];Planned.Target=Towers.IsValidIndex(Target)?Target:INDEX_NONE;Planned.Destination=Goal;Planned.HasDestination=true;Planned.InterceptTarget=INDEX_NONE;Planned.MarchDelay=0;Planned.RetargetIn=0;Planned.Evacuating=false;
    Planned.HasGroundDestination=Planned.Target==INDEX_NONE;
    if(Planned.HasGroundDestination)Planned.GroundDestination=ClampMoveDestination(Tutorial&&(TutorialStep==5||TutorialStep==6)?CellPosition(Goal):P);
    if(!RepathSquad(Planned)){Message(TEXT("路线被河道阻断，请先建桥。"));return false;}
    Squads[I]=MoveTemp(Planned);CompleteCommand();Message(Target==INDEX_NONE?TEXT("部队前往指定空地，抵达后驻守拦截。 "):TEXT("部队已收到新的进攻或增援命令。"));return true;
}
bool AVoxelBattle::RepathSquad(FVoxelSquad& S)
{
    FIntPoint Start=ToCell(S.Position),Goal=Towers.IsValidIndex(S.Target)?Towers[S.Target].Cell:S.Destination;
    S.Route.Empty();S.Step=0;S.Blocked=false;S.Holding=false;S.Evacuating=false;
    if(IsRiver(Start)&&!Walkable(Start))
    {
        FIntPoint Bank=S.LastLand;
        if(!ContainsCell(Bank)||IsRiver(Bank))
        {
            float Best=FLT_MAX;
            for(int Y=-8;Y<=8;++Y)for(int X=-8;X<=8;++X)
            {FIntPoint C=Start+FIntPoint(X,Y);if(!Walkable(C)||IsRiver(C))continue;float D=FVector::DistSquared2D(S.Position,CellPosition(C));if(D<Best){Best=D;Bank=C;}}
        }
        S.Route.Add(CellPosition(Bank)+FVector(0,0,27));S.Evacuating=true;S.Blocked=true;S.InterceptTarget=INDEX_NONE;return false;
    }
    if(!S.HasDestination&&!Towers.IsValidIndex(S.Target)){S.Blocked=true;S.Holding=true;return false;}
    S.Route=FindGroundRoute(S.Position,S.Target==INDEX_NONE&&S.HasGroundDestination?S.GroundDestination:CellPosition(Goal)+FVector(0,0,27));
    if(S.Route.IsEmpty()&&Start==Goal&&Walkable(Goal))S.Route.Add(CellPosition(Goal)+FVector(0,0,27));
    if(S.Route.IsEmpty()){S.Blocked=true;S.Holding=true;S.InterceptTarget=INDEX_NONE;return false;}
    return true;
}
void AVoxelBattle::RepathArmies()
{for(auto& S:Squads)if(S.Troops>0){S.InterceptTarget=INDEX_NONE;RepathSquad(S);}}

void AVoxelBattle::SetGuardPost(FVoxelSquad& Squad,FVector Post)
{
    Squad.Target=INDEX_NONE;
    Squad.Destination=ToCell(Post);
    Squad.GroundDestination=Post;
    Squad.HasDestination=Squad.HasGroundDestination=true;
    Squad.InterceptTarget=INDEX_NONE;
    Squad.RetargetIn=0;
    Squad.Route.Reset();
    Squad.Step=0;
    Squad.Blocked=false;
    Squad.Holding=FVector::DistSquared2D(Squad.Position,Post)<1.f;
    if(!Squad.Holding)RepathSquad(Squad);
}

void AVoxelBattle::HoldOutsideTower(FVoxelSquad& Squad,int32 TowerIndex)
{
    const FVector Center=CellPosition(Towers[TowerIndex].Cell);
    if(FVector::DistSquared2D(Squad.Position,Center)>=FMath::Square(GarrisonApproachRadius-1)&&
        Walkable(ToCell(Squad.Position))&&!IsRiver(ToCell(Squad.Position)))
    {
        SetGuardPost(Squad,Squad.Position);
        return;
    }
    FVector Direction=(Squad.Position-Center).GetSafeNormal2D();
    if(Direction.IsNearlyZero())Direction=-FRotator(0,Squad.VisualYaw,0).Vector();
    for(int Step : {0,1,-1,2,-2,3,-3,4})
    {
        FVector Post=Center+Direction.RotateAngleAxis(Step*45.f,FVector::UpVector)*GarrisonApproachRadius;
        const FIntPoint Cell=ToCell(Post);
        if(IsRiver(Cell)||!WalkableSegment(Squad.Position,Post))continue;
        if(Towers.ContainsByPredicate([&](const FVoxelTower& Tower)
        {
            return &Tower!=&Towers[TowerIndex]&&FVector::DistSquared2D(CellPosition(Tower.Cell),Post)<FMath::Square(220.f);
        }))continue;
        Post.Z=Height(Cell)+27;
        SetGuardPost(Squad,Post);
        return;
    }
    SetGuardPost(Squad,Squad.Position);
}

void AVoxelBattle::Arrive(FVoxelSquad& S)
{
    if(MenuPaused||Paused||Winner||S.Troops<=0||!Towers.IsValidIndex(S.Target)||S.Team<1||S.Team>2)return;
    auto& T=Towers[S.Target];
    if(!CanAttackTower(S.Target,S.Team))
    {
        SetGuardPost(S,S.Position);
        if(S.Team==1)Message(TEXT("敌方分城已恢复主塔保护，部队原地待命。"));return;
    }
    int32 PreviousTeam=T.Team;
    const int32 TowerIndex=S.Target;
    const float Capacity=TroopCapacity(TowerIndex);
    if(T.Team==S.Team)
    {
        const float Accepted=FMath::Min(S.Troops,FMath::Max(0.f,Capacity-T.Troops));
        T.Troops+=Accepted;
        S.Troops-=Accepted;
        if(S.Troops>0)
        {
            HoldOutsideTower(S,TowerIndex);
            if(S.Team==1)Message(TEXT("据点驻军已满，余下部队在塔外驻守。"));
        }
        return;
    }
    if(VoxelRules::ResolveAttack(S.Troops,T.Troops))
    {
        T.Team=S.Team;
        S.Troops=FMath::Max(0.f,T.Troops-Capacity);
        T.Troops=FMath::Min(Capacity,T.Troops);
        if(S.Troops>0)HoldOutsideTower(S,TowerIndex);
        if(T.Module==EVoxelModule::Arrow && S.Team==1 && !T.PlayerBound) T.Arrow=EVoxelArrow::Unbound;
        if(T.Module==EVoxelModule::Arrow && S.Team==2 && !T.PlayerBound) T.Arrow=EVoxelArrow::Poison;
        if(S.Team==1)Message(S.Troops>0?TEXT("据点已占领！驻军已满，余下部队在塔外驻守。"):TEXT("据点已占领！幸存士兵转为驻军。"));
        if(T.Module!=EVoxelModule::Command&&PreviousTeam!=0&&OutpostsRemaining(PreviousTeam)==0)
            Message(PreviousTeam==2?TEXT("敌方全部分城已失守，主塔保护解除！"):TEXT("我方全部分城已失守，主塔保护解除！"));
        if(T.Module==EVoxelModule::Command)
        {int32 Team=S.Team;FinishBattle(Team,Team==1?TEXT("战役胜利！敌方主城已被攻占。"):TEXT("战役失败，我方主城已经失守。"));}
        return;
    }
    S.Troops=0;
}
void AVoxelBattle::BindArrow(EVoxelArrow Type)
{
    if(MenuPaused||Paused||Winner||!Towers.IsValidIndex(Selected)) return;
    if(!TutorialAllowsAction(4,1))return;
    auto& T=Towers[Selected];
    if(T.Team!=1||T.Module!=EVoxelModule::Arrow) return;
    if(T.PlayerBound){Message(TEXT("箭种已永久绑定，即使据点易主也会保留。"));return;}
    T.Arrow=Type; T.PlayerBound=true;
    CompleteCommand();
    Message(Type==EVoxelArrow::Poison?TEXT("已选择毒箭：在落点留下持续伤害区域。"):TEXT("已选择穿透箭：直线重击多个目标，装填较慢。"));
}
void AVoxelBattle::Meteor(FVector P)
{
    if(MenuPaused||Paused||Winner) return;
    if(!TutorialAllowsAction(8))return;
    if(Gold[1]<MeteorCost||MeteorReady>0){Message(TEXT("陨石需要 55 金币，且必须等待冷却结束。"));return;}
    if(FMath::Abs(P.X)>MapHalfWidth()*Tile||FMath::Abs(P.Y)>MapHalfHeight()*Tile)return;
    Gold[1]-=MeteorCost; MeteorReady=MeteorCooldown; MeteorMode=false;
    CompleteCommand();
    FVoxelEffect E; E.Position=P; E.Remaining=1.5f; E.Radius=340; E.Meteor=true; Effects.Add(E);
    Message(TEXT("陨石即将落下！圈内双方在途军队都会受到伤害。"));
}
float AVoxelBattle::Incoming(int32 Target,int32 Team) const
{ float N=0; for(const auto& S:Squads) if(S.Target==Target&&S.Team==Team)N+=S.Troops; return N; }
void AVoxelBattle::RunAI()
{
    if(!AIEnabled||MenuPaused||Paused||Winner||Time+KINDA_SMALL_NUMBER<NextAIAction)return;
    NextAIAction=Time+AIInterval;
    if(ExecuteAIOrder())return;
    // Same visible state, movement, production and combat rules as the player. No free troops.
    for(int32 I=0;I<Towers.Num();++I)
    {
        auto& T=Towers[I]; if(T.Team!=2)continue;
        float Threat=Incoming(I,1);
        if(Threat>T.Troops+Incoming(I,2))
        {
            int32 Donor=-1; float Dist=FLT_MAX;
            for(int32 J=0;J<Towers.Num();++J) if(J!=I&&Towers[J].Team==2&&Towers[J].Troops>20&&Incoming(J,1)<5&&CanReach(Towers[I].Cell,Towers[J].Cell))
            {float D=FVector::Dist2D(CellPosition(T.Cell),CellPosition(Towers[J].Cell));if(D<Dist){Dist=D;Donor=J;}}
            if(Donor>=0&&Dispatch(Donor,I,.65f))return;
        }
    }
    if(BuildAIBridge())return;
    if(Time>=NextAIUpgrade&&UpgradeAIFrontline()){NextAIUpgrade=Time+30;return;}
    int32 Source=-1,Target=-1; float Best=-FLT_MAX;
    for(int32 I=0;I<Towers.Num();++I) if(Towers[I].Team==2)
    {
        float Reserve=(Towers[I].Module==EVoxelModule::Command?28.f:10.f)+Incoming(I,1);
        float Available=Towers[I].Troops-Reserve; if(Available<9)continue;
        TArray<int32> Candidates;
        for(int J=0;J<Towers.Num();++J)
        {
            if(Towers[J].Team==2||!CanAttackTower(J,2))continue;
            // Filter every disconnected island before taking the top six scores.
            if(!CanReach(Towers[I].Cell,Towers[J].Cell))continue;
            float Need=Towers[J].Troops+7-Incoming(J,2);
            if(Need>0&&Need<=Available)Candidates.Add(J);
        }
        auto Potential=[&](int J){return (Towers[J].Module==EVoxelModule::Command?180.f:Towers[J].Module==EVoxelModule::Village?95.f:55.f)-FVector::Dist2D(CellPosition(Towers[I].Cell),CellPosition(Towers[J].Cell))*.018f-Towers[J].Troops*.6f;};
        Candidates.Sort([&](int A,int B){return Potential(A)>Potential(B);});
        if(Candidates.Num()>6)Candidates.SetNum(6);
        for(int32 J:Candidates)
        {
            float Need=Towers[J].Troops+7-Incoming(J,2);
            if(Need<0||Available<Need)continue;
            auto R=FindRoute(Towers[I].Cell,Towers[J].Cell,2); if(R.IsEmpty())continue;
            float Length=0; FVector P=CellPosition(Towers[I].Cell);float Exposure=RouteExposure(P,R,2);
            for(auto Q:R)
            {
                Length+=FVector::Dist2D(P,Q); P=Q;
            }
            if(Available<Need+Exposure)continue;
            float Value=Towers[J].Module==EVoxelModule::Command?180:Towers[J].Module==EVoxelModule::Village?95:55;
            float Score=Value-Length*.018f-Need*.6f+(Towers[J].Overheated?30:0);
            if(Score>Best){Best=Score;Source=I;Target=J;}
        }
    }
    if(Source>=0)
    {
        float Need=Towers[Target].Troops+14-Incoming(Target,2);
        float Reserve=Towers[Source].Module==EVoxelModule::Command?28:10;
        if(Dispatch(Source,Target,FMath::Min(Towers[Source].Troops-Reserve,FMath::Max(Need,Towers[Source].Troops*.65f))/Towers[Source].Troops))return;
    }
    // A capped headquarters cannot be beaten by one capped tower with a reserve.
    // Pool real troops from several towers instead of waiting forever for a solo attack.
    if(LaunchCommandAssault())return;
    if(DeployAIBridgeGuard())return;
    // Move idle reserves toward the frontier when no safe attack is available.
    for(int32 I=0;I<Towers.Num();++I) if(Towers[I].Team==2&&Towers[I].Troops>60&&Incoming(I,1)==0)
    {
        int32 Front=-1; float BestDistance=FLT_MAX;
        for(int32 J=0;J<Towers.Num();++J) if(J!=I&&Towers[J].Team==2&&Towers[J].Troops+Incoming(J,2)<65&&CanReach(Towers[I].Cell,Towers[J].Cell))
        {float D=FLT_MAX;for(const auto& T:Towers)if(T.Team!=2)D=FMath::Min(D,FVector::Dist2D(CellPosition(T.Cell),CellPosition(Towers[J].Cell)));if(D<BestDistance){BestDistance=D;Front=J;}}
        if(Front>=0&&Dispatch(I,Front,.4f))return;
    }
}

bool AVoxelBattle::LaunchCommandAssault()
{
    if(SoldierSpeed<=0)return false;
    for(int32 Target=0;Target<Towers.Num();++Target)
    {
        if(Towers[Target].Team!=1||Towers[Target].Module!=EVoxelModule::Command||!CanAttackTower(Target,2)||Incoming(Target,2)>0)continue;
        TArray<int32> Sources;
        for(int32 I=0;I<Towers.Num();++I)if(Towers[I].Team==2&&Towers[I].Troops-(Towers[I].Module==EVoxelModule::Command?28:10)-Incoming(I,1)>=9&&CanReach(Towers[I].Cell,Towers[Target].Cell))Sources.Add(I);
        Sources.Sort([&](int32 A,int32 B){return FVector::DistSquared2D(CellPosition(Towers[A].Cell),CellPosition(Towers[Target].Cell))<FVector::DistSquared2D(CellPosition(Towers[B].Cell),CellPosition(Towers[Target].Cell));});
        if(Sources.Num()>6)Sources.SetNum(6);
        struct FAssaultOrder { int32 Source, Count; float Travel; };
        TArray<FAssaultOrder> Orders;
        float Total=0,Longest=0,Exposure=0;bool Enough=false;
        for(int32 Source:Sources)
        {
            auto Route=FindRoute(Towers[Source].Cell,Towers[Target].Cell,2);if(Route.IsEmpty())continue;
            float Length=0;FVector P=CellPosition(Towers[Source].Cell)+FVector(0,0,27);
            Exposure+=RouteExposure(P,Route,2);
            for(const FVector& Q:Route)
            {
                Length+=FVector::Dist2D(P,Q);P=Q;
            }
            int32 Count=FMath::FloorToInt(Towers[Source].Troops-(Towers[Source].Module==EVoxelModule::Command?28:10)-Incoming(Source,1));
            float Travel=Length/SoldierSpeed;Longest=FMath::Max(Longest,Travel);Total+=Count;
            Orders.Add({Source,Count,Travel});
            float Defense=FMath::Min(TroopCapacity(Target),Towers[Target].Troops+Longest*CommandRecovery(1));
            if(Orders.Num()>=2&&Total>Defense+35+Exposure){Enough=true;break;}
        }
        if(!Enough)continue;
        float Arrival=Time+Longest+AIInterval*Orders.Num()+1;
        for(const auto& Order:Orders)
            AIOrders.Add({Order.Source,Target,Order.Count,Arrival});
        return ExecuteAIOrder();
    }
    return false;
}

bool AVoxelBattle::ExecuteAIOrder()
{
    while(!AIOrders.IsEmpty())
    {
        FAIOrder O=AIOrders[0];AIOrders.RemoveAt(0);
        if(!Towers.IsValidIndex(O.Source)||!Towers.IsValidIndex(O.Target)||Towers[O.Source].Team!=2||Towers[O.Target].Team==2||!CanAttackTower(O.Target,2)||Towers[O.Source].Troops<O.Count)continue;
        if(Dispatch(O.Source,O.Target,(O.Count+.01f)/Towers[O.Source].Troops))
        {
            auto& S=Squads.Last();float Length=0;FVector P=S.Position;for(const auto& Q:S.Route){Length+=FVector::Dist2D(P,Q);P=Q;}
            S.MarchDelay=FMath::Max(0.f,O.ArrivalTime-Time-Length/FMath::Max(1.f,SoldierSpeed));return true;
        }
    }
    return false;
}
bool AVoxelBattle::BuildAIBridge()
{
    if(Time<45||(HasCrossing()&&Time<NextAIBridge)||Gold[2]<BridgeCost||Bridges.ContainsByPredicate([](const FVoxelBridge& B){return B.Remaining>0;}))return false;
    FIntPoint Choice=FIntPoint::ZeroValue;float Best=FLT_MAX;
    for(int Y=-MapHalfHeight();Y<=MapHalfHeight();++Y)for(int X=-MapHalfWidth();X<=MapHalfWidth();++X)
    {
        FIntPoint C(X,Y);FVoxelBridge Site;if(!BridgeSiteAt(C,Site)||BridgeIndexAt(C)!=INDEX_NONE||C!=FIntPoint(Site.Column,Site.Row))continue;
        FIntPoint A=BridgeBank(Site,false),B=BridgeBank(Site,true);if(CanReach(A,B))continue;
        for(int Flip:{0,1})
        {
            FIntPoint Near=Flip?B:A,Far=Flip?A:B;float Friendly=FLT_MAX,Other=FLT_MAX;
            for(const auto& T:Towers)
            {
                if(T.Team==2&&CanReach(T.Cell,Near))Friendly=FMath::Min(Friendly,float(FMath::Abs(T.Cell.X-Near.X)+FMath::Abs(T.Cell.Y-Near.Y)));
                if(T.Team!=2&&CanReach(T.Cell,Far))Other=FMath::Min(Other,float(FMath::Abs(T.Cell.X-Far.X)+FMath::Abs(T.Cell.Y-Far.Y))+(T.Module==EVoxelModule::Village?-4.f:0.f));
            }
            float Score=Friendly+Other+Site.Length*2.f;
            if(Friendly<FLT_MAX&&Other<FLT_MAX&&Score<Best){Best=Score;Choice=C;}
        }
    }
    bool Built=Best<FLT_MAX&&BuildBridgeAt(Choice,2);
    if(Built)NextAIBridge=Time+BridgeBuildTime+60;
    return Built;
}
bool AVoxelBattle::UpgradeAIFrontline()
{
    if(!AIEnabled||Time<45||MenuPaused||Paused||Winner)return false;
    float Reserve=!HasCrossing()&&Bridges.IsEmpty()?FMath::Max(60.f,BridgeCost):60.f;
    bool HasMilitary=false,Fortified=false;
    for(const auto& T:Towers)if(T.Team==2&&(T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon))
    {HasMilitary|=T.Level<MaxTowerLevel;Fortified|=T.Level>1;}
    // Fortify arrow/cannon towers first; economic and command upgrades are fallbacks.
    for(int Pass=0;Pass<2;++Pass)
    {
        int32 Best=INDEX_NONE;float Distance=FLT_MAX;
        for(int32 I=0;I<Towers.Num();++I)
        {
            bool Defensive=Towers[I].Module==EVoxelModule::Arrow||Towers[I].Module==EVoxelModule::Cannon;
            if(Defensive!=(Pass==0)||Towers[I].Team!=2||UpgradeCost(I)<=0||Gold[2]<UpgradeCost(I)+Reserve)continue;
            float D=FLT_MAX;for(const auto& T:Towers)if(T.Team==1)D=FMath::Min(D,FVector::Dist2D(CellPosition(T.Cell),CellPosition(Towers[I].Cell)));
            D+=300*(Towers[I].Level-1);if(D<Distance){Distance=D;Best=I;}
        }
        if(Best!=INDEX_NONE)return UpgradeTower(Best,2);
        if(Pass==0&&HasMilitary&&!Fortified)return false;
    }
    return false;
}

bool AVoxelBattle::DeployAIBridgeGuard()
{
    if(GuardRadius<=0)return false;
    for(const FVoxelBridge& Bridge:Bridges)
    {
        if(Bridge.Remaining>0)continue;
        for(bool Far : {false,true})
        {
            const FIntPoint Bank=BridgeBank(Bridge,Far);
            const FVector BridgeMouth=CellPosition(Bank);
            if(Squads.ContainsByPredicate([&](const FVoxelSquad& Squad)
            {
                return Squad.Team==2&&Squad.Troops>0&&Squad.Target==INDEX_NONE&&Squad.HasDestination&&
                    FVector::Dist2D(CellPosition(Squad.Destination),BridgeMouth)<GuardRadius;
            }))continue;

            // Prefer the red side by comparing the closest local red and blue holdings.
            int32 Source=INDEX_NONE;
            double RedDistance=FMath::Square(2400.f),BlueDistance=DBL_MAX;
            for(int32 I=0;I<Towers.Num();++I)
            {
                const FVoxelTower& Tower=Towers[I];
                const double Distance=FVector::DistSquared2D(CellPosition(Tower.Cell),BridgeMouth);
                if(Tower.Team==1)BlueDistance=FMath::Min(BlueDistance,Distance);
                if(Tower.Team!=2||Distance>=RedDistance||!CanReach(Tower.Cell,Bank))continue;
                RedDistance=Distance;
                Source=I;
            }
            if(Source==INDEX_NONE||RedDistance>=BlueDistance)continue;
            const FVoxelTower& Donor=Towers[Source];
            const float Reserve=(Donor.Module==EVoxelModule::Command?45.f:25.f)+Incoming(Source,1);
            const float Available=Donor.Troops-Reserve;
            if(Available<20)continue;

            for(FIntPoint Offset : {FIntPoint(0,0),FIntPoint(1,0),FIntPoint(-1,0),FIntPoint(0,1),FIntPoint(0,-1)})
            {
                const FIntPoint Post=Bank+Offset;
                if(IsRiver(Post)||!Walkable(Post)||Towers.ContainsByPredicate([&](const FVoxelTower& Tower){return Tower.Cell==Post;}))continue;
                const float Fraction=FMath::Min(Available,FMath::Clamp(Donor.Troops*.25f,20.f,45.f))/Donor.Troops;
                if(DispatchTo(Source,CellPosition(Post),Fraction))return true;
            }
        }
    }
    return false;
}

void AVoxelBattle::MergeNearbySquads(TMap<int32,FVector>* RallyPoints)
{
    if(MenuPaused||Paused||Winner||SquadMergeRadius<=0)return;
    TArray<int32> Priority;
    for(int32 I=0;I<Squads.Num();++I)
        if(Squads[I].Troops>0&&Squads[I].Team>=1&&Squads[I].Team<=2&&!IsMeteorLessonSquad(Squads[I]))Priority.Add(I);
    Priority.Sort([&](int32 A,int32 B)
    {
        bool SelectedA=Squads[A].ID==SelectedSquad,SelectedB=Squads[B].ID==SelectedSquad;
        if(SelectedA!=SelectedB)return SelectedA;
        if(Squads[A].Troops!=Squads[B].Troops)return Squads[A].Troops>Squads[B].Troops;
        return Squads[A].ID<Squads[B].ID;
    });
    TMap<int32,int32> ReplacedIDs;
    for(int32 P=0;P<Priority.Num();++P)
    {
        auto& Host=Squads[Priority[P]];
        if(Host.Troops<=0||Host.Evacuating||!Walkable(ToCell(Host.Position))||
            (RallyPoints&&RallyPoints->Contains(Host.ID)))continue;
        for(int32 Q=P+1;Q<Priority.Num();++Q)
        {
            auto& Other=Squads[Priority[Q]];
            const double Distance=FVector::DistSquared2D(Host.Position,Other.Position);
            if(Other.Troops<=0||Other.Team!=Host.Team||Other.Evacuating||Distance>FMath::Square(SquadMergeRadius)||
                (RallyPoints&&RallyPoints->Contains(Other.ID)))continue;
            // Armies with separate building orders continue toward their own objectives.
            if(Host.HasDestination&&Other.HasDestination&&Host.Target!=Other.Target)continue;
            // Require continuous walkable ground, including the endpoints; never merge across water.
            if(!WalkableSegment(Host.Position,Other.Position))continue;
            if(Distance>FMath::Square(SquadMergeContact))
            {
                if(RallyPoints&&!Host.Blocked&&!Other.Blocked&&Other.MarchDelay<=0&&Other.InterceptTarget==INDEX_NONE)
                    RallyPoints->Add(Other.ID,Host.Position);
                continue;
            }
            // Keep the chosen host's route, anchor, delay and ID. Field armies have no garrison cap.
            Host.Troops+=Other.Troops;Other.Troops=0;
            ReplacedIDs.Add(Other.ID,Host.ID);
        }
    }
    if(ReplacedIDs.IsEmpty())return;
    for(auto& S:Squads)
        if(const int32* Replacement=ReplacedIDs.Find(S.InterceptTarget))
        {S.InterceptTarget=*Replacement;S.RetargetIn=0;}
    Squads.RemoveAll([](const FVoxelSquad& S){return S.Troops<=0;});
}

void AVoxelBattle::UpdateInterception(float Dt)
{
    for(auto& S:Squads)
    {
        S.RetargetIn-=Dt;
        if(S.Troops<=0||Towers.IsValidIndex(S.Target)||!S.HasDestination||S.Blocked||S.Evacuating||IsMeteorLessonSquad(S))continue;
        const FVector Anchor=S.HasGroundDestination?S.GroundDestination:CellPosition(S.Destination);
        const bool Pursuing=S.InterceptTarget!=INDEX_NONE;
        if(!S.Holding&&!Pursuing)continue;
        if(S.RetargetIn>0)continue;
        S.RetargetIn=.3f;
        const float Radius=FMath::Max(0.f,GuardRadius);
        double Best=FMath::Square(Radius);
        int32 Enemy=INDEX_NONE;
        TArray<FVector> BestRoute;
        for(const FVoxelSquad& Candidate:Squads)
        {
            if(Candidate.Team==S.Team||Candidate.Troops<=0||IsMeteorLessonSquad(Candidate))continue;
            const double Distance=FVector::DistSquared2D(Candidate.Position,Anchor);
            if(Distance>=Best)continue;
            TArray<FVector> Route=FindGroundRoute(S.Position,Candidate.Position);
            if(Route.IsEmpty())continue;
            double Length=0;
            FVector Previous=S.Position;
            for(const FVector& Point:Route)
            {
                Length+=FVector::Dist2D(Previous,Point);
                Previous=Point;
            }
            if(Length>Radius*2)continue;
            Best=Distance;
            Enemy=Candidate.ID;
            BestRoute=MoveTemp(Route);
        }
        S.InterceptTarget=Enemy;
        if(Enemy!=INDEX_NONE)
        {
            S.Route=MoveTemp(BestRoute);
            S.Step=0;
            S.Holding=false;
        }
        else if(Pursuing)RepathSquad(S);
    }
}

void AVoxelBattle::FireTowers(float Dt)
{
    for(int32 I=0;I<Towers.Num();++I)
    {
        auto& T=Towers[I]; T.FireTimer-=Dt;T.ShotAge+=Dt;
        if(T.Module==EVoxelModule::Cannon)
        {
            T.SpinSpeed=FMath::FInterpTo(T.SpinSpeed,!T.Overheated&&T.ShotAge<.6f?720.f:0.f,Dt,8.f);
            T.BarrelAngle=FMath::Fmod(T.BarrelAngle+T.SpinSpeed*Dt,360.f);
        }
        T.Heat=FMath::Max(0.f,T.Heat-Dt*(T.Overheated?23:8));
        if(T.Overheated&&T.Heat<=18)T.Overheated=false;
        if(T.Team==0||T.Module==EVoxelModule::Village||T.FireTimer>0||T.Overheated)continue;
        if(T.Module==EVoxelModule::Arrow&&T.Arrow==EVoxelArrow::Unbound)continue;
        int32 Target=-1; float Closest=FLT_MAX; FVector Origin=CellPosition(T.Cell);
        for(int32 J=0;J<Squads.Num();++J)if(Squads[J].Team!=T.Team&&Squads[J].Troops>0)
        {float D=FVector::Dist2D(Origin,Squads[J].Position);if(D<TowerRange(I,Squads[J].Position)&&D<Closest&&Visible(Origin,Squads[J].Position)){Target=J;Closest=D;}}
        if(Target<0)continue;
        FVector Hit=Squads[Target].Position; float Damage=TowerDamage(I);
        if(T.Module==EVoxelModule::Arrow)
        {
            if(T.Arrow==EVoxelArrow::Poison)
            {FVoxelEffect E;E.Position=Hit;E.Radius=190;E.Remaining=4;E.Team=T.Team;E.DamagePerSecond=2.2f*(1+.2f*(T.Level-1));Effects.Add(E);Squads[Target].Troops-=Damage;}
            else
            {
                FVector Dir=(Hit-Origin).GetSafeNormal2D();
                for(auto& S:Squads)if(S.Team!=T.Team)
                {FVector Delta=S.Position-Origin;float Along=FVector::DotProduct(Delta,Dir);if(Along>0&&Along<TowerRange(I,S.Position)&&FVector::Dist2D(Origin+Dir*Along,S.Position)<75&&Visible(Origin,S.Position))S.Troops-=Damage*2.8f;}
            }
        }
        else
        {
            Squads[Target].Troops-=Damage;
            if(T.Module==EVoxelModule::Cannon){T.Heat=FMath::Min(100.f,T.Heat+17);if(T.Heat>=100)T.Overheated=true;}
        }
        T.FireTimer=TowerInterval(I);
        if(T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon)
        {T.AimTarget=Hit;T.AimSquad=Squads[Target].ID;T.HasAim=true;T.ShotAge=0;RecordWeaponShot(I,Hit);}
        else DrawDebugLine(GetWorld(),Origin+FVector(0,0,T.Tall?280:170),Hit,T.Team==1?FColor::Cyan:FColor::Orange,false,.22f,0,3);
    }
}
void AVoxelBattle::Simulate(float Dt)
{
    if(MenuPaused||Paused||Winner||Dt<=0||(Tutorial&&TutorialObjectiveComplete))return;
    if(TimeLimitEnabled&&Time>=BattleTimeLimit){ResolveTimeLimit();return;}
    if(TimeLimitEnabled)Dt=FMath::Min(Dt,BattleTimeLimit-Time);
    for(auto& Shot:WeaponShots)Shot.Age+=Dt;
    WeaponShots.RemoveAll([](const FVoxelWeaponShot& Shot){return Shot.Age>=Shot.FlightTime+.32f;});
    for(auto& Burst:MeteorBursts)Burst.Age+=Dt;
    MeteorBursts.RemoveAll([](const FVoxelMeteorBurst& Burst){return Burst.Age>=MeteorBurstLifetime;});
    Time+=Dt; MeteorReady=FMath::Max(0.f,MeteorReady-Dt);
    bool Opened=false;for(auto& B:Bridges)if(B.Remaining>0){B.Remaining=FMath::Max(0.f,B.Remaining-Dt);if(B.Remaining<.001f)B.Remaining=0;Opened|=B.Remaining<=0;}
    if(Opened){RepathArmies();Message(TEXT("桥梁已完工！双方可通行，被阻断的部队重新寻路。"));}
    float Recovery[3]={0,CommandRecovery(1),CommandRecovery(2)};
    for(int Team=1;Team<=2;++Team)Gold[Team]=FMath::Min(999.f,Gold[Team]+Dt*GoldRecovery(Team));
    for(int32 I=0;I<Towers.Num();++I) if(Towers[I].Team!=0)
    {
        auto& T=Towers[I];float Rate=T.Module==EVoxelModule::Command?Recovery[T.Team]:T.Module==EVoxelModule::Village?.12f:.22f;
        if(Tutorial&&T.Team==2)Rate=0;
        T.Troops=FMath::Min(TroopCapacity(I),T.Troops+Dt*Rate);
    }
    for(auto& S:Squads)
    {
        if(S.ID==INDEX_NONE)S.ID=NextSquadID++;
        S.PreviousPose={S.Position,S.VisualYaw,S.WalkPhase,S.MeleePhase,float(S.VisualMoving),float(S.VisualMelee)};
        S.HasPreviousPose=true;
    }
    UpdateInterception(Dt);
    TMap<int32,FVector> RallyPoints;
    MergeNearbySquads(&RallyPoints);
    TArray<bool> Fighting; Fighting.Init(false,Squads.Num());
    TArray<int32> MeleeEnemies;MeleeEnemies.Init(INDEX_NONE,Squads.Num());
    TArray<float> Loss; Loss.Init(0,Squads.Num());
    for(int I=0;I<Squads.Num();++I)for(int J=I+1;J<Squads.Num();++J)
        if(Squads[I].Troops>0&&Squads[J].Troops>0&&Squads[I].Team!=Squads[J].Team&&
           FVector::DistSquared2D(Squads[I].Position,Squads[J].Position)<FMath::Square(105.f)&&
           !IsMeteorLessonSquad(Squads[I])&&!IsMeteorLessonSquad(Squads[J])&&
           WalkableSegment(Squads[I].Position,Squads[J].Position))
        {
            if(Tutorial&&TutorialStep==6&&(Squads[I].ID==TutorialRaidID||Squads[J].ID==TutorialRaidID))TutorialEncounterSeen=true;
            Fighting[I]=Fighting[J]=true;
            if(MeleeEnemies[I]==INDEX_NONE||FVector::DistSquared2D(Squads[I].Position,Squads[J].Position)<FVector::DistSquared2D(Squads[I].Position,Squads[MeleeEnemies[I]].Position))MeleeEnemies[I]=J;
            if(MeleeEnemies[J]==INDEX_NONE||FVector::DistSquared2D(Squads[J].Position,Squads[I].Position)<FVector::DistSquared2D(Squads[J].Position,Squads[MeleeEnemies[J]].Position))MeleeEnemies[J]=I;
            Loss[I]+=FMath::Min(Squads[J].Troops,18.f)*.55f*Dt;Loss[J]+=FMath::Min(Squads[I].Troops,18.f)*.55f*Dt;
        }
    for(int I=0;I<Squads.Num();++I)Squads[I].Troops=FMath::Max(0.f,Squads[I].Troops-Loss[I]);
    for(int I=0;I<Squads.Num();++I)
    {
        auto& S=Squads[I];
        FVector Previous=S.Position,FacingTarget=MeleeEnemies[I]!=INDEX_NONE?Squads[MeleeEnemies[I]].Position:S.Position;
        float Wait=FMath::Min(S.MarchDelay,Dt);S.MarchDelay-=Wait;
        const FVector* Rally=S.InterceptTarget==INDEX_NONE?RallyPoints.Find(S.ID):nullptr;
        if(S.Troops<=0||Fighting[I]||(S.Holding&&!Rally)||(S.Blocked&&!S.Evacuating))
        {UpdateSoldierPose(S,Previous,FacingTarget,Fighting[I],Dt);continue;}
        float Remaining=SoldierSpeed*(Dt-Wait);
        if(Rally&&Remaining>0)
        {
            const FVector Direction=(*Rally-S.Position).GetSafeNormal2D();
            const double Distance=FVector::Dist2D(*Rally,S.Position);
            S.Position+=Direction*FMath::Min(Distance,double(Remaining*1.6f));
            const FIntPoint Cell=ToCell(S.Position);
            S.Position.Z=Height(Cell)+27;
            if(!IsRiver(Cell))S.LastLand=Cell;
            if(S.Holding)RepathSquad(S);
            UpdateSoldierPose(S,Previous,FacingTarget,false,Dt);
            continue;
        }
        if(!S.Evacuating&&Wait<Dt&&Towers.IsValidIndex(S.Target))
        {
            const FVoxelTower& Destination=Towers[S.Target];
            const FVector Center=CellPosition(Destination.Cell);
            if(Destination.Team==S.Team&&Destination.Troops+S.Troops>TroopCapacity(S.Target)&&
               FVector::DistSquared2D(S.Position,Center)<=FMath::Square(GarrisonApproachRadius+1)&&
               !IsRiver(ToCell(S.Position))&&WalkableSegment(S.Position,Center))
            {
                Arrive(S);
                UpdateSoldierPose(S,Previous,FacingTarget,false,Dt);
                continue;
            }
        }
        while(Remaining>0&&S.Step<S.Route.Num())
        {
            FVector D=S.Route[S.Step]-S.Position;D.Z=0;float Dist=D.Size();
            if(Dist<=Remaining){S.Position=S.Route[S.Step++];Remaining-=Dist;}else{S.Position+=D.GetSafeNormal()*Remaining;Remaining=0;}
            S.Position.Z=Height(ToCell(S.Position))+27;
        }
        FIntPoint Current=ToCell(S.Position);if(!IsRiver(Current)&&Walkable(Current))S.LastLand=Current;
        if(S.Step>=S.Route.Num())
        {
            if(S.Evacuating){S.Evacuating=false;RepathSquad(S);}
            else if(Towers.IsValidIndex(S.Target))Arrive(S);
            else S.Holding=true;
        }
        UpdateSoldierPose(S,Previous,FacingTarget,false,Dt);
        if(Winner)break;
    }
    if(Winner)return;
    FireTowers(Dt);
    for(auto& E:Effects)
    {
        E.Remaining-=Dt;
        if(E.Remaining<.0001f)E.Remaining=0;
        if(E.Meteor&&E.Remaining<=0)
        {
            for(auto& S:Squads)if(S.Troops>0&&FVector::Dist2D(S.Position,E.Position)<E.Radius){if(Tutorial&&TutorialStep==8){TutorialBlueHit|=S.ID==TutorialBlueDemoID;TutorialRedHit|=S.ID==TutorialRedDemoID;}S.Troops-=38;}
            DestroyBridges(E.Position,E.Radius);
            FVoxelMeteorBurst Burst;Burst.Position=E.Position;Burst.Radius=E.Radius;MeteorBursts.Add(Burst);
        }
        else if(!E.Meteor)for(auto& S:Squads)if(S.Team!=E.Team&&FVector::Dist2D(S.Position,E.Position)<E.Radius)S.Troops-=Dt*E.DamagePerSecond;
    }
    Effects.RemoveAll([](const FVoxelEffect& E){return E.Remaining<=0;});
    Squads.RemoveAll([](const FVoxelSquad& S){return S.Troops<=0;});
    if(TimeLimitEnabled&&Time+KINDA_SMALL_NUMBER>=BattleTimeLimit){ResolveTimeLimit();return;}
    if(AIEnabled)AISince+=Dt;if(AIEnabled&&Time>8&&AISince>=AIInterval){AISince=0;RunAI();}
}
void AVoxelBattle::SelectTower(int32 Index)
{
    Selected=Towers.IsValidIndex(Index)?Index:INDEX_NONE;
    SelectedSquad=INDEX_NONE;
    bTacticalThinking=Selected!=INDEX_NONE&&!Winner;
}
void AVoxelBattle::CompleteCommand(){bTacticalThinking=false;}
void AVoxelBattle::SelectSquad(int32 ID){Selected=INDEX_NONE;SelectedSquad=SquadIndex(ID)!=INDEX_NONE?ID:INDEX_NONE;bTacticalThinking=SelectedSquad!=INDEX_NONE&&!Winner;}
void AVoxelBattle::CancelCommand(){Selected=INDEX_NONE;SelectedSquad=INDEX_NONE;MeteorMode=false;BridgeMode=false;CompleteCommand();}
bool AVoxelBattle::IsThinking() const {return !Winner&&(BridgeMode||(bTacticalThinking&&(Towers.IsValidIndex(Selected)||SquadIndex(SelectedSquad)!=INDEX_NONE)));}
void AVoxelBattle::SetPlaybackSpeed(int32 Speed)
{
    if((Speed!=1&&Speed!=2)||MenuPaused||Winner||(Tutorial&&TutorialObjectiveComplete))return;
    PlaybackSpeed=Speed;TutorialFastForward=false;
}
float AVoxelBattle::GetBattleSpeed() const {return MenuPaused||Paused||Winner||(Tutorial&&TutorialObjectiveComplete)?0.f:IsThinking()?.2f:TutorialFastForward&&TutorialCanFastForward()?5.f:float(PlaybackSpeed);}
void AVoxelBattle::AdvanceBattle(float RealDeltaTime)
{
    const float Speed=GetBattleSpeed();
    if(Speed<=0)return;
    // Scale only simulation time: troops, weapons, economy, effects and AI share this clock.
    // Camera, input and UI continue to receive the unscaled real frame delta.
    Accumulator+=FMath::Clamp(RealDeltaTime,0.f,.25f)*Speed;
    while(Accumulator+SMALL_NUMBER>=.05f)
    {
        Accumulator=FMath::Max(0.f,Accumulator-.05f);
        Simulate(.05f);UpdateTutorial();
        if(Winner||(Tutorial&&TutorialObjectiveComplete)){Accumulator=0;break;}
    }
}
void AVoxelBattle::Tick(float Dt)
{
    Super::Tick(Dt);
    AdvanceBattle(Dt);UpdateTutorial();UpdateBattleAudio(Dt);
    DrawWorld();
    if(Tutorial&&!MenuPaused&&!Winner){FVector Goal=TutorialGoalPosition()+FVector(0,0,16);DrawDebugCircle(GetWorld(),Goal,130,40,FColor::Yellow,false,0,0,5,FVector(1,0,0),FVector(0,1,0),false);}
    const int32 GuardIndex=SquadIndex(SelectedSquad);
    if(!MenuPaused&&!Winner&&GuardRadius>0&&Squads.IsValidIndex(GuardIndex))
    {
        const FVoxelSquad& Guard=Squads[GuardIndex];
        if(Guard.HasDestination&&Guard.Target==INDEX_NONE&&!Guard.Blocked&&(Guard.Holding||Guard.InterceptTarget!=INDEX_NONE))
        {
            const FVector Anchor=Guard.HasGroundDestination?Guard.GroundDestination:CellPosition(Guard.Destination);
            const FColor Color=Guard.Team==1?FColor(84,193,255):FColor(255,114,93);
            auto EdgePoint=[&](int32 Index)
            {
                const float Angle=Index*2.f*PI/48;
                FVector Point=Anchor+GuardRadius*FVector(FMath::Cos(Angle),FMath::Sin(Angle),0);
                Point.Z=Height(ToCell(Point))+22;
                return Point;
            };
            for(int32 Segment=0;Segment<48;Segment+=2)
                DrawDebugLine(GetWorld(),EdgePoint(Segment),EdgePoint(Segment+1),Color,false,0,0,3);
        }
    }
    UpdateRangeOverlay();
}

AVoxelGameMode::AVoxelGameMode()
{PlayerControllerClass=AVoxelCommander::StaticClass();HUDClass=AVoxelHUD::StaticClass();DefaultPawnClass=nullptr;}
void AVoxelGameMode::BeginPlay()
{
    Super::BeginPlay();
    auto* PreviewBattle=GetWorld()->SpawnActor<AVoxelBattle>();PreviewBattle->MenuPaused=true;
    auto* Sun=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,2400),FRotator(-55,-35,0));
    Sun->GetLightComponent()->SetIntensity(3.5f);
    Cast<UDirectionalLightComponent>(Sun->GetLightComponent())->SetForwardShadingPriority(1);
    auto* Fill=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,2400),FRotator(-40,140,0));
    Fill->GetLightComponent()->SetIntensity(1.5f);
    Fill->GetLightComponent()->SetCastShadows(false);
    // A soft light from the strategy camera keeps faces and shields readable
    // underneath helmets, including on the D3D11 preview path without Lumen.
    auto* CharacterFill=GetWorld()->SpawnActor<ADirectionalLight>(FVector(0,0,2400),FRotator(-20,50,0));
    CharacterFill->GetLightComponent()->SetIntensity(1.25f);
    CharacterFill->GetLightComponent()->SetCastShadows(false);
#if !UE_BUILD_SHIPPING
    float PreviewAge=0;if(FParse::Value(FCommandLine::Get(),TEXT("MeteorPreview="),PreviewAge))
        if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))P->SetupMeteorPreview(PreviewAge);
    if(FParse::Value(FCommandLine::Get(),TEXT("PoisonPreview="),PreviewAge))
        if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))P->SetupPoisonPreview(PreviewAge);
    int WeaponStage=0;if(FParse::Value(FCommandLine::Get(),TEXT("WeaponPreview="),WeaponStage))
        if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))P->SetupWeaponPreview(WeaponStage);
    int SoldierStage=0;if(FParse::Value(FCommandLine::Get(),TEXT("SoldierPreview="),SoldierStage))
        if(auto* P=Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()))P->SetupSoldierPreview(SoldierStage);
#endif
}

AVoxelBattle* AVoxelCommander::Battle() const
{for(TActorIterator<AVoxelBattle> I(GetWorld());I;++I)return *I;return nullptr;}
void AVoxelCommander::BeginPlay()
{
    Super::BeginPlay(); bShowMouseCursor=true;
    LoadPreferences();
    if(auto* B=Battle())B->MenuPaused=IsMenuOpen();
    FInputModeGameAndUI Mode;Mode.SetHideCursorDuringCapture(false);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);
    BattleCamera=GetWorld()->SpawnActor<ACameraActor>();
    BattleCamera->GetCameraComponent()->SetProjectionMode(ECameraProjectionMode::Orthographic);
    BattleCamera->GetCameraComponent()->SetConstraintAspectRatio(false);
    BattleCamera->GetCameraComponent()->SetOrthoWidth(Zoom);
    BattleCamera->GetCameraComponent()->SetAutoCalculateOrthoPlanes(true);
    auto& Exposure=BattleCamera->GetCameraComponent()->PostProcessSettings;
    Exposure.bOverride_AutoExposureMinBrightness=true;Exposure.bOverride_AutoExposureMaxBrightness=true;
    Exposure.AutoExposureMinBrightness=1.5f;Exposure.AutoExposureMaxBrightness=1.5f;
    Exposure.bOverride_AutoExposureBias=true;Exposure.AutoExposureBias=0;
    SetViewTarget(BattleCamera);
}
void AVoxelCommander::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&AVoxelCommander::LeftClick);
    InputComponent->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&AVoxelCommander::RightClick);
    InputComponent->BindKey(EKeys::B,IE_Pressed,this,&AVoxelCommander::BridgeKey);
    InputComponent->BindKey(EKeys::U,IE_Pressed,this,&AVoxelCommander::UpgradeKey);
    InputComponent->BindKey(EKeys::M,IE_Pressed,this,&AVoxelCommander::MeteorKey);
    InputComponent->BindKey(EKeys::One,IE_Pressed,this,&AVoxelCommander::PoisonKey);
    InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&AVoxelCommander::PierceKey);
    InputComponent->BindKey(EKeys::Tab,IE_Pressed,this,&AVoxelCommander::FractionKey);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&AVoxelCommander::PauseKey);
    InputComponent->BindKey(EKeys::Home,IE_Pressed,this,&AVoxelCommander::OverviewKey);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&AVoxelCommander::CancelKey);
    InputComponent->BindKey(EKeys::R,IE_Pressed,this,&AVoxelCommander::RestartKey);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&AVoxelCommander::ConfirmKey);
#if !UE_BUILD_SHIPPING
    float PreviewAge=0;if(FParse::Value(FCommandLine::Get(),TEXT("MeteorPreview="),PreviewAge))InputComponent->BindKey(EKeys::F8,IE_Pressed,this,&AVoxelCommander::PreviewMeteorKey);
    if(FParse::Value(FCommandLine::Get(),TEXT("PoisonPreview="),PreviewAge))InputComponent->BindKey(EKeys::F8,IE_Pressed,this,&AVoxelCommander::PreviewPoisonKey);
    int WeaponStage=0;if(FParse::Value(FCommandLine::Get(),TEXT("WeaponPreview="),WeaponStage))InputComponent->BindKey(EKeys::F8,IE_Pressed,this,&AVoxelCommander::PreviewWeaponKey);
    int SoldierStage=0;if(FParse::Value(FCommandLine::Get(),TEXT("SoldierPreview="),SoldierStage))InputComponent->BindKey(EKeys::F8,IE_Pressed,this,&AVoxelCommander::PreviewSoldierKey);
#endif
    InputComponent->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&AVoxelCommander::ZoomIn);
    InputComponent->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&AVoxelCommander::ZoomOut);
}
void AVoxelCommander::PlayerTick(float Dt)
{
    Super::PlayerTick(Dt); if(!BattleCamera)return;
    if(IsMenuOpen())
    {
        float T=GetWorld()->GetRealTimeSeconds();FVector Preview(650+FMath::Sin(T*.055f)*150,450+FMath::Cos(T*.055f)*120,0);
        BattleCamera->SetActorLocation(Preview+FVector(-3300,-4000,5200));BattleCamera->SetActorRotation((Preview-BattleCamera->GetActorLocation()).Rotation());
        BattleCamera->GetCameraComponent()->SetOrthoWidth(Battle()&&Battle()->Tutorial?13500:21000);return;
    }
    FVector Forward=FVector(3300,4000,0).GetSafeNormal(),Right=FVector(-Forward.Y,Forward.X,0),D=FVector::ZeroVector;
    if(IsInputKeyDown(EKeys::W))D+=Forward;if(IsInputKeyDown(EKeys::S))D-=Forward;
    if(IsInputKeyDown(EKeys::D))D+=Right;if(IsInputKeyDown(EKeys::A))D-=Right;
    Focus+=D.GetSafeNormal()*Dt*CameraSpeed*(IsInputKeyDown(EKeys::LeftShift)?3600:1900);
    Focus.X=FMath::Clamp(Focus.X,-AVoxelBattle::W/2*AVoxelBattle::Tile,AVoxelBattle::W/2*AVoxelBattle::Tile);
    Focus.Y=FMath::Clamp(Focus.Y,-AVoxelBattle::H/2*AVoxelBattle::Tile,AVoxelBattle::H/2*AVoxelBattle::Tile);
    BattleCamera->SetActorLocation(Focus+FVector(-3300,-4000,SoldierPreviewActive?3000:5200));BattleCamera->SetActorRotation((Focus-BattleCamera->GetActorLocation()).Rotation());
    BattleCamera->GetCameraComponent()->SetOrthoWidth(Zoom);
    if(auto* B=Battle();B&&B->MeteorMode)
        DrawDebugCircle(GetWorld(),CursorWorld()+FVector(0,0,25),340,48,FColor::Orange,false,0,0,4,FVector(1,0,0),FVector(0,1,0),false);
    if(auto* B=Battle();B&&B->BridgeMode)
    {
        FIntPoint C=B->ToCell(CursorWorld());FVoxelBridge Site;
        if(B->BridgeSiteAt(C,Site))
        {
            bool Clear=true;FIntPoint BridgeDirection=Site.Vertical?FIntPoint(0,1):FIntPoint(1,0);
            for(int I=0;I<Site.Length;++I)Clear&=B->BridgeIndexAt(FIntPoint(Site.Column,Site.Row)+BridgeDirection*I)==INDEX_NONE;
            FVector Extent=Site.Vertical?FVector(80,Site.Length*B->Tile*.5f+20,10):FVector(Site.Length*B->Tile*.5f+20,80,10);
            DrawDebugBox(GetWorld(),B->BridgeCenter(Site)+FVector(0,0,12),Extent,Clear&&B->Gold[1]>=B->BridgeCost?FColor::Green:FColor::Orange,false,0,0,4);
        }
        else if(B->ContainsCell(C))DrawDebugBox(GetWorld(),B->CellPosition(C)+FVector(0,0,12),FVector(80,80,10),FColor::Red,false,0,0,3);
    }
}
FVector AVoxelCommander::CursorWorld() const
{
    FVector O,D; if(!DeprojectMousePositionToWorld(O,D)||FMath::Abs(D.Z)<.001f)return FVector::ZeroVector;
    FVector P=O+D*((80-O.Z)/D.Z);
    if(auto* B=Battle())P=O+D*((B->Height(B->ToCell(P))-O.Z)/D.Z);
    return P;
}
void AVoxelCommander::LeftClick()
{
    float X,Y;GetMousePosition(X,Y);if(auto* H=Cast<AVoxelHUD>(GetHUD());H&&H->HandleClick(X,Y))return;
    auto* B=Battle();if(!B)return;
    if(B->BridgeMode)
    {
        FIntPoint C=B->ToCell(CursorWorld());
        B->BuildBridgeAt(C);return;
    }
    if(B->MeteorMode){B->Meteor(CursorWorld());return;}
    int32 Hit=TowerUnderCursor(FVector2D(X,Y));
    int32 Unit=INDEX_NONE;float UnitBest=35;
    for(const auto& S:B->Squads)if(S.Troops>0)
    {
        FVector2D Label,Body;ProjectWorldLocationToScreen(B->SquadLabelPosition(S),Label);ProjectWorldLocationToScreen(B->SoldierRenderPose(S).Position+FVector(0,0,35),Body);
        float D=FMath::Min(FVector2D::Distance(Label,FVector2D(X,Y)),FVector2D::Distance(Body,FVector2D(X,Y)));
        if(D<UnitBest){UnitBest=D;Unit=S.ID;}
    }
    if(Unit!=INDEX_NONE)B->SelectSquad(Unit);else B->SelectTower(Hit);
}
void AVoxelCommander::RightClick()
{
    if(IsMenuOpen())return;
    auto* B=Battle();if(!B)return;if(B->MeteorMode||B->BridgeMode){B->CancelCommand();return;}
    int32 Unit=B->SquadIndex(B->SelectedSquad);
    bool HasTower=B->Towers.IsValidIndex(B->Selected)&&B->Towers[B->Selected].Team==1;
    if(!HasTower&&(Unit==INDEX_NONE||B->Squads[Unit].Team!=1))return;
    float X,Y;GetMousePosition(X,Y);int VX,VY;GetViewportSize(VX,VY);float S=FMath::Min(VX/1440.f,VY/900.f);if(Y<90*S||Y>VY-104*S||X>VX-282*S)return;
    if(auto* H=Cast<AVoxelHUD>(GetHUD());H&&H->IsTreasuryHit(X,Y))return;
    int32 Hit=TowerUnderCursor(FVector2D(X,Y));
    if(Unit!=INDEX_NONE)B->OrderSquad(B->SelectedSquad,Hit,CursorWorld());
    else if(Hit>=0)B->Dispatch(B->Selected,Hit,B->DispatchFraction);else B->DispatchTo(B->Selected,CursorWorld(),B->DispatchFraction);
}
void AVoxelCommander::BridgeKey(){if(IsMenuOpen())return;if(auto* B=Battle();B&&!B->Winner&&!B->Paused){B->BridgeMode=!B->BridgeMode;B->MeteorMode=false;if(B->BridgeMode)B->Message(TEXT("左键点击较窄河段，自动跨接两岸：75金、施工60秒。右键或 Esc 取消。"));}}
void AVoxelCommander::MeteorKey(){if(IsMenuOpen())return;if(auto* B=Battle();B&&!B->Winner&&!B->Paused){B->MeteorMode=!B->MeteorMode;B->BridgeMode=false;}}
void AVoxelCommander::PoisonKey(){if(IsMenuOpen())return;if(auto* B=Battle())B->BindArrow(EVoxelArrow::Poison);}
void AVoxelCommander::PierceKey(){if(IsMenuOpen())return;if(auto* B=Battle())B->BindArrow(EVoxelArrow::Pierce);}
void AVoxelCommander::FractionKey(){if(IsMenuOpen())return;if(auto* B=Battle()){B->DispatchFraction=B->DispatchFraction<.7f?.75f:B->DispatchFraction<.9f?1.f:.5f;if(B->Towers.IsValidIndex(B->Selected))B->SelectTower(B->Selected);else if(B->SquadIndex(B->SelectedSquad)!=INDEX_NONE)B->SelectSquad(B->SelectedSquad);}}
void AVoxelCommander::PauseKey(){if(IsMenuOpen())return;if(auto* B=Battle();B&&!B->Winner)B->Paused=!B->Paused;}
void AVoxelCommander::RestartKey(){if(IsMenuOpen())return;StartBattle();}
void AVoxelCommander::ZoomIn(){if(IsMenuOpen())return;Zoom=SoldierPreviewActive?FMath::Max(450.f,Zoom-150):FMath::Max(3200.f,Zoom-600);}
void AVoxelCommander::ZoomOut(){if(IsMenuOpen())return;Zoom=SoldierPreviewActive?FMath::Min(2800.f,Zoom+150):FMath::Min(23000.f,Zoom+600);}


void AVoxelCommander::OverviewKey(){if(IsMenuOpen())return;if(auto* B=Battle();B&&B->Tutorial){Focus=FVector(0,200,0);Zoom=7200;return;}Focus=FVector(-1500,1800,0);Zoom=12800;}
void AVoxelCommander::CancelKey()
{
    if(IsMenuOpen()){if(MenuPage==EVoxelMenuPage::Main)ResumeBattle();else ShowMenu();return;}
    if(auto* B=Battle())
    {
        if(B->Selected!=INDEX_NONE||B->SelectedSquad!=INDEX_NONE||B->MeteorMode||B->BridgeMode){B->CancelCommand();return;}
        ShowMenu();
    }
}
void AVoxelCommander::UpgradeKey(){if(IsMenuOpen())return;if(auto* B=Battle())B->UpgradeTower(B->Selected);}
