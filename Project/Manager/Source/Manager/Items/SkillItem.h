// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Items/UsableItem.h"
#include "SkillItem.generated.h"

/**
 * 
 */
UCLASS()
class MANAGER_API ASkillItem : public AUsableItem
{
	GENERATED_BODY()
public:
    ASkillItem();

    virtual void OnPickedUp(class AManagerCharacter* Player) override;
};
