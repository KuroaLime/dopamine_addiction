// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinCharacter.h"
#include "Default/Ability/GAS/PFGASC.h"
#include "Default/Ability/GAS/PFGAttributeSet.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinDirectorComponent.h"
#include "Game/InGame/TPS/Actor/Monster/GoldenGoblin/GoldenGoblinHPWidget.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "NiagaraFunctionLibrary.h"

AGoldenGoblinCharacter::AGoldenGoblinCharacter()
{
	// HP 위젯을 매 프레임 카메라 쪽으로 돌리고 가림 여부를 확인해야 해서 틱이 필요하다.
	PrimaryActorTick.bCanEverTick = true;
	SetReplicates(true);

	// 컨트롤러 회전을 그대로 받지 않고, 이동 방향을 보고 부드럽게 도는 방식으로 전환.
	// 이게 없으면 AIController가 매 순간 목표 방향으로 순간 스냅 회전시켜서 뻣뻣하고
	// 옆으로 미끄러지듯 움직이는 것처럼 보인다.
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bOrientRotationToMovement = true;
		Movement->RotationRate = FRotator(0.f, 300.f, 0.f);
		// 급정지/급가속 대신 서서히 붙고 서서히 멈추도록.
		Movement->BrakingDecelerationWalking = 900.f;
		Movement->MaxAcceleration = 900.f;
	}

	AbilitySystemComponent = CreateDefaultSubobject<UPFGASC>(TEXT("AbilitySystemComponent"));
}

void AGoldenGoblinCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority() && AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
	{
		UPFGAttributeSet* AttributeSet = AbilitySystemComponent->AttributeSet;
		AttributeSet->SetMaxHealth(GoblinMaxHealth);
		// 생성자 기본값(100)에서 실제 GoblinMaxHealth로 채워서 풀피 상태로 시작.
		AttributeSet->ApplyHealthDelta(GoblinMaxHealth);
	}

	SetGoblinMoveSpeed(PatrolMoveSpeed);

	TryBindHealthDelegate();

	// 블루프린트에 배치된 머리 위 HP 위젯을 자동으로 찾아 연결한다 (별도 BP 그래프 배선 불필요).
	// MainCharacter가 HPBarWidget->GetUserWidgetObject()를 캐스팅해서 BindCharacterState를 호출하는 것과 같은 패턴.
	if (UWidgetComponent* WidgetComp = FindComponentByClass<UWidgetComponent>())
	{
		if (UGoldenGoblinHPWidget* HPWidget = Cast<UGoldenGoblinHPWidget>(WidgetComp->GetUserWidgetObject()))
		{
			HPWidget->BindGoblin(this);
		}
	}
}

void AGoldenGoblinCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateHPWidgetVisibility();
}

void AGoldenGoblinCharacter::UpdateHPWidgetVisibility()
{
	UWidgetComponent* WidgetComp = FindComponentByClass<UWidgetComponent>();
	if (!WidgetComp)
	{
		return;
	}

	// 죽었으면 무조건 숨김 (각도/가림 판정보다 우선).
	if (bIsDead)
	{
		WidgetComp->SetVisibility(false);
		return;
	}

	// 이 코드는 서버/클라 양쪽에서 다 도는데, 로컬 뷰어(이 화면을 보는 사람)가 없으면
	// (데디케이티드 서버) 카메라 기준 판정을 할 이유가 없으니 스킵한다.
	const APlayerController* LocalPC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!LocalPC || !LocalPC->PlayerCameraManager)
	{
		return;
	}

	const FVector CameraLocation = LocalPC->PlayerCameraManager->GetCameraLocation();
	const FVector WidgetLocation = WidgetComp->GetComponentLocation();

	// 카메라 쪽을 항상 바라보도록(빌보드) — 특정 각도에서 안 보이던 문제 해결.
	WidgetComp->SetWorldRotation((CameraLocation - WidgetLocation).Rotation());

	// 카메라와 위젯 사이에 장애물이 있으면 숨긴다.
	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GoblinHPWidgetOcclusion), false);
	QueryParams.AddIgnoredActor(this);
	const bool bOccluded = GetWorld()->LineTraceSingleByChannel(Hit, CameraLocation, WidgetLocation, ECC_Visibility, QueryParams);

	WidgetComp->SetVisibility(!bOccluded);
}

void AGoldenGoblinCharacter::TryBindHealthDelegate()
{
	if (AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
	{
		AbilitySystemComponent->AttributeSet->OnHealthChanged.AddUObject(this, &AGoldenGoblinCharacter::HandleHealthChanged);
		// 위젯이 시작하자마자 정확한 값을 갖도록 현재 값으로 한 번 방송.
		HandleHealthChanged(0.f, AbilitySystemComponent->AttributeSet->Health.GetCurrentValue());
		return;
	}

	// 클라이언트에서는 AttributeSet(리플리케이트된 서브오브젝트)이 BeginPlay 시점에
	// 아직 도착하지 않았을 수 있다. 도착할 때까지 짧게 재시도.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(HealthBindRetryTimer, this, &AGoldenGoblinCharacter::TryBindHealthDelegate, 0.2f, false);
	}
}

void AGoldenGoblinCharacter::HandleHealthChanged(float OldValue, float NewValue)
{
	const float MaxHealth = GetMaxHealthValue();
	const float Ratio = MaxHealth > 0.f ? FMath::Clamp(NewValue / MaxHealth, 0.f, 1.f) : 0.f;
	OnGoblinHealthChanged.Broadcast(NewValue, MaxHealth, Ratio);
}

float AGoldenGoblinCharacter::GetCurrentHealth() const
{
	return (AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
		? AbilitySystemComponent->AttributeSet->Health.GetCurrentValue()
		: 0.f;
}

float AGoldenGoblinCharacter::GetMaxHealthValue() const
{
	return (AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
		? AbilitySystemComponent->AttributeSet->MaxHealth.GetCurrentValue()
		: GoblinMaxHealth;
}

float AGoldenGoblinCharacter::GetHealthRatio() const
{
	const float MaxHealth = GetMaxHealthValue();
	return MaxHealth > 0.f ? FMath::Clamp(GetCurrentHealth() / MaxHealth, 0.f, 1.f) : 0.f;
}

void AGoldenGoblinCharacter::SetGoblinMoveSpeed(float NewSpeed)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->MaxWalkSpeed = NewSpeed;
	}
}

float AGoldenGoblinCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bIsDead)
	{
		return 0.0f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage > 0.0f)
	{
		if (UGoldenGoblinDirectorComponent* Director = UGoldenGoblinDirectorComponent::GetActive(this))
		{
			Director->NotifyCombatEvent();
		}
	}

	if (AbilitySystemComponent && AbilitySystemComponent->AttributeSet)
	{
		UPFGAttributeSet* AttributeSet = AbilitySystemComponent->AttributeSet;
		const float OldHealth = AttributeSet->Health.GetCurrentValue();
		AttributeSet->ApplyHealthDelta(-ActualDamage);
		const float NewHealth = AttributeSet->Health.GetCurrentValue();

		UE_LOG(LogTemp, Warning, TEXT("[Goblin] TakeDamage Amount=%.1f HP %.1f -> %.1f (Max=%.1f)"),
			ActualDamage, OldHealth, NewHealth, AttributeSet->MaxHealth.GetCurrentValue());

		if (OldHealth > 0.0f && NewHealth <= 0.0f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Goblin] Death threshold crossed, calling HandleDeath"));
			HandleDeath();
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Goblin] TakeDamage: AbilitySystemComponent or AttributeSet is NULL, HP never updated!"));
	}

	return ActualDamage;
}

void AGoldenGoblinCharacter::Multicast_PlayDeathEffect_Implementation(UNiagaraSystem* Effect, FVector Location)
{
	if (Effect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), Effect, Location);
	}
}

void AGoldenGoblinCharacter::HandleDeath()
{
	if (bIsDead)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Goblin] HandleDeath called again, already dead - ignored"));
		return;
	}

	bIsDead = true;
	UE_LOG(LogTemp, Warning, TEXT("[Goblin] HandleDeath: bIsDead=true, broadcasting OnGoblinDied (bound listeners=%d)"), OnGoblinDied.IsBound());
	OnGoblinDied.Broadcast();
}
