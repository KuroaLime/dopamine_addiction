#include "Game/InGame/Card/CardPhaseStrategy.h"

void UCardPhaseStrategy::OnPhaseStart()
{
    UE_LOG(LogTemp, Warning, TEXT("[DS] CardPhase OnPhaseStart"));
    LoadStage();
}

void UCardPhaseStrategy::OnPhaseEnd()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RoundTimerHandle);
    }

    UE_LOG(LogTemp, Warning, TEXT("[DS] CardPhase OnPhaseEnd"));
}

void UCardPhaseStrategy::OnTimerTick()
{
}

void UCardPhaseStrategy::OnPlayerAction(AActor* Executor, FName ActionName)
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
