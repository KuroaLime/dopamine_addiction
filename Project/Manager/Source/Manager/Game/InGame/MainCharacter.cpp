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
#include "Game/InGame/Interface/PhasePlayerControllerInterface.h"
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
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerStart.h"
#include "Game/InGame/TPS/System/HealthRegenComponent.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/TPS/Actor/Spawn/Ability/SpawnManagerComponent.h"
#include "Game/InGame/TPS/Actor/Spawn/A_Spawn.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinDirectorComponent.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

// Sets default values
AMainCharacter::AMainCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;

	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 505.f;
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

	AIStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("AIStimuliSource"));

	bWasAiming = false;
	SetReplicates(true);
}

// Called when the game starts or when spawned
void AMainCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestRegister(this);
	}

	if (AIStimuliSource)
	{
		// 몬스터(황금 고블린 등)의 AIPerceptionComponent(Sight)가 이 플레이어를 감지 대상으로 인식하게 등록.
		AIStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
		AIStimuliSource->RegisterWithPerceptionSystem();
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

	if (HasAuthority())
	{
		EWeaponType TargetWeapon = EWeaponType::None;
		if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
			{
				TargetWeapon = GS->GetWeaponID();
			}
		if (TargetWeapon != EWeaponType::None){
			EquipWeapon(TargetWeapon);
		}
		else if (m_cGun){
			FActorSpawnParameters SpawnParams;
			SpawnParams.Owner = this;
			SpawnParams.Instigator = GetInstigator();
			m_pEquippedGun = GetWorld()->SpawnActor<AWeapon>(m_cGun, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			if (m_pEquippedGun){
				const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
				m_pEquippedGun->AttachToComponent(GetMesh(), AttachmentRules, TEXT("HandGun_R"));
			}
		}
	}

	CharacterState->OnHPIsZero.AddUObject(this, &AMainCharacter::OnCharacterDeath);

	auto CharacterWidget = Cast<UTpsCharacterWidget>(HPBarWidget->GetUserWidgetObject());
	if (nullptr != CharacterWidget)
		CharacterWidget->BindCharacterState(CharacterState);
}

// Called every frame
void AMainCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	bool bIsCurrentlyAiming = IsCharacterAiming();
	if (bIsCurrentlyAiming != bWasAiming)
	{
		bWasAiming = bIsCurrentlyAiming;
		UpdateCharacterStats();
	}

	if (!HPBarWidget || !HPBarWidget->IsVisible()) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return;

	FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();

	FVector CamForward = CameraRotation.Vector();
	FVector CamUp = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Z);

	FRotator StickerRotation = FRotationMatrix::MakeFromXZ(-CamForward, CamUp).Rotator();
	HPBarWidget->SetWorldRotation(StickerRotation);
}

void AMainCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AMainCharacter, m_pEquippedGun);
	DOREPLIFETIME(AMainCharacter, bIsSitting);
}

void AMainCharacter::SetSitting(bool bNewSitting)
{
	if (!HasAuthority())
	{
		return;
	}

	bIsSitting = bNewSitting;
	ForceNetUpdate();
}

void AMainCharacter::OnRep_EquippedGun()
{
	if (m_pEquippedGun)
	{
		const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
		m_pEquippedGun->AttachToComponent(GetMesh(), AttachmentRules, TEXT("HandGun_R"));
	}
}
void AMainCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestUnregister(this);
	}

	if (AMainPlayerState* PS = GetPlayerState<AMainPlayerState>())
	{
		PS->OnPlayerDataChangedNative.RemoveAll(this);
		PS->OnAccumulatedUpgradesChangedNative.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AMainCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (HasAuthority() && !HealthRegen)
	{
		HealthRegen = NewObject<UHealthRegenComponent>(this, TEXT("HEALTHREGEN"));
		if (HealthRegen)
		{
			HealthRegen->RegisterComponent();
		}
	}

	InitPlayerData();
}

void AMainCharacter::UnPossessed()
{
	if (HealthRegen)
	{
		HealthRegen->UnregisterComponent();
	}

	Super::UnPossessed();
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

    if (HasAuthority())
    {
        if (IPhasePlayerStateInterface* PSInterface = Cast<IPhasePlayerStateInterface>(CurrentPS))
        {
            if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
            {
                PSInterface->SetWeaponID(GS->GetWeaponID());
            }
        }
    }

    if (AMainPlayerState* PS = Cast<AMainPlayerState>(CurrentPS))
    {
        if (CharacterState)
        {
            CharacterState->BindToPlayerState(PS);
        }

        PS->OnPlayerDataChangedNative.RemoveAll(this);
        PS->OnAccumulatedUpgradesChangedNative.RemoveAll(this);
        PS->OnPlayerDataChangedNative.AddUObject(this, &AMainCharacter::OnPlayerDataChanged);
        PS->OnAccumulatedUpgradesChangedNative.AddUObject(this, &AMainCharacter::OnAccumulatedUpgradesChanged);
        UpdateCharacterStats();
    }
}

void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AMainCharacter::FellOutOfWorld(const UDamageType& DmgType)
{
	if (HasAuthority())
	{
		FVector SafeLocation = FVector::ZeroVector;
		FRotator SafeRotation = FRotator::ZeroRotator;
		bool bFoundSpawn = false;

		USpawnManagerComponent* ActiveSpawnManager = USpawnManagerComponent::GetActive(this);
		if (ActiveSpawnManager)
		{
			AA_Spawn* CenterSpawn = ActiveSpawnManager->GetRandomCenterSpawnActor();
			if (CenterSpawn)
			{
				SafeLocation = CenterSpawn->GetActorLocation() + FVector(0.f, 0.f, 200.f);
				SafeRotation = CenterSpawn->GetActorRotation();
				bFoundSpawn = true;
			}
		}

		if (!bFoundSpawn)
		{
			SafeLocation = FVector(0.f, 0.f, 500.f);
		}

		TeleportTo(SafeLocation, SafeRotation);

		AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
		if (PS)
		{
			int32 CurrentHP = PS->CurPlayerData.CurrentHP;
			if (CurrentHP > 0)
			{
				PS->ApplyDamage(static_cast<float>(CurrentHP));
			}

			if (PS->CurPlayerData.CurrentHP <= 0)
			{
				OnCharacterDeath();
			}
		}
	}
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

bool AMainCharacter::IsCharacterDeath() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent->HasAnyMatchingGameplayTags(
			FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("State.Movement.Death"))));
	}
	return false;
}

void AMainCharacter::EquipWeapon(EWeaponType NewWeaponID)
{
    if (!HasAuthority() || !GetWorld()) return;

    TSubclassOf<AWeapon> WeaponClassToSpawn = nullptr;
    if (const TSubclassOf<AWeapon>* FoundClass = WeaponClasses.Find(NewWeaponID))
    {
        WeaponClassToSpawn = *FoundClass;
    }

    if (!WeaponClassToSpawn)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[DS] EquipWeapon missing mapped class WeaponType=%d Character=%s; using fallback class."),
            static_cast<int32>(NewWeaponID),
            *GetNameSafe(this));
        WeaponClassToSpawn = m_cGun;
    }
    if (!WeaponClassToSpawn)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[DS] EquipWeapon failed: mapped and fallback classes are null WeaponType=%d Character=%s"),
            static_cast<int32>(NewWeaponID),
            *GetNameSafe(this));
        return;
    }

    if (m_pEquippedGun)
    {
        m_pEquippedGun->Destroy();
        m_pEquippedGun = nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator();
    m_pEquippedGun = GetWorld()->SpawnActor<AWeapon>(
        WeaponClassToSpawn,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        SpawnParams);

    if (m_pEquippedGun)
    {
        if (m_pEquippedGun->Setting)
        {
            m_pEquippedGun->Setting->SetWeaponType(NewWeaponID);
            m_pEquippedGun->Setting->SetCurrentAmmo(
                m_pEquippedGun->Setting->GetMaxMagazineCapacity());
        }

        const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
        m_pEquippedGun->AttachToComponent(GetMesh(), AttachmentRules, TEXT("HandGun_R"));
        m_pEquippedGun->ForceNetUpdate();
    }
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
	if (IsCharacterDeath()) return;

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
	if (IsCharacterDeath()) return;

	if (GetController() != nullptr)
	{
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

float AMainCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority())
	{
		return 0.0f;
	}

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage > 0.0f)
	{
		// 황금 고블린 디렉터가 "전투 소강 상태"를 판단하는 데 쓰는 신호. 새 시스템을 따로 두지 않고
		// 기존 데미지 처리 지점에 한 줄만 훅.
		if (UGoldenGoblinDirectorComponent* Director = UGoldenGoblinDirectorComponent::GetActive(this))
		{
			Director->NotifyCombatEvent();
		}
	}

	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (PS)
	{
		const int32 OldHP = PS->CurPlayerData.CurrentHP;
		PS->ApplyDamage(ActualDamage);

		if (OldHP > 0 && PS->CurPlayerData.CurrentHP <= 0)
		{
			DS_LOG(TEXT("[DS] TPS DeathTrigger Player=%s Damage=%.2f HP=%d->%d"),
				*PS->GetPlayerName(),
				ActualDamage,
				OldHP,
				PS->CurPlayerData.CurrentHP);

			OnCharacterDeath();
		}
	}

	return ActualDamage;
}

void AMainCharacter::OnCharacterDeath()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Release crouch state
	UnCrouch();

	// Death must terminate every held TPS action, not only aiming.
	FGameplayTagContainer ActionTags;
	ActionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));
	ActionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Aim")));
	ActionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Crouch")));
	ActionTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.CardDiscard")));
	AbilitySystemComponent->CancelAbilitiesWithTag(ActionTags);

	if (m_pEquippedGun && m_pEquippedGun->Setting)
	{
		m_pEquippedGun->Setting->CancelReloadLock();
	}

	// Stop playing any montages immediately so the death state machine/animation plays cleanly
	UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		AnimInstance->Montage_Stop(0.2f);
	}

	// Switch state to Death for the local controller immediately to guarantee Death UI and action deactivation
	if (IsLocallyControlled())
	{
		IPhasePlayerControllerInterface* PC = Cast<IPhasePlayerControllerInterface>(GetController());
		if (PC)
		{
			PC->SwitchState(EGamePhase::Death);
		}
	}

	if (!AbilitySystemComponent->HasAnyMatchingGameplayTags(
		FGameplayTagContainer(FGameplayTag::RequestGameplayTag(FName("State.Movement.Death")))))
	{
		static const FGameplayTag DeathTag =
			FGameplayTag::RequestGameplayTag(FName("Ability.Action.Death"));
		AbilitySystemComponent->TryActivateAbilityByTag(DeathTag);
	}
}

void AMainCharacter::UpdateCharacterStats()
{
	AMainPlayerState* PS = GetPlayerState<AMainPlayerState>();
	if (!PS) return;
	if (GetCharacterMovement())
	{
		float BaseSpeed = IsCharacterAiming() ? 300.f : 505.f;
		GetCharacterMovement()->MaxWalkSpeed = PS->GetFinalMoveSpeed(BaseSpeed);
	}
}
void AMainCharacter::OnPlayerDataChanged(const FPlayerData& NewData)
{
	UpdateCharacterStats();
}
void AMainCharacter::OnAccumulatedUpgradesChanged(const FAccumulatedUpgrades& NewUpgrades)
{
	UpdateCharacterStats();
}