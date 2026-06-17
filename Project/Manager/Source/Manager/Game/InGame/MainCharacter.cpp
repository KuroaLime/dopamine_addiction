// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/InGame/MainCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "InputActionValue.h"
#include "Manager.h"

#include "PlayerManager.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/GAS/PFGAbility.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"

#include "Default/Data/CharacterStateComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Game/InGame/TPS/UI/TpsCharacterWidget.h" 
//#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Default/Component/Player/InteractionComponent.h"
#include "Game/InGame/MainPlayerState.h"
#include "Default/Data/CameraStateComponent.h"

// Sets default values
AMainCharacter::AMainCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 75.0f, 50.0f);
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->FieldOfView = 90.0f;
	FollowCamera->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UPFGASC>(TEXT("AbilitySystemComponent"));
	PrimaryActorTick.bCanEverTick = true;

	CharacterState = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("CHARACTERSTATE"));
	CameraState = CreateDefaultSubobject<UCameraStateComponent>(TEXT("CAMERASTATE"));

	HPBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBARWIDGET"));
	HPBarWidget->SetupAttachment(RootComponent);
	HPBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	HPBarWidget->SetWidgetSpace(EWidgetSpace::World);
	static ConstructorHelpers::FClassFinder<UUserWidget> UI_HUD(TEXT("/Game/InGame/TPS/UI/UI_HPBar.UI_HPBar_C"));
	if (UI_HUD.Succeeded()) {
		HPBarWidget->SetWidgetClass(UI_HUD.Class);
		HPBarWidget->SetDrawSize(FVector2D(150.0f, 50.0f));
	}
	HPBarWidget->SetDrawAtDesiredSize(true);
	HPBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPBarWidget->SetOwnerNoSee(true);

	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
}

// Called when the game starts or when spawned
void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestRegister(this);
	}

	if (AbilitySystemComponent)
	{
		for (TSubclassOf<UPFGAbility> AbilityClass : DefaultAbilities)
		{
			if (AbilityClass)
			{
				AbilitySystemComponent->GiveAbility(AbilityClass);
			}
		}
	}

	if (m_cGun)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = GetInstigator();

		m_pEquippedGun = GetWorld()->SpawnActor<AWeapon>(m_cGun, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

		if (m_pEquippedGun)
		{
			const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
			m_pEquippedGun->AttachToComponent(GetMesh(), AttachmentRules, TEXT("HandGun_R"));
			m_pEquippedGun->SetActorRelativeRotation(FRotator(0.f, 180.f, 0.f));
		}
	}

	CharacterState->OnHPIsZero.AddLambda([this]()->void {
		SetActorEnableCollision(false);
		});

	auto CharacterWidget = Cast<UTpsCharacterWidget>(HPBarWidget->GetUserWidgetObject());
	if (nullptr != CharacterWidget)
		CharacterWidget->BindCharacterState(CharacterState);
}

// Called every frame
void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!HPBarWidget || !HPBarWidget->IsVisible()) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return;

	FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();

	FVector CamForward = CameraRotation.Vector();
	FVector CamUp = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);

	FRotator StickerRotation = FRotationMatrix::MakeFromXZ(-CamForward, CamUp).Rotator();
	HPBarWidget->SetWorldRotation(StickerRotation);
}

void AMainCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestUnregister(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AMainCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitPlayerData();
}

void AMainCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitPlayerData();
}

void AMainCharacter::InitPlayerData()
{
	APlayerState* CurrentPS = GetPlayerState();
	if (!CurrentPS) return;

	if (IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(CurrentPS))
	{
		if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
		{
			PS_Interface->SetWeaponID(GS->GetWeaponID());
		}
	}

	if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>())
	{
		if (CharacterState){
			CharacterState->BindToPlayerState(PS);
		}
	}
}

// Called to bind functionality to input
void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

bool AMainCharacter::IsCharacterAiming() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->HasAnyMatchingGameplayTags(
			FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("State.Movement.Aiming"))));
	}
	return false;
}

void AMainCharacter::EquipWeapon(EWeaponType NewWeaponID)
{
	if (!HasAuthority()) return;
}

void AMainCharacter::Move(const FInputActionValue& Value)
{
	FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMainCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMainCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMainCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

float AMainCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (HasAuthority()) {
		AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
		if (PS) {
			PS->ApplyDamage(ActualDamage);
		}
	}


	return ActualDamage;
}

