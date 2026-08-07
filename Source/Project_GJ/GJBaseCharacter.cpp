#include "GJBaseCharacter.h"
#include "CharacterStateComponent.h"
#include "MotionWarpingComponent.h"
#include "AbilitySystemComponent.h" // GAS 시스템용 (선택)
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGJBaseCharacter::AGJBaseCharacter()
{
    // 캐릭터가 매 프레임 업데이트(Tick)를 사용하도록 설정
    PrimaryActorTick.bCanEverTick = true;

    // 1. 공통 상태 관리 컴포넌트 생성
    StateComponent = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("StateComponent"));

    // 2. 공통 모션 워핑 컴포넌트 생성
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
}

void AGJBaseCharacter::BeginPlay()
{
    Super::BeginPlay();

    CurrentHP = MaxHP;
}

void AGJBaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

// 기존 GJCharacter에 있던 GAS 인터페이스 이동
UAbilitySystemComponent* AGJBaseCharacter::GetAbilitySystemComponent() const
{
    // 베이스 캐릭터 단계에서는 아직 컴포넌트가 생성되지 않았으므로 nullptr 반환.
    // 플레이어(GJCharacter)나 적(GJEnemyCharacter) 클래스에서
    // 실제 컴포넌트를 생성하고 오버라이드하여 반환하도록 설계합니다.
    return nullptr;
}

float AGJBaseCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
    float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

    if (IsDead() || ActualDamage <= 0.f)
    {
        return ActualDamage;
    }

    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.f, MaxHP);
    OnDamaged.Broadcast(ActualDamage, DamageCauser);

    // 피격 확인용 디버그 출력 (데이터테이블/충돌 세팅 확인 끝나면 지워도 됨)
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("Damaged!!!!"));
    }

    if (CurrentHP <= 0.f)
    {
        HandleDeath();
    }

    return ActualDamage;
}

void AGJBaseCharacter::HandleDeath()
{
    if (StateComponent)
    {
        StateComponent->SetState(ECharacterState::Dead);
    }

    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 사망 후에는 총알 등 어떤 콜리전 판정도 받지 않도록 메시 콜리전도 함께 꺼줌
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    OnDeath();
}
