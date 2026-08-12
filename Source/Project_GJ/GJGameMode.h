// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "GJGameMode.generated.h"

class UGJGameOverWidget;

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

	// 게임오버 화면 위젯 클래스. BP_GJGameMode 디테일 패널에서 WBP_GameOver를 할당한다.
	// 비어 있으면 위젯 없이 곧바로 허브로 이동한다(에셋을 아직 안 만들었어도 루프가 멈추지 않도록)
	UPROPERTY(EditDefaultsOnly, Category = "Run")
	TSubclassOf<UGJGameOverWidget> GameOverWidgetClass;

	UPROPERTY()
	UGJGameOverWidget* GameOverWidgetInstance;

	// 딜레이가 끝났을 때 호출된다
	void ShowGameOverScreen();

	// 사망 통보가 여러 번 들어와도 한 번만 처리되게 하는 플래그
	bool bRunEnded = false;

	FTimerHandle GameOverTimerHandle;

};
