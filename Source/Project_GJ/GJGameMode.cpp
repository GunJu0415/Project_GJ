// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameMode.h"
#include "GJPlayerController.h" // <== 추가!
#include "GJCharacter.h"

AGJGameMode::AGJGameMode()
{
	// 기본 컨트롤러와 폰(캐릭터) 클래스 덮어쓰기
	PlayerControllerClass = AGJPlayerController::StaticClass();
	DefaultPawnClass = AGJCharacter::StaticClass();
}
