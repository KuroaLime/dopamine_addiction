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
	if (!bIsClientFire)
	{
		ACharacter* Character = Cast<ACharacter>(InOwner);
		if (!Character) return;

		IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(Character);
		if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

		IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
		IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());
		if (!PS_Interface || !GS_Interface) return;

		int32 BaseFireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(),
			EWeaponBaseStatType::FireRate);
		int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);

		float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);

		float CurrentTime = Character->GetWorld()->GetTimeSeconds();
		float TimeSinceLastShot = CurrentTime - LastClientFireTime;

		bIsClientFire = true;

		TWeakObjectPtr<ACharacter> WeakChar(Character);

		if (TimeSinceLastShot >= FireRate)
		{
			// 즉시 발사 가능
			Client_ExecuteFire(InOwner);
			LastClientFireTime = CurrentTime;

			// 이후 루핑 타이머 시작
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
			// 지연 발사: 남은 쿨다운만큼 대기 후 첫 발사 실행, 이후 루핑 전환
			float InitialDelay = FireRate - TimeSinceLastShot;

			FTimerDelegate FirstShotDelegate;
			FirstShotDelegate.BindLambda([this, WeakChar, FireRate]()
				{
					if (!WeakChar.IsValid()) return;
					Client_ExecuteFire(WeakChar.Get());
					LastClientFireTime = WeakChar->GetWorld()->GetTimeSeconds();

					// 첫 발사 후 정규 루핑 타이머로 재설정
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
				});

			Character->GetWorldTimerManager().SetTimer(
				ClientFireTimerHandle,
				FirstShotDelegate,
				InitialDelay,
				false
			);
		}
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

	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(OwnerCharacter);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

	if (!bIsServerFire)
	{
		IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
		IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
		IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

		if (!Owner || !PS_Interface || !GS_Interface) return;

		int32 BaseFireRate = GS_Interface->GetWeaponBaseData(
			PS_Interface->GetWeaponID(),
			EWeaponBaseStatType::FireRate);
		int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);

		float FireRate = CalculateFireRate(BaseFireRate, LvFireRate);

		float CurrentTime = OwnerCharacter->GetWorld()->GetTimeSeconds();
		float TimeSinceLastShot = CurrentTime - LastServerFireTime;

		bIsServerFire = true;

		TWeakObjectPtr<ACharacter> WeakChar(OwnerCharacter);

		if (TimeSinceLastShot >= FireRate)
		{
			Server_ExecuteFire();
			LastServerFireTime = CurrentTime;

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
			// 지연 발사: 서버에서도 남은 시간만큼 대기 후 첫 발사 실행, 이후 루핑 전환
			float InitialDelay = FireRate - TimeSinceLastShot;

			FTimerDelegate FirstShotDelegate;
			FirstShotDelegate.BindLambda([this, WeakChar, FireRate]()
				{
					if (!WeakChar.IsValid()) return;
					Server_ExecuteFire();
					LastServerFireTime = WeakChar->GetWorld()->GetTimeSeconds();

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
				});

			OwnerCharacter->GetWorldTimerManager().SetTimer(
				ServerFireTimerHandle,
				FirstShotDelegate,
				InitialDelay,
				false
			);
		}
	}
}

void UAbility_Fire::EndAbility(bool bWasCancelled)
{
	if (!OwnerCharacter || !OwnerCharacter->HasAuthority()) return;
	OwnerCharacter->GetWorldTimerManager().ClearTimer(ServerFireTimerHandle);
	bIsServerFire = false;
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