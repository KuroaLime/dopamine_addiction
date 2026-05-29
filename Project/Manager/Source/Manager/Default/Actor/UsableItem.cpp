// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Actor/UsableItem.h"
#include "Components/SphereComponent.h"
#include "Default/Ability/CustomASC.h"

#include "GameplayTagContainer.h"

#include "Game/InGame/ManagerCharacter.h"
#include "Default/Component/Player/InteractionComponent.h"


AUsableItem::AUsableItem()
{
	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);

	InteractionSphere->SetSphereRadius(500.f);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	InteractionSphere->SetHiddenInGame(false);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(RootComponent);
	InteractionWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetVisibility(false);

}
void AUsableItem::BeginPlay() {
	Super::BeginPlay();

	if (InteractionSphere)
	{
		InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AUsableItem::OnSphereBeginOverlap);
		InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AUsableItem::OnSphereEndOverlap);

		InteractionSphere->SetGenerateOverlapEvents(true);
	}
}
void AUsableItem::OnBeginFocus_Implementation()
{
	MeshComp->SetRenderCustomDepth(true);
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(true);
	}
}

void AUsableItem::OnEndFocus_Implementation()
{
	MeshComp->SetRenderCustomDepth(false);
	if (InteractionWidget)
	{
		InteractionWidget->SetVisibility(false);
	}
}
void AUsableItem::OnPickedUp(AManagerCharacter* Interactor) {
	if (!Interactor) return;


	// 1. 캐릭터의 CustomASC 가져오기
	UCustomASC* ASC = Interactor->GetCustomASC();
	if (!ASC) return;

	// 2. 전달할 데이터(Payload) 생성 
	// [수정됨] 정식 GAS의 FGameplayEventData가 아닌, 우리가 만든 FCustomGameplayEventData 사용!
	FCustomGameplayEventData Payload;
	Payload.Instigator = this;             // 이벤트를 발생시킨 주체
	Payload.TargetObject = this;           // [수정됨] OptionalObject 대신 우리가 정의한 TargetObject 사용!

	// 3. 이벤트 태그 설정
	FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName("Event.Interaction.ReceiveItem"));

	// 4. ASC의 HandleGameplayEvent를 직접 호출하여 이벤트 발송
	ASC->HandleGameplayEvent(EventTag, Payload);

	Super::OnPickedUp(Interactor);

	// 5. 아이템 처리 (인벤토리에 들어갔으므로 월드에서는 숨김 처리)
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	if (InteractionSphere)
	{
		InteractionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InteractionSphere->SetGenerateOverlapEvents(false);
	}
}
// [핵심 수정] 유저님의 Custom GAS에 맞춰 신호를 보냅니다.
void AUsableItem::Interact_Implementation(APawn* Interactor)
{
	if (!Interactor) return;

	AManagerCharacter* MyChar = Cast<AManagerCharacter>(Interactor);
	if (MyChar)
	{
		// [핵심] 과거의 PickUp 강제 실행과 Destroy()를 모두 지우고, 
		// 우리가 새롭게 만든 페이로드 전달 함수(OnPickedUp)를 호출합니다!
		OnPickedUp(MyChar);
	}
}

void AUsableItem::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Green, FString::Printf(TEXT("Itemmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm")));
	if (auto* Comp = OtherActor->FindComponentByClass<UInteractionComponent>())
	{
		Comp->AddNearByInteractable(1); //
	}
}

void AUsableItem::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (auto* Comp = OtherActor->FindComponentByClass<UInteractionComponent>())
	{
		Comp->AddNearByInteractable(-1); //
	}
}