// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/System/UManagerGameInstance.h"
#include "Manager.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"

#define UE_ASYNC_GUARD if (!IsValid(this) || !GetWorld()) return;

namespace
{
    FString MakePlayerNameUrlOption(const FString& RawName)
    {
        FString TrimmedName = RawName;
        TrimmedName.TrimStartAndEndInline();

        FString SafeName;
        SafeName.Reserve(TrimmedName.Len());

        for (int32 Index = 0; Index < TrimmedName.Len(); ++Index)
        {
            const TCHAR Ch = TrimmedName[Index];
            SafeName.AppendChar((FChar::IsAlnum(Ch) || Ch == TEXT('_') || Ch == TEXT('-')) ? Ch : TEXT('_'));
        }

        if (SafeName.IsEmpty())
        {
            SafeName = TEXT("Player");
        }

        return SafeName.Left(32);
    }

    FString ManagerGameInstanceGetTrimmedEnvValue(const TCHAR* EnvName)
    {
        FString Value;
#ifdef GetEnvironmentVariable
#pragma push_macro("GetEnvironmentVariable")
#undef GetEnvironmentVariable
#define MANAGER_GI_RESTORE_GETENV_MACRO 1
#endif
        Value = FPlatformMisc::GetEnvironmentVariable(EnvName);
#ifdef MANAGER_GI_RESTORE_GETENV_MACRO
#pragma pop_macro("GetEnvironmentVariable")
#undef MANAGER_GI_RESTORE_GETENV_MACRO
#endif
        Value.TrimStartAndEndInline();
        return Value;
    }

    FString ManagerGameInstanceResolveIocpHost()
    {
        const FString Host = ManagerGameInstanceGetTrimmedEnvValue(TEXT("MANAGER_IOCP_HOST"));
        return Host.IsEmpty() ? FString(TEXT("127.0.0.1")) : Host;
    }

    uint16_t ManagerGameInstanceResolveIocpPort()
    {
        const FString PortText = ManagerGameInstanceGetTrimmedEnvValue(TEXT("MANAGER_IOCP_PORT"));
        if (PortText.IsNumeric())
        {
            const int32 ParsedPort = FCString::Atoi(*PortText);
            if (ParsedPort > 0 && ParsedPort <= 65535)
            {
                return static_cast<uint16_t>(ParsedPort);
            }
        }

        return 9000;
    }
}

UUManagerGameInstance::UUManagerGameInstance() {
    FString CharacterDataPath = TEXT("/Game/GameData/ABCharacterData.ABCharacterData");
    static ConstructorHelpers::FObjectFinder<UDataTable> DT_ABCHARACTER(*CharacterDataPath);
    
    ABCharacterTable = DT_ABCHARACTER.Object;
}




void UUManagerGameInstance::Init() {
	

    FString ip = ManagerGameInstanceResolveIocpHost();
    const uint16_t port = ManagerGameInstanceResolveIocpPort();

    bool bConnected = Connect(ip, port);
    if (!bConnected && !ip.Equals(TEXT("127.0.0.1"), ESearchCase::IgnoreCase))
    {
        if (GEngine)
        {
            DS_SCREEN(-1, 5.f, FColor::Yellow,
                FString::Printf(TEXT("Login server connect failed: %s:%u, fallback to 127.0.0.1:%u"),
                    *ip,
                    static_cast<uint32>(port),
                    static_cast<uint32>(port)));
        }

        ip = TEXT("127.0.0.1");
        bConnected = Connect(ip, port);
    }

    if (bConnected)
    {
        if (GEngine)
        {
            DS_SCREEN(-1, 5.f, FColor::Green,
                FString::Printf(TEXT("Connected to login server %s:%u"),
                    *ip,
                    static_cast<uint32>(port)));
        }
    }
    else
    {
        if (GEngine)
        {
            DS_SCREEN(-1, 5.f, FColor::Red, TEXT("Failed to connect to login server"));
        }
    }
    Super::Init();
}

void UUManagerGameInstance::OnStart() {
	Super::OnStart();
}

void UUManagerGameInstance::Shutdown() {
    Disconnect();
	Super::Shutdown();
}


bool UUManagerGameInstance::Connect(const FString& ip, uint16_t port)
{
    if (m_running.load()) return false;

    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET)
    {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, TCHAR_TO_UTF8(*ip), &addr.sin_addr) != 1)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    if (connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
        return false;
    }

    m_running.store(true);
    m_recvThread = std::thread(&UUManagerGameInstance::RecvLoop, this);

    return true;
}

void UUManagerGameInstance::Disconnect()
{
    bool wasRunning = m_running.exchange(false);

    if (m_sock != INVALID_SOCKET)
    {
        shutdown(m_sock, SD_BOTH);
        closesocket(m_sock);
        m_sock = INVALID_SOCKET;
    }

    if (m_recvThread.joinable())
    {
        m_recvThread.join();
    }

    if (wasRunning)
    {
        //std::cout << "[INFO] Disconnected\n";
    }
}

void UUManagerGameInstance::RecvLoop()
{
    char temp[4096];

    while (m_running.load())
    {
        int ret = recv(m_sock, temp, sizeof(temp), 0);
        if (ret == 0)
        {
            break;
        }
        if (ret == SOCKET_ERROR)
        {
            if (m_running.load())
            {
                //std::cout << "[ERR] recv() failed: " << WSAGetLastError() << "\n";
            }
            break;
        }

        m_recvBuf.insert(m_recvBuf.end(), temp, temp + ret);

        while (m_recvBuf.size() >= sizeof(PacketHeader))
        {
            PacketHeader hdr{};
            std::memcpy(&hdr, m_recvBuf.data(), sizeof(hdr));

            uint16_t totalSize = ntohs(hdr.size);
            uint16_t type = ntohs(hdr.type);

            if (totalSize < sizeof(PacketHeader) || totalSize > MAX_PACKET_SIZE)
            {
                //std::cout << "[ERR] invalid packet size from server: " << totalSize << "\n";
                m_running.store(false);
                break;
            }

            if (m_recvBuf.size() < totalSize)
            {
                break;
            }

            const char* payload = m_recvBuf.data() + sizeof(PacketHeader);
            uint16_t payloadLen = totalSize - static_cast<uint16_t>(sizeof(PacketHeader));

            HandlePacket(static_cast<PacketType>(type), payload, payloadLen);

            m_recvBuf.erase(m_recvBuf.begin(), m_recvBuf.begin() + totalSize);
        }
    }

    m_running.store(false);
}


void UUManagerGameInstance::HandlePacket(PacketType type, const char* payload, uint16_t payloadLen)
{
    size_t off = 0;

    switch (type)
    {
    case PacketType::S2C_WELCOME:
    {
        uint32_t sid = 0;
        if (ReadU32(payload, payloadLen, off, sid))
        {
            m_sessionId = sid;
        }
        break;
    }

    case PacketType::S2C_LOGIN_RES:
    {
        uint8_t r = 0;
        if (ReadU8(payload, payloadLen, off, r))
        {
            auto result = static_cast<LoginResult>(r);

            switch (result)
            {
            case LoginResult::OK:
            {
                AsyncTask(ENamedThreads::GameThread, [this]()
                    {
                        UE_ASYNC_GUARD
                        UGameplayStatics::OpenLevel(GetWorld(), FName("Lobby_Stage"));
                    });
                break;
            }
            case LoginResult::OK_RECONNECT:      break;
            case LoginResult::ID_NOT_FOUND:      SendRegister(PlayerID, PlayerPW); break;
            case LoginResult::WRONG_PASSWORD:    break;
            case LoginResult::ALREADY_LOGGED_IN: break;
            case LoginResult::ID_ALREADY_EXISTS: break;
            case LoginResult::INVALID_FORMAT:    break;
            default:                             break;
            }
        }
        break;
    }

    case PacketType::S2C_REGISTER_RES:
    {
        uint8_t r = 0;
        if (ReadU8(payload, payloadLen, off, r))
        {
            auto result = static_cast<LoginResult>(r);
            SendLogin(PlayerID, PlayerPW);
        }
        break;
    }

    case PacketType::S2C_ROOM_LIST_RES:
    {
        uint16_t count = 0;
        if (!ReadU16(payload, payloadLen, off, count))
        {
            break;
        }

        TArray<RoomInfoView> rooms;
        for (uint16_t i = 0; i < count; ++i)
        {
            RoomViewParsed room{};
            if (ParseRoomView(payload, payloadLen, off, room))
            {
                rooms.Add(ToRoomInfoView(room));
            }
            else
            {
                break;
            }
        }

        AsyncTask(ENamedThreads::GameThread, [this, rooms]()
            {
                UE_ASYNC_GUARD

                ALobbyController* PC = Cast<ALobbyController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
                if (PC)
                {
                    PC->Client_GetRoomList(rooms);
                }
            });

        break;
    }

    case PacketType::S2C_ROOM_CREATE_RES:
    {
        uint8_t r = 0;
        if (!ReadU8(payload, payloadLen, off, r))
        {
            break;
        }

        auto result = static_cast<RoomResult>(r);

        if (result == RoomResult::OK)
        {
            RoomViewParsed CreatedRoom{};
            RoomInfoView CreatedInfo{};

            if (ParseRoomView(payload, payloadLen, off, CreatedRoom))
            {
                CreatedInfo = ToRoomInfoView(CreatedRoom);
            }

            AsyncTask(ENamedThreads::GameThread, [this, CreatedInfo]()
                {
                    UE_ASYNC_GUARD
                    SwitchLobbyState(ELobbyState::InRoom);
                });
        }

        break;
	}

    case PacketType::S2C_ROOM_JOIN_RES:
    {
        uint8_t r = 0;
        if (!ReadU8(payload, payloadLen, off, r))
        {
            break;
        }

        auto result = static_cast<RoomResult>(r);

        if (result == RoomResult::OK)
        {
			bHasEnteredRoom = false;
            RoomViewParsed room{};
            if (ParseRoomView(payload, payloadLen, off, room))
            {
                RoomInfoView info = ToRoomInfoView(room);

                AsyncTask(ENamedThreads::GameThread, [this, info]()
                    {
                        UE_ASYNC_GUARD
                        SwitchLobbyState(ELobbyState::InRoom);

                        if (GEngine)
                        {
                            DS_SCREEN(-1, 8.f, FColor::Cyan, TEXT("================ [IOCP] Successfully Joined the Room! ================"));
                            DS_SCREEN(-1, 8.f, FColor::Green, FString::Printf(TEXT("[Room Info] Title: %s (ID: %d)"), *info.title, info.roomId));
                            DS_SCREEN(-1, 8.f, FColor::Orange, FString::Printf(TEXT("[Room Players] %d / %d (Host UID: %d)"), info.curPlayers, info.maxPlayers, info.hostId));
                            DS_SCREEN(-1, 8.f, FColor::Cyan, TEXT("====================================================================="));
                        }
                    });
            }
        }
        break;
	}

    case PacketType::S2C_ROOM_MEMBER_LIST:
    {
        uint32_t roomId = 0;
        TArray<FRoomMemberInfoView> members;

        if (ParseRoomMemberList(payload, payloadLen, off, roomId, members))
        {
            CachedRoomId = roomId;
            CachedRoomMembers = members;

            AsyncTask(ENamedThreads::GameThread, [this, members]() {
                UE_ASYNC_GUARD

                OnRoomMemberListUpdated.Broadcast(members);

                if (!bHasEnteredRoom)
                {
                    SwitchLobbyState(ELobbyState::InRoom);
                    bHasEnteredRoom = true;
                }
                });
        }
        break;
    }

    case PacketType::S2C_ROOM_LEAVE_RES:
    {
        uint8_t r = 0;
        if (ReadU8(payload, payloadLen, off, r))
        {
            auto result = static_cast<RoomResult>(r);

            if (result == RoomResult::OK)
            {
                AsyncTask(ENamedThreads::GameThread, [this]()
                    {
                        UE_ASYNC_GUARD
                        SwitchLobbyState(ELobbyState::RoomList);
                    });
            }
        }
        break;
    }

    case PacketType::S2C_ROOM_READY_BRD:
    {
        uint32_t sid = 0;
        uint8_t ready = 0;
        if (ReadU32(payload, payloadLen, off, sid) && ReadU8(payload, payloadLen, off, ready))
        {
            /*AsyncTask(ENamedThreads::GameThread, [this, sid, ready]()
                {
                    UE_ASYNC_GUARD
                });*/
        }
        break;
	}


    case PacketType::S2C_ROOM_START_RES:
    {
        uint8_t r = 0;
        if (ReadU8(payload, payloadLen, off, r))
        {
            RoomResult result = static_cast<RoomResult>(r);
            AsyncTask(ENamedThreads::GameThread, [result]()
                {
                    if (GEngine)
                    {
                        const FColor MsgColor = result == RoomResult::OK ? FColor::Green : FColor::Red;
                        DS_SCREEN(-1, 5.f, MsgColor,
                            FString::Printf(TEXT("[IOCP] RoomStartRes=%s"), UTF8_TO_TCHAR(RoomResultToString(result))));
                    }
                });
        }
        break;
    }

    case PacketType::S2C_GAME_START:
    {
        uint8_t ipLen = 0;
        if (!ReadU8(payload, payloadLen, off, ipLen))
        {
            break;
        }

        std::string ipBytes = ReadString(payload, payloadLen, off, ipLen);
        if (ipBytes.size() != ipLen)
        {
            break;
        }

        uint32_t ticket = 0;
        uint16_t port = 0;
        if (!ReadU32(payload, payloadLen, off, ticket) || !ReadU16(payload, payloadLen, off, port))
        {
            break;
        }

        FString ServerIp = FString(UTF8_TO_TCHAR(ipBytes.c_str()));
        const FString PlayerNameOption = MakePlayerNameUrlOption(PlayerID);

        AsyncTask(ENamedThreads::GameThread, [this, ServerIp, port, ticket, PlayerNameOption]()
            {
                UE_ASYNC_GUARD

                APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
                if (!PC)
                {
                    return;
                }

                const FString TravelURL = FString::Printf(
                    TEXT("%s:%u?ticket=%u?Name=%s?PlayerName=%s"),
                    *ServerIp,
                    static_cast<uint32>(port),
                    ticket,
                    *PlayerNameOption,
                    *PlayerNameOption);

                if (GEngine)
                {
                    DS_SCREEN(-1, 8.f, FColor::Yellow,
                        FString::Printf(TEXT("[IOCP] GAME_START travel %s"), *TravelURL));
                }

                PC->ClientTravel(TravelURL, TRAVEL_Absolute);
            });

        break;
    }

    default:
    {
        break;
    }
    }
}

bool UUManagerGameInstance::SendAuth(PacketType type, const FString& id, const FString& pw)
{
    const FTCHARToUTF8 IdUtf8(*id);
    const FTCHARToUTF8 PwUtf8(*pw);
    const int32 IdByteLen = IdUtf8.Length();
    const int32 PwByteLen = PwUtf8.Length();

    if (id.IsEmpty() || id.Len() > MAX_ID_CHAR_LEN ||
        IdByteLen <= 0 || IdByteLen > MAX_ID_LEN ||
        PwByteLen <= 0 || PwByteLen > MAX_PW_LEN)
    {
        return false;
    }

    std::vector<char> payload;
    payload.reserve(static_cast<size_t>(2 + IdByteLen + PwByteLen));

    AppendU8(payload, static_cast<uint8_t>(IdByteLen));
    payload.insert(payload.end(), IdUtf8.Get(), IdUtf8.Get() + IdByteLen);

    AppendU8(payload, static_cast<uint8_t>(PwByteLen));
    payload.insert(payload.end(), PwUtf8.Get(), PwUtf8.Get() + PwByteLen);

    return SendPacket(type, payload.data(), static_cast<uint16_t>(payload.size()));
}

bool UUManagerGameInstance::SendPacket(PacketType type, const void* payload, uint16_t payloadLen)
{
    if (!m_running.load() || m_sock == INVALID_SOCKET)
    {
        return false;
    }

    const uint16_t totalSize = static_cast<uint16_t>(sizeof(PacketHeader) + payloadLen);
    if (totalSize > MAX_PACKET_SIZE)
    {
        return false;
    }

    std::vector<char> pkt(totalSize);
    PacketHeader hdr{};
    hdr.size = htons(totalSize);
    hdr.type = htons(static_cast<uint16_t>(type));

    std::memcpy(pkt.data(), &hdr, sizeof(hdr));
    if (payloadLen > 0 && payload != nullptr)
    {
        std::memcpy(pkt.data() + sizeof(hdr), payload, payloadLen);
    }

    std::lock_guard<std::mutex> lock(m_sendLock);

    size_t sentTotal = 0;
    while (sentTotal < pkt.size())
    {
        int sent = send(m_sock, pkt.data() + sentTotal, static_cast<int>(pkt.size() - sentTotal), 0);
        if (sent == SOCKET_ERROR)
        {
            return false;
        }
        if (sent == 0)
        {
            return false;
        }
        sentTotal += static_cast<size_t>(sent);
    }

    return true;
}

void UUManagerGameInstance::SwitchLobbyState(ELobbyState NewState)
{
    ALobbyController* PC = Cast<ALobbyController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
    if (PC)
    {
        PC->TransitionToLobbyState(true, NewState);
    }
}

bool UUManagerGameInstance::SendRegister(const FString& id, const FString& pw)
{
    return SendAuth(PacketType::C2S_REGISTER_REQ, id, pw);
}

bool UUManagerGameInstance::SendLogin(const FString& id, const FString& pw)
{
    PlayerID = id; PlayerPW = pw;

    return SendAuth(PacketType::C2S_LOGIN_REQ, id, pw);
}

bool UUManagerGameInstance::SendRoomListReq()
{
    return SendPacket(PacketType::C2S_ROOM_LIST_REQ, nullptr, 0);
}

bool UUManagerGameInstance::SendCreateRoom(const FString& roomName)
{
    FTCHARToUTF8 Converted(*roomName);
    const int32 ByteLen = Converted.Length();

    if (ByteLen <= 0 || ByteLen > ROOM_TITLE_MAX)
    {
        return false;
    }

    std::vector<char> payload;
    AppendU8(payload, static_cast<uint8_t>(ByteLen));
    payload.insert(payload.end(), Converted.Get(), Converted.Get() + ByteLen);
    return SendPacket(PacketType::C2S_ROOM_CREATE_REQ, payload.data(), static_cast<uint16_t>(payload.size()));
}

bool UUManagerGameInstance::SendJoinRoom(uint32_t roomId)
{
	std::vector<char> payload;
	AppendU32(payload, htonl(roomId));
    return SendPacket(PacketType::C2S_ROOM_JOIN_REQ, payload.data(), static_cast<uint16_t>(payload.size()));
}

bool UUManagerGameInstance::SendLeaveRoom()
{
    return SendPacket(PacketType::C2S_ROOM_LEAVE_REQ, nullptr, 0);
}

bool UUManagerGameInstance::SendReady(bool ready)
{
    uint8_t v = ready ? 1 : 0;
    return SendPacket(PacketType::C2S_ROOM_READY_REQ, &v, 1);
}


FABCharacterData* UUManagerGameInstance::GetABCharacterData(int32 Level) {
    if (nullptr == ABCharacterTable) return nullptr;

    bool bIsStructMatch = false;
    if (ABCharacterTable->RowStruct != nullptr) {
        bIsStructMatch = ABCharacterTable->RowStruct->IsChildOf(FABCharacterData::StaticStruct());
    }

    return ABCharacterTable->FindRow<FABCharacterData>(*FString::FromInt(Level), TEXT(""));
}
bool UUManagerGameInstance::SendRoomStart()
{
    return SendPacket(PacketType::C2S_ROOM_START_REQ, nullptr, 0);
}

void UUManagerGameInstance::MarkReturnToRoomAfterMatch()
{
    bReturnToRoomAfterMatch = true;
    bHasEnteredRoom = false;

    UE_LOG(LogTemp, Warning, TEXT("[LOBBY_RETURN] MarkReturnToRoomAfterMatch CachedRoomId=%u CachedMembers=%d"),
        CachedRoomId,
        CachedRoomMembers.Num());
}

bool UUManagerGameInstance::ConsumeReturnToRoomAfterMatch()
{
    const bool bResult = bReturnToRoomAfterMatch;
    bReturnToRoomAfterMatch = false;

    UE_LOG(LogTemp, Warning, TEXT("[LOBBY_RETURN] ConsumeReturnToRoomAfterMatch Result=%d CachedRoomId=%u CachedMembers=%d"),
        bResult ? 1 : 0,
        CachedRoomId,
        CachedRoomMembers.Num());

    return bResult;
}

void UUManagerGameInstance::BroadcastCachedRoomMembers()
{
    if (CachedRoomMembers.Num() <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[LOBBY_RETURN] BroadcastCachedRoomMembers skipped. Empty CachedRoomId=%u"),
            CachedRoomId);
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("[LOBBY_RETURN] BroadcastCachedRoomMembers RoomId=%u Members=%d"),
        CachedRoomId,
        CachedRoomMembers.Num());

    OnRoomMemberListUpdated.Broadcast(CachedRoomMembers);
}
