// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStateComponent.generated.h"


// 1. 관리할 상태들 정의 (언제든 추가 가능)
UENUM(BlueprintType)
enum class ECharacterState : uint8
{
	Idle,
	Rolling,
	Attacking,
	Hit,
	Dead,
	Reloading,
	Dashing,
	Dodge,
	Attack 
};

// 2. 상태가 변할 때 알림을 뿌려줄 델리게이트 선언
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateChangedSignature, ECharacterState, PrevState, ECharacterState, NewState);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_GJ_API UCharacterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCharacterStateComponent();

	// 상태 변경 함수 (블루프린트에서도 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetState(ECharacterState NewState);

	// 현재 상태 확인 함수
	UFUNCTION(BlueprintPure, Category = "State")
	ECharacterState GetState() const { return CurrentState; }

	// 상태 변경 이벤트 매니저 (블루프린트 이벤트 그래프에서 바인딩 가능)
	UPROPERTY(BlueprintAssignable, Category = "State")
	FOnStateChangedSignature OnStateChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	ECharacterState CurrentState;

	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
};
