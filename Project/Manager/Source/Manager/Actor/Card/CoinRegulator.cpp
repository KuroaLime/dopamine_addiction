// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/Card/CoinRegulator.h"
#include "Actor/Card/WG_HavingCoin.h"
#include "Data/CharacterStateComponent.h"
#include "Components/WidgetComponent.h"

#include "Kismet/GameplayStatics.h"
#include "ManagerCharacter.h"
// Sets default values
ACoinRegulator::ACoinRegulator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BODY"));

	RootComponent = Body;

	HavingCoin = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBARWIDGET"));
	HavingCoin->SetupAttachment(RootComponent);
	HavingCoin->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	HavingCoin->SetWidgetSpace(EWidgetSpace::World);
	static ConstructorHelpers::FClassFinder<UUserWidget> UI_HUD(TEXT("/Game/Actor/Card/CoinRegulatorWidget.CoinRegulatorWidget_C"));
	
	if (UI_HUD.Succeeded()) {
		UE_LOG(LogTemp, Warning, TEXT("--- [CoinRegulator] BeginPlay Start ---"));
		HavingCoin->SetWidgetClass(UI_HUD.Class);
		//HavingCoin->SetDrawSize(FVector2D(150.0f, 50.0f));
	}
	HavingCoin->SetDrawAtDesiredSize(true);
	HavingCoin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HavingCoin->SetOwnerNoSee(false);
}

// Called when the game starts or when spawned
void ACoinRegulator::BeginPlay()
{
	//if (CoinWidgetClass)
	//{
	//	HavingCoin->SetWidgetClass(CoinWidgetClass);
	//}
	//else
	//{
	//	UE_LOG(LogTemp, Error, TEXT("CoinWidgetClass is NULL! Please set it in Blueprint Editor."));
	//}

	Super::BeginPlay();
	
	if (!TargetPS)
	{
		ACharacter* MyCharacter = UGameplayStatics::GetPlayerCharacter(this, 0);

		if (AManagerCharacter* ManagerChar = Cast<AManagerCharacter>(MyCharacter))
		{
			TargetPS = ManagerChar->CharacterState;
		}
	}

	auto CharacterWidget = Cast<UWG_HavingCoin>(HavingCoin->GetUserWidgetObject());
	if (nullptr != CharacterWidget && nullptr != TargetPS)
	{
		CharacterWidget->BindCharacterState(TargetPS);
	}

}

// Called every frame
void ACoinRegulator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}






