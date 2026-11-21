#include "VoxelWar.h"

namespace
{
constexpr float GroundClearance=24.f;
int CellID(FIntPoint C){return (C.Y+AVoxelBattle::H/2)*AVoxelBattle::W+C.X+AVoxelBattle::W/2;}

bool SegmentHitsBox(FVector A,FVector B,FVector Center,double Half)
{
    double Enter=0,Leave=1;
    for(int Axis=0;Axis<2;++Axis)
    {
        double D=B[Axis]-A[Axis],Min=Center[Axis]-Half,Max=Center[Axis]+Half;
        if(FMath::Abs(D)<1.e-9){if(A[Axis]<Min||A[Axis]>Max)return false;continue;}
        double Near=(Min-A[Axis])/D,Far=(Max-A[Axis])/D;if(Near>Far)Swap(Near,Far);
        Enter=FMath::Max(Enter,Near);Leave=FMath::Min(Leave,Far);if(Enter>Leave)return false;
    }
    return true;
}
}

bool AVoxelBattle::NavigationCell(FIntPoint C) const
{
    return FMath::Abs(C.X)<=W/2&&FMath::Abs(C.Y)<=H/2&&NavigationWalkable.IsValidIndex(CellID(C))&&NavigationWalkable[CellID(C)]!=0;
}

void AVoxelBattle::UpdateNavigation() const
{
    uint32 Signature=Tutorial?1:2;
    for(const auto& B:Bridges)
    {
        Signature=HashCombineFast(Signature,GetTypeHash(FIntPoint(B.Column,B.Row)));
        Signature=HashCombineFast(Signature,uint32(B.Length*4+B.Vertical*2+(B.Remaining<=0)));
    }
    if(LastNavigationSignature==Signature&&NavigationRegions.Num()==W*H&&NavigationWalkable.Num()==W*H)return;
    LastNavigationSignature=Signature;NavigationRegions.Init(0,W*H);NavigationWalkable.Init(0,W*H);
    for(int Y=-MapHalfHeight();Y<=MapHalfHeight();++Y)for(int X=-MapHalfWidth();X<=MapHalfWidth();++X)
        NavigationWalkable[CellID(FIntPoint(X,Y))]=Walkable(FIntPoint(X,Y));
    int Region=0;const FIntPoint Directions[]={{1,0},{-1,0},{0,1},{0,-1}};
    for(int Y=-MapHalfHeight();Y<=MapHalfHeight();++Y)for(int X=-MapHalfWidth();X<=MapHalfWidth();++X)
    {
        FIntPoint C(X,Y);if(!NavigationCell(C)||NavigationRegions[CellID(C)])continue;
        TArray<FIntPoint> Queue;Queue.Add(C);NavigationRegions[CellID(C)]=++Region;
        for(int Head=0;Head<Queue.Num();++Head)for(FIntPoint D:Directions)
        {
            FIntPoint N=Queue[Head]+D;if(!NavigationCell(N)||NavigationRegions[CellID(N)])continue;
            NavigationRegions[CellID(N)]=Region;Queue.Add(N);
        }
    }
    NavigationCorners.Empty();NavigationCornerRegions.Empty();
    // Only reflex corners of the walkable area can bend a shortest path.
    // Offset them from the water/coast so a diagonal never clips a blocked tile.
    for(int Y=-MapHalfHeight()-1;Y<=MapHalfHeight();++Y)for(int X=-MapHalfWidth()-1;X<=MapHalfWidth();++X)
    {
        int Blocked=0,BX=0,BY=0;
        for(int DY=0;DY<=1;++DY)for(int DX=0;DX<=1;++DX)if(!NavigationCell(FIntPoint(X+DX,Y+DY))){++Blocked;BX=DX;BY=DY;}
        if(Blocked!=1)continue;
        FVector P((X+.5f)*Tile+(BX?-1:1)*(GroundClearance+1),(Y+.5f)*Tile+(BY?-1:1)*(GroundClearance+1),0);
        if(!ClearNavigationLine(P,P))continue;
        P.Z=Height(ToCell(P))+27;NavigationCorners.Add(P);NavigationCornerRegions.Add(NavigationRegions[CellID(ToCell(P))]);
    }
    // Lazily cache corner visibility. Building or destroying a bridge invalidates
    // the graph; troop movement and each AI decision reuse it.
    NavigationCornerVisibility.Init(-1,NavigationCorners.Num()*NavigationCorners.Num());
}

bool AVoxelBattle::ClearNavigationLine(FVector A,FVector B) const
{
    FIntPoint C=ToCell(A),End=ToCell(B);if(!NavigationCell(C)||!NavigationCell(End))return false;
    FVector D=B-A;int SX=D.X>0?1:D.X<0?-1:0,SY=D.Y>0?1:D.Y<0?-1:0;
    double NextX=SX?((C.X+SX*.5)*Tile-A.X)/D.X:DBL_MAX,NextY=SY?((C.Y+SY*.5)*Tile-A.Y)/D.Y:DBL_MAX;
    double StepX=SX?Tile/FMath::Abs(D.X):DBL_MAX,StepY=SY?Tile/FMath::Abs(D.Y):DBL_MAX;
    for(int Step=0;Step<W+H+4;++Step)
    {
        // Supercover traversal plus exact expanded-box intersection catches even
        // a very short diagonal crossing of water, unlike fixed-distance samples.
        for(int Y=-1;Y<=1;++Y)for(int X=-1;X<=1;++X)
        {
            FIntPoint N=C+FIntPoint(X,Y);if(NavigationCell(N))continue;
            if(SegmentHitsBox(A,B,FVector(N.X*Tile,N.Y*Tile,0),Tile*.5+GroundClearance))return false;
        }
        if(C==End)return true;
        if(NextX<NextY){C.X+=SX;NextX+=StepX;}
        else if(NextY<NextX){C.Y+=SY;NextY+=StepY;}
        else{C.X+=SX;C.Y+=SY;NextX+=StepX;NextY+=StepY;}
    }
    return false;
}

bool AVoxelBattle::WalkableSegment(FVector A,FVector B) const
{UpdateNavigation();return ClearNavigationLine(A,B);}

FVector AVoxelBattle::ClampMoveDestination(FVector P) const
{
    UpdateNavigation();FIntPoint C=ToCell(P);if(!NavigationCell(C))return P;
    for(int Pass=0;Pass<4&&!ClearNavigationLine(P,P);++Pass)
        for(int Y=-1;Y<=1;++Y)for(int X=-1;X<=1;++X)
        {
            FIntPoint N=C+FIntPoint(X,Y);if(NavigationCell(N))continue;
            float Half=Tile*.5f+GroundClearance;double MinX=N.X*Tile-Half,MaxX=N.X*Tile+Half,MinY=N.Y*Tile-Half,MaxY=N.Y*Tile+Half;
            if(P.X<MinX||P.X>MaxX||P.Y<MinY||P.Y>MaxY)continue;
            double Push[]={MinX-P.X-1,MaxX-P.X+1,MinY-P.Y-1,MaxY-P.Y+1};int Best=0;
            for(int I=1;I<4;++I)if(FMath::Abs(Push[I])<FMath::Abs(Push[Best]))Best=I;
            if(Best<2)P.X+=Push[Best];else P.Y+=Push[Best];
        }
    if(!ClearNavigationLine(P,P))P=CellPosition(C);
    P.Z=Height(ToCell(P))+27;return P;
}

TArray<FVector> AVoxelBattle::FindRoute(FIntPoint Start,FIntPoint Goal,int32 /*Team*/,bool /*Safe*/) const
{return FindGroundRoute(CellPosition(Start)+FVector(0,0,27),CellPosition(Goal)+FVector(0,0,27));}

TArray<FVector> AVoxelBattle::FindGroundRoute(FVector Start,FVector Goal) const
{
    TArray<FVector> Route;UpdateNavigation();
    FIntPoint From=ToCell(Start),To=ToCell(Goal);
    if(!NavigationCell(From)||!NavigationCell(To)||NavigationRegions[CellID(From)]!=NavigationRegions[CellID(To)])return Route;
    Goal=ClampMoveDestination(Goal);if(!ClearNavigationLine(Start,Start))return Route;
    if(ClearNavigationLine(Start,Goal)){Route.Add(Goal);return Route;}
    int Corners=NavigationCorners.Num(),Count=Corners+2,Region=NavigationRegions[CellID(From)];
    auto Point=[&](int I){return I==0?Start:I==1?Goal:NavigationCorners[I-2];};
    TArray<double> Cost;Cost.Init(DBL_MAX,Count);TArray<int32> Parent;Parent.Init(INDEX_NONE,Count);
    TArray<uint8> Closed;Closed.Init(0,Count);TArray<int32> Open;Open.Add(0);Cost[0]=0;
    while(!Open.IsEmpty())
    {
        int Best=0;double BestScore=DBL_MAX;
        for(int I=0;I<Open.Num();++I)
        {double Score=Cost[Open[I]]+FVector::Dist2D(Point(Open[I]),Goal);if(Score<BestScore){Best=I;BestScore=Score;}}
        int Current=Open[Best];Open.RemoveAtSwap(Best);if(Closed[Current])continue;Closed[Current]=1;if(Current==1)break;
        for(int Next=1;Next<Count;++Next)
        {
            if(Closed[Next]||(Next>=2&&NavigationCornerRegions[Next-2]!=Region))continue;
            bool Clear;
            if(Current<2||Next<2)Clear=ClearNavigationLine(Point(Current),Point(Next));
            else
            {
                int A=FMath::Min(Current-2,Next-2),B=FMath::Max(Current-2,Next-2);int8& Cached=NavigationCornerVisibility[A*Corners+B];
                if(Cached<0)Cached=ClearNavigationLine(Point(Current),Point(Next))?1:0;Clear=Cached!=0;
            }
            if(!Clear)continue;
            double NewCost=Cost[Current]+FVector::Dist2D(Point(Current),Point(Next));
            if(NewCost+.001<Cost[Next]){Cost[Next]=NewCost;Parent[Next]=Current;Open.Add(Next);}
        }
    }
    if(Cost[1]==DBL_MAX)return Route;
    for(int I=1;I!=0;I=Parent[I])
    {if(I==INDEX_NONE){Route.Empty();return Route;}Route.Insert(Point(I),0);}
    return Route;
}

float AVoxelBattle::RouteExposure(FVector Start,const TArray<FVector>& Route,int32 Team) const
{
    float Exposure=0;
    for(FVector End:Route)
    {
        int Steps=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Start,End)/Tile));
        for(int S=1;S<=Steps;++S)
        {
            FVector P=FMath::Lerp(Start,End,float(S)/Steps);P.Z=Height(ToCell(P))+27;
            for(int I=0;I<Towers.Num();++I)
            {
                const auto& T=Towers[I];if(T.Team==0||T.Team==Team||T.Module==EVoxelModule::Village||T.Overheated)continue;
                if(FVector::Dist2D(P,CellPosition(T.Cell))<TowerRange(I,P))Exposure+=.65f;
            }
        }
        Start=End;
    }
    return Exposure;
}
