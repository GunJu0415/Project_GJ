// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameInstance.h"

void UGJGameInstance::Init()
{
	Super::Init();
	// 게임이 켜질 때 1회 실행되는 초기화 로직 (현재는 비워둠)
	UE_LOG(LogTemp, Warning, TEXT("Rebirth Game Instance Initialized!"));
}

void UGJGameInstance::IncrementRebirthCount()
{
	RebirthCount++;
	UE_LOG(LogTemp, Log, TEXT("Player Reborn. Current Rebirth Count: %d"), RebirthCount);
}

int32 UGJGameInstance::GetRebirthCount() const
{
	return RebirthCount;
}