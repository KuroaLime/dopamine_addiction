#include "Game/InGame/TPS/System/TPSPhaseStrategy.h"
#include "Manager.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/Card/CardGameService.h"
#include "Game/InGame/Interface/PhasePlayerStateInterface.h"
#include "Game/InGame/Interface/PhaseGameStateInterface.h"
#include "Game/InGame/Interface/PhaseCharacterInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"

void UTPSPhaseStrategy::OnPhaseStart()
{
	DS_LOG(TEXT("[DS] TPSPhase OnPhaseStart"));

	// 배틀로얄 진입 셋업. GameMode를 Context로 보고 공개 API로만 조작한다(friend/내부상태 접근 없음).
	// 상태/타이머/레벨 스트리밍 판단은 GameMode가 소유한다.
	if (AMainGameMode* GM = GetMainGameMode())
	{
		GM->EnsureBattleRoyaleStageLoaded();
		GM->BroadcastSwitchMode(EGamePhase::TPS);
		GM->RequestBattleRoyaleCardSpawnAfterStreamReady(TEXT("TPSPhaseStart"));
		// Keep pawns visible/collidable while the server commits the full card bundle
		// and every required client confirms that all card actors are replicated.
		GM->SetPlayerPawnGameplayState(true, false, true, TEXT("BattleRoyalePreparingCards"));
	}

	LoadStage();
}

void UTPSPhaseStrategy::OnPhaseEnd()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	DS_LOG(TEXT("[DS] TPSPhase OnPhaseEnd"));

	// 배틀로얄 종료 teardown: 기존 AMainGameMode::StartTransitionToCardPhase 본문에서 이전.
	// 전략이 페이즈 진입/종료를 모두 소유한다. (레벨 전환·좌석 이동은 전환 글루로 GameMode 유지)
	if (AMainGameMode* GM = GetMainGameMode())
	{
		GM->SetPlayerPawnGameplayState(false, false, true, TEXT("TransitionToCard"));
		GM->GetCardGameService()->ClearCardDrops();
		GM->ClearPlayerPawnMovementBases(TEXT("TransitionToCard"));
	}
}

void UTPSPhaseStrategy::OnTimerTick()
{
}

void UTPSPhaseStrategy::LoadStage()
{
	if (!GetWorld() || !GetWorld()->GetAuthGameMode()) return;

	DS_LOG(TEXT("[DS] TPSPhase LoadStage"));

	// 라운드마다 무기를 랜덤 배정한다(AR=1 ~ SNIPER=5). 모든 플레이어는 그 라운드 동안 같은 무기를 든다.
	const int32 RandomIndex = FMath::RandRange(static_cast<int32>(EWeaponType::AR), static_cast<int32>(EWeaponType::SNIPER));
	const EWeaponType RoundWeapon = static_cast<EWeaponType>(RandomIndex);

	if (IPhaseGameStateInterface* GS = Cast<IPhaseGameStateInterface>(GetWorld()->GetGameState()))
	{
		GS->SetRoundWeapon(RoundWeapon);
	}

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->PlayerState)
		{
			if (IPhasePlayerStateInterface* PS = Cast<IPhasePlayerStateInterface>(PC->PlayerState))
			{
				PS->SetWeaponID(RoundWeapon);
			}

			if (APawn* PlayerPawn = PC->GetPawn())
			{
				if (IPhaseCharacterInterface* IC = Cast<IPhaseCharacterInterface>(PlayerPawn))
				{
					IC->EquipWeapon(RoundWeapon);
					IC->SetSitting(false);
				}
			}
		}
	}
}

void UTPSPhaseStrategy::UnloadStage()
{
}

void UTPSPhaseStrategy::StartPhaseTimer()
{
}

void UTPSPhaseStrategy::OnPhaseTimeout()
{
}
