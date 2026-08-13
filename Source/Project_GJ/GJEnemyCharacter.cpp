#include "GJEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "AI/GJEnemyAIController.h"
#include "TimerManager.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "GJCombatStatics.h"
#include "Components/WidgetComponent.h"
#include "GJHealthBarWidget.h"
#include "GJCharacter.h"

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

    // 5. 전용 데스 애니메이션이 아직 없어서, 이미 공격에 쓰고 있는 MM_Rifle_Fire_Montage를 임시로 재활용함
    // (BP 디테일 패널에서 DeathMontage 값을 바꾸면 언제든 다른 애님으로 교체 가능)
    static ConstructorHelpers::FObjectFinder<UAnimMontage> DeathMontageFinder(TEXT("/Game/GJ/Animation/MM_Rifle_Fire_Montage.MM_Rifle_Fire_Montage"));
    if (DeathMontageFinder.Succeeded())
    {
        DeathMontage = DeathMontageFinder.Object;
    }

    // 6. 머리 위 체력바 위젯 컴포넌트 - 항상 화면을 바라보는 Screen 스페이스라 탑다운 카메라 각도와 무관하게 잘 보임
    HealthBarWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBarWidgetComponent"));
    HealthBarWidgetComponent->SetupAttachment(GetRootComponent());
    HealthBarWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
    HealthBarWidgetComponent->SetDrawSize(FVector2D(120.f, 16.f));
    HealthBarWidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 110.f));
}

void AGJEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();

    ApplyEnemyStat();

    // 체력바 위젯을 데이터 테이블로 세팅된 최종 MaxHP 기준으로 초기화하고, 이후 피격마다 갱신되도록 바인딩
    OnDamaged.AddDynamic(this, &AGJEnemyCharacter::OnHealthChanged);
    UpdateHealthBarWidget();
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
    AttackWindup = RowData->AttackWindup;
    GetCharacterMovement()->MaxWalkSpeed = RowData->MoveSpeed;

    // 전투 스탯 - TakeDamage(방어력)와 ApplyAttackDamage(치명타)가 읽는다
    Defense = RowData->Defense;
    CritChance = RowData->CritChance;
    CritMultiplier = RowData->CritMultiplier;

    // HandleDeath에서 다시 데이터 테이블을 조회하지 않도록 여기서 멤버에 복사해 둔다
    // (다른 스탯들과 동일한 패턴)
    ExpReward = RowData->ExpReward;
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
    PendingAttackTarget = TargetPlayer;

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

    // 공격을 결정한 즉시 데미지를 넣지 않고, 몽타주의 타격 타이밍에 맞춘 선딜레이(AttackWindup) 후에 판정함
    GetWorldTimerManager().SetTimer(AttackWindupTimerHandle, this, &AGJEnemyCharacter::ApplyAttackDamage, AttackWindup, false);
}

void AGJEnemyCharacter::ApplyAttackDamage()
{
    if (IsDead())
    {
        return;
    }

    APawn* TargetPlayer = PendingAttackTarget.Get();
    if (!TargetPlayer)
    {
        return;
    }

    // 선딜레이 동안 플레이어가 사거리 밖으로 빠져나갔으면 헛스윙 처리 (약간의 여유값 포함)
    const float EffectiveRange = AttackRange + 50.f;
    const float DistSq = FVector::DistSquared(GetActorLocation(), TargetPlayer->GetActorLocation());
    if (DistSq > FMath::Square(EffectiveRange))
    {
        return;
    }

    // 적은 공격력 배율을 쓰지 않는다 - AttackDamage가 이미 최종 공격력이라 AttackPower에 0을 넘긴다.
    // 치명타는 적도 굴린다.
    bool bWasCritical = false;
    const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
        AttackDamage, 0.f, CritChance, CritMultiplier, bWasCritical);

    UGameplayStatics::ApplyDamage(TargetPlayer, OutgoingDamage, GetController(), this, UDamageType::StaticClass());
}

void AGJEnemyCharacter::HandleDeath()
{
    Super::HandleDeath();

    // 죽인 주체에게 경험치를 준다. TakeDamage가 사망 직전에 기억해 둔 가해자 컨트롤러를 쓴다.
    // 캐스팅이 실패하는 경우(적이 적을 죽임, 환경 사망, 컨트롤러가 이미 파괴됨)에는 아무에게도
    // 주지 않는다 - 여기서는 "받을 사람이 없다"가 정답이지 오류가 아니다.
    if (AController* KillerController = LastDamageInstigator.Get())
    {
        if (AGJCharacter* KillerCharacter = Cast<AGJCharacter>(KillerController->GetPawn()))
        {
            KillerCharacter->AddEXP(ExpReward);
        }
    }

    // 죽는 순간 대기 중이던 공격 판정이 있다면 취소
    GetWorldTimerManager().ClearTimer(AttackWindupTimerHandle);

    // 죽은 뒤에는 더 이상 추적/공격 판단을 할 필요가 없으므로 비헤이비어 트리 로직과 이동을 정지시킴
    if (AAIController* AICon = Cast<AAIController>(GetController()))
    {
        if (UBrainComponent* Brain = AICon->GetBrainComponent())
        {
            Brain->StopLogic(TEXT("Dead"));
        }
        AICon->StopMovement();
    }

    // 전용 데스 애니메이션이 없어서 임시로 DeathMontage(기본값: MM_Rifle_Fire_Montage 재활용)를 재생
    if (DeathMontage)
    {
        PlayAnimMontage(DeathMontage);
    }

    // 죽었으니 체력바는 더 이상 표시하지 않음
    if (HealthBarWidgetComponent)
    {
        HealthBarWidgetComponent->SetVisibility(false);
    }

    FTimerHandle DestroyTimerHandle;
    GetWorldTimerManager().SetTimer(DestroyTimerHandle, this, &AGJEnemyCharacter::DestroySelf, DestroyDelay, false);
}

void AGJEnemyCharacter::DestroySelf()
{
    Destroy();
}

void AGJEnemyCharacter::OnHealthChanged(float DamageAmount, AActor* DamageCauser)
{
    UpdateHealthBarWidget();
}

void AGJEnemyCharacter::UpdateHealthBarWidget()
{
    if (!HealthBarWidgetComponent)
    {
        return;
    }

    if (UGJHealthBarWidget* HealthBarWidget = Cast<UGJHealthBarWidget>(HealthBarWidgetComponent->GetUserWidgetObject()))
    {
        HealthBarWidget->UpdateHealth(CurrentHP, MaxHP);
    }
}
