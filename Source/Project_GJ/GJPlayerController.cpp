// Fill out your copyright notice in the Description page of Project Settings.


#include "GJPlayerController.h"

AGJPlayerController::AGJPlayerController()
{
    // 탑다운 뷰이므로 게임 내내 마우스 커서를 보여줍니다.
    bShowMouseCursor = true;
    DefaultMouseCursor = EMouseCursor::Crosshairs; // 십자선 커서로 변경
}