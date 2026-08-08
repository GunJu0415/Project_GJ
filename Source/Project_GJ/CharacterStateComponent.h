// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CharacterStateComponent.generated.h"


// 1. ������ ���µ� ���� (������ �߰� ����)
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
	Attack,
	WeaponSwap
};

// 2. ���°� ���� �� �˸��� �ѷ��� ��������Ʈ ����
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateChangedSignature, ECharacterState, PrevState, ECharacterState, NewState);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PROJECT_GJ_API UCharacterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCharacterStateComponent();

	// ���� ���� �Լ� (��������Ʈ������ ȣ�� ����)
	UFUNCTION(BlueprintCallable, Category = "State")
	void SetState(ECharacterState NewState);

	// ���� ���� Ȯ�� �Լ�
	UFUNCTION(BlueprintPure, Category = "State")
	ECharacterState GetState() const { return CurrentState; }

	// ���� ���� �̺�Ʈ �Ŵ��� (��������Ʈ �̺�Ʈ �׷������� ���ε� ����)
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
