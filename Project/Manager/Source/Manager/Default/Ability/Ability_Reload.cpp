// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Ability/Ability_Reload.h"
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
#include "Game/InGame/MainPlayerState.h"

UAbility_Reload::UAbility_Reload()
{
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Action.Reload")));
}
void UAbility_Reload::ActivateAbility()
{

	AMainCharacter* MainChar = Cast<AMainCharacter>(OwnerCharacter);
	if (MainChar)
	{
		AWeapon* EquippedWeapon = MainChar->GetEquippedGun();
		if (EquippedWeapon)
		{
			UWeaponComponent* WeaponComp = EquippedWeapon->Setting;
			if (WeaponComp)
			{
				int32 CurrentAmmo = WeaponComp->GetCurrentAmmo();
				int32 MaxMagazineCapacity = WeaponComp->GetMaxMagazineCapacity();
				if (CurrentAmmo >= MaxMagazineCapacity)
				{
					EndAbility(true);
					return;
				}
				if (OwnerCharacter->HasAuthority())
				{

					if (CurrentAmmo < MaxMagazineCapacity)
					{
						if (AMainPlayerState* PS = OwnerCharacter->GetPlayerState<AMainPlayerState>())
						{
							int32 AmmoNeeded = MaxMagazineCapacity - CurrentAmmo;
							EWeaponType WeaponType = PS->GetWeaponID();
							int32 AvailableAmmo = PS->GetCarriedAmmoByWeaponType(WeaponType);

							if (AvailableAmmo > 0)
							{
								int32 AmmoToLoad = FMath::Min(AmmoNeeded, AvailableAmmo);
								CurrentAmmo += AmmoToLoad;
								WeaponComp->SetCurrentAmmo(CurrentAmmo);
								PS->AddCarriedAmmoByWeaponType(WeaponType, -AmmoToLoad);

								WeaponComp->Multicast_PlayReloadFeedback();

								// 장전 잠금: 이 시간 동안 사격 불가. 무기 데이터의 ReloadTime(초) 사용, 없으면 기본 1.5초.
								float ReloadLockTime = 1.5f;
								if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
								{
									const int32 BaseReload = GS->GetWeaponBaseData(WeaponType, EWeaponBaseStatType::ReloadTime);
									if (BaseReload > 0)
									{
										ReloadLockTime = static_cast<float>(BaseReload);
									}
								}
								WeaponComp->StartReloadLock(ReloadLockTime);
							}
						}
					}
				}
			}
		}
	}
	EndAbility(true);
}

void UAbility_Reload::EndAbility(bool bWasCancelled)
{
	
	Super::EndAbility(bWasCancelled);
}