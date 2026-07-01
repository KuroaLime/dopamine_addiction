
#include "Default/Ability/GA_Interaction.h"
#include "Game/InGame/ManagerCharacter.h"
#include "Default/Component/Player/InteractionComponent.h"
#include "Default/Actor/InteractableInterface.h"

UGA_Interaction::UGA_Interaction() {
	AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(FName("Ability.Interaction.Interact")));
}

void UGA_Interaction::ActivateAbility() {


	// CustomAbility.h에 정의된 OwnerCharacter와 AvatarActor를 사용합니다.
	if (!OwnerCharacter) return;

	// 컴포넌트에서 현재 바라보고 있는(Focused) 타겟 가져오기
	UInteractionComponent* InteractComp = OwnerCharacter->FindComponentByClass<UInteractionComponent>();
	if (InteractComp)
	{
		AActor* Target = InteractComp->GetFocusedActor();
		if (Target && Target->Implements<UInteractableInterface>())
		{
			
			// 인터페이스를 통해 상호작용 실행 명령을 내립니다.
			IInteractableInterface::Execute_Interact(Target, OwnerCharacter);
		}
	}

	// 어빌리티 종료 (CustomAbility에 정의된 함수 호출)
	EndAbility(false);
}