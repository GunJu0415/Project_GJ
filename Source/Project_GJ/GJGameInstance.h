// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "GJGameInstance.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_GJ_API UGJGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	// 윤회(죽음) 횟수를 추적하는 함수와 변수
	UFUNCTION(BlueprintCallable, Category = "Rebirth System")
	void IncrementRebirthCount();

	UFUNCTION(BlueprintPure, Category = "Rebirth System")
	int32 GetRebirthCount() const;

protected:
	// 플레이어가 죽고 재시작해도 유지되는 데이터
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rebirth System")
	int32 RebirthCount = 0;
	
};
