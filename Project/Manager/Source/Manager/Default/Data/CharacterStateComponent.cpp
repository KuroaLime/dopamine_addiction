// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Data/CharacterStateComponent.h"
#include "Default/System/UManagerGameInstance.h"
#include "Kismet/GameplayStatics.h"
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
	HoldingGold = 10000;
}


// Called when the game starts
void UCharacterStateComponent::BeginPlay()
{
	Super::BeginPlay();
	SetNewLevel(Level);
	// ...
	
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
			SetHP(CurrentStateData->MaxHP);
			//CurrentHP = CurrentStateData->MaxHP;
		}
		else {
			//ABLOG(Error, TEXT("Level (%d) data doesn't exist"), NewLevel);
		}
	}



}

void UCharacterStateComponent::SetDamage(float NewDamage) {
	//CurrentHP = FMath::Clamp<float>(CurrentHP - NewDamage, 0.0f, CurrentStateData->MaxHP);
	//if (CurrentHP <= 0.0f) {
	//	OnHPIsZero.Broadcast();
	//}
	if (CurrentStateData == nullptr) return;
	SetHP(FMath::Clamp<float>(CurrentHP - NewDamage, 0.0f, CurrentStateData->MaxHP));
}

void UCharacterStateComponent::SetHP(float NewHP) {
	CurrentHP = NewHP;
	OnHPChanged.Broadcast();
	OnLEVELChanged.Broadcast();
	if (CurrentHP < KINDA_SMALL_NUMBER) {
		CurrentHP = 0.0f;
		OnHPIsZero.Broadcast();
		OnGoldChanged.Broadcast();
	}
}

float UCharacterStateComponent::GetAttack() {
	return 10.0f;

}
float UCharacterStateComponent::GetHPRatio() {
	if (CurrentStateData != nullptr) {
		return (CurrentStateData->MaxHP < KINDA_SMALL_NUMBER) ? 0.0f : (CurrentHP / CurrentStateData->MaxHP);
		
	}
	return 0.0f;
}
float UCharacterStateComponent::GetCurrentHP() {
	return (CurrentHP);
}
float UCharacterStateComponent::GetGold() {
	return (HoldingGold);
}
float UCharacterStateComponent::GetMaxHP() {
	if (CurrentStateData == nullptr) return 100.0f;
	return (CurrentStateData->MaxHP);
}
int UCharacterStateComponent::GetLevel() {
	if (CurrentStateData == nullptr) return 1;
	return (CurrentStateData->Level);
}


void UCharacterStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCharacterStateComponent, CurrentHP);
	DOREPLIFETIME(UCharacterStateComponent, Level);
}
void UCharacterStateComponent::OnRep_CurrentHP()
{
	// 클라이언트의 위젯들에게 값이 바뀌었음을 알림
	OnHPChanged.Broadcast();

	if (CurrentHP < KINDA_SMALL_NUMBER)
	{
		OnHPIsZero.Broadcast();
	}
}
void UCharacterStateComponent::OnRep_Level()
{
	SetNewLevel(Level);
	OnLEVELChanged.Broadcast();
}

void UCharacterStateComponent::OnRep_HoldingGold() {
	if (CurrentHP < KINDA_SMALL_NUMBER) {
		OnGoldChanged.Broadcast();
	}
}