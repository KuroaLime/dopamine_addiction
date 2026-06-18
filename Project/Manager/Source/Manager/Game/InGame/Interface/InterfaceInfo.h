// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

UENUM(BlueprintType)
enum class EGamePhase : uint8
{
	TPS,
	Card,
	Shop,
	Death,
};