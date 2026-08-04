#include "GJBaseCharacter.h"
#include "CharacterStateComponent.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h" // GAS 시스템용 (선택)

AGJBaseCharacter::AGJBaseCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 공통 상태 관리 컴포넌트 생성 (꺾쇠 안에 타입을 꼭 적어주세요!)
    StateComponent = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("StateComponent"));

    // 2. 공통 모션 워핑 컴포넌트 생성 (마찬가지로 타입을 명시해야 합니다)
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void AGJBaseCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void AGJBaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 기존 GJCharacter에 있던 GAS 인터페이스 이동
UAbilitySystemComponent* AGJBaseCharacter::GetAbilitySystemComponent() const
{
    return nullptr;
}