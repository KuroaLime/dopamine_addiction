// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/Protocol_Client/Protocol_D.h"
#include "Game/Lobby/LobbyController.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "UManagerGameInstance.generated.h"

/**
 * 
 */

USTRUCT()
struct FABCharacterData : public FTableRowBase
{
	GENERATED_BODY()

public:
	FABCharacterData() : Level(1), MaxHP(100.0f), DropPercent(5), DropExp(10), NextExp(30) {}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	int32 Level;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	float MaxHP;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	int32 DropPercent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	int32 DropExp;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Data")
	int32 NextExp;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomMemberListUpdated, const TArray<FRoomMemberInfoView>&, MemberList);

UCLASS()
class MANAGER_API UUManagerGameInstance : public UGameInstance
{
	GENERATED_BODY()
protected:
	virtual void Init() override;
	virtual void OnStart() override;

	virtual void Shutdown() override;

	FString PlayerID;
	FString PlayerPW;

	UPROPERTY()
	class UDataTable* ABCharacterTable;
public:
	UUManagerGameInstance();
private:
	SOCKET m_sock = INVALID_SOCKET;
	std::atomic<bool> m_running{ false };
	std::thread m_recvThread;
	std::mutex m_sendLock;
	std::vector<char> m_recvBuf;
	uint32_t m_sessionId = 0;

	bool Connect(const FString& ip, uint16_t port);
	void Disconnect();
	void RecvLoop();
	void HandlePacket(PacketType type, const char* payload, uint16_t payloadLen);

	bool SendAuth(PacketType type, const FString& id, const FString& pw);
	bool SendPacket(PacketType type, const void* payload, uint16_t payloadLen);

	void SwitchLobbyState(ELobbyState NewState);

	bool bHasEnteredRoom = false;
public:
	bool SendRegister(const FString& id, const FString& pw);
	bool SendLogin(const FString& id, const FString& pw);
	bool SendRoomListReq();
	bool SendCreateRoom(const FString& roomName);
	bool SendJoinRoom(uint32_t roomId);
	bool SendLeaveRoom();
	bool SendReady(bool ready);
	bool SendRoomStart();
	uint32_t GetSessionId() const { return m_sessionId; }

	UPROPERTY(BlueprintAssignable)
	FOnRoomMemberListUpdated OnRoomMemberListUpdated;

	FABCharacterData* GetABCharacterData(int32 Level);



};
