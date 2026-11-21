#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/HUD.h"
#include "VoxelWar.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class ACameraActor;
class UAudioComponent;
class USoundWave;
class UStaticMeshComponent;
class SVoxelLabels;
class SVoxelHandCursor;
class UTexture2D;

UENUM(BlueprintType)
enum class EVoxelModule : uint8 { Command, Cannon, Arrow, Village };
UENUM(BlueprintType)
enum class EVoxelArrow : uint8 { Unbound, Poison, Pierce };
enum class EVoxelMenuPage : uint8 { None, Main, Setup, Settings, Guide, ConfirmNew, ConfirmQuit };

USTRUCT(BlueprintType)
struct FVoxelTower
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint Cell = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Team = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EVoxelModule Module = EVoxelModule::Cannon;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool Tall = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Troops = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1", ClampMax="5")) int32 Level = 1;
    UPROPERTY(BlueprintReadOnly) EVoxelArrow Arrow = EVoxelArrow::Unbound;
    UPROPERTY(BlueprintReadOnly) bool PlayerBound = false;
    UPROPERTY(BlueprintReadOnly) float Heat = 0;
    UPROPERTY(BlueprintReadOnly) bool Overheated = false;
    float FireTimer = 0;
    FVector AimTarget = FVector::ZeroVector;
    int32 AimSquad = INDEX_NONE;
    bool HasAim = false;
    float ShotAge = 100, BarrelAngle = 0, SpinSpeed = 0;
};

struct FVoxelWeaponShot
{
    FVector From=FVector::ZeroVector,To=FVector::ZeroVector;
    EVoxelModule Module=EVoxelModule::Arrow;
    EVoxelArrow Arrow=EVoxelArrow::Unbound;
    float Age=0,FlightTime=.3f;
    int32 Tower=INDEX_NONE;
};

struct FVoxelSoldierPose
{
    FVector Position=FVector::ZeroVector;
    float Yaw=0,WalkPhase=0,MeleePhase=0,Moving=0,Melee=0;
};

USTRUCT()
struct FVoxelSquad
{
    GENERATED_BODY()
    int32 Team = 1;
    int32 Target = 0;
    float Troops = 0;
    FVector Position = FVector::ZeroVector;
    TArray<FVector> Route;
    int32 Step = 0;
    float MarchDelay = 0;
    int32 ID = INDEX_NONE;
    FIntPoint Destination = FIntPoint::ZeroValue;
    FVector GroundDestination = FVector::ZeroVector;
    bool HasGroundDestination = false;
    FIntPoint LastLand = FIntPoint::ZeroValue;
    bool HasDestination = false;
    bool Holding = false;
    bool Blocked = false;
    bool Evacuating = false;
    int32 InterceptTarget = INDEX_NONE;
    float RetargetIn = 0;
    // Presentation only: these never change movement, damage or troop counts.
    float VisualYaw = 0, WalkPhase = 0, MeleePhase = 0;
    bool VisualMoving = false, VisualMelee = false;
    FVoxelSoldierPose PreviousPose;
    bool HasPreviousPose = false;
};

USTRUCT()
struct FVoxelBridge
{
    GENERATED_BODY()
    int32 Row = 0;
    int32 Column = 0;
    int32 Length = 1;
    bool Vertical = false;
    float Elevation = 70;
    int32 Team = 1;
    float Remaining = 60;
};

USTRUCT()
struct FVoxelEffect
{
    GENERATED_BODY()
    FVector Position = FVector::ZeroVector;
    float Remaining = 0;
    float Radius = 0;
    int32 Team = 0;
    bool Meteor = false;
    float DamagePerSecond = 2.2f;
};

struct FVoxelMeteorBurst
{
    FVector Position = FVector::ZeroVector;
    float Age = 0, Radius = 340;
};

// Pure rules are shared by simulation and automation tests.
namespace VoxelRules
{
    inline FVector MeteorPosition(FVector Target,float Remaining)
    {
        float Progress=1-FMath::Clamp(Remaining/1.5f,0.f,1.f);
        return Target+FVector(0,0,65)+FVector(-680,460,1900)*(1-FMath::Pow(Progress,1.6f));
    }
    inline bool ResolveAttack(float Attack, float& Defense)
    {
        if (Attack > Defense) { Defense = Attack - Defense; return true; }
        Defense -= Attack; return false;
    }
    inline float Range(bool Tall, float HeightDelta)
    { return (Tall ? 820.f : 570.f) + FMath::Clamp(HeightDelta * .65f, -200.f, 260.f); }
    inline float WeaponAudioGain(float Zoom,FVector Focus,FVector Source)
    {
        float Width=FMath::Max(450.f,Zoom),Distance=FVector::Dist2D(Focus,Source)/(Width*.60f);
        float Framing=FMath::Clamp((1.35f-Distance)/.35f,0.f,1.f)/(1+4*FMath::Pow(Distance,4));
        return FMath::Clamp(FMath::Pow(3800.f/Width,.95f),.10f,.90f)*Framing;
    }
}

UCLASS()
class TEST_API AVoxelBattle : public AActor
{
    GENERATED_BODY()
public:
    AVoxelBattle();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battle") TArray<FVoxelTower> Towers;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float CommandRegen = .65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float AIInterval = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float BattleTimeLimit = 1200.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float SoldierSpeed = 220;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float BridgeCost = 75;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float BridgeBuildTime = 60;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance", meta=(ClampMin="0")) float GuardRadius = 540;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance", meta=(ClampMin="0")) float SquadMergeRadius = 360;
    static constexpr float SquadMergeContact = 120.f;
    static constexpr float GarrisonApproachRadius = 320.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float MeteorCost = 55;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float MeteorCooldown = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float VillageTroopBonus = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Balance") float VillageGoldBonus = .4f;
    TArray<FVoxelSquad> Squads;
    TArray<FVoxelEffect> Effects;
    TArray<FVoxelMeteorBurst> MeteorBursts;
    TArray<FVoxelWeaponShot> WeaponShots;
    static constexpr float MeteorBurstLifetime = 2.8f;
    TArray<FVoxelBridge> Bridges;
    float Gold[3] = {0, 0, 0};
    float Time = 0, MeteorReady = 0, AISince = 0;
    float DispatchFraction = .5f;
    int32 Selected = INDEX_NONE;
    int32 SelectedSquad = INDEX_NONE;
    bool Paused = false, MeteorMode = false, BridgeMode = false;
    bool MenuPaused = false, AIEnabled = true, TimeLimitEnabled = true;
    bool Tutorial = false, TutorialObjectiveComplete = false, TutorialFastForward = false;
    int32 TutorialStep = 0, TutorialBridgeRow = 0;
    static constexpr int32 TutorialLessonCount = 12;
    int32 Winner = 0;
    bool TimeLimitReached = false;
    int32 FinalHoldings[3] = {0,0,0};
    float FinalTroops[3] = {0,0,0};
    FString Notice;
    float NoticeUntil = 0;
    static constexpr int32 W = 85;
    static constexpr int32 H = 61;
    static constexpr float Tile = 180.f;
    static constexpr int32 MaxTowerLevel = 5;
    void InitializeLayout();
    void ResetBattle(bool Practice = false, bool ForTutorial = false);
    int32 MapHalfWidth() const { return Tutorial?10:W/2; }
    int32 MapHalfHeight() const { return Tutorial?7:H/2; }
    void UpdateTutorial();
    void NextTutorialLesson();
    void RetryTutorialLesson();
    void ToggleTutorialFastForward();
    void FocusTutorialObjective();
    FVector TutorialGoalPosition() const;
    FString TutorialTitle() const;
    TArray<FString> TutorialInstructions() const;
    FString TutorialProgress() const;
    bool TutorialCanFastForward() const;
    void SelectTower(int32 Index);
    void SelectSquad(int32 ID);
    int32 SquadIndex(int32 ID) const;
    void CancelCommand();
    void CompleteCommand();
    bool IsThinking() const;
    int32 GetPlaybackSpeed() const { return PlaybackSpeed; }
    void SetPlaybackSpeed(int32 Speed);
    float GetBattleSpeed() const;
    void AdvanceBattle(float RealDeltaTime);
    FVector CellPosition(FIntPoint Cell) const;
    FIntPoint ToCell(FVector Position) const;
    float Height(FIntPoint Cell) const;
    bool ContainsCell(FIntPoint Cell) const;
    bool BridgeSiteValid(int32 Row) const;
    bool IsRiver(FIntPoint Cell) const;
    bool CanReach(FIntPoint From,FIntPoint To) const;
    bool BridgeSiteAt(FIntPoint Cell,FVoxelBridge& Site) const;
    int32 BridgeIndexAt(FIntPoint Cell) const;
    FVector BridgeCenter(const FVoxelBridge& Bridge) const;
    FIntPoint BridgeBank(const FVoxelBridge& Bridge,bool Far) const;
    bool BuildBridgeAt(FIntPoint Cell,int32 Team=1);
    FVector2D MapUV(FVector Position) const;
    FVector FocusFromMapUV(FVector2D UV) const;
    bool Walkable(FIntPoint Cell) const;
    TArray<FVector> FindRoute(FIntPoint Start, FIntPoint Goal, int32 Team, bool Safe = true) const;
    TArray<FVector> FindGroundRoute(FVector Start,FVector Goal) const;
    bool WalkableSegment(FVector Start,FVector Goal) const;
    UFUNCTION(BlueprintCallable) bool Dispatch(int32 From, int32 To, float Fraction);
    bool DispatchTo(int32 From, FVector Destination, float Fraction);
    bool OrderSquad(int32 ID, int32 Target, FVector Destination);
    UFUNCTION(BlueprintCallable) bool UpgradeTower(int32 Tower, int32 Team = 1);
    int32 UpgradeCost(int32 Tower) const;
    float TroopCapacity(int32 Tower) const;
    float TowerDamage(int32 Tower) const;
    float TowerInterval(int32 Tower) const;
    int32 VillageCount(int32 Team) const;
    float VillageTroopSupply(int32 Tower) const;
    float VillageGoldSupply(int32 Tower) const;
    float CommandRecovery(int32 Team) const;
    float GoldRecovery(int32 Team) const;
    int32 HoldingCount(int32 Team) const;
    int32 OutpostsRemaining(int32 Team) const;
    bool CanAttackTower(int32 Tower, int32 Attacker) const;
    float TotalArmy(int32 Team) const;
    UFUNCTION(BlueprintCallable) bool BuildBridge(int32 Row, int32 Team = 1);
    int32 BridgeIndex(int32 Row) const;
    bool HasCrossing() const;
    UFUNCTION(BlueprintCallable) void BindArrow(EVoxelArrow Type);
    void Meteor(FVector Position);
    void Message(const FString& Text);
    void RunAI();
    float Incoming(int32 Target, int32 Team) const;
    bool Visible(FVector From, FVector To) const;
    float TowerRange(int32 Tower, FVector Target) const;
    float TowerBaseRange(int32 Tower) const;
    float TowerLabelHeight(int32 Tower) const;
    void DrawWorld();
    void RebuildTerrain();
    FVector SquadLabelPosition(const FVoxelSquad& Squad) const;
    FVoxelSoldierPose SoldierRenderPose(const FVoxelSquad& Squad) const;
private:
    friend class FVoxelSimulationTest;
    friend class FVoxelCombatTest;
    friend class FVoxelVictoryTest;
    friend class FVoxelUpgradeTest;
    friend class FVoxelVillageTest;
    friend class FVoxelBridgeTest;
    friend class FVoxelFieldTest;
    friend class FVoxelPacingTest;
    friend class FVoxelProtectionTest;
    friend class FVoxelTimeLimitTest;
    friend class FVoxelMergeTest;
    friend class FVoxelMeteorCooldownTest;
    friend class FVoxelMenuFlowTest;
    friend class FVoxelTutorialTest;
    friend class FVoxelMeteorVisualTest;
    friend class FVoxelRiverNetworkTest;
    friend class FVoxelRangeTest;
    friend class FVoxelWeaponVisualTest;
    friend class FVoxelPierceVisualTest;
    friend class FVoxelAnyAngleRouteTest;
    friend class FVoxelSoldierVisualTest;
    friend class FVoxelTowerAudioTest;
    friend class FVoxelRangeOverlayTest;
    void FinishBattle(int32 Result, const FString& Summary);
    void ResolveTimeLimit();
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Palette;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> BuildingBatches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> BuildingMaterials;
    void InitializeReferenceBuildings();
    bool DrawReferenceBuilding(int32 Tower);
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> MeteorBatches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> MeteorMaterials;
    void InitializeMeteorVisuals();
    void DrawMeteorVisuals();
    void MeteorParticle(int32 Slot,FVector Position,FVector Dimensions,FQuat Rotation=FQuat::Identity,float Fade=1.f);
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> PoisonBatches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> PoisonMaterials;
    void InitializePoisonVisuals();
    void DrawPoisonVisuals();
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> WeaponBatches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> WeaponMaterials;
    void InitializeWeaponVisuals();
    void DrawWeaponVisuals();
    void RecordWeaponShot(int32 Tower,FVector Target);
    FVector WeaponPivot(int32 Tower) const;
    UPROPERTY() TArray<TObjectPtr<USoundWave>> TowerSounds;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> ShotAudio;
    TArray<FVector> ShotAudioOrigins;
    TArray<uint64> ShotAudioOrder;
    uint64 NextAudioOrder = 1;
    float NextAudioAudit = 0;
    void InitializeBattleAudio();
    void PlayTowerAudio(int32 Tower);
    void UpdateBattleAudio(float RealDeltaTime);
    void StopBattleAudio();
    UPROPERTY() TObjectPtr<UStaticMeshComponent> RangeOverlay;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RangeMaterial;
    uint32 LastRangeSignature = MAX_uint32;
    void InitializeRangeOverlay();
    void UpdateRangeOverlay();
    UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> SoldierBatches;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> SoldierMaterials;
    UPROPERTY() TArray<TObjectPtr<UStaticMesh>> SoldierMeshes;
    void InitializeSoldierVisuals();
    void DrawSoldierVisuals();
    void UpdateSoldierPose(FVoxelSquad& Squad,FVector Previous,FVector FacingTarget,bool Fighting,float Dt);
    void Box(int32 Color, FVector P, FVector Size, bool World = false);
    void MakeMap();
    void InitializeTutorialLayout();
    void BeginTutorialLesson(int32 Step);
    bool TutorialAllowsOrder(int32 Team,int32 Target,FIntPoint Destination);
    bool TutorialAllowsAction(int32 Step,int32 Tower = INDEX_NONE);
    int32 TutorialRaidID = INDEX_NONE, TutorialBlueDemoID = INDEX_NONE, TutorialRedDemoID = INDEX_NONE;
    bool TutorialEncounterSeen = false, TutorialBlueHit = false, TutorialRedHit = false;
    struct FTutorialCheckpoint
    {
        TArray<FVoxelTower> Towers;
        TArray<FVoxelSquad> Squads;
        TArray<FVoxelBridge> Bridges;
        TArray<FVoxelEffect> Effects;
        float Gold[3]={0,0,0}, Time=0, MeteorReady=0;
        int32 NextID=1, RaidID=INDEX_NONE, BlueID=INDEX_NONE, RedID=INDEX_NONE, BridgeRow=0;
    };
    FTutorialCheckpoint TutorialCheckpoint;
    void Simulate(float Dt);
    void FireTowers(float Dt);
    void Arrive(FVoxelSquad& Squad);
    bool LaunchCommandAssault();
    bool UpgradeAIFrontline();
    bool BuildAIBridge();
    bool DeployAIBridgeGuard();
    struct FAIOrder { int32 Source, Target, Count; float ArrivalTime; };
    TArray<FAIOrder> AIOrders;
    bool ExecuteAIOrder();
    float NextAIAction = 0;
    float NextAIUpgrade = 45;
    float NextAIBridge = 45;
    int32 NextSquadID = 1;
    bool SendTroops(int32 From, int32 Target, FIntPoint Destination, float Fraction,const FVector* GroundDestination = nullptr);
    bool RepathSquad(FVoxelSquad& Squad);
    void RepathArmies();
    void UpdateInterception(float Dt);
    bool IsMeteorLessonSquad(const FVoxelSquad& Squad) const
    {
        return Tutorial&&TutorialStep==8&&Squad.ID!=INDEX_NONE&&(Squad.ID==TutorialBlueDemoID||Squad.ID==TutorialRedDemoID);
    }
    void MergeNearbySquads(TMap<int32,FVector>* RallyPoints=nullptr);
    void SetGuardPost(FVoxelSquad& Squad,FVector Post);
    void HoldOutsideTower(FVoxelSquad& Squad,int32 TowerIndex);
    void DestroyBridges(FVector Position, float Radius);
    bool bTerrain = false;
    mutable uint32 LastNavigationSignature = MAX_uint32;
    mutable TArray<int32> NavigationRegions;
    mutable TArray<uint8> NavigationWalkable;
    mutable TArray<FVector> NavigationCorners;
    mutable TArray<int32> NavigationCornerRegions;
    mutable TArray<int8> NavigationCornerVisibility;
    void UpdateNavigation() const;
    bool NavigationCell(FIntPoint Cell) const;
    bool ClearNavigationLine(FVector Start,FVector Goal) const;
    FVector ClampMoveDestination(FVector Position) const;
    float RouteExposure(FVector Start,const TArray<FVector>& Route,int32 Team) const;
    float Accumulator = 0;
    int32 PlaybackSpeed = 1;
    bool bDrawingTowers = false;
    uint32 LastTowerSignature = MAX_uint32;
    bool bTacticalThinking = false;
};

UCLASS()
class TEST_API AVoxelCommander : public APlayerController
{
    GENERATED_BODY()
public:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float Dt) override;
    AVoxelBattle* Battle() const;
    void LeftClick();
    void RightClick();
    void BridgeKey();
    void MeteorKey();
    void PoisonKey();
    void PierceKey();
    void FractionKey();
    void PauseKey();
    void RestartKey();
    void ZoomIn();
    void ZoomOut();
    void OverviewKey();
    void CancelKey();
    void UpgradeKey();
    bool IsMenuOpen() const { return MenuPage!=EVoxelMenuPage::None; }
    void ShowMenu(EVoxelMenuPage Page = EVoxelMenuPage::Main);
    void MenuAction(FName Action);
    void StartBattle();
    void ResumeBattle();
    void ConfirmKey();
    void LoadPreferences();
    void LoadSettingsDraft();
    void ApplySettingsDraft();
    void MarkTutorialCompleted();
    void SetupMeteorPreview(float Age);
    void PreviewMeteorKey();
    void SetupPoisonPreview(float Age);
    void PreviewPoisonKey();
    void SetupWeaponPreview(int32 Stage);
    void PreviewWeaponKey();
    void SetupSoldierPreview(int32 Stage);
    void PreviewSoldierKey();
    EVoxelMenuPage MenuPage = EVoxelMenuPage::Main;
    bool BattleStarted = false;
    bool TutorialCompleted = false;
    bool MeteorPreviewActive = false;
    bool PoisonPreviewActive = false;
    bool WeaponPreviewActive = false;
    int32 WeaponPreviewStage = 0;
    bool SoldierPreviewActive = false;
    int32 SoldierPreviewStage = 0;
    int32 SelectedMode = 0;
    float CameraSpeed = 1.f;
    int32 DraftQuality = 2, DraftFrameLimit = 1, DraftCameraSpeed = 1;
    bool DraftVSync = false;
    FString MenuNotice;
    FVector CursorWorld() const;
    int32 TowerUnderCursor(FVector2D Cursor) const;
    UPROPERTY() TObjectPtr<ACameraActor> BattleCamera;
    FVector Focus = FVector(-3400,900,0);
    float Zoom = 7500;
};

UCLASS()
class TEST_API AVoxelHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool HandleClick(float X, float Y);
    bool IsTreasuryHit(float X, float Y) const { return TreasuryRect.IsInside(FVector2D(X,Y)); }
private:
    friend class SVoxelLabels;
    struct FButton { FName ID; FBox2D Rect; bool Active=true; };
    TArray<FButton> Buttons;
    float Scale = 1;
    struct FLabel
    {
        FString String;
        float X,Y,Size;
        FLinearColor Color;
        bool MenuBold=false, FlatBold=false;
        FVector2D TextBox=FVector2D::ZeroVector;
        bool CenterHorizontally=false;
    };
    TArray<FLabel> Labels;
    struct FIcon { UTexture2D* Texture; FVector2D Position,Size; FBox2D UV=FBox2D(FVector2D::ZeroVector,FVector2D(1,1)); float Angle=0; bool LongAxisSpin=false; };
    TArray<FIcon> Icons;
    UPROPERTY() TObjectPtr<UTexture2D> MoneyBag;
    UPROPERTY() TObjectPtr<UTexture2D> GoldDigits;
    UPROPERTY() TObjectPtr<UTexture2D> SelectionBlade;
    UPROPERTY() TObjectPtr<UTexture2D> HandCursor;
    UPROPERTY() TObjectPtr<UTexture2D> MenuBackground;
    UPROPERTY() TObjectPtr<UTexture2D> MenuReference;
    TMap<FName,float> MenuHover;
    void MenuImage(UTexture2D* Texture,float X,float Y,float W,float H,FVector2D UVMin=FVector2D::ZeroVector,FVector2D UVSize=FVector2D(1,1),float Alpha=1);
    void MenuPlate(float X,float Y,float W,float H,FLinearColor Top,FLinearColor Bottom);
    void MenuButton(FName ID,const FString& Label,int IconRow,float X,float Y,float W,float H);
    TSharedPtr<SVoxelLabels> LabelWidget;
    TSharedPtr<SVoxelHandCursor> CursorWidget;
    FVector2D ViewportSize = FVector2D::ZeroVector;
    FBox2D MinimapRect = FBox2D(ForceInit);
    FBox2D TreasuryRect = FBox2D(ForceInit);
    void Text(const FString& S, float X, float Y, float Size, FLinearColor Color);
    void Panel(float X,float Y,float W,float H,FLinearColor Color);
    void DrawFrontEnd(AVoxelCommander* P,float W,float H);
    void HUDText(const FString& S,float X,float Y,float Size,FLinearColor Color,bool OnPaper=false);
    void HUDFrame(float X,float Y,float W,float H,bool Paper=false,FLinearColor Tint=FLinearColor::White);
    void HUDIcon(int Icon,float X,float Y,float W,float H);
    void DrawTroopPlate(int32 Team,float Troops,FVector2D Anchor,float Size,int32 Level=1,const FString& Caption=FString(),float CaptionWidth=0);
    void Button(FName ID,const FString& Label,float X,float Y,float W,bool Active=true,float H=38,int Icon=-1,bool Selected=false);
    void DrawBattleTop(AVoxelBattle* B,AVoxelCommander* P,float W);
    void DrawIdleOrders(float RX,float Y);
    void DrawTutorial(AVoxelBattle* B,AVoxelCommander* P,float W,float H);
    void PublishLabels();
    void InitializeUIArt();
    void DrawTreasury(AVoxelBattle* B,float H);
    void DrawSelectionBlade(AVoxelBattle* B,AVoxelCommander* P,float W,float H);
};

UCLASS()
class TEST_API AVoxelGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    AVoxelGameMode();
    virtual void BeginPlay() override;
};

