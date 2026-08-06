#pragma once

#include "CoreMinimal.h"
#include "GJBaseCharacter.h" // 부모 클래스 포함
#include "GJGameTypes.h"
#include "GJEnemyCharacter.generated.h"

class APawn;
class UDataTable;

UCLASS()
class PROJECT_GJ_API AGJEnemyCharacter : public AGJBaseCharacter
{
    GENERATED_BODY()

public:
    AGJEnemyCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

    // --------------------------------------------------
    // 기본 잡몹 스탯 (실제 추적/공격 판단은 비헤이비어 트리(BTService_UpdateTarget, BTTask_MeleeAttack)에서 담당함.
    // 여기서는 스탯 값과, BT 태스크가 호출할 공격 실행 함수만 제공함)
    // --------------------------------------------------

    UFUNCTION(BlueprintPure, Category = "AI")
    float GetDetectionRange() const { return DetectionRange; }

    UFUNCTION(BlueprintPure, Category = "AI")
    float GetAttackRange() const { return AttackRange; }

    // BTTask_MeleeAttack에서 호출함 - 쿨다운 체크 후 플레이어를 바라보고 데미지를 적용함
    UFUNCTION(BlueprintCallable, Category = "AI")
    void PerformAttack();

protected:
    // 잡몹 스탯 데이터 테이블 (할당하면 아래 개별 값들을 덮어씀 - 종류별로 여러 행을 만들어 재사용 가능)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stat")
    FDataTableRowHandle EnemyDataHandle;

    void ApplyEnemyStat();

    // 이 거리 안에 플레이어가 들어오면 추적을 시작함 (BTService_UpdateTarget이 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float DetectionRange = 800.f;

    // 이 거리 안에 들어오면 추적을 멈추고 공격함 (BTService_UpdateTarget이 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackRange = 150.f;

    // 공격 1회당 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackDamage = 10.f;

    // 공격 사이 최소 간격(초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackCooldown = 1.5f;

    // 공격 시 재생할 몽타주 (비워두면 애니메이션 없이 데미지만 적용됨)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    UAnimMontage* AttackMontage;

    float LastAttackTime = -100.f;

    // --------------------------------------------------
    // 사망 처리
    // --------------------------------------------------
    virtual void HandleDeath() override;

    // 사망 후 액터를 실제로 없애기까지의 지연 시간(초) - 나중에 사망 애니메이션을 붙일 여유를 위해 기본 2초로 둠
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float DestroyDelay = 2.f;

    void DestroySelf();
};
