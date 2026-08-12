// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GJGameMode.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_GJ_API AGJGameMode : public AGameMode
{
	GENERATED_BODY()
	
public:
	AGJGameMode();

	// 플레이어가 죽었을 때 캐릭터가 호출한다 - 런 종료 흐름을 시작한다
	void OnPlayerDied();

protected:
	// 사망부터 게임오버 화면까지의 딜레이(초)
	UPROPERTY(EditDefaultsOnly, Category = "Run")
	float DeathToGameOverDelay = 2.0f;

	// 딜레이가 끝났을 때 호출된다
	void ShowGameOverScreen();

	// 사망 통보가 여러 번 들어와도 한 번만 처리되게 하는 플래그
	bool bRunEnded = false;

	FTimerHandle GameOverTimerHandle;

};
