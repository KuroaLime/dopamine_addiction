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
		GM->SetPlayerPawnGameplayEnabled(true, TEXT("BattleRoyale"));
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

	int32 RandomIndex = FMath::RandRange(1, 5);
	EWeaponType RoundWeapon = static_cast<EWeaponType>(RandomIndex);

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
