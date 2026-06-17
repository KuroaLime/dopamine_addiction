// Fill out your copyright notice in the Description page of Project Settings.

#include "Default/System/UManagerGameInstance.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#define UE_ASYNC_GUARD if (!IsValid(this) || !GetWorld()) return;

UUManagerGameInstance::UUManagerGameInstance() {
    FString CharacterDataPath = TEXT("/Game/GameData/ABCharacterData.ABCharacterData");
    static ConstructorHelpers::FObjectFinder<UDataTable> DT_ABCHARACTER(*CharacterDataPath);
    
    ABCharacterTable = DT_ABCHARACTER.Object;
}




void UUManagerGameInstance::Init() {
	
    Super::Init();

    const bool bServerProcess = IsRunningDedicatedServer() || FParse::Param(FCommandLine::Get(), TEXT("server"));
    if (bServerProcess)
    {
        UE_LOG(LogTemp, Warning, TEXT("[IOCP] Server process. Skip lobby TCP connect."));
        return;
    }

    const FString ip = "127.0.0.1";
    const uint16_t port = 9000;

    if (Connect(ip, port))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Connected to login server"));
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Failed to connect to login server"));
        }
    }
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
                            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("================ [IOCP] Successfully Joined the Room! ================"));
                            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Green, FString::Printf(TEXT("[Room Info] Title: %s (ID: %d)"), *info.title, info.roomId));
                            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Orange, FString::Printf(TEXT("[Room Players] %d / %d (Host UID: %d)"), info.curPlayers, info.maxPlayers, info.hostId));
                            GEngine->AddOnScreenDebugMessage(-1, 8.f, FColor::Cyan, TEXT("====================================================================="));
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

    case PacketType::S2C_ROOM_START_RES:
    {
        uint8_t r = 0;
        if (ReadU8(payload, payloadLen, off, r))
        {
            UE_LOG(LogTemp, Warning, TEXT("[IOCP] RoomStart result=%d"), static_cast<int32>(r));
        }
        break;
    }

    case PacketType::S2C_GAME_START:
    {
        uint8_t ipLen = 0;
        uint32_t ticket = 0;
        uint16_t port = 0;

        if (!ReadU8(payload, payloadLen, off, ipLen))
        {
            break;
        }

        std::string ipUtf8 = ReadString(payload, payloadLen, off, ipLen);
        if (ipUtf8.size() != ipLen)
        {
            break;
        }

        if (!ReadU32(payload, payloadLen, off, ticket))
        {
            break;
        }

        if (!ReadU16(payload, payloadLen, off, port))
        {
            break;
        }

        const FString TargetIp = UTF8_TO_TCHAR(ipUtf8.c_str());
        const uint32_t TravelTicket = ticket != 0 ? ticket : m_sessionId;
        const FString InGameMapPath = TEXT("/Game/InGame/System/Main_Game_World");
        const FString URL = FString::Printf(
            TEXT("%s:%d%s?ticket=%u"),
            *TargetIp,
            static_cast<int32>(port),
            *InGameMapPath,
            TravelTicket
        );

        AsyncTask(ENamedThreads::GameThread, [this, URL]()
            {
                UE_ASYNC_GUARD

                UE_LOG(LogTemp, Warning, TEXT("[IOCP] GameStart received. ClientTravel URL=%s"), *URL);

                APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
                if (PC)
                {
                    PC->ClientTravel(URL, TRAVEL_Absolute);
                }
            });
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

    default:
    {
        break;
    }
    }
}

bool UUManagerGameInstance::SendAuth(PacketType type, const FString& id, const FString& pw)
{
    if (id.Len() > 255 || pw.Len() > 255)
    {
        return false;
    }

    std::vector<char> payload;

    AppendU8(payload, static_cast<uint8_t>(id.Len()));
    std::string idUtf8 = TCHAR_TO_UTF8(*id);
    payload.insert(payload.end(), idUtf8.begin(), idUtf8.end());

    AppendU8(payload, static_cast<uint8_t>(pw.Len()));
    std::string pwUtf8 = TCHAR_TO_UTF8(*pw);
    payload.insert(payload.end(), pwUtf8.begin(), pwUtf8.end());

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
    if (roomName.Len() > 255)
    {
        return false;
    }
    std::vector<char> payload;
    AppendU8(payload, static_cast<uint8_t>(roomName.Len()));
    std::string roomNameUtf8 = TCHAR_TO_UTF8(*roomName);
    payload.insert(payload.end(), roomNameUtf8.begin(), roomNameUtf8.end());
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

bool UUManagerGameInstance::SendRoomStart()
{
    return SendPacket(PacketType::C2S_ROOM_START_REQ, nullptr, 0);
}


FABCharacterData* UUManagerGameInstance::GetABCharacterData(int32 Level) {
    if (nullptr == ABCharacterTable) return nullptr;

    bool bIsStructMatch = false;
    if (ABCharacterTable->RowStruct != nullptr) {
        bIsStructMatch = ABCharacterTable->RowStruct->IsChildOf(FABCharacterData::StaticStruct());
    }

    return ABCharacterTable->FindRow<FABCharacterData>(*FString::FromInt(Level), TEXT(""));
}