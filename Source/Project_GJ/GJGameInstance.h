// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GJGameInstance.generated.h"

/**
 * 
 */
UCLASS(config=Game)
class PROJECT_GJ_API UGJGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// ��ȸ(����) Ƚ���� �����ϴ� �Լ��� ����
	UFUNCTION(BlueprintCallable, Category = "Rebirth System")
	void IncrementRebirthCount();

	UFUNCTION(BlueprintPure, Category = "Rebirth System")
	int32 GetRebirthCount() const;

	// 런 종료 - 회차 카운트만 올린다. 실제 이동은 ReturnToHub()가 담당.
	// 사망 시점에 딱 한 번 호출되므로, 게임오버 위젯을 거치든 안 거치든 카운트가 중복되지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Run")
	void EndRun();

	// 허브 레벨로 이동
	UFUNCTION(BlueprintCallable, Category = "Run")
	void ReturnToHub();

	// 새 런 시작 - 전투 레벨로 이동 (카운트는 올리지 않음)
	UFUNCTION(BlueprintCallable, Category = "Run")
	void StartNewRun();

protected:
	// �÷��̾ �װ� ������ص� �����Ǵ� ������
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rebirth System")
	int32 RebirthCount = 0;

	// 레벨 경로는 Config/DefaultGame.ini의 [/Script/Project_GJ.GJGameInstance] 섹션에서 지정한다.
	// Config 프로퍼티로 둔 덕분에 블루프린트 서브클래스를 따로 만들 필요가 없다.
	UPROPERTY(Config, EditDefaultsOnly, Category = "Run")
	FName HubLevelName;

	UPROPERTY(Config, EditDefaultsOnly, Category = "Run")
	FName CombatLevelName;

};
