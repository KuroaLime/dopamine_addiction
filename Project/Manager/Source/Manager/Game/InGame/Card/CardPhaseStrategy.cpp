#include "Game/InGame/Card/CardPhaseStrategy.h"
#include "Game/InGame/MainGameMode.h"
#include "Game/InGame/Card/CardGameService.h"

void UCardPhaseStrategy::OnPhaseStart()
{
	UE_LOG(LogTemp, Warning, TEXT("[DS] CardPhase OnPhaseStart"));

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
	}

	LoadStage();
}

void UCardPhaseStrategy::OnPhaseEnd()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RoundTimerHandle);
	}

	UE_LOG(LogTemp, Warning, TEXT("[DS] CardPhase OnPhaseEnd"));

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
	UE_LOG(LogTemp, Warning, TEXT("[DS] CardPhase LoadStage"));
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
