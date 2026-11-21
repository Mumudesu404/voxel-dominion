#include "VoxelWar.h"

bool AVoxelBattle::IsRiver(FIntPoint C) const
{
    if(!ContainsCell(C))return false;
    if(Tutorial)return C.X==0;
    static const TArray<uint8> Water=[]()
    {
        TArray<uint8> Mask;Mask.SetNumZeroed(W*H);
        auto Mark=[&](int X,int Y,bool Wide)
        {
            for(int Mirror:{-1,1})for(int Offset=Wide&&FMath::Abs(Y)>=6?-1:0;Offset<=(Wide&&FMath::Abs(Y)>=6?1:0);++Offset)
            {
                int A=Mirror*(X+Offset),B=Mirror*Y;
                if(FMath::Abs(A)<=W/2&&FMath::Abs(B)<=H/2)Mask[(B+H/2)*W+A+W/2]=1;
            }
        };
        auto Line=[&](FIntPoint A,FIntPoint B,bool Wide)
        {
            int DX=FMath::Abs(B.X-A.X),DY=FMath::Abs(B.Y-A.Y),SX=A.X<B.X?1:-1,SY=A.Y<B.Y?1:-1,Error=DX-DY;
            Mark(A.X,A.Y,Wide);
            while(A!=B)
            {
                int E=2*Error;
                if(E>-DY){Error-=DY;A.X+=SX;Mark(A.X,A.Y,Wide);}
                if(E<DX){Error+=DX;A.Y+=SY;Mark(A.X,A.Y,Wide);}
            }
        };
        const FIntPoint Main[]={{0,-33},{-4,-24},{-3,-17},{3,-10},{0,-4},{0,4},{-3,10},{3,17},{4,24},{0,33}};
        const FIntPoint Tributary[]={{3,-10},{10,-12},{18,-8},{25,-12},{34,-7},{44,-10}};
        const FIntPoint Fork[]={{18,-8},{20,0},{27,4},{30,13},{40,18}};
        for(int I=1;I<UE_ARRAY_COUNT(Main);++I)Line(Main[I-1],Main[I],true);
        for(int I=1;I<UE_ARRAY_COUNT(Tributary);++I)Line(Tributary[I-1],Tributary[I],false);
        for(int I=1;I<UE_ARRAY_COUNT(Fork);++I)Line(Fork[I-1],Fork[I],false);
        return Mask;
    }();
    return Water[(C.Y+H/2)*W+C.X+W/2]!=0;
}

FIntPoint AVoxelBattle::BridgeBank(const FVoxelBridge& B,bool Far) const
{return FIntPoint(B.Column,B.Row)+(B.Vertical?FIntPoint(0,1):FIntPoint(1,0))*(Far?B.Length:-1);}
FVector AVoxelBattle::BridgeCenter(const FVoxelBridge& B) const
{
    FVector D=B.Vertical?FVector(0,1,0):FVector(1,0,0);
    return FVector(B.Column*Tile,B.Row*Tile,B.Elevation)+D*((B.Length-1)*Tile*.5f);
}
int32 AVoxelBattle::BridgeIndexAt(FIntPoint C) const
{
    return Bridges.IndexOfByPredicate([&](const FVoxelBridge& B)
    {return B.Vertical?C.X==B.Column&&C.Y>=B.Row&&C.Y<B.Row+B.Length:C.Y==B.Row&&C.X>=B.Column&&C.X<B.Column+B.Length;});
}
bool AVoxelBattle::BridgeSiteAt(FIntPoint C,FVoxelBridge& Site) const
{
    if(!IsRiver(C))return false;
    int Best=MAX_int32;
    for(FIntPoint D:{FIntPoint(1,0),FIntPoint(0,1)})
    {
        FIntPoint A=C,B=C;
        for(int I=0;I<8&&IsRiver(A);++I)A-=D;
        for(int I=0;I<8&&IsRiver(B);++I)B+=D;
        if(!ContainsCell(A)||!ContainsCell(B)||IsRiver(A)||IsRiver(B))continue;
        int Length=FMath::Abs(B.X-A.X)+FMath::Abs(B.Y-A.Y)-1;
        if(Length<1||Length>7||Length>=Best)continue;
        Best=Length;FIntPoint Start=A+D;Site.Column=Start.X;Site.Row=Start.Y;Site.Vertical=D.Y!=0;Site.Length=Length;
        Site.Elevation=FMath::Max(Height(A),Height(B));
    }
    return Best!=MAX_int32;
}
bool AVoxelBattle::BridgeSiteValid(int32 Row) const
{
    FVoxelBridge Site;
    for(int Offset=0;Offset<=8;++Offset)for(int S:{-1,1})if(BridgeSiteAt(FIntPoint(Offset*S,Row),Site))return true;
    return false;
}
int32 AVoxelBattle::BridgeIndex(int32 Row) const
{
    for(int Offset=0;Offset<=8;++Offset)for(int S:{-1,1})
    {int I=BridgeIndexAt(FIntPoint(Offset*S,Row));if(I!=INDEX_NONE)return I;}
    return INDEX_NONE;
}
bool AVoxelBattle::HasCrossing() const
{return Bridges.ContainsByPredicate([](const FVoxelBridge& B){return B.Remaining<=0;});}
bool AVoxelBattle::BuildBridge(int32 Row,int32 Team)
{
    FVoxelBridge Site;
    for(int Offset=0;Offset<=8;++Offset)for(int S:{-1,1})
        if(BridgeSiteAt(FIntPoint(Offset*S,Row),Site))return BuildBridgeAt(FIntPoint(Offset*S,Row),Team);
    return false;
}
bool AVoxelBattle::BuildBridgeAt(FIntPoint C,int32 Team)
{
    if(MenuPaused||Paused||Winner||Team<1||Team>2)return false;
    FVoxelBridge Site;
    if(!BridgeSiteAt(C,Site)){if(Team==1)Message(TEXT("请在两岸完整的较窄河段选址，避开宽阔河口。"));return false;}
    if(Tutorial&&Team==1&&(!TutorialAllowsAction(TutorialStep==9?9:7)||!Bridges.IsEmpty()))return false;
    FIntPoint D=Site.Vertical?FIntPoint(0,1):FIntPoint(1,0);
    for(int I=0;I<Site.Length;++I)if(BridgeIndexAt(FIntPoint(Site.Column,Site.Row)+D*I)!=INDEX_NONE)
    {if(Team==1)Message(TEXT("此处已有桥梁或工地，不能重叠施工。"));return false;}
    if(Gold[Team]<BridgeCost){if(Team==1)Message(FString::Printf(TEXT("建桥需要 %.0f 金币，当前金币不足。"),BridgeCost));return false;}
    if(Tutorial&&Team==1)TutorialBridgeRow=C.Y;
    Gold[Team]-=BridgeCost;Site.Team=Team;Site.Remaining=BridgeBuildTime;Bridges.Add(Site);
    if(Team==1){BridgeMode=false;CompleteCommand();Message(FString::Printf(TEXT("桥梁开始施工，%.0f 秒后可通行。双方共享，陨石可毁桥。"),BridgeBuildTime));}
    return true;
}
void AVoxelBattle::DestroyBridges(FVector P,float Radius)
{
    int Removed=Bridges.RemoveAll([&](const FVoxelBridge& B)
    {
        FVector A(B.Column*Tile,B.Row*Tile,0),D=B.Vertical?FVector(0,1,0):FVector(1,0,0),End=A+D*((B.Length-1)*Tile);
        FVector Point(P.X,P.Y,0),Closest=FMath::ClosestPointOnSegment(Point,A,End);
        return FVector::Dist2D(Point,Closest)<=Radius+Tile*.42f;
    });
    if(Removed>0){AIOrders.Empty();RepathArmies();Message(TEXT("陨石摧毁了桥梁或工地！部队改道，无路时在岸边等待。"));}
}

bool AVoxelBattle::CanReach(FIntPoint A,FIntPoint B) const
{
    if(!Walkable(A)||!Walkable(B))return false;
    UpdateNavigation();
    return NavigationRegions[(A.Y+H/2)*W+A.X+W/2]==NavigationRegions[(B.Y+H/2)*W+B.X+W/2];
}
