#pragma once

#include "CoreMinimal.h"
#include "GJBaseCharacter.h" // 부모 클래스 포함
#include "GJGameTypes.h"
#include "GJEnemyCharacter.generated.h"

class APawn;
class UDataTable;
class UWidgetComponent;

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

    // 공격 판정(데미지 적용)까지의 선딜레이(초) - 몽타주의 실제 타격 프레임 타이밍에 맞춰 조절
    // (EnemyDataHandle을 할당하면 FEnemyStat.AttackWindup 값으로 덮어써짐)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackWindup = 0.3f;

    // 이 적을 죽인 플레이어에게 주는 경험치
    // (EnemyDataHandle을 할당하면 FEnemyStat.ExpReward 값으로 덮어써짐)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float ExpReward = 10.f;

    // 선딜레이가 끝난 시점에 실제로 데미지를 적용함 (플레이어가 그새 사거리 밖으로 나갔으면 헛스윙 처리)
    void ApplyAttackDamage();

    FTimerHandle AttackWindupTimerHandle;
    TWeakObjectPtr<APawn> PendingAttackTarget;

    float LastAttackTime = -100.f;

    // --------------------------------------------------
    // 사망 처리
    // --------------------------------------------------
    virtual void HandleDeath() override;

    // 사망 시 재생할 몽타주. 전용 데스 애니메이션이 아직 없어서 생성자에서 MM_Rifle_Fire_Montage를 임시로 기본값에 채워둠
    // (BP 디테일 패널에서 다른 애님으로 언제든 교체 가능 - 나중에 진짜 데스 애님이 생기면 이 필드만 바꾸면 됨)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI")
    UAnimMontage* DeathMontage;

    // 사망 후 액터를 실제로 없애기까지의 지연 시간(초) - 데스 애니메이션이 재생될 여유를 위해 기본 2초로 둠
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float DestroyDelay = 2.f;

    void DestroySelf();

    // --------------------------------------------------
    // 머리 위 체력바 (월드에 떠 있는 위젯 - 항상 화면을 바라봄)
    // --------------------------------------------------

    // BP 디테일 패널의 Components 목록에서 이 컴포넌트를 선택한 뒤 "Widget Class"에
    // UGJHealthBarWidget을 상속한 위젯 블루프린트(예: WBP_EnemyHealthBar)를 지정해야 표시됨
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UWidgetComponent* HealthBarWidgetComponent;

    // OnDamaged 델리게이트에 바인딩되는 핸들러 - 체력바 위젯을 최신 HP로 갱신함
    UFUNCTION()
    void OnHealthChanged(float DamageAmount, AActor* DamageCauser);

    void UpdateHealthBarWidget();
};
