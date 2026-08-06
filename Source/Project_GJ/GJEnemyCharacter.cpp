#include "GJEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "AI/GJEnemyAIController.h"
#include "TimerManager.h"

AGJEnemyCharacter::AGJEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 맵에 배치되거나 스폰될 때 자동으로 AI Controller가 빙의하도록 설정
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AIControllerClass = AGJEnemyAIController::StaticClass();

    // 2. 샌드백 역할이므로 플레이어나 무기와 충돌할 수 있도록 캡슐 콜리전 설정
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

    // 3. 적 캐릭터가 플레이어와 밀리지 않도록 무게나 회전 세팅 (선택 사항)
    GetCharacterMovement()->bOrientRotationToMovement = true;

    // 4. 기본 추적 속도 (엔진 기본값 600은 잡몹치고 너무 빠름 - EnemyDataHandle을 할당하면 덮어씀)
    GetCharacterMovement()->MaxWalkSpeed = 300.f;
}

void AGJEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    ApplyEnemyStat();
}

void AGJEnemyCharacter::ApplyEnemyStat()
{
    if (EnemyDataHandle.IsNull())
    {
        return;
    }

    FEnemyStat* RowData = EnemyDataHandle.GetRow<FEnemyStat>(TEXT("Enemy Initialization"));
    if (!RowData)
    {
        return;
    }

    MaxHP = RowData->MaxHP;
    CurrentHP = MaxHP;
    AttackDamage = RowData->AttackDamage;
    AttackRange = RowData->AttackRange;
    DetectionRange = RowData->DetectionRange;
    AttackCooldown = RowData->AttackCooldown;
    GetCharacterMovement()->MaxWalkSpeed = RowData->MoveSpeed;
}

void AGJEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AGJEnemyCharacter::PerformAttack()
{
    if (IsDead())
    {
        return;
    }

    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastAttackTime < AttackCooldown)
    {
        return;
    }

    APawn* TargetPlayer = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!TargetPlayer)
    {
        return;
    }

    LastAttackTime = CurrentTime;

    // 공격 대상을 바라보도록 회전
    FVector LookDirection = TargetPlayer->GetActorLocation() - GetActorLocation();
    LookDirection.Z = 0.f;
    if (!LookDirection.IsNearlyZero())
    {
        SetActorRotation(LookDirection.Rotation());
    }

    if (AttackMontage)
    {
        PlayAnimMontage(AttackMontage);
    }

    // 노티파이 없이 공격이 실행된 시점 기준으로 즉시 데미지 적용 (기본 잡몹용 단순 처리)
    UGameplayStatics::ApplyDamage(TargetPlayer, AttackDamage, GetController(), this, UDamageType::StaticClass());
}

void AGJEnemyCharacter::HandleDeath()
{
    Super::HandleDeath();

    FTimerHandle DestroyTimerHandle;
    GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &AGJEnemyCharacter::DestroySelf, DestroyDelay, false);
}

void AGJEnemyCharacter::DestroySelf()
{
    Destroy();
}
