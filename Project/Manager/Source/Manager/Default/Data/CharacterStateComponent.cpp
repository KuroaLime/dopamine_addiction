// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Data/CharacterStateComponent.h"
#include "Default/System/UManagerGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Game/InGame/MainPlayerState.h"
#include "Game/InGame/MainPlayerController.h"

#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
UCharacterStateComponent::UCharacterStateComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = true;
	SetIsReplicatedByDefault(true);
	// ...
	Level = 1;
	//HoldingGold = 10000;

	
}

// Called when the game starts
void UCharacterStateComponent::BeginPlay()
{
	Super::BeginPlay();
	SetNewLevel(Level);
	// ...
	
	if (APawn* Pawn = Cast<APawn>(GetOwner())) {
		if (AMainPlayerState* PS = Cast<AMainPlayerState>(Pawn->GetPlayerState())) {
			BindToPlayerState(PS);
		}
	}
}

// Called every frame
void UCharacterStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UCharacterStateComponent::InitializeComponent() {
	
	Super::InitializeComponent();
	//SetNewLevel(Level);
}

void UCharacterStateComponent::SetNewLevel(int32 NewLevel) {
	auto ABGameInstance = Cast<UUManagerGameInstance>(UGameplayStatics::GetGameInstance(GetWorld()));

	//ABCHECK(nullptr != ABGameInstance);
	if(nullptr != ABGameInstance)
	{
		CurrentStateData = ABGameInstance->GetABCharacterData(NewLevel);
		if (nullptr != CurrentStateData) {
			Level = NewLevel;
			
		}
		else {
			//ABLOG(Error, TEXT("Level (%d) data doesn't exist"), NewLevel);
		}
	}
}

float UCharacterStateComponent::GetAttack() {
	return 10.0f;

}

float UCharacterStateComponent::GetHPRatio() {
	float MaxHP = GetMaxHP();
	if (MaxHP < KINDA_SMALL_NUMBER)
		return 0.0f;
	return GetCurrentHP() / MaxHP;
}

float UCharacterStateComponent::GetCurrentHP() {
	if (APawn* Pawn = Cast<APawn>(GetOwner())) {
		if (AMainPlayerState* PS = Cast<AMainPlayerState>(Pawn->GetPlayerState())) {
			return PS->CurPlayerData.CurrentHP;

		}
	}
	return 0.0f;
}
int32 UCharacterStateComponent::GetCurrentAmmoCount() {
	if (APawn* Pawn = Cast<APawn>(GetOwner())) {
		if (AMainPlayerState* PS = Cast<AMainPlayerState>(Pawn->GetPlayerState())) {

			return PS->GetCarriedAmmoByWeaponType(PS->GetWeaponID());

		}
	}
	return 0;
}

//데이터 테이블 만들어서 수정 필요(하드)
int32 UCharacterStateComponent::GetMaxAmmoCount() {
	if (APawn* Pawn = Cast<APawn>(GetOwner())) {
		if (AMainPlayerState* PS = Cast<AMainPlayerState>(Pawn->GetPlayerState())) {

			return PS->GetCarriedAmmoByWeaponType(PS->GetWeaponID());

		}
	}
	return 0;
}

float UCharacterStateComponent::GetMaxHP() {
	if (CurrentStateData == nullptr) return 100.0f;
	float BaseHP = CurrentStateData->MaxHP;

	if (APawn* Pawn = Cast<APawn>(GetOwner())) {
		if (AMainPlayerState* PS = Cast<AMainPlayerState>(Pawn->GetPlayerState())) {
			return PS->GetFinalMaxHP(BaseHP);
		}
	}
	return BaseHP;
}

int UCharacterStateComponent::GetLevel() {
	if (CurrentStateData == nullptr) return 1;
	return (CurrentStateData->Level);
}

void UCharacterStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//DOREPLIFETIME(UCharacterStateComponent, CurrentHP);
	DOREPLIFETIME(UCharacterStateComponent, Level);
}

void UCharacterStateComponent::OnRep_Level()
{
	SetNewLevel(Level);
	OnLEVELChanged.Broadcast();
}

void UCharacterStateComponent::OnRep_HoldingGold(float NewGold) {
	if (NewGold < MAX_GOLD) {
		OnGoldChanged.Broadcast(NewGold);
	}
}

void UCharacterStateComponent::OnRep_ChangeCurrentHP(float NewHP)
{
	OnHPChanged.Broadcast();
	if (NewHP <= 0.0f) {
		OnHPIsZero.Broadcast();
	}
}

void UCharacterStateComponent::BindToPlayerState(AMainPlayerState* PS)
{
	if (PS) {
		PS->OnGoldChnageNative.RemoveAll(this);
		PS->OnGoldChnageNative.AddUObject(this, &UCharacterStateComponent::OnRep_HoldingGold);
		PS->OnHPChnageNative.AddUObject(this, &UCharacterStateComponent::OnRep_ChangeCurrentHP);

		OnRep_HoldingGold(PS->CurPlayerData.HoldingGold);
		OnRep_ChangeCurrentHP(PS->CurPlayerData.CurrentHP);
	}
}
