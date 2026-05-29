// Fill out your copyright notice in the Description page of Project Settings.


#include "Default/Actor/ConsumptionItem.h"
#include "Game/InGame/ManagerCharacter.h"

AConsumptionItem::AConsumptionItem()
{
	ItemType = EPickableType::Item;
	MaxStackCount = 5;
}

void AConsumptionItem::OnPickedUp(AManagerCharacter* Player)
{
	Super::OnPickedUp(Player);
}