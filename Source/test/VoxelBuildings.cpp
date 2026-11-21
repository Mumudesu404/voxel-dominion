#include "VoxelWar.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
enum { Stone,LightStone,StoneShade,Slate,Wood,WoodLight,Iron,TileRed,TileLight,Brass,Window,Grass,Wheat,BlueTrim,RedTrim,NeutralTrim };
}
float AVoxelBattle::TowerLabelHeight(int32 I) const
{
    const auto& T=Towers[I];
    return T.Module==EVoxelModule::Command?580.f:T.Module==EVoxelModule::Village?320.f:T.Module==EVoxelModule::Arrow?(T.Tall?490.f:420.f):T.Tall?535.f:475.f;
}
void AVoxelBattle::InitializeReferenceBuildings()
{
    const FLinearColor Colors[]={
        {.66f,.60f,.48f},{.83f,.77f,.64f},{.47f,.44f,.37f},{.16f,.18f,.18f},
        {.26f,.115f,.045f},{.43f,.23f,.09f},{.12f,.13f,.12f},{.57f,.125f,.045f},
        {.76f,.23f,.075f},{.95f,.56f,.065f},{.045f,.055f,.05f},{.35f,.48f,.15f},
        {.88f,.62f,.12f},{.035f,.24f,.57f},{.62f,.08f,.035f},{.62f,.45f,.16f}};
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Voxel/Materials/M_Voxel.M_Voxel"));
    for(const auto& Color:Colors)
    {
        auto* Batch=NewObject<UInstancedStaticMeshComponent>(this);Batch->SetupAttachment(RootComponent);
        Batch->SetStaticMesh(Cube);Batch->SetCollisionEnabled(ECollisionEnabled::NoCollision);Batch->SetCastShadow(true);
        auto* MID=UMaterialInstanceDynamic::Create(Material,this);MID->SetVectorParameterValue(TEXT("Tint"),Color);
        Batch->SetMaterial(0,MID);Batch->RegisterComponent();BuildingMaterials.Add(MID);BuildingBatches.Add(Batch);
    }
}
bool AVoxelBattle::DrawReferenceBuilding(int32 Index)
{
    const auto& T=Towers[Index];if(BuildingBatches.IsEmpty())return false;
    FVector Origin=CellPosition(T.Cell);int Team=T.Team==1?BlueTrim:T.Team==2?RedTrim:NeutralTrim;
    auto B=[&](int Color,FVector P,FVector Size,FQuat Rotation=FQuat::Identity)
    {BuildingBatches[Color]->AddInstance(FTransform(Rotation,Origin+P,Size/100.f));};
    auto Beam=[&](int Color,FVector A,FVector End,float Width,float Depth)
    {FVector D=End-A;B(Color,(A+End)*.5f,FVector(Width,Depth,D.Size()),FQuat::FindBetweenNormals(FVector::UpVector,D.GetSafeNormal()));};
    auto Flag=[&](FVector P,float Scale=1.f)
    {
        B(Wood,P+FVector(0,0,45*Scale),FVector(6,6,100)*Scale);
        for(int K=0;K<4;++K)B(Team,P+FVector((13+K*15)*Scale,FMath::Sin(K*1.3f)*4*Scale,(76-K*5)*Scale),FVector(17,5,30-K*3)*Scale);
        B(Brass,P+FVector(0,0,99*Scale),FVector(8,8,8)*Scale);
    };
    auto Masonry=[&](FVector P,float Width,float Depth,float Height)
    {
        B(StoneShade,P+FVector(0,0,Height*.5f),FVector(Width,Depth,Height));
        int Rows=FMath::CeilToInt(Height/24.f),Cols=FMath::CeilToInt(Width/29.f);
        for(int R=0;R<Rows;++R)for(int C=0;C<Cols;++C)
        {
            float CW=Width/Cols,CH=Height/Rows,X=-Width*.5f+(C+.5f)*CW,Z=(R+.5f)*CH;
            B((R+C)%5==0?LightStone:Stone,P+FVector(X,0,Z),FVector(CW-.7f,Depth+.4f,CH-.7f));
        }
    };
    auto Roof=[&](FVector P,float Width,float Depth,int Color,int Steps=6)
    {
        for(int R=0;R<Steps;++R)
        {
            float RW=Width*(1.f-float(R)/(Steps+1));
            B(R%3==0&&Color==TileRed?TileLight:Color,P+FVector(0,0,R*14),FVector(RW,Depth,14));
            for(int S:{-1,1})for(int K=0;K<4;++K)
                B(Color==TileRed?TileLight:Color,P+FVector(S*(RW*.5f-6),-Depth*.5f+(K+.5f)*Depth/4,R*14+2),FVector(12,Depth/4-.8f,12));
        }
    };
    auto WindowFront=[&](FVector P,float Width=20.f,float Height=30.f)
    {
        B(StoneShade,P,FVector(Width+9,5,Height+9));B(Window,P+FVector(0,-3,0),FVector(Width,3,Height));
        B(LightStone,P+FVector(0,-5,-Height*.5f-4),FVector(Width+13,9,7));
    };
    float Footprint=T.Module==EVoxelModule::Command?420:T.Module==EVoxelModule::Village?440:T.Module==EVoxelModule::Cannon?260:210;
    if(T.Module==EVoxelModule::Arrow||T.Module==EVoxelModule::Cannon)
    {
        bool Cannon=T.Module==EVoxelModule::Cannon;
        float Height=T.Tall?230.f:Cannon?180.f:158.f,Width=Cannon?(T.Tall?116.f:146.f):(T.Tall?94.f:126.f);
        B(Slate,FVector(0,0,9),Cannon?FVector(238,222,18):FVector(194,180,18));Masonry(FVector(0,0,18),Cannon?210:168,Cannon?194:154,25);
        Masonry(FVector(0,0,43),Cannon?180:142,Cannon?166:128,27);Masonry(FVector(0,0,70),Width,Width,Height-70);
        for(int S:{-1,1})B(StoneShade,FVector(S*(Width*.5f-5),0,(Height+70)*.5f),FVector(13,Width+5,Height-70));
        WindowFront(FVector(0,-Width*.5f-2,Height*.58f),22,Height*.28f);
        Beam(StoneShade,FVector(-16,-Width*.5f-4,Height*.73f),FVector(0,-Width*.5f-4,Height*.81f),8,5);
        Beam(StoneShade,FVector(0,-Width*.5f-4,Height*.81f),FVector(16,-Width*.5f-4,Height*.73f),8,5);
        for(int S:{-1,1})
        {
            B(StoneShade,FVector(S*(Width*.5f+2),0,Height*.58f),FVector(5,31,Height*.28f+9));
            B(Window,FVector(S*(Width*.5f+5),0,Height*.58f),FVector(3,22,Height*.28f));
            B(LightStone,FVector(S*(Width*.5f+7),0,Height*.44f-4),FVector(9,35,7));
            Beam(StoneShade,FVector(S*(Width*.5f+5),-16,Height*.73f),FVector(S*(Width*.5f+5),0,Height*.81f),8,5);
            Beam(StoneShade,FVector(S*(Width*.5f+5),0,Height*.81f),FVector(S*(Width*.5f+5),16,Height*.73f),8,5);
        }
        B(Team,FVector(0,0,Height-8),FVector(Width+4,Width+4,22));
        B(StoneShade,FVector(0,0,Height+10),FVector(Width+25,Width+25,14));
        B(LightStone,FVector(0,0,Height+24),FVector(Width+45,Width+45,18));
        for(int S:{-1,1})for(int K=-1;K<=1;++K)
        {B(Stone,FVector(K*(Width+20)/3,S*(Width+30)*.5f,Height+48),FVector(25,22,30));B(LightStone,FVector(S*(Width+30)*.5f,K*(Width+20)/3,Height+48),FVector(22,25,30));}
        Flag(FVector(Width*.5f+18,15,Height+8),.7f);
    }
    else if(T.Module==EVoxelModule::Village)
    {
        B(StoneShade,FVector(0,0,5),FVector(424,330,10));B(Grass,FVector(0,0,13),FVector(420,326,10));
        auto House=[&](FVector P,float S)
        {
            B(Stone,P+FVector(0,0,13*S),FVector(114,106,20)*S);
            B(LightStone,P+FVector(0,0,71*S),FVector(109,99,102)*S);
            Roof(P+FVector(0,0,125*S),138*S,126*S,TileRed,5);
            B(Wood,P+FVector(0,-51*S,48*S),FVector(30,5,57)*S);
            for(int J=-1;J<=1;++J)B(WoodLight,P+FVector(J*9*S,-55*S,48*S),FVector(7,3,52)*S);
            B(Iron,P+FVector(8,-58,47)*S,FVector(4,3,7)*S);
            for(int Side:{-1,1})
            {
                B(Brass,P+FVector(Side*36,-51,81)*S,FVector(22,5,25)*S);
                for(int X:{-1,1})for(int Z:{-1,1})B(Window,P+FVector(Side*36+X*5,-54,81+Z*6)*S,FVector(7,2,8)*S);
            }
            B(TileRed,P+FVector(28,24,184)*S,FVector(22,24,48)*S);B(TileLight,P+FVector(28,24,211)*S,FVector(29,29,9)*S);
        };
        House(FVector(0,17,19),1.f);House(FVector(-137,78,19),.65f);House(FVector(145,75,19),.67f);
        for(int Y=0;Y<5;++Y)for(int X=-1;X<=1;++X)B((X+Y)%3?Stone:StoneShade,FVector(X*21,-53-Y*18,22),FVector(20,17,5));
        B(Wood,FVector(-130,-80,23),FVector(105,103,8));
        for(int X=0;X<6;++X)for(int Y=0;Y<5;++Y)
        {FVector P(-171+X*16,-120+Y*19,32);B(Wheat,P+FVector(0,0,13),FVector(5,5,27));B(Brass,P+FVector(0,0,28+(X+Y)%3*3),FVector(10,9,14));}
        auto Crate=[&](FVector P)
        {B(WoodLight,P,FVector(31,29,28));for(int S:{-1,1}){B(Wood,P+FVector(S*12,-15,0),FVector(4,3,28));B(Wood,P+FVector(0,-15,S*10),FVector(30,3,4));}B(Wood,P+FVector(0,0,15),FVector(4,29,3));};
        for(int K=0;K<7;++K)Crate(FVector(99+(K==6?1:K%3)*33,-116+(K==6?0:K/3)*34,39+(K==6?28:0)));
        for(int K=0;K<2;++K)
        {
            FVector P(174,-45+K*37,39);B(WoodLight,P,FVector(30,22,32));B(Wood,P,FVector(22,30,32));
            for(int S:{-1,1})B(Iron,P+FVector(0,0,S*10),FVector(31,23,4));
        }
        for(int Side:{-1,1})
        {
            for(int K=0;K<6;++K){float X=-195+K*78;if(FMath::Abs(X)>35)B(WoodLight,FVector(X,Side*150,39),FVector(9,9,44));}
            for(int S:{-1,1})B(Wood,FVector(S*115,Side*150,40),FVector(150,6,9));
            for(int K=0;K<5;++K)B(WoodLight,FVector(Side*198,-145+K*72,39),FVector(9,9,44));
            B(Wood,FVector(Side*198,0,40),FVector(6,290,9));
        }
        Flag(FVector(75,36,158),.8f);
    }
    else
    {
        B(Slate,FVector(0,0,10),FVector(410,370,20));B(StoneShade,FVector(0,0,24),FVector(390,350,9));
        // Courtyard, four corner towers, curtain walls and an open gate passage.
        B(Stone,FVector(0,0,32),FVector(350,310,9));
        Masonry(FVector(0,145,35),300,24,100);B(Team,FVector(0,145,113),FVector(302,26,17));
        for(int K=-4;K<=4;++K)B(LightStone,FVector(K*32,145,148),FVector(20,29,27));
        for(int S:{-1,1})
        {
            Masonry(FVector(S*165,0,35),24,260,100);
            B(Team,FVector(S*165,0,113),FVector(26,262,17));
            Masonry(FVector(S*108,-145,35),98,28,100);B(Team,FVector(S*108,-145,113),FVector(100,30,17));
            for(int K=-4;K<=4;++K)B(LightStone,FVector(S*165,K*30,148),FVector(29,18,27));
            for(int K=2;K<=4;++K)B(LightStone,FVector(S*K*32,-145,148),FVector(20,29,27));
        }
        for(int X:{-1,1})for(int Y:{-1,1})
        {
            FVector P(X*162,Y*142,35);Masonry(P,66,66,164);B(Team,P+FVector(0,0,105),FVector(68,68,18));
            WindowFront(P+FVector(0,-35,128),14,40);B(StoneShade,P+FVector(0,0,170),FVector(80,80,14));Roof(P+FVector(0,0,184),73,75,Team,3);
        }
        Masonry(FVector(-53,-149,35),28,45,99);Masonry(FVector(53,-149,35),28,45,99);
        B(StoneShade,FVector(0,-149,131),FVector(106,48,27));
        for(int S:{-1,1})Beam(StoneShade,FVector(S*40,-174,111),FVector(S*27,-174,132),13,12);
        B(Wood,FVector(0,-157,72),FVector(76,12,75));for(int K=-3;K<=3;++K)B(WoodLight,FVector(K*10,-164,72),FVector(8,4,70));
        for(int Z:{51,89})B(Iron,FVector(0,-168,Z),FVector(72,4,6));for(int S:{-1,1})B(Iron,FVector(S*8,-171,72),FVector(5,4,9));
        Masonry(FVector(0,23,36),142,132,244);B(Team,FVector(0,23,175),FVector(150,140,16));
        B(StoneShade,FVector(0,23,281),FVector(162,151,18));Roof(FVector(0,23,297),178,167,Team,5);
        for(int Z:{125,237})for(int X:{-1,1})WindowFront(FVector(X*40,-45,Z),18,38);
        for(int S:{-1,1}){Masonry(FVector(S*104,47,36),54,74,166);Roof(FVector(S*104,47,211),76,93,Team,3);}
        B(Team,FVector(0,-52,227),FVector(20,6,75));B(Brass,FVector(0,-57,269),FVector(32,8,5));
        B(Slate,FVector(0,23,372),FVector(54,52,12));B(Brass,FVector(0,23,392),FVector(29,29,28));B(LightStone,FVector(0,7,394),FVector(13,3,13));
        Flag(FVector(0,23,410),1.f);
    }
    for(int L=1;L<T.Level;++L)B(Brass,FVector(-45+(L-1)*30,-Footprint*.5f-5,23),FVector(19,8,12));
    if(Index==Selected)for(int S:{-1,1}){B(Brass,FVector(S*Footprint*.5f,0,15),FVector(5,Footprint,6));B(Brass,FVector(0,S*Footprint*.5f,15),FVector(Footprint,5,6));}
    return true;
}

int32 AVoxelCommander::TowerUnderCursor(FVector2D Cursor) const
{
    const auto* B=Battle();if(!B)return INDEX_NONE;
    int32 Hit=INDEX_NONE;double Best=DBL_MAX;
    for(int32 I=0;I<B->Towers.Num();++I)
    {
        const auto& T=B->Towers[I];FVector P=B->CellPosition(T.Cell);
        FVector Extent=T.Module==EVoxelModule::Command?FVector(220,200,515):T.Module==EVoxelModule::Village?FVector(220,175,270):T.Module==EVoxelModule::Arrow?FVector(195,195,T.Tall?430:358):FVector(245,245,T.Tall?470:420);
        FBox2D Rect(ForceInit);
        for(int X:{-1,1})for(int Y:{-1,1})for(int Z:{0,1})
        {FVector2D S;if(ProjectWorldLocationToScreen(P+FVector(X*Extent.X,Y*Extent.Y,Z*Extent.Z),S))Rect+=S;}
        FVector2D Label,Center;
        if(!ProjectWorldLocationToScreen(P+FVector(0,0,B->TowerLabelHeight(I)),Label)||!ProjectWorldLocationToScreen(P+FVector(0,0,Extent.Z*.5),Center))continue;
        Rect+=Label-FVector2D(25,15);Rect+=Label+FVector2D(25,15);
        double Score=FVector2D::DistSquared(Cursor,Center);
        if(Rect.ExpandBy(6).IsInside(Cursor)&&Score<Best){Best=Score;Hit=I;}
    }
    return Hit;
}
