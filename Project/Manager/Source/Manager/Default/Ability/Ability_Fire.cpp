#include "Default/Ability/Ability_Fire.h"
#include "Manager.h"
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
#include "Game/InGame/Handler/UIHandler.h"
#include "Game/InGame/TPS/UI/TpsPlayerMainHUD.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/MainPlayerController.h"

namespace
{
	constexpr float MinFireIntervalSeconds = 0.01f;
	constexpr float FireRetryIntervalSeconds = 0.02f;
}

UAbility_Fire::UAbility_Fire()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Fire")));

	CooldownDuration = 0.0;
	CooldownTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Cooldown.Fire")));

	ActivationOwnedTags.AddTag(FGameplayTag::RequestGameplayTag(FName("State.Movement.Firing")));

	bIsServerFire = false;
	LastServerFireTime = 0.f;
}

void UAbility_Fire::LocalActivateWithOwner(AActor* InOwner)
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	UWorld* World = Character ? Character->GetWorld() : nullptr;
	if (!Character || !World) return;

	IAbilityOwnerInterface* OwnerInterface = Cast<IAbilityOwnerInterface>(Character);
	AWeapon* EquippedGun = OwnerInterface ? Cast<AWeapon>(OwnerInterface->GetEquippedWeapon()) : nullptr;
	UWeaponComponent* WeaponComp = EquippedGun ? EquippedGun->Setting : nullptr;
	if (!WeaponComp) return;

	if (WeaponComp->IsClientFiring()) return;

	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(Character);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming()) return;

	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(World->GetGameState());
	if (!PS_Interface || !GS_Interface) return;

	const EWeaponType WeaponID = PS_Interface->GetWeaponID();

	int32 BaseFireRate = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::FireRate);
	int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);
	const float FireRate = FMath::Max(CalculateFireRate(BaseFireRate, LvFireRate), MinFireIntervalSeconds);

	bool bFullAuto = true;
	GS_Interface->GetWeaponFireMode(WeaponID, bFullAuto);

	WeaponComp->SetClientFiring(true);

	float CurrentTime = Character->GetWorld()->GetTimeSeconds();
	float TimeSinceLastShot = CurrentTime - WeaponComp->GetLastClientFireTime();

	TWeakObjectPtr<ACharacter> WeakChar(Character);
	TWeakObjectPtr<UWeaponComponent> WeakWeaponComp(WeaponComp);

	if (TimeSinceLastShot >= FireRate)
	{
		Client_ExecuteFire(InOwner);
		WeaponComp->SetLastClientFireTime(CurrentTime);

		// 세미오토(샷건/저격 등): 한 발만 나가고 홀드해도 루프를 걸지 않음. 다음 발은 재클릭해야 함.
		if (!bFullAuto) return;

		FTimerDelegate Delegate;
		Delegate.BindLambda([this, WeakChar, WeakWeaponComp]()
			{
				if (!WeakChar.IsValid() || !WeakWeaponComp.IsValid()) return;
				Client_ExecuteFire(WeakChar.Get());
				WeakWeaponComp->SetLastClientFireTime(WeakChar->GetWorld()->GetTimeSeconds());
			});

		Character->GetWorldTimerManager().SetTimer(
			WeaponComp->GetClientFireTimerHandle(),
			Delegate,
			FireRate,
			true
		);
	}
	else
	{
		// 쿨다운이 아직 안 끝났으면, 버튼을 누르고 있는 동안만 짧은 간격으로 재검사해서 쿨다운이 끝나는
		// 순간 자동 발사한다(상용 게임과 동일). 손을 떼면(SetClientFiring(false)) 그 즉시 재검사를 멈춘다.
		constexpr float RetryInterval = 0.02f;

		FTimerDelegate RetryDelegate;
		RetryDelegate.BindLambda([this, WeakChar, WeakWeaponComp, bFullAuto, FireRate]()
			{
				if (!WeakChar.IsValid() || !WeakWeaponComp.IsValid() || !WeakWeaponComp->IsClientFiring())
				{
					if (WeakChar.IsValid() && WeakWeaponComp.IsValid())
					{
						WeakChar->GetWorldTimerManager().ClearTimer(WeakWeaponComp->GetClientFireRetryTimerHandle());
					}
					return;
				}

				ACharacter* Char = WeakChar.Get();
				UWeaponComponent* WComp = WeakWeaponComp.Get();
				const float Now = Char->GetWorld()->GetTimeSeconds();
				if (Now - WComp->GetLastClientFireTime() < FireRate) return; // 아직 준비 안 됨, 다음 재검사 때 다시 확인

				Char->GetWorldTimerManager().ClearTimer(WComp->GetClientFireRetryTimerHandle());

				Client_ExecuteFire(Char);
				WComp->SetLastClientFireTime(Now);

				if (!bFullAuto) return;

				FTimerDelegate LoopDelegate;
				LoopDelegate.BindLambda([this, WeakChar, WeakWeaponComp]()
					{
						if (!WeakChar.IsValid() || !WeakWeaponComp.IsValid()) return;
						Client_ExecuteFire(WeakChar.Get());
						WeakWeaponComp->SetLastClientFireTime(WeakChar->GetWorld()->GetTimeSeconds());
					});

				Char->GetWorldTimerManager().SetTimer(WComp->GetClientFireTimerHandle(), LoopDelegate, FireRate, true);
			});

		Character->GetWorldTimerManager().SetTimer(WeaponComp->GetClientFireRetryTimerHandle(), RetryDelegate, RetryInterval, true);
	}
}

void UAbility_Fire::LocalCancelWithOwner(AActor* InOwner)
{
	if (!InOwner) return;

	IAbilityOwnerInterface* OwnerInterface = Cast<IAbilityOwnerInterface>(InOwner);
	AWeapon* EquippedGun = OwnerInterface ? Cast<AWeapon>(OwnerInterface->GetEquippedWeapon()) : nullptr;
	UWeaponComponent* WeaponComp = EquippedGun ? EquippedGun->Setting : nullptr;
	if (!WeaponComp) return;

	InOwner->GetWorldTimerManager().ClearTimer(WeaponComp->GetClientFireTimerHandle());
	InOwner->GetWorldTimerManager().ClearTimer(WeaponComp->GetClientFireRetryTimerHandle());
	WeaponComp->SetClientFiring(false);

	// 트리거를 놓으면 조준점 블룸 표시도 리셋.
	WeaponComp->SetClientShotsFiredInBurst(0);
	WeaponComp->SetCurrentBloomDegrees(0.f);
}

void UAbility_Fire::ActivateAbility()
{
	UWorld* World = IsValid(OwnerCharacter) ? OwnerCharacter->GetWorld() : nullptr;
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !World || bIsServerFire)
	{
		EndAbility(true);
		return;
	}

	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(OwnerCharacter);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming())
	{
		EndAbility(true);
		return;
	}

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(World->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface)
	{
		EndAbility(true);
		return;
	}

	const EWeaponType WeaponID = PS_Interface->GetWeaponID();

	int32 BaseFireRate = GS_Interface->GetWeaponBaseData(WeaponID, EWeaponBaseStatType::FireRate);
	int32 LvFireRate = PS_Interface->GetWeaponStatLV(EWeaponStatType::FireRate);
	const float FireRate = FMath::Max(CalculateFireRate(BaseFireRate, LvFireRate), MinFireIntervalSeconds);

	bool bFullAuto = true;
	GS_Interface->GetWeaponFireMode(WeaponID, bFullAuto);

	bIsServerFire = true;
	ActiveServerFireRate = FireRate;
	bServerFullAuto = bFullAuto;

	const float CurrentTime = World->GetTimeSeconds();
	const float TimeSinceLastShot = CurrentTime - LastServerFireTime;

	if (TimeSinceLastShot >= FireRate)
	{
		Server_ExecuteFire();
		if (!IsValid(this) || !IsActive() || !bIsServerFire || !IsValid(World)) return;
		LastServerFireTime = World->GetTimeSeconds();

		// 세미오토(샷건/저격 등): 한 발만 나가고 홀드해도 루프를 걸지 않음. 다음 발은 재클릭해야 함.
		if (!bFullAuto) return;

		World->GetTimerManager().SetTimer(
			ServerFireTimerHandle,
			this,
			&UAbility_Fire::HandleServerFireLoop,
			FireRate,
			true
		);
	}
	else
	{
		// 쿨다운이 아직 안 끝났으면, 버튼을 누르고 있는 동안만 짧은 간격으로 재검사해서 쿨다운이 끝나는
		// 순간 자동 발사한다(상용 게임과 동일). 손을 떼면(bIsServerFire=false) 그 즉시 재검사를 멈춘다.
		World->GetTimerManager().SetTimer(
			ServerFireRetryTimerHandle,
			this,
			&UAbility_Fire::HandleServerFireRetry,
			FireRetryIntervalSeconds,
			true);
	}
}

void UAbility_Fire::HandleServerFireLoop()
{
	UWorld* World = IsValid(OwnerCharacter) ? OwnerCharacter->GetWorld() : nullptr;
	if (!IsActive() || !bIsServerFire)
	{
		ClearServerFireTimers(World);
		bIsServerFire = false;
		return;
	}
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !World)
	{
		EndAbility(true);
		return;
	}

	Server_ExecuteFire();
	if (!IsValid(this) || !IsActive() || !bIsServerFire || !IsValid(World)) return;

	LastServerFireTime = World->GetTimeSeconds();
}

void UAbility_Fire::HandleServerFireRetry()
{
	UWorld* World = IsValid(OwnerCharacter) ? OwnerCharacter->GetWorld() : nullptr;
	if (!IsActive() || !bIsServerFire)
	{
		ClearServerFireTimers(World);
		bIsServerFire = false;
		return;
	}
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !World)
	{
		EndAbility(true);
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now - LastServerFireTime < ActiveServerFireRate) return;

	World->GetTimerManager().ClearTimer(ServerFireRetryTimerHandle);
	Server_ExecuteFire();
	if (!IsValid(this) || !IsActive() || !bIsServerFire || !IsValid(World)) return;

	LastServerFireTime = World->GetTimeSeconds();
	if (bServerFullAuto)
	{
		World->GetTimerManager().SetTimer(
			ServerFireTimerHandle,
			this,
			&UAbility_Fire::HandleServerFireLoop,
			ActiveServerFireRate,
			true);
	}
}

void UAbility_Fire::ClearServerFireTimers(UWorld* World)
{
	if (!World && IsValid(OwnerCharacter))
	{
		World = OwnerCharacter->GetWorld();
	}
	if (!World) return;

	World->GetTimerManager().ClearTimer(ServerFireTimerHandle);
	World->GetTimerManager().ClearTimer(ServerFireRetryTimerHandle);
}

void UAbility_Fire::EndAbility(bool bWasCancelled)
{
	UWorld* World = IsValid(OwnerCharacter)
		? OwnerCharacter->GetWorld()
		: (IsValid(AvatarActor) ? AvatarActor->GetWorld() : nullptr);
	ClearServerFireTimers(World);
	bIsServerFire = false;
	bServerFullAuto = false;
	CurrentBloomAngle = 0.f; // 트리거를 놓으면 다음 사격은 다시 최소 탄퍼짐부터 시작
	ShotsFiredInBurst = 0;

	// Standalone/리슨서버 호스트(HasAuthority()==true라서 LocalCancelWithOwner가 안 불리는 경우):
	// 여기서도 조준점 블룸 표시를 리셋해야 트리거를 놓았을 때 크로스헤어가 원래 크기로 돌아온다.
	if (IsValid(OwnerCharacter) && OwnerCharacter->IsLocallyControlled())
	{
		if (IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter))
		{
			if (AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon()))
			{
				if (EquippedGun->Setting)
				{
					EquippedGun->Setting->SetCurrentBloomDegrees(0.f);
					if (UTpsPlayerMainHUD* HUD = ResolveHUD(OwnerCharacter))
					{
						HUD->RefreshAimSpread();
					}
				}
			}
		}
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

	// 탄약이 없거나 장전 중이면 발사 자체(반동 포함)를 하지 않는다. Server_ExecuteFire의 체크와 동일.
	if (EquippedGun->Setting->IsReloading() || EquippedGun->Setting->GetCurrentAmmo() <= 0) return;

	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
	//EquippedGun->Setting->Fire(MuzzleLoc);

	// 조준점 UI용 블룸: 서버의 Server_ExecuteFire와 같은 규칙(BloomStartShotCount발까지는 그대로,
	// 그 이후 MaxBloomAngle까지 누적)을 로컬에서도 계산해서 크로스헤어를 그 진행도로 키운다.
	// 누적값은 WeaponComponent(EquippedGun->Setting)에 저장한다 — 캐릭터별로 실제 존재하는 곳이라
	// 여러 캐릭터/세션이 상태를 공유할 일이 없다.
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(Character->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(Character->GetWorld()->GetGameState());
	if (PS_Interface && GS_Interface)
	{
		const EWeaponType WeaponID = PS_Interface->GetWeaponID();

		float BloomPerShot = 0.f;
		float MaxBloomAngle = 0.f;
		int32 BloomStartShotCount = 0;
		GS_Interface->GetWeaponBloom(WeaponID, BloomPerShot, MaxBloomAngle, BloomStartShotCount);

		UWeaponComponent* WeaponComp = EquippedGun->Setting;
		const int32 NewShotsFired = WeaponComp->GetClientShotsFiredInBurst() + 1;
		WeaponComp->SetClientShotsFiredInBurst(NewShotsFired);

		float NewBloomDegrees = WeaponComp->GetCurrentBloomDegrees();
		if (NewShotsFired > BloomStartShotCount)
		{
			NewBloomDegrees = FMath::Min(NewBloomDegrees + BloomPerShot, MaxBloomAngle);
		}
		WeaponComp->SetCurrentBloomDegrees(NewBloomDegrees);

		UE_LOG(LogTemp, VeryVerbose, TEXT("[CL] Bloom: WeaponID=%d Shots=%d BloomPerShot=%.2f MaxBloomAngle=%.2f BloomStartShotCount=%d CurrentBloomAngle=%.2f"),
			static_cast<int32>(WeaponID), NewShotsFired, BloomPerShot, MaxBloomAngle, BloomStartShotCount, NewBloomDegrees);

		// Tick을 기다리지 않고 발사 즉시 조준점 위젯을 갱신한다.
		if (UTpsPlayerMainHUD* HUD = ResolveHUD(Character))
		{
			HUD->RefreshAimSpread();
		}
	}
}

UTpsPlayerMainHUD* UAbility_Fire::ResolveHUD(AActor* InOwner) const
{
	ACharacter* Character = Cast<ACharacter>(InOwner);
	if (!Character) return nullptr;

	APlayerController* PC = Cast<APlayerController>(Character->GetController());
	if (!PC) return nullptr;

	UUIHandler* UIHandler = PC->FindComponentByClass<UUIHandler>();
	if (!UIHandler) return nullptr;

	return Cast<UTpsPlayerMainHUD>(UIHandler->GetWidget());
}

void UAbility_Fire::Server_ExecuteFire()
{
	UWorld* World = IsValid(OwnerCharacter) ? OwnerCharacter->GetWorld() : nullptr;
	AMainGameMode* GameMode = World ? World->GetAuthGameMode<AMainGameMode>() : nullptr;
	if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !GameMode || !GameMode->IsBattleRoyalePhase())
	{
		EndAbility(true);
		return;
	}

	// 연사 도중 조준을 풀면 그 즉시(다음 발부터) 발사가 멈추도록 매 발마다 재확인한다.
	IAbilityCheckInterface* CheckInterface = Cast<IAbilityCheckInterface>(OwnerCharacter);
	if (!CheckInterface || !CheckInterface->IsCharacterAiming())
	{
		EndAbility(true);
		return;
	}

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface)
	{
		EndAbility(true);
		return;
	}

	UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
	if (!FollowCamera)
	{
		EndAbility(true);
		return;
	}

	AWeapon* EquippedGun = Cast<AWeapon>(Owner->GetEquippedWeapon());
	if (!IsValid(EquippedGun) || !IsValid(EquippedGun->Setting) || !IsValid(EquippedGun->m_pMesh))
	{
		EndAbility(true);
		return;
	}

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

	// 블룸: 트리거 홀드 시작 후 BloomStartShotCount발까지는 SpreadAngle 그대로(목표에 정확히 맞음),
	// 그 이후부터 한 발마다 판정 원뿔이 MaxBloomAngle까지 조금씩 더 벌어진다.
	float BloomPerShot = 0.f;
	float MaxBloomAngle = 0.f;
	int32 BloomStartShotCount = 0;
	GS_Interface->GetWeaponBloom(WeaponID, BloomPerShot, MaxBloomAngle, BloomStartShotCount);

	SpreadAngle += CurrentBloomAngle;
	++ShotsFiredInBurst;
	if (ShotsFiredInBurst > BloomStartShotCount)
	{
		CurrentBloomAngle = FMath::Min(CurrentBloomAngle + BloomPerShot, MaxBloomAngle);
	}

	// Standalone/리슨서버 호스트처럼 이 캐릭터를 로컬에서도 직접 조작 중이면(HasAuthority()==true라서
	// LocalActivateWithOwner/Client_ExecuteFire가 아예 안 불리는 경우), 여기서도 조준점 UI를 갱신해야 한다.
	if (IsValid(OwnerCharacter) && OwnerCharacter->IsLocallyControlled())
	{
		EquippedGun->Setting->SetCurrentBloomDegrees(CurrentBloomAngle);
		if (UTpsPlayerMainHUD* HUD = ResolveHUD(OwnerCharacter))
		{
			HUD->RefreshAimSpread();
		}
	}

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
		DS_DRAW_LINE(World, CamStart, DebugEndPoint, DebugColor, false, 3.f, 0, 1.5f);
		if (TraceRadius > 0.f)
		{
			// 명중 여부와 무관하게 시작/끝에 실제 스윕 반지름 그대로 그려서 판정 두께가 눈에 보이게 한다.
			DS_DRAW_SPHERE(World, CamStart, TraceRadius, 12, DebugColor, false, 3.f);
			DS_DRAW_SPHERE(World, DebugEndPoint, TraceRadius, 12, DebugColor, false, 3.f);
		}

		if (PelletIndex == 0)
		{
			RepresentativeTargetLoc = (bCamHit && CamHit.bBlockingHit) ? CamHit.ImpactPoint : CamEnd;
		}

		if (bCamHit && CamHit.GetActor())
		{
			AActor* HitActor = CamHit.GetActor();
			const bool bHitPlayer = HitActor->IsA<APawn>();

			UGameplayStatics::ApplyDamage(
				HitActor,
				Damage,
				OwnerCharacter->GetController(),
				OwnerCharacter,
				nullptr
			);

			// ApplyDamage가 사망/라운드 종료를 동기적으로 일으키면 사격 Ability와 무기가
			// 바로 정리될 수 있다. 그 뒤의 펠릿이나 피드백에서 해제된 상태를 다시 쓰지 않는다.
			if (!IsValid(this) || !IsActive() || !bIsServerFire || !IsValid(World) ||
				!IsValid(OwnerCharacter) || !IsValid(EquippedGun) || !IsValid(EquippedGun->Setting))
			{
				return;
			}

			// 플레이어(폰)를 맞췄을 때만 사수 화면에 히트마커를 띄운다.
			if (bHitPlayer)
			{
				if (AMainPlayerController* ShooterPC = Cast<AMainPlayerController>(OwnerCharacter->GetController()))
				{
					ShooterPC->Client_NotifyHitConfirmed();
				}
			}
		}
	}

	if (IsActive() && bIsServerFire && IsValid(EquippedGun) && IsValid(EquippedGun->Setting))
	{
		EquippedGun->Setting->Multicast_PlayFireFeedback(MuzzleLoc, RepresentativeTargetLoc);
	}
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
