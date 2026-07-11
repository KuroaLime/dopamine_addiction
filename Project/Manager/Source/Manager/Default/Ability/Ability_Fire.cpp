#include "Default/Ability/Ability_Fire.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
#include "Default/Ability/Interface/AbilityCheckInterface.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/TPS/Actor/Weapon/WeaponComponent.h"
#include "Game/InGame/TPS/Actor/Weapon/Weapon.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "CollisionShape.h"
#include "Engine/StaticMesh.h"
#include "Game/InGame/MainCharacter.h"

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

	CooldownDuration = 0.0;
	CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));

	bIsServerFire = false;
	bIsClientFire = false;
	LastClientFireTime = 0.f;
	LastServerFireTime = 0.f;
}

void UAbility_Fire::LocalActivateWithOwner(AActor* InOwner)
{
	if (bIsClientFire) return;

	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(Character);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());
	if (!PS_Interface || !GS_Interface) return;

	const EWeaponType WeaponID = PS_Interface->GetWeaponID();

	int32 BaseFireRate = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::FireRate);
	int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);
	float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);

	bool bFullAuto = true;
	GS_Interface->GetWeaponFireMode(WeaponID, bFullAuto);

	bIsClientFire = true;

	float CurrentTime = Character->GetWorld()->GetTimeSeconds();
	float TimeSinceLastShot = CurrentTime - LastClientFireTime;

	TWeakObjectPtr<ACharacter> WeakChar(Character);

	if (TimeSinceLastShot >= FireRate)
	{
		Client_ExecuteFire(InOwner);
		LastClientFireTime = CurrentTime;

		// 세미오토(샷건/저격 등): 한 발만 나가고 홀드해도 루프를 걸지 않음. 다음 발은 재클릭해야 함.
		if (!bFullAuto) return;

		FTimerDelegate Delegate;
		Delegate.BindLambda([this, WeakChar]()
			{
				if (!WeakChar.IsValid()) return;
				Client_ExecuteFire(WeakChar.Get());
				LastClientFireTime = WeakChar->GetWorld()->GetTimeSeconds();
			});

		Character->GetWorldTimerManager().SetTimer(
			ClientFireTimerHandle,
			Delegate,
			FireRate,
			true
		);
	}
	else
	{
		// 쿨다운 중 클릭: 버리지 않고 남은 시간만큼 큐잉. LocalCancelWithOwner가 이 타이머는 건드리지 않으므로
		// 손을 떼도(정상 릴리즈) 예정대로 발사된다.
		float RemainingDelay = FireRate - TimeSinceLastShot;

		FTimerDelegate PendingDelegate;
		PendingDelegate.BindLambda([this, WeakChar, bFullAuto, FireRate]()
			{
				if (!WeakChar.IsValid()) return;
				Client_ExecuteFire(WeakChar.Get());
				LastClientFireTime = WeakChar->GetWorld()->GetTimeSeconds();

				// 큐잉된 발사가 실행되는 시점에도 여전히 누르고 있고 풀오토라면 그때부터 연사 루프 시작.
				if (bFullAuto && bIsClientFire)
				{
					FTimerDelegate LoopDelegate;
					LoopDelegate.BindLambda([this, WeakChar]()
						{
							if (!WeakChar.IsValid()) return;
							Client_ExecuteFire(WeakChar.Get());
							LastClientFireTime = WeakChar->GetWorld()->GetTimeSeconds();
						});

					WeakChar->GetWorldTimerManager().SetTimer(
						ClientFireTimerHandle,
						LoopDelegate,
						FireRate,
						true
					);
				}
			});

		Character->GetWorldTimerManager().SetTimer(
			PendingClientShotTimerHandle,
			PendingDelegate,
			RemainingDelay,
			false
		);
	}
}

void UAbility_Fire::LocalCancelWithOwner(AActor* InOwner)
{
	if (!InOwner) return;
	InOwner->GetWorldTimerManager().ClearTimer(ClientFireTimerHandle);
	bIsClientFire = false;
}

void UAbility_Fire::ActivateAbility()
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	if (bIsServerFire) return;

	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(OwnerCharacter);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface) return;

	const EWeaponType WeaponID = PS_Interface->GetWeaponID();

	int32 BaseFireRate = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::FireRate);
	int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);
	float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);

	bool bFullAuto = true;
	GS_Interface->GetWeaponFireMode(WeaponID, bFullAuto);

	bIsServerFire = true;

	float CurrentTime = OwnerCharacter->GetWorld()->GetTimeSeconds();
	float TimeSinceLastShot = CurrentTime - LastServerFireTime;

	TWeakObjectPtr<ACharacter> WeakChar(OwnerCharacter);

	if (TimeSinceLastShot >= FireRate)
	{
		Server_ExecuteFire();
		LastServerFireTime = CurrentTime;

		// 세미오토(샷건/저격 등): 한 발만 나가고 홀드해도 루프를 걸지 않음. 다음 발은 재클릭해야 함.
		if (!bFullAuto) return;

		FTimerDelegate Delegate;
		Delegate.BindLambda([this, WeakChar]()
			{
				if (!WeakChar.IsValid()) return;
				Server_ExecuteFire();
				LastServerFireTime = WeakChar->GetWorld()->GetTimeSeconds();
			});

		OwnerCharacter->GetWorldTimerManager().SetTimer(
			ServerFireTimerHandle,
			Delegate,
			FireRate,
			true
		);
	}
	else
	{
		// 쿨다운 중 클릭: 버리지 않고 남은 시간만큼 큐잉. EndAbility가 정상 릴리즈(bWasCancelled=false)면
		// 이 타이머를 건드리지 않으므로 손을 떼도 예정대로 발사된다.
		float RemainingDelay = FireRate - TimeSinceLastShot;

		FTimerDelegate PendingDelegate;
		PendingDelegate.BindLambda([this, WeakChar, bFullAuto, FireRate]()
			{
				if (!WeakChar.IsValid()) return;
				Server_ExecuteFire();
				LastServerFireTime = WeakChar->GetWorld()->GetTimeSeconds();

				// 큐잉된 발사가 실행되는 시점에도 여전히 누르고 있고 풀오토라면 그때부터 연사 루프 시작.
				if (bFullAuto && bIsServerFire)
				{
					FTimerDelegate LoopDelegate;
					LoopDelegate.BindLambda([this, WeakChar]()
						{
							if (!WeakChar.IsValid()) return;
							Server_ExecuteFire();
							LastServerFireTime = WeakChar->GetWorld()->GetTimeSeconds();
						});

					WeakChar->GetWorldTimerManager().SetTimer(
						ServerFireTimerHandle,
						LoopDelegate,
						FireRate,
						true
					);
				}
			});

		OwnerCharacter->GetWorldTimerManager().SetTimer(
			PendingServerShotTimerHandle,
			PendingDelegate,
			RemainingDelay,
			false
		);
	}
}

void UAbility_Fire::EndAbility(bool bWasCancelled)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);
	bIsServerFire = false;

	if (bWasCancelled)
	{
		// 사망/무기교체 등 강제 종료라면 큐잉된 발사도 함께 취소. 정상 릴리즈(bWasCancelled=false)면
		// 큐잉된 발사는 그대로 둬서 예정대로 나가게 한다.
		OwnerCharacter->GetWorldTimerManager().ClearTimer(PendingServerShotTimerHandle);
	}
	Super::EndAbility(bWasCancelled);
}

void UAbility_Fire::Client_ExecuteFire(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(Character);
	if (!Owner) return;

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting || !EquippedGun->m_pMesh) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
	//EquippedGun->Setting->Fire(MuzzleLoc);
}

void UAbility_Fire::Server_ExecuteFire()
{
	// 연사 도중 조준을 풀면 그 즉시(다음 발부터) 발사가 멈추도록 매 발마다 재확인한다.
	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(OwnerCharacter);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface) return;

	UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
	if (!FollowCamera) return;

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!EquippedGun || !EquippedGun->Setting || !EquippedGun->m_pMesh) return;

	AMainCharacter* MainChar = Cast<AMainCharacter>(OwnerCharacter);
	if (MainChar)
	{
		AWeapon* EquippedWeapon = MainChar->GetEquippedGun();
		if (EquippedWeapon)
		{
			UWeaponComponent* WeaponComp = EquippedWeapon->Setting;
			if (WeaponComp)
			{
				// 장전 중이면 사격 차단 (장전이 끝날 때까지)
				if (WeaponComp->IsReloading())
				{
					return;
				}
				if (WeaponComp->GetCurrentAmmo() <= 0)
				{
					EndAbility(true);
					return;
				}
				if (OwnerCharacter->HasAuthority())
				{
					WeaponComp->ConsumeAmmo();
				}
			}
		}
	}

	const EWeaponType WeaponID = PS_Interface->GetWeaponID();

	int32 BaseRange = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::Range);
	int32 LvRange = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float Range = CalculateRange(BaseRange, LvRange);

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::Damage);
	int32 LvDamage = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float Damage = CalculateDamage(BaseDamage, LvDamage);

	// 무기별 탄퍼짐/펠릿수/히트 판정 두께 (DT_Weapon에 값이 없으면 기존과 동일하게 무탄퍼짐·단발·라인 트레이스로 동작).
	float SpreadAngle = 0.f;
	int32 PelletCount = 1;
	float TraceRadius = 0.f;
	GS_Interface->GetWeaponFireProfile(WeaponID, SpreadAngle, PelletCount, TraceRadius);

	FVector CamStart = FollowCamera->GetComponentLocation();
	FRotator CamRot = FollowCamera->GetComponentRotation();

	// "Muzzle" 소켓이 무기 스태틱 메시에 없으면 GetSocketLocation이 조용히 m_pMesh 자신의 위치(무기 액터 위치)로
	// 폴백된다. 총구 대신 발밑에서 디버그 라인이 나오는 경우 대부분 이 소켓이 없거나 위치가 잘못된 것.
	if (!EquippedGun->m_pMesh->DoesSocketExist(TEXT("Muzzle")))
	{
		UStaticMesh* GunMesh = EquippedGun->m_pMesh->GetStaticMesh();
		UE_LOG(LogTemp, Warning, TEXT("[DS] Ability_Fire: '%s' 무기 메시에 'Muzzle' 소켓이 없어 MuzzleLoc이 무기 액터 위치로 폴백됨."),
			GunMesh ? *GunMesh->GetName() : TEXT("<NoMesh>"));
	}
	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));

	UWorld* World = OwnerCharacter->GetWorld();
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);

	// 총알 트레이서가 향할 목표(피드백용 대표 지점): 첫 번째 펠릿 기준으로 명중하면 충돌점, 빗나가면 사거리 끝.
	FVector RepresentativeTargetLoc = CamStart + (CamRot.Vector() * Range);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const FVector AimDir = (SpreadAngle > 0.f)
			? FMath::VRandCone(CamRot.Vector(), FMath::DegreesToRadians(SpreadAngle))
			: CamRot.Vector();
		const FVector CamEnd = CamStart + (AimDir * Range);

		FHitResult CamHit;
		const bool bCamHit = (TraceRadius > 0.f)
			? World->SweepSingleByChannel(CamHit, CamStart, CamEnd, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(TraceRadius), Params)
			: World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);

		// 디버그: 실제 판정 트레이스 시각화. 초록=액터 명중, 노랑=명중은 했지만 액터 없음(벽 등), 빨강=완전 미스.
		const FColor DebugColor = (bCamHit && CamHit.GetActor()) ? FColor::Green : (bCamHit ? FColor::Yellow : FColor::Red);
		const FVector DebugEndPoint = bCamHit ? CamHit.ImpactPoint : CamEnd;
		DrawDebugLine(World, CamStart, DebugEndPoint, DebugColor, false, 3.f, 0, 1.5f);
		if (TraceRadius > 0.f)
		{
			// 명중 여부와 무관하게 시작/끝에 실제 스윕 반지름 그대로 그려서 판정 두께가 눈에 보이게 한다.
			DrawDebugSphere(World, CamStart, TraceRadius, 12, DebugColor, false, 3.f);
			DrawDebugSphere(World, DebugEndPoint, TraceRadius, 12, DebugColor, false, 3.f);
		}

		if (PelletIndex == 0)
		{
			RepresentativeTargetLoc = (bCamHit && CamHit.bBlockingHit) ? CamHit.ImpactPoint : CamEnd;
		}

		if (bCamHit && CamHit.GetActor())
		{
			UGameplayStatics::ApplyDamage(
				CamHit.GetActor(),
				Damage,
				OwnerCharacter->GetController(),
				OwnerCharacter,
				nullptr
			);
		}
	}

	EquippedGun->Setting->Multicast_PlayFireFeedback(MuzzleLoc, RepresentativeTargetLoc);
}

float UAbility_Fire::CalculateDamage(int32 Base, int32 Level) const
{
	return static_cast<float>(Base) + (Level * 5.0f);
}

float UAbility_Fire::CalculateRange(int32 Base, int32 Level) const
{
	float Range = static_cast<float>(Base);
	return Range + (Range * (Level * 0.1f));
}

float UAbility_Fire::CalculateFireRate(int32 Base, int32 Level) const
{
	float BaseDelay = Base * 0.1f;

	float FireRate = BaseDelay / (1.f + (Level * 0.1f));
	return FireRate;
}