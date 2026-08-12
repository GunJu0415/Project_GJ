// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameInstance.h"
#include "Kismet/GameplayStatics.h"

void UGJGameInstance::Init()
{
	Super::Init();
	// ������ ���� �� 1ȸ ����Ǵ� �ʱ�ȭ ���� (����� �����)
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

void UGJGameInstance::EndRun()
{
	// 회차를 올리는 곳은 여기 한 곳뿐이다. 사망 시점에 호출되므로,
	// 게임오버 위젯이 있든 없든(폴백 경로) 정확히 한 번만 증가한다.
	IncrementRebirthCount();
}

void UGJGameInstance::ReturnToHub()
{
	if (HubLevelName.IsNone())
	{
		// 잘못된 이름으로 OpenLevel을 호출하면 빈 맵에 갇혀버리므로, 아예 이동하지 않고 로그만 남긴다
		UE_LOG(LogTemp, Error, TEXT("ReturnToHub: HubLevelName is not set. Check [/Script/Project_GJ.GJGameInstance] in Config/DefaultGame.ini"));
		return;
	}

	UGameplayStatics::OpenLevel(this, HubLevelName);
}

void UGJGameInstance::StartNewRun()
{
	if (CombatLevelName.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("StartNewRun: CombatLevelName is not set. Check [/Script/Project_GJ.GJGameInstance] in Config/DefaultGame.ini"));
		return;
	}

	UGameplayStatics::OpenLevel(this, CombatLevelName);
}