// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Lobby/Actor/Spawner/Lobby_RoomCharacterSpawner.h"
#include "Default/System/UManagerGameInstance.h"
#include "Game/Lobby/UI/RoomUserWidget.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"

// Sets default values
ALobby_RoomCharacterSpawner::ALobby_RoomCharacterSpawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	bSupportsBarrierControl = false;
	bReplicates = false;
	bAlwaysRelevant = false;


    SpawnArrow = CreateDefaultSubobject< UArrowComponent>(TEXT("SpawnArrow"));
    SpawnArrow->SetupAttachment(RootComponent);

	UserInfoWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("UserInfoWidget"));
	UserInfoWidget->SetupAttachment(RootComponent);

	// UserInfoWidget 초기 설정
    UserInfoWidget->SetWidgetSpace(EWidgetSpace::World);
    UserInfoWidget->SetDrawSize(FVector2D(300.f, 300.f));
    UserInfoWidget->SetVisibility(false);
}

// Called every frame
void ALobby_RoomCharacterSpawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ALobby_RoomCharacterSpawner::BeginPlay()
{
    Super::BeginPlay();

    if (UUManagerGameInstance* GameInst = Cast<UUManagerGameInstance>(GetGameInstance()))
    {
        GameInst->OnRoomMemberListUpdated.AddDynamic(this, &ALobby_RoomCharacterSpawner::UpdateLobbyCharacters);
    }
}


void ALobby_RoomCharacterSpawner::SpawnCharacter()
{
    if (CharacterClass && SpawnArrow)
    {
        FTransform SpawnTransform = SpawnArrow->GetComponentTransform();
        SpawnedCharacter = GetWorld()->SpawnActor<AActor>(CharacterClass, SpawnTransform);
        ACharacter* LobbyChar = Cast<ACharacter>(SpawnedCharacter);
        if (LobbyChar && LobbyChar->GetMesh() && LobbyMeshes.IsValidIndex(SpawnPointID))
        {
            LobbyChar->GetMesh()->SetSkeletalMesh(LobbyMeshes[SpawnPointID]);
        }
    }
}

void ALobby_RoomCharacterSpawner::ClearCharacter()
{
    if (SpawnedCharacter)
    {
        SpawnedCharacter->Destroy();
        SpawnedCharacter = nullptr;

		ClearUserInfoWidget();
    }
}

void ALobby_RoomCharacterSpawner::UpdateUserInfoWidget(const FRoomMemberInfoView& MemberInfo)
{
    if (UserInfoWidget)
    {
        if (URoomUserWidget* UserWidget = Cast<URoomUserWidget>(UserInfoWidget->GetUserWidgetObject()))
        {
			UserWidget->UpdateUserInfo(MemberInfo.nickname, MemberInfo.isReady);
            // 위치는 더 이상 코드에서 강제로 덮어쓰지 않는다 — 블루프린트에서 UserInfoWidget
            // 컴포넌트에 직접 잡아준 상대 트랜스폼(위치/회전)이 그대로 적용된다.
            UserInfoWidget->SetVisibility(true);
        }
    }
}

void ALobby_RoomCharacterSpawner::ClearUserInfoWidget()
{
    if (UserInfoWidget)
    {
        UserInfoWidget->SetVisibility(false);
    }
}

void ALobby_RoomCharacterSpawner::UpdateLobbyCharacters(const TArray<FRoomMemberInfoView>& Members)
{
    if (Members.IsValidIndex(SpawnPointID))
    {
        if (!SpawnedCharacter) SpawnCharacter();
		UpdateUserInfoWidget(Members[SpawnPointID]);
        // (선택 사항) 나중에 여기서 Members[SpawnPointID].isReady 값에 따라 
        // SpawnedCharacter의 애니메이션을 바꾸거나 할 수 있습니다!
    }
    else
        if (SpawnedCharacter) ClearCharacter();
}
