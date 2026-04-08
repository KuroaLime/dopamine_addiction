// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/GA_ReceiveItem.h"
#include "ManagerCharacter.h"
#include "CustomASC.h"
#include "Items/BaseItem.h" // 프로젝트의 아이템 최상위 클래스 헤더

void UGA_ReceiveItem::ActivateAbilityWithEvent(const FCustomGameplayEventData& Payload)
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("아이템 획득 검토중"));
	}
	// 1. 전달받은 Payload에서 아이템을 안전하게 꺼냅니다.
	ABaseItem* ReceivedItem = Cast<ABaseItem>(Payload.TargetObject);

	AManagerCharacter* MyChar = Cast<AManagerCharacter>(OwnerCharacter);

	// 2. 캐릭터와 아이템이 모두 유효하다면 인벤토리에 추가합니다.
	if (MyChar && ReceivedItem)
	{
		// 이 부분은 작성해두신 인벤토리 시스템에 맞게 수정해주세요!
		// 예시: MyChar->GetInventory()->Add(ReceivedItem);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("아이템 획득 성공!"));
		}
	}

	// 3. 어빌리티 로직이 끝났으므로 정상 종료
	EndAbility(false);
}
