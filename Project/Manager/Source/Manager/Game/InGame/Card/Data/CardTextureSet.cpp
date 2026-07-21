#include "Game/InGame/Card/Data/CardTextureSet.h"

#include "UObject/UObjectGlobals.h"

UCardTextureSet* UCardTextureSet::LoadDefault()
{
	return LoadObject<UCardTextureSet>(
		nullptr,
		TEXT("/Game/InGame/CARD/DA_CardTextures.DA_CardTextures"));
}
