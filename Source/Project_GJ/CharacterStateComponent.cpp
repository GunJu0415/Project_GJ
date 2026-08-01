// Fill out your copyright notice in the Description page of Project Settings.


#include "CharacterStateComponent.h"

// Sets default values for this component's properties
UCharacterStateComponent::UCharacterStateComponent()
{
	// 상태 관리만 하므로 틱(Tick)은 꺼두어 성능을 확보합니다.
	PrimaryComponentTick.bCanEverTick = false;

	// 시작 시 기본 상태 세팅
	CurrentState = ECharacterState::Idle;
	// ...
}


// Called when the game starts
void UCharacterStateComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UCharacterStateComponent::SetState(ECharacterState NewState)
{
	// 현재 상태와 동일한 상태로 변경하려 하면 무시
	if (CurrentState == NewState)
	{
		return;
	}

	ECharacterState PrevState = CurrentState;
	CurrentState = NewState;

	// 상태가 변경되었음을 델리게이트를 통해 구독자들에게 알림
	OnStateChanged.Broadcast(PrevState, CurrentState);
}

// Called every frame
void UCharacterStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

