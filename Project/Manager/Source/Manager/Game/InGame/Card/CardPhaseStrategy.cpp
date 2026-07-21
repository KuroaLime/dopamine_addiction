#include "Game/InGame/Card/CardPhaseStrategy.h"
#include "Manager.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/Card/CardGameService.h"
#include "Game/InGame/Interface/PhaseCharacterInterface.h"
#include "GameFramework/PlayerController.h"

void UCardPhaseStrategy::OnPhaseStart()
{
	DS_LOG(TEXT("[DS] CardPhase OnPhaseStart"));

	// 카드게임 진입 셋업: 기존 AMainGameMode::StartCardGamePhase 본문에서 이전.
	// 페이즈 상태/타이머 관리는 GameMode(Context)에 그대로 남아 있다.
	if (AMainGameMode* GM = GetMainGameMode())
	{
		GM->ClearPlayerPawnMovementBases(TEXT("CardGame"));
		GM->RequestMovePlayersToCardIslandSeats(TEXT("CardGame"));
		GM->SetPlayerPawnGameplayState(true, false, true, TEXT("CardGame"));
		GM->GetCardGameService()->EnsureThreeCardsForCardGame();
		GM->GetCardGameService()->ResetSeotdaRoundStates();
		GM->BroadcastSwitchMode(EGamePhase::Card);

		if (UWorld* World = GetWorld())
		{
			for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
			{
				APlayerController* PC = It->Get();
				if (APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr)
				{
					if (IPhaseCharacterInterface* IC = Cast<IPhaseCharacterInterface>(PlayerPawn))
					{
						IC->SetSitting(true);
					}
				}
			}
		}
	}

	LoadStage();
}

void UCardPhaseStrategy::OnPhaseEnd()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	DS_LOG(TEXT("[DS] CardPhase OnPhaseEnd"));

	// 카드게임 종료 마무리: 라운드 결과가 아직 안 났으면 정산(기존 StartResultPhase 폴백에서 이전).
	if (AMainGameMode* GM = GetMainGameMode())
	{
		if (UCardGameService* Cards = GM->GetCardGameService())
		{
			if (!Cards->IsRoundResolved() && Cards->GetRoundStateCount() > 0)
			{
				Cards->ResolveSeotdaRoundResult(TEXT("ResultPhaseFallback"));
			}
		}
	}
}

void UCardPhaseStrategy::OnTimerTick()
{
}

void UCardPhaseStrategy::LoadStage()
{
	DS_LOG(TEXT("[DS] CardPhase LoadStage"));
}

void UCardPhaseStrategy::UnloadStage()
{
}

void UCardPhaseStrategy::StartPhaseTimer()
{
}

void UCardPhaseStrategy::OnPhaseTimeout()
{
}
