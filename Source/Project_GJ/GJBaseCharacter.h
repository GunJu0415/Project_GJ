#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h" // GAS 인터페이스 상속용
#include "GJBaseCharacter.generated.h"

// 컴파일 속도 향상 및 순환 참조 방지를 위한 전방 선언
class UCharacterStateComponent;
class UMotionWarpingComponent;
class UAbilitySystemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamagedSignature, float, DamageAmount, AActor*, DamageCauser);

// 베이스 클래스이므로 레벨에 직접 스폰되지 않도록 Abstract 키워드를 추가하는 것이 좋습니다.
UCLASS(Abstract)
class PROJECT_GJ_API AGJBaseCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AGJBaseCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // --------------------------------------------------
    // Components
    // --------------------------------------------------

    // 상태 관리 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCharacterStateComponent* StateComponent;

    // 모션 워핑 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UMotionWarpingComponent* MotionWarpingComponent;

    // --------------------------------------------------
    // GAS (Gameplay Ability System) 인터페이스 구현
    // --------------------------------------------------
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    // --------------------------------------------------
    // Health / Damage
    // --------------------------------------------------
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 100.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float CurrentHP = 100.f;

    UFUNCTION(BlueprintPure, Category = "Stat")
    bool IsDead() const { return CurrentHP <= 0.f; }

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnDamagedSignature OnDamaged;

protected:
    // 추후에 피격 이펙트나 사망 연출을 붙일 수 있도록 블루프린트에 노출해 둔 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Stat")
    void OnDeath();

    virtual void HandleDeath();
};
