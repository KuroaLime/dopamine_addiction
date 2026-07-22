// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogManager, Log, All);

/**
 * 디버그 트레이스 로그. 기존 [DS] UE_LOG(LogTemp, Warning, ...)를 대체한다.
 * - 평소에는 출력되지 않음(Verbose 레벨). 디버깅 시 콘솔에서 `Log LogManager Verbose`로 켠다.
 * - Shipping 빌드에서는 Verbose가 컴파일 제거되어 비용 0.
 * 사용: DS_LOG(TEXT("[DS] ... %d"), Value);
 */
#define DS_LOG(Format, ...) UE_LOG(LogManager, Verbose, Format, ##__VA_ARGS__)

/**
 * 화면 디버그 메시지 / 디버그 드로우(라인·구·점) 전체를 켜고 끄는 스위치.
 * 개발 중 필요할 때만 1로 바꿔서 켠다. 평소에는 0으로 꺼둔다.
 */
#define MANAGER_DEBUG_VISUALS 0

/**
 * 화면 디버그 메시지. 기존 GEngine->AddOnScreenDebugMessage(...) 호출을 대체한다.
 * - Shipping 빌드에서는 완전히 컴파일 제거(비용 0).
 * - 개발 빌드에서도 MANAGER_DEBUG_VISUALS가 0이면 표시되지 않는다.
 * 사용: DS_SCREEN(-1, 2.f, FColor::Green, TEXT("..."));
 */
#if UE_BUILD_SHIPPING || !MANAGER_DEBUG_VISUALS
	#define DS_SCREEN(...) do {} while (0)
#else
	#define DS_SCREEN(...) do { if (GEngine) { GEngine->AddOnScreenDebugMessage(__VA_ARGS__); } } while (0)
#endif

/**
 * 디버그 드로우(라인/구/점). 기존 DrawDebugLine/Sphere/Point(...) 직접 호출을 대체한다.
 * - Shipping 빌드 및 MANAGER_DEBUG_VISUALS=0일 때 완전히 컴파일 제거(비용 0).
 * 사용: DS_DRAW_LINE(World, Start, End, FColor::Green, false, 1.f, 0, 1.f);
 */
#if UE_BUILD_SHIPPING || !MANAGER_DEBUG_VISUALS
	#define DS_DRAW_LINE(...) do {} while (0)
	#define DS_DRAW_SPHERE(...) do {} while (0)
	#define DS_DRAW_POINT(...) do {} while (0)
#else
	#define DS_DRAW_LINE(...) DrawDebugLine(__VA_ARGS__)
	#define DS_DRAW_SPHERE(...) DrawDebugSphere(__VA_ARGS__)
	#define DS_DRAW_POINT(...) DrawDebugPoint(__VA_ARGS__)
#endif