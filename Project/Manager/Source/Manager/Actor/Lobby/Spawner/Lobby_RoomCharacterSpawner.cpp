// Fill out your copyright notice in the Description page of Project Settings.


#include "Actor/Lobby/Spawner/Lobby_RoomCharacterSpawner.h"
#include "Game/Lobby/GS_Lobby.h"

// Sets default values
ALobby_RoomCharacterSpawner::ALobby_RoomCharacterSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;


    SpawnArrow = CreateDefaultSubobject< UArrowComponent>(TEXT("SpawnArrow"));
    SpawnArrow->SetupAttachment(RootComponent);
}



// Called every frame
void ALobby_RoomCharacterSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ALobby_RoomCharacterSpawner::BeginPlay()
{
    Super::BeginPlay();

    if (AGS_Lobby* GS = GetWorld()->GetGameState<AGS_Lobby>())
    {
        GS->OnLobbyUpdated.AddDynamic(this, &ALobby_RoomCharacterSpawner::UpdateDisplay);
    }
    UpdateDisplay();
}

void ALobby_RoomCharacterSpawner::UpdateDisplay()
{
    AGS_Lobby* GS = GetWorld()->GetGameState<AGS_Lobby>();
    if (!GS) return;

    // 부모 클래스 AA_Spawn에 정의된 SpawnPointID를 인덱스로 사용
    if (GS->LobbySlots.IsValidIndex(SpawnPointID))
    {
        const FLobbySlotData& MyData = GS->LobbySlots[SpawnPointID];

        if (MyData.PlayerState != nullptr)
        {
            if (!SpawnedCharacter) SpawnCharacter();
        }
        else
        {
            if (SpawnedCharacter) ClearCharacter();
        }
    }
}

void ALobby_RoomCharacterSpawner::SpawnCharacter()
{
    if (!CharacterClass || !SpawnArrow) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    // 현재 스포너의 위치와 회전값으로 스폰
    SpawnedCharacter = GetWorld()->SpawnActor<AActor>(CharacterClass, SpawnArrow->GetComponentTransform(), Params);
}

void ALobby_RoomCharacterSpawner::ClearCharacter()
{
    if (SpawnedCharacter)
    {
        SpawnedCharacter->Destroy();
        SpawnedCharacter = nullptr;
    }
}