// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SeotdaTypes.h"
#include "ManagerGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */

class AManagerPlayerController;
class AManagerGameState;

UCLASS(abstract)
class AManagerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AManagerGameMode();

	/** ������ ���ʷ� �����ϴ� ������ */
	UFUNCTION(BlueprintCallable, Category = "Seotda|GameControl")
	void StartSeotdaGame();
	void AddPlayer();
	void AddAI();

public:
	virtual void BeginPlay() override;
	
	//Ÿ�̸� �Լ�
	void RoundTimerTick();

	/** �ܺηκ��� �׼��� ���޹޴� ���ε� �Լ� */
	void OnPlayerAction(AActor* Executor, FName ActionName);
private:
	FTimerHandle RoundTimerHandle;

protected:
	/** --- ���� ���� �� ������ ��ü --- */

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Data")
	TArray<FSeotdaPlayerInfo> Players;

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Internal")
	FSeotdaDealer CardDealer;

	UPROPERTY()
	AManagerGameState* MGS_Ptr;

protected:
	/** --- ���� �帧 ���� --- */

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Flow")
	int32 CurrentTurnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Flow")
	int32 FirstBetterIndex = -1;

	/** ���� �� �� ��ȯ ���� */
	void StartNextRound();
	void StartCurrentTurn();
	void NextTurn();
	int32 GetNextValidPlayerIndex(int32 StartingIndex);

	/** ī�� ��� �� ���� ó�� */
	void DealCards();
	void DetermineWinner();
	TArray<int32> GetWinnerIndices();
	void DistributePot(const TArray<int32>& WinnerIndices);
	void ScheduleNextRound(float Delay);

protected:
	/** --- 3. �ٽ� ���� ���� --- */

	/** ���� ���μ��� ó�� */
	void ProcessBetting(int32 PlayerIndex, EBettingAction Action);
	bool CheckBettingRoundEnd();

	/** ī�� ���� ���μ��� ó�� */
	void ProcessCardSelection(int32 PlayerIndex, int32 Index1, int32 Index2);
	bool HandleCardSelection(FName ActionName, int32 PlayerIndex);

protected:
	/** --- 4. ��� �� UI ���� --- */

	/** �÷��̾� ��Ʈ�ѷ� �ĺ� �� RPC ���� */
	AManagerPlayerController* GetPlayerControllerFromIndex(int32 Index);
	void SendHandDataToClient(int32 PlayerIndex);
	void NotifyClientStateReset();
	int32 GetPlayerIndexFromController(AManagerPlayerController* PC) const;

	/** ������ �ʱ�ȭ ��ƿ��Ƽ */
	void TableDataReset();
	void PlayerDataReset();

	/** �ð�ȭ ���� */
	void UpdateGameStatusHUD();
	void DisplayFinalResults(const TArray<int32> WinnerIndices);

	/** GameState ���� �Լ� */
	FORCEINLINE AManagerGameState* GetGS() const
	{
		if (MGS_Ptr) return MGS_Ptr;

		AManagerGameMode* MutableThis = const_cast<AManagerGameMode*>(this);
		MutableThis->MGS_Ptr = Cast<AManagerGameState>(GetWorld()->GetGameState());

		return MGS_Ptr;
	}

protected:
	/** --- 5. AI �ǻ���� ���� --- */
	EBettingAction DetermineAIDecision(int32 PlayerIndex);
	void ExecuteAIHandSelection(int32 PlayerIndex);
	void RunAILogicDelayed(int32 PlayerIndex, float Delay);
public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Manager")
	class USpawnManagerComponent* SpawnManager;


	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};



