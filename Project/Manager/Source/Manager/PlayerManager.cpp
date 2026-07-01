// Fill out your copyright notice in the Description page of Project Settings.


#include "PlayerManager.h"

void UPlayerManager::Initialize(FSubsystemCollectionBase& Collection) {
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("Create Manager"));
}

void UPlayerManager::Deinitialize()
{
	ManagedPlayers.Empty();
	PendingAdd.Empty();
	PendingRemove.Empty();

	FPlayerCommand Dummy;
	while (CommandInbox.Dequeue(Dummy)) {}

	UE_LOG(LogTemp, Warning, TEXT("[PlayerManager] Deinitialize"));
	Super::Deinitialize();
}

void UPlayerManager::RequestRegister(ACharacter* NewPlayer)
{
	if (!NewPlayer) return;
	UE_LOG(LogTemp, Warning, TEXT("[PlayerManager] ReQuestRegister"));
	PendingAdd.AddUnique(NewPlayer);
}

void UPlayerManager::RequestUnregister(ACharacter* OldPlayer)
{
	if (!OldPlayer) return;
	UE_LOG(LogTemp, Warning, TEXT("[PlayerManager] UnReQuestRegister"));
	PendingRemove.AddUnique(OldPlayer);
}

TArray<ACharacter*> UPlayerManager::GetAllPlayers() const
{
	TArray<ACharacter*> Result;
	Result.Reserve(ManagedPlayers.Num());

	for (const TWeakObjectPtr<ACharacter>& W : ManagedPlayers)
	{
		if (ACharacter* P = W.Get())
		{
			Result.Add(P);
		}
	}
	return Result;
}

void UPlayerManager::AddCommand(AActor* InExecutor, FName InAction)
{
	if (!IsValid(InExecutor) || InAction.IsNone()) return;
	CommandInbox.Enqueue(FPlayerCommand(InExecutor, InAction));
}

void UPlayerManager::Tick(float DeltaTime)
{
	ApplyPending();

	FPlayerCommand Cmd;
	while (CommandInbox.Dequeue(Cmd))
	{
		if (AActor* Exec = Cmd.Executor.Get())
		{
			OnPlayerActionEvent.Broadcast(Exec, Cmd.ActionName);
		}
	}

	CleanupInvalid();
}

void UPlayerManager::ApplyPending()
{
	for (const TWeakObjectPtr<ACharacter>& W : PendingRemove)
	{
		ACharacter* P = W.Get();
		if (!P) continue;

		ManagedPlayers.RemoveAll([P](const TWeakObjectPtr<ACharacter>& Elem)
			{
				return Elem.Get() == P;
			});

		OnPlayerUnregistered.Broadcast(P);
	}
	PendingRemove.Reset();

	for (const TWeakObjectPtr<ACharacter>& W : PendingAdd)
	{
		ACharacter* P = W.Get();
		if (!P) continue;

		if (!ManagedPlayers.Contains(P))
		{
			ManagedPlayers.Add(P);
			OnPlayerRegistered.Broadcast(P);
		}
	}
	PendingAdd.Reset();
}

void UPlayerManager::CleanupInvalid()
{
	ManagedPlayers.RemoveAll([](const TWeakObjectPtr<ACharacter>& Elem)
		{
			return !Elem.IsValid();
		});
}

TStatId UPlayerManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPlayerManager, STATGROUP_Tickables);
}