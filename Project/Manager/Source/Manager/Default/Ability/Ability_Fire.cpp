#include "Default/Ability/Ability_Fire.h"
#include "Default/Ability/Interface/AbilityOwnerInterface.h"
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

	IAbilityOwnerInterface* Owner = Cast<IAbilityOwnerInterface>(OwnerCharacter);
	IPhasePlayerStateInterface* PS_Interface = Cast<IPhasePlayerStateInterface>(OwnerCharacter->GetPlayerState());
	IPhaseGameStateInterface* GS_Interface = Cast<IPhaseGameStateInterface>(OwnerCharacter->GetWorld()->GetGameState());

	if (!Owner || !PS_Interface || !GS_Interface) return;

	UCameraComponent* FollowCamera = Owner->GetFollowCameraComponent();
	if (!FollowCamera) return;


	int32 BaseRange = GS_Interface->GetWeaponBaseData(PS_Interface->GetWeaponID(), EWeaponBaseStatType::Range);
	int32 LvRange = PS_Interface->GetWeaponStatLV(EWeaponStatType::Range);
	float Range = CalculateRange(BaseRange, LvRange);


	FVector CamStart = FollowCamera->GetComponentLocation();
	FRotator CamRot = FollowCamera->GetComponentRotation();
	FVector CamEnd = CamStart + (CamRot.Vector() * Range);

	FHitResult CamHit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(OwnerCharacter);

	UWorld* World = OwnerCharacter->GetWorld();
	bool bCamHit = World->LineTraceSingleByChannel(CamHit, CamStart, CamEnd, ECC_Pawn, Params);


	// -------------------------------------------------------
	// ����� ǥ��
	//   �ʷ�: ��Ʈ   (���� CamStart �� ImpactPoint + �ڽ�)
	//   ����: �̽�   (���� CamStart �� CamEnd)
	// -------------------------------------------------------
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


	FVector MuzzleLoc = EquippedGun->m_pMesh->GetSocketLocation(TEXT("Muzzle"));
	// 총알 트레이서가 향할 목표: 명중하면 충돌점, 빗나가면 카메라 최대 사거리 끝.
	FVector TargetLoc = (bCamHit && CamHit.bBlockingHit) ? CamHit.ImpactPoint : CamEnd;
	EquippedGun->Setting->Multicast_PlayFireFeedback(MuzzleLoc, TargetLoc);

	int32 BaseDamage = GS_Interface->GetWeaponBaseData(
		PS_Interface->GetWeaponID(), EWeaponBaseStatType::Damage);
	int32 LvDamage = PS_Interface->GetWeaponStatLV(EWeaponStatType::Damage);
	float Damage = CalculateDamage(BaseDamage, LvDamage);

	if (!bCamHit || !CamHit.GetActor()) return;

	UGameplayStatics::ApplyDamage(
		CamHit.GetActor(),
		Damage,
		OwnerCharacter ? OwnerCharacter->GetController() : nullptr,
		OwnerCharacter,
		nullptr
	);
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