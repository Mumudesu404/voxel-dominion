#include "VoxelWar.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundWave.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

void AVoxelBattle::InitializeBattleAudio()
{
    if(!TowerSounds.IsEmpty())return;
    for(const TCHAR* Family:{TEXT("CannonBurst"),TEXT("ArrowRelease")})for(int Variant=1;Variant<=3;++Variant)
    {
        FString Name=FString::Printf(TEXT("S_%s_%02d"),Family,Variant);
        TowerSounds.Add(LoadObject<USoundWave>(nullptr,*FString::Printf(TEXT("/Game/Voxel/Audio/%s.%s"),*Name,*Name)));
    }
    // Independent, reusable voice slots reserve space for the quieter crossbows.
    ShotAudio.SetNum(12);ShotAudioOrigins.SetNumZeroed(12);ShotAudioOrder.SetNumZeroed(12);
}

void AVoxelBattle::PlayTowerAudio(int32 I)
{
    if(!Towers.IsValidIndex(I)||MenuPaused||Paused||Winner||(Tutorial&&TutorialObjectiveComplete))return;
    auto* P=GetWorld()?Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()):nullptr;
    if(!P||P->IsMenuOpen()||!GetWorld()->HasBegunPlay()||!GetWorld()->GetAudioDevice().IsValid())return;
    const bool Cannon=Towers[I].Module==EVoxelModule::Cannon;
    if(!Cannon&&Towers[I].Module!=EVoxelModule::Arrow)return;
    FVector Source=CellPosition(Towers[I].Cell);
    float Gain=VoxelRules::WeaponAudioGain(P->Zoom,P->Focus,Source);
    if(Gain<.008f)return;
    InitializeBattleAudio();int Variant=int((NextAudioOrder-1)%3),SoundIndex=(Cannon?0:3)+Variant;
    if(!TowerSounds.IsValidIndex(SoundIndex)||!TowerSounds[SoundIndex])return;
    int Begin=Cannon?0:8,End=Cannon?8:12,Slot=Begin;
    for(int K=Begin;K<End;++K)
    {
        if(!IsValid(ShotAudio[K])||(!ShotAudio[K]->IsPlaying()&&!ShotAudio[K]->bIsPaused)){Slot=K;break;}
        if(ShotAudioOrder[K]<ShotAudioOrder[Slot])Slot=K;
    }
    if(!IsValid(ShotAudio[Slot]))
    {
        ShotAudio[Slot]=UGameplayStatics::CreateSound2D(this,TowerSounds[SoundIndex],1,1,0,nullptr,false,false);
        if(!ShotAudio[Slot])return;
        ShotAudio[Slot]->SetUISound(false);
    }
    auto* Voice=ShotAudio[Slot].Get();Voice->Stop();Voice->SetPaused(false);Voice->SetSound(TowerSounds[SoundIndex]);
    Voice->SetPitchMultiplier(.97f+Variant*.025f);
    Voice->SetVolumeMultiplier(Gain*(Cannon?.60f:.85f));
    Voice->SetLowPassFilterEnabled(true);Voice->SetLowPassFilterFrequency(FMath::Lerp(6000.f,16000.f,FMath::Clamp(3800.f/P->Zoom,0.f,1.f)));
    ShotAudioOrigins[Slot]=Source;ShotAudioOrder[Slot]=NextAudioOrder++;
    Voice->Play();
}

void AVoxelBattle::UpdateBattleAudio(float Dt)
{
    if(MenuPaused||Winner||(Tutorial&&TutorialObjectiveComplete)){StopBattleAudio();return;}
    auto* P=GetWorld()?Cast<AVoxelCommander>(GetWorld()->GetFirstPlayerController()):nullptr;
    if(!P||P->IsMenuOpen()){StopBattleAudio();return;}
    int Active=0;for(const auto& Voice:ShotAudio)if(IsValid(Voice)&&(Voice->IsPlaying()||Voice->bIsPaused))++Active;
    float Mix=1/FMath::Sqrt(FMath::Max(1.f,Active/3.f));
    for(int I=0;I<ShotAudio.Num();++I)if(auto* Voice=ShotAudio[I].Get();IsValid(Voice)&&(Voice->IsPlaying()||Voice->bIsPaused))
    {
        Voice->SetPaused(Paused);
        float Goal=VoxelRules::WeaponAudioGain(P->Zoom,P->Focus,ShotAudioOrigins[I])*(I<8?.60f:.85f)*Mix;
        Voice->SetVolumeMultiplier(FMath::FInterpTo(Voice->VolumeMultiplier,Goal,Dt,14.f));
        Voice->SetLowPassFilterFrequency(FMath::Lerp(6000.f,16000.f,FMath::Clamp(3800.f/P->Zoom,0.f,1.f)));
    }
#if !UE_BUILD_SHIPPING
    if(FParse::Param(FCommandLine::Get(),TEXT("AudioAudit"))&&GetWorld()->GetRealTimeSeconds()>=NextAudioAudit)
    {
        NextAudioAudit=GetWorld()->GetRealTimeSeconds()+2;
        float Maximum=0;for(const auto& Voice:ShotAudio)if(IsValid(Voice)&&Voice->IsPlaying())Maximum=FMath::Max(Maximum,Voice->VolumeMultiplier);
        UE_LOG(LogTemp,Display,TEXT("VOXEL_AUDIO: zoom=%.0f active=%d peakVoiceGain=%.3f paused=%d"),P->Zoom,Active,Maximum,Paused);
    }
#endif
}

void AVoxelBattle::StopBattleAudio()
{
    for(const auto& Voice:ShotAudio)if(IsValid(Voice)){Voice->Stop();Voice->SetPaused(false);}
}

void AVoxelBattle::EndPlay(const EEndPlayReason::Type Reason)
{
    StopBattleAudio();
    for(const auto& Voice:ShotAudio)if(IsValid(Voice))Voice->DestroyComponent();
    ShotAudio.Empty();Super::EndPlay(Reason);
}
