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

// 이 캐릭터가 죽었다. 룸이 자기가 스폰한 적의 전멸을 세는 데 쓴다.
// OnDeath(BlueprintImplementableEvent)와 별도로 두는 이유: 그건 C++에서 구독할 수 없다.
class AGJBaseCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDiedSignature, AGJBaseCharacter*, DeadCharacter);

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

    // 받는 데미지 경감에 쓰인다. TakeDamage가 읽으므로 플레이어/적 모두 여기에 둔다.
    // 실제 값은 각자의 데이터 테이블에서 채운다 (플레이어는 UpdateCharacterStat, 적은 ApplyEnemyStat).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float Defense = 0.f;

    // 치명타 확률 (0.0~1.0). 공격할 때 굴린다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float CritMultiplier = 2.f;

    UFUNCTION(BlueprintPure, Category = "Stat")
    bool IsDead() const { return CurrentHP <= 0.f; }

    virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnDamagedSignature OnDamaged;

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnCharacterDiedSignature OnCharacterDied;

protected:
    // 마지막으로 이 캐릭터에게 데미지를 준 컨트롤러. HandleDeath()에는 가해자 정보가 전혀
    // 없어서(인자도 없고 멤버로도 안 남음), TakeDamage에서 사망이 확정되는 순간 기억해 둔다.
    // 적 처치 경험치를 "실제로 죽인 플레이어"에게 주기 위해 필요하다.
    // 약참조인 이유: 적은 죽고 DestroyDelay(기본 2초) 뒤에 파괴되므로, 그 사이에 가해자
    // 컨트롤러가 먼저 사라져도 댕글링 포인터가 되지 않아야 한다.
    UPROPERTY()
    TWeakObjectPtr<AController> LastDamageInstigator;

    // 추후에 피격 이펙트나 사망 연출을 붙일 수 있도록 블루프린트에 노출해 둔 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Stat")
    void OnDeath();

    virtual void HandleDeath();
};
