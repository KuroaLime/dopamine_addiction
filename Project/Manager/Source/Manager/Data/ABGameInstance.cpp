// Fill out your copyright notice in the Description page of Project Settings.


#include "Data/ABGameInstance.h"

UABGameInstance::UABGameInstance() {
	FString CharacterDataPath = TEXT("/Game/GameData/ABCharacterData.ABCharacterData");
	static ConstructorHelpers::FObjectFinder<UDataTable> DT_ABCHARACTER(*CharacterDataPath);
	//ABCHECK(DT_ABCHARACTER.Succeeded());
	ABCharacterTable = DT_ABCHARACTER.Object;
	//ABCHECK(ABCharacterTable->GetRowMap().Num() > 0);
}

void UABGameInstance::Init() {
	Super::Init();
	UE_LOG(LogTemp, Warning, TEXT("My GameInstance Init is Running!!!!!!"));
}

FABCharacterData* UABGameInstance::GetABCharacterData(int32 Level) {
	if (nullptr == ABCharacterTable) {
		UE_LOG(LogTemp, Error, TEXT("ABCharacterTable is NULL!"));
		return nullptr;
	}
	return ABCharacterTable->FindRow<FABCharacterData>(*FString::FromInt(Level), TEXT(""));
}