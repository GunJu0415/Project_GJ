// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameMode.h"
#include "GJPlayerController.h" // <== �߰�!
#include "GJCharacter.h"
#include "GJGameInstance.h"
#include "GJGameOverWidget.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AGJGameMode::AGJGameMode()
{
	// �⺻ ��Ʈ�ѷ��� ��(ĳ����) Ŭ���� �����
	PlayerControllerClass = AGJPlayerController::StaticClass();
	DefaultPawnClass = AGJCharacter::StaticClass();
}

void AGJGameMode::OnPlayerDied()
{
	// 사망 통보가 중복으로 들어와도 타이머를 두 번 걸지 않도록 막는다
	if (bRunEnded)
	{
		return;
	}
	bRunEnded = true;

	// 런이 끝나는 시점은 "죽은 순간"이다. 화면이 뜨는 시점이 아니라 여기서 회차를 올려야
	// 게임오버 위젯이 방금 끝난 도전 번호를 그대로 표시할 수 있다.
	if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
	{
		GJGameInstance->EndRun();
	}

	GetWorldTimerManager().SetTimer(
		GameOverTimerHandle, this, &AGJGameMode::ShowGameOverScreen, DeathToGameOverDelay, false);
}

void AGJGameMode::ShowGameOverScreen()
{
	// 위젯 클래스가 아직 할당되지 않았어도 플레이어가 갇히지 않도록, 위젯 없이 곧바로 허브로 보낸다
	if (!GameOverWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("ShowGameOverScreen: GameOverWidgetClass is not set on BP_GJGameMode. Skipping the game over screen."));
		if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
		{
			GJGameInstance->ReturnToHub();
		}
		return;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}

	GameOverWidgetInstance = CreateWidget<UGJGameOverWidget>(PC, GameOverWidgetClass);
	if (!GameOverWidgetInstance)
	{
		return;
	}

	GameOverWidgetInstance->AddToViewport();

	// 회차는 사망 시점에 이미 올라가 있으므로, 지금 값이 곧 방금 끝난 도전의 번호다
	if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
	{
		GameOverWidgetInstance->SetRunCount(GJGameInstance->GetRebirthCount());
	}

	// 게임을 일시정지하지 않고 입력만 UI로 넘긴다. 죽은 뒤에도 적이 계속 움직이지만
	// 플레이어의 캡슐/메시 콜리전이 이미 해제되어 있어 무해하고,
	// 일시정지 + 입력 모드 조합에서 겪었던 포커스 문제를 피할 수 있다.
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameOverWidgetInstance->TakeWidget());
	PC->SetInputMode(InputMode);
	PC->bShowMouseCursor = true;
}
