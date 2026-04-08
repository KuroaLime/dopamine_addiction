// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/BaseItem.h"
#include "Items/IDLE/InteractableInterface.h"
#include "Ability/CustomAbility.h"
#include "Components/WidgetComponent.h"
#include "UsableItem.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API AUsableItem : public ABaseItem, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AUsableItem();
	virtual void BeginPlay() override;
	virtual void OnBeginFocus_Implementation() override;
	virtual void OnEndFocus_Implementation() override;
	virtual void Interact_Implementation(APawn* Interactor) override;
	virtual void OnPickedUp(class AManagerCharacter* Interactor) override;

	// 사용 시 발동될 어빌리티 클래스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Data")
	TSubclassOf<class UCustomAbility> GrantAbilityClass;

protected:
	UPROPERTY(EditAnywhere, Category ="Data")
	TObjectPtr<class UItemData> ItemData;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<class USphereComponent> InteractionSphere;

	/*UPROPERTY(VisibleAnywhere)
	TObjectPtr<class UStaticMeshComponent> ItemMesh;*/

	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWidgetComponent> InteractionWidget;
};
