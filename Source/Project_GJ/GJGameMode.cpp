// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameMode.h"
#include "GJPlayerController.h" // <== �߰�!
#include "GJCharacter.h"
#include "GJGameInstance.h"
#include "TimerManager.h"

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
	// Task 3에서 게임오버 위젯을 띄우는 코드로 교체된다.
	// 지금은 위젯 없이 곧바로 허브로 보내서 루프가 돌아가는지부터 확인한다.
	if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
	{
		GJGameInstance->ReturnToHub();
	}
}
