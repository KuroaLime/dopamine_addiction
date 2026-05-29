// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/Login/LoginGameMode.h"
#include "Game/Login/LoginPlayerController.h"

ALoginGameMode::ALoginGameMode() {
	PlayerControllerClass = ALoginPlayerController::StaticClass();
}