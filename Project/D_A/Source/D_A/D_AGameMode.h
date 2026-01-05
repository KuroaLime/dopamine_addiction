// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "D_AGameMode.generated.h"

/**
 *  Simple GameMode for a third person game
 */
UCLASS(abstract)
class AD_AGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	
	/** Constructor */
	AD_AGameMode();
};



