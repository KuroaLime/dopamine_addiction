// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/TPS/System/HealthRegenComponent.h"
#include "Game/InGame/MainPlayerState.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"   

UHealthRegenComponent::UHealthRegenComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UHealthRegenComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* OwnerActor = GetOwner();
    if (OwnerActor && OwnerActor->HasAuthority())
    {
        //뚜까 맞을때 해당 함수를 실행하라
        OwnerActor->OnTakeAnyDamage.AddDynamic(this, &UHealthRegenComponent::OnOwnerTakeDamage);
        StartRegenCooldown();
    }
}


void UHealthRegenComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

}

void UHealthRegenComponent::OnOwnerTakeDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
    if (Damage > 0.0f)
    {
        //타이머 강제 종료 및 초기화
        GetWorld()->GetTimerManager().ClearTimer(RegenLoopTimerHandle);
        GetWorld()->GetTimerManager().ClearTimer(RegenCooldownTimerHandle);
        FractionalHP = 0.0f;
        //재시작 대기시간 타이머 시작
        StartRegenCooldown();
    }
}

void UHealthRegenComponent::StartRegenCooldown()
{
    GetWorld()->GetTimerManager().SetTimer(RegenCooldownTimerHandle,this,
        &UHealthRegenComponent::StartRegenLoop, CooldownDelay, false
    );
}

void UHealthRegenComponent::StartRegenLoop()
{
    GetWorld()->GetTimerManager().SetTimer(
        RegenLoopTimerHandle, this, &UHealthRegenComponent::TickRegeneration, RegenInterval, true
    );
}

void UHealthRegenComponent::TickRegeneration()
{
    APawn* PawnOwner = Cast<APawn>(GetOwner());
    AMainPlayerState* PS = PawnOwner ? Cast<AMainPlayerState>(PawnOwner->GetPlayerState()) : nullptr;
    if (PS)
    {
        float MaxHP = PS->GetFinalMaxHP(100.0f);
        float RegenRate = PS->GetFinalRegenRate(0.0f);

        if (PS->CurPlayerData.CurrentHP <= 0 || PS->CurPlayerData.CurrentHP >= MaxHP)
        {
            GetWorld()->GetTimerManager().ClearTimer(RegenLoopTimerHandle);
            FractionalHP = 0.0f;
            return;
        }

        float RecoveryAmount = RegenRate * RegenInterval;
        FractionalHP += RecoveryAmount;
        if (FractionalHP >= 1.0f)
        {
            int32 AddHP = FMath::FloorToInt(FractionalHP);
            FractionalHP -= static_cast<float>(AddHP);
            PS->CurPlayerData.CurrentHP = FMath::Min(static_cast<int32>(MaxHP), PS->CurPlayerData.CurrentHP + AddHP);
            PS->ForceNetUpdate();
        }
    }
}