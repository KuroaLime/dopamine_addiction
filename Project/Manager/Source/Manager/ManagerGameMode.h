// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SeotdaTypes.h"
#include "ManagerGameMode.generated.h"

/**
 * Simple GameMode for a third person game
 */

class AManagerPlayerController;
class AManagerGameState;
class APlayerController;
class AController;

UCLASS(abstract)
class AManagerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AManagerGameMode();

	/** 섯다 게임을 시작하는 함수 */
	UFUNCTION(BlueprintCallable, Category = "Seotda|GameControl")
	void StartSeotdaGame();

	void AddPlayer();
	void AddAI();

public:
	virtual void BeginPlay() override;

	// 클라이언트가 데디 서버에 접속했을 때 서버에서 호출됨
	virtual void PostLogin(APlayerController* NewPlayer) override;

	// 클라이언트가 데디 서버에서 나갔을 때 서버에서 호출됨
	virtual void Logout(AController* Exiting) override;

	// 타이머 함수
	void RoundTimerTick();

	/** 외부로부터 액션을 전달받는 바인드 함수 */
	void OnPlayerAction(AActor* Executor, FName ActionName);

private:
	FTimerHandle RoundTimerHandle;

	// 현재 접속자 수를 보고 게임 시작 가능 여부를 판단
	void TryStartGameIfReady();

	// 현재 접속한 실제 플레이어 컨트롤러 수 계산
	int32 CountConnectedHumanPlayers() const;

protected:
	/** --- Dedicated Server Settings --- */

	// 몇 명이 접속해야 게임을 시작할지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dedicated Server")
	int32 RequiredPlayerCount = 4;

	// 테스트용 AI 채우기 여부. 기본값 false면 사람만으로 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dedicated Server")
	bool bFillEmptySlotsWithAI = false;

	// 게임이 이미 시작되었는지
	UPROPERTY(BlueprintReadOnly, Category = "Dedicated Server")
	bool bGameStarted = false;

	// 현재 서버 게임 단계
	UPROPERTY(BlueprintReadOnly, Category = "Game Flow")
	EMatchPhase CurrentPhase = EMatchPhase::WaitingForPlayers;

	// 배틀로얄 라운드 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	int32 BattleRoyaleDuration = 60;

	// 섯다에 필요한 카드 수. 현재 클라 입력/선택 구조가 3장 기준이므로 3으로 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow")
	int32 RequiredSeotdaCardCount = 3;

protected:
	/** --- 게임 진행 및 플레이어 데이터 --- */

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Data")
	TArray<FSeotdaPlayerInfo> Players;

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Internal")
	FSeotdaDealer CardDealer;

	UPROPERTY()
	AManagerGameState* MGS_Ptr;

protected:
	/** --- 게임 흐름 관리 --- */

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Flow")
	int32 CurrentTurnIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Seotda|Flow")
	int32 FirstBetterIndex = -1;

	/** 전체 게임 단계 전환 관련 */
	void StartBattleRoyalePhase();
	void StartSeotdaPhase();

	/** 섯다 단계 준비 */
	bool ValidateSeotdaHands() const;
	void PrepareSeotdaCardSelection();
	/** 배틀로얄에서 획득한 카드 아이템을 섯다 Hand로 반영 */
	void LoadCollectedCardsForSeotda();

	/** 섯다 카드 선택 완료 및 베팅 단계 전환 */
	bool AreAllSeotdaSelectionsComplete() const;
	void StartSeotdaBettingPhase();

	/** 섯다 라운드 및 턴 전환 관련 */
	void StartNextRound();
	void StartCurrentTurn();
	void NextTurn();
	int32 GetNextValidPlayerIndex(int32 StartingIndex);

	/** 카드 분배 및 승자 처리 */
	void DealCards();
	void DetermineWinner();
	TArray<int32> GetWinnerIndices();
	void DistributePot(const TArray<int32>& WinnerIndices);
	void ScheduleNextRound(float Delay);
	void ScheduleNextBattleRoyale(float Delay);

protected:
	/** --- 베팅 및 카드 선택 처리 --- */

	/** 베팅 프로세스 처리 */
	void ProcessBetting(int32 PlayerIndex, EBettingAction Action);
	bool CheckBettingRoundEnd();

	/** 카드 선택 프로세스 처리 */
	void ProcessCardSelection(int32 PlayerIndex, int32 Index1, int32 Index2);
	bool HandleCardSelection(FName ActionName, int32 PlayerIndex);

protected:
	/** --- 통신 및 UI 갱신 --- */

	/** 플레이어 컨트롤러 식별 및 RPC 전송 */
	AManagerPlayerController* GetPlayerControllerFromIndex(int32 Index);
	void SendHandDataToClient(int32 PlayerIndex);
	void NotifyClientStateReset();
	int32 GetPlayerIndexFromController(AManagerPlayerController* PC) const;

	/** 데이터 초기화 유틸리티 */
	void TableDataReset();
	void PlayerDataReset();

	/** 화면 갱신 관련 */
	void UpdateGameStatusHUD();
	void DisplayFinalResults(const TArray<int32> WinnerIndices);

	/** GameState 접근 함수 */
	FORCEINLINE AManagerGameState* GetGS() const;

protected:
	/** --- AI 의사결정 관련 --- */

	EBettingAction DetermineAIDecision(int32 PlayerIndex);
	void ExecuteAIHandSelection(int32 PlayerIndex);
	void RunAILogicDelayed(int32 PlayerIndex, float Delay);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Manager")
	class USpawnManagerComponent* SpawnManager;

	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
};