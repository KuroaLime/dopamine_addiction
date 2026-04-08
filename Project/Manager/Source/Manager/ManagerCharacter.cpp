// Copyright Epic Games, Inc. All Rights Reserved.

#include "ManagerCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Manager.h"

#include "PlayerManager.h"
#include "Items/BaseItem.h"
#include "Items/SkillItem.h"
#include "CustomASC.h"
#include "Ability/CustomAbility.h"
#include "Ability/AbilityFire.h"
#include "Ability/AbilityJump.h"
#include "Ability/AbilityAim.h"

#include "Data/CharacterStateComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UI/TpsCharacterWidget.h"
#include "UI/TpsPlayerMainHUD.h"
#include "UI/CardPlayerMainHUD.h"
#include "Component/Player/InteractionComponent.h"
#include"Data/CameraStateComponent.h"

DEFINE_LOG_CATEGORY(LogTemplateCharacter);

AManagerCharacter::AManagerCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->SocketOffset = FVector(0.0f, 75.0f, 50.0f);
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->FieldOfView = 90.0f;
	FollowCamera->bUsePawnControlRotation = false;

	AbilitySystemComponent = CreateDefaultSubobject<UCustomASC>(TEXT("AbilitySystemComponent"));
	PrimaryActorTick.bCanEverTick = true;
	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named TshirdPersonCharacter (to avoid direct content references in C++)


	//데이터 컴포넌트 초기화
	CharacterState = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("CHARACTERSTATE"));
	//카메라 상태 컴포넌트 초기화
	CameraState = CreateDefaultSubobject<UCameraStateComponent>(TEXT("CAMERASTATE"));


	//캐릭터 위 HPBar
	HPBarWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("HPBARWIDGET"));
	HPBarWidget->SetupAttachment(RootComponent);
	HPBarWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 180.0f));
	HPBarWidget->SetWidgetSpace(EWidgetSpace::World);
	static ConstructorHelpers::FClassFinder<UUserWidget> UI_HUD(TEXT("/Game/UI/UI_HPBar.UI_HPBar_C"));
	if (UI_HUD.Succeeded()) {
		HPBarWidget->SetWidgetClass(UI_HUD.Class);
		HPBarWidget->SetDrawSize(FVector2D(150.0f, 50.0f));
	}
	HPBarWidget->SetDrawAtDesiredSize(true);
	HPBarWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HPBarWidget->SetOwnerNoSee(true);
	

	//플레이어 디폴트 UI
	InteractionComponent = CreateDefaultSubobject<UInteractionComponent>(TEXT("InteractionComponent"));
	
}

void AManagerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestRegister(this);
	}

	//사용 가능 어빌리티 등록
	if (AbilitySystemComponent)
	{
		for (TSubclassOf<UCustomAbility> AbilityClass : DefaultAbilities)
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

		// 1. 무기 액터 생성
		m_pEquippedGun = GetWorld()->SpawnActor<AWeapon>(m_cGun, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
		
		if (m_pEquippedGun)
		{
			// 2. 캐릭터 손 소켓에 부착 (소켓 이름 확인 필수)
			const FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
			m_pEquippedGun->AttachToComponent(GetMesh(), AttachmentRules, TEXT("HandGun_R"));
			m_pEquippedGun->SetActorRelativeRotation(FRotator(0.f, 180.f, 0.f));
		}
	}

	CharacterState->OnHPIsZero.AddLambda([this]()->void {
		SetActorEnableCollision(false);
		});
	//
	auto CharacterWidget = Cast<UTpsCharacterWidget>(HPBarWidget->GetUserWidgetObject());
	if (nullptr != CharacterWidget)
		CharacterWidget->BindCharacterState(CharacterState);

}

void AManagerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{

	if (UPlayerManager* Manager = GetWorld()->GetSubsystem<UPlayerManager>())
	{
		Manager->RequestUnregister(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AManagerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	//// Set up action bindings
	//if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
	//	// Moving
	//	EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AManagerCharacter::Move);
	//	EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AManagerCharacter::Look);

	//	// Looking
	//	EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AManagerCharacter::Look);

	//	UE_LOG(LogTemp, Warning, TEXT("IA_Skill00 = %s"),
	//		IA_Skill00 ? *IA_Skill00->GetName() : TEXT("NULL"));
	//}
	//else
	//{
	//	UE_LOG(LogManager, Error, 
	//		TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	//}
}
void AManagerCharacter::Tick(float DeltaTime)
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
void AManagerCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AManagerCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AManagerCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AManagerCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}






float AManagerCharacter::TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser)
{
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (CharacterState)
	{
		CharacterState->SetDamage(ActualDamage);
	}

	if (CharacterState->GetCurrentHP() <= 0) { GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Dieeeeeeeeeeeeeeeeeeeeeeeeeeee!")); }
	else {
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("Damageddddd!"));
	}

	return ActualDamage;
}

void AManagerCharacter::AddItemToInventory(ABaseItem* NewItem)
{
	if (NewItem)
	{
		Inventory.Add(NewItem);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green,
				FString::Printf(TEXT("Inventory Count: %d"), Inventory.Num()));
		}
	}
}

void AManagerCharacter::AddSkillToInventory(ASkillItem* NewSkill)
{
	if (NewSkill)
	{
		SkillInventory.Add(NewSkill);

		if (GEngine)
		{
			FString Msg = FString::Printf(TEXT("SKILL Added! Total Skills: %d"), SkillInventory.Num());
			GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan, Msg);
		}
	}
}

void AManagerCharacter::DropLastItem()
{
	if (Inventory.Num() == 0) return;

	ABaseItem* ItemToDrop = Inventory.Pop();

	if (ItemToDrop && ItemToDrop->IsValidLowLevel())
	{
		FVector DropLoc = GetActorLocation() + (GetActorForwardVector() * 150.0f);

		ItemToDrop->OnDropped(DropLoc);

		UE_LOG(LogTemp, Warning, TEXT("Dropped Item. Remaining Inventory: %d"), Inventory.Num());
	}
}