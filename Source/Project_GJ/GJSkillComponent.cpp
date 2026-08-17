#include "GJSkillComponent.h"
#include "GJCharacter.h"
#include "GJProjectile.h"
#include "GJCombatStatics.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

UGJSkillComponent::UGJSkillComponent()
{
    // 차징과 쿨타임을 시각 비교로 계산하므로 매 프레임 할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;

    EquippedSkills.Init(NAME_None, GJ_SKILL_SLOT_COUNT);
    CooldownEndTime.Init(0.f, GJ_SKILL_SLOT_COUNT);
}

void UGJSkillComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwnerCharacter())
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: 소유자가 AGJCharacter가 아닙니다. 스킬이 동작하지 않습니다."));
    }

    // 테이블이 없으면 모든 스킬이 조용히 안 나간다. 시작할 때 한 번만 알려준다 -
    // 발사 시점에 찍으면 우클릭할 때마다 스팸된다.
    if (!SkillTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: SkillTable이 비어 있어 스킬을 쓸 수 없습니다. BP_GJCharacter의 SkillComponent를 확인하세요."));
    }
}

AGJCharacter* UGJSkillComponent::GetOwnerCharacter() const
{
    return Cast<AGJCharacter>(GetOwner());
}

const FSkillData* UGJSkillComponent::FindSkill(FName SkillId) const
{
    if (!SkillTable || SkillId.IsNone())
    {
        return nullptr;
    }

    return SkillTable->FindRow<FSkillData>(SkillId, TEXT("FindSkill"), false);
}

FName UGJSkillComponent::GetSkillInSlot(int32 SlotIndex) const
{
    return EquippedSkills.IsValidIndex(SlotIndex) ? EquippedSkills[SlotIndex] : NAME_None;
}

bool UGJSkillComponent::EquipSkill(FName SkillId)
{
    if (!FindSkill(SkillId))
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipSkill: '%s'가 DT_SkillData에 없습니다."), *SkillId.ToString());
        return true;  // 슬롯 문제가 아니므로 교체 화면을 띄울 이유가 없다
    }

    for (int32 i = 0; i < EquippedSkills.Num(); i++)
    {
        if (EquippedSkills[i].IsNone())
        {
            EquipSkillInSlot(i, SkillId);
            return true;
        }
    }

    // 빈 슬롯이 없다. 호출자가 무엇을 버릴지 물어야 한다.
    return false;
}

void UGJSkillComponent::EquipSkillInSlot(int32 SlotIndex, FName SkillId)
{
    if (!EquippedSkills.IsValidIndex(SlotIndex))
    {
        return;
    }

    // 교체 대상 슬롯이 차징 중이었다면 버린다. 안 그러면 뗐을 때 없어진 스킬이 나간다.
    if (ChargingSlot == SlotIndex)
    {
        CancelCharge();
    }

    EquippedSkills[SlotIndex] = SkillId;

    // 새 스킬을 쿨타임 없이 바로 쓰게 한다. 교체는 손해가 아니어야 한다.
    CooldownEndTime[SlotIndex] = 0.f;

    UE_LOG(LogTemp, Log, TEXT("EquipSkillInSlot: 슬롯 %d <- %s"), SlotIndex, *SkillId.ToString());

    OnSkillSlotsChanged.Broadcast();
}

void UGJSkillComponent::CancelCharge()
{
    ChargingSlot = INDEX_NONE;
    ChargeStartTime = 0.f;
}

void UGJSkillComponent::OnSkillPressed(int32 SlotIndex)
{
    if (!EquippedSkills.IsValidIndex(SlotIndex))
    {
        return;
    }

    // 이미 다른 슬롯을 차징 중이면 무시한다. 두 개를 동시에 차징하면
    // 뗄 때 어느 쪽인지 판정이 갈리고, 그 조작을 설명할 방법도 없다.
    if (ChargingSlot != INDEX_NONE)
    {
        return;
    }

    const FSkillData* Skill = FindSkill(EquippedSkills[SlotIndex]);
    if (!Skill)
    {
        // 빈 슬롯이다. 로그를 찍으면 클릭할 때마다 스팸된다.
        return;
    }

    UWorld* World = GetWorld();
    AGJCharacter* Character = GetOwnerCharacter();
    if (!World || !Character)
    {
        return;
    }

    if (World->GetTimeSeconds() < CooldownEndTime[SlotIndex])
    {
        return;
    }

    if (Character->GetCurrentMP() < Skill->MPCost)
    {
        return;
    }

    // 차징이 없는 스킬은 누르는 순간 나간다. 뗄 때까지 기다리면 차징도 없는데
    // 손을 떼야 발사되는 이상한 감각이 된다.
    if (Skill->ChargeTime <= 0.f)
    {
        FireSkill(SlotIndex, *Skill, 0.f);
        return;
    }

    ChargingSlot = SlotIndex;
    ChargeStartTime = World->GetTimeSeconds();
}

void UGJSkillComponent::OnSkillReleased(int32 SlotIndex)
{
    if (ChargingSlot != SlotIndex)
    {
        return;
    }

    const FSkillData* Skill = FindSkill(EquippedSkills[SlotIndex]);
    UWorld* World = GetWorld();

    // 어떤 경로로 끝나든 차징 상태는 반드시 푼다.
    const float StartTime = ChargeStartTime;
    CancelCharge();

    if (!Skill || !World)
    {
        return;
    }

    const float Elapsed = World->GetTimeSeconds() - StartTime;
    const float Ratio = FMath::Clamp(Elapsed / Skill->ChargeTime, 0.f, 1.f);

    FireSkill(SlotIndex, *Skill, Ratio);
}

AGJProjectile* UGJSkillComponent::GetPooledProjectile(TSubclassOf<AGJProjectile> ProjClass)
{
    if (!ProjClass)
    {
        ProjClass = DefaultProjectileClass;
    }
    if (!ProjClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: DefaultProjectileClass가 비어 있어 구체를 만들 수 없습니다."));
        return nullptr;
    }

    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();
    if (!OwnerActor || !World)
    {
        return nullptr;
    }

    FGJProjectilePool& Pool = ProjectilePools.FindOrAdd(ProjClass);

    for (AGJProjectile* Existing : Pool.Projectiles)
    {
        if (Existing && !Existing->IsActive())
        {
            return Existing;
        }
    }

    if (Pool.Projectiles.Num() >= PoolSizePerClass)
    {
        return nullptr;
    }

    // 미리 다 만들지 않고 필요할 때 하나씩 늘린다. 스킬을 안 쓰는 플레이에서는
    // 구체가 하나도 안 만들어진다.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwnerActor;
    SpawnParams.Instigator = Cast<APawn>(OwnerActor);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGJProjectile* NewProjectile = World->SpawnActor<AGJProjectile>(
        ProjClass, OwnerActor->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

    if (NewProjectile)
    {
        Pool.Projectiles.Add(NewProjectile);
    }

    return NewProjectile;
}

void UGJSkillComponent::FireSkill(int32 SlotIndex, const FSkillData& Skill, float ChargeRatio)
{
    AGJCharacter* Character = GetOwnerCharacter();
    UWorld* World = GetWorld();
    if (!Character || !World)
    {
        return;
    }

    if (Skill.SkillType != ESkillType::Projectile)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("FireSkill: 지속형 스킬 '%s'는 아직 미구현입니다."), *EquippedSkills[SlotIndex].ToString());
        return;
    }

    // 구체를 먼저 확보한다. MP를 먼저 깎으면 풀이 비었을 때 MP만 사라진다.
    AGJProjectile* Projectile = GetPooledProjectile(Skill.ProjectileClass);
    if (!Projectile)
    {
        // 조용히 넘기면 "가끔 스킬이 안 나간다"로 보이고 원인을 못 찾는다.
        UE_LOG(LogTemp, Warning, TEXT("FireSkill: 구체를 얻지 못했습니다 (풀 소진 또는 스폰 실패)."));
        return;
    }

    // 차징 중에 MP가 빠졌을 수 있으므로 여기서 다시 확인한다.
    if (!Character->ConsumeMP(Skill.MPCost))
    {
        return;
    }

    const float Multiplier = 1.f + (Skill.MaxChargeMultiplier - 1.f) * ChargeRatio;

    const FVector Forward = Character->GetActorForwardVector();
    const FVector SpawnLocation = Character->GetActorLocation()
        + Forward * MuzzleOffset.X
        + Character->GetActorRightVector() * MuzzleOffset.Y
        + FVector::UpVector * MuzzleOffset.Z;

    Projectile->SetActorLocationAndRotation(SpawnLocation, Forward.Rotation());

    // 풀 구체는 스폰 시점의 인스티게이터로 굳으므로 발사할 때마다 갱신한다.
    // 안 하면 적 처치 경험치를 줄 대상을 못 찾고 자기 피격 방지도 안 먹는다.
    Projectile->SetInstigator(Character);

    bool bWasCritical = false;
    const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
        Skill.BaseDamage * Multiplier,
        Character->GetSkillPower(),
        Character->CritChance,
        Character->CritMultiplier,
        bWasCritical);

    Projectile->FireInDirection(
        Forward, OutgoingDamage, Skill.ProjectileSpeed, Skill.Range,
        Skill.BaseScale * Multiplier, Skill.PierceCount);

    CooldownEndTime[SlotIndex] = World->GetTimeSeconds() + Skill.Cooldown;

    UE_LOG(LogTemp, Log,
        TEXT("FireSkill: 슬롯 %d '%s' 차징 %.0f%% -> 배율 x%.2f, 데미지 %.1f%s, 크기 x%.2f, MP -%.0f, 쿨 %.1fs"),
        SlotIndex, *EquippedSkills[SlotIndex].ToString(), ChargeRatio * 100.f, Multiplier,
        OutgoingDamage, bWasCritical ? TEXT(" (치명타)") : TEXT(""),
        Skill.BaseScale * Multiplier, Skill.MPCost, Skill.Cooldown);
}

FText UGJSkillComponent::GetSlotKeyLabel(int32 SlotIndex)
{
    // IMC_GJ의 IA_Skill1/2/3 매핑과 일치해야 한다. 매핑을 바꾸면 여기도 바꾼다.
    switch (SlotIndex)
    {
    case 0:  return NSLOCTEXT("GJ", "SkillKey0", "우클릭");
    case 1:  return NSLOCTEXT("GJ", "SkillKey1", "Q");
    case 2:  return NSLOCTEXT("GJ", "SkillKey2", "F");
    default: return FText::GetEmpty();
    }
}

float UGJSkillComponent::GetCooldownRatio(int32 SlotIndex) const
{
    if (!CooldownEndTime.IsValidIndex(SlotIndex))
    {
        return 0.f;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return 0.f;
    }

    const FSkillData* Skill = FindSkill(GetSkillInSlot(SlotIndex));
    if (!Skill || Skill->Cooldown <= 0.f)
    {
        return 0.f;
    }

    const float Remaining = CooldownEndTime[SlotIndex] - World->GetTimeSeconds();
    return FMath::Clamp(Remaining / Skill->Cooldown, 0.f, 1.f);
}

void UGJSkillComponent::SwapSkillSlots(int32 SlotA, int32 SlotB)
{
    if (!EquippedSkills.IsValidIndex(SlotA) || !EquippedSkills.IsValidIndex(SlotB) || SlotA == SlotB)
    {
        return;
    }

    // 차징 중인 슬롯이 섞이면 손을 뗐을 때 의도하지 않은 스킬이 나간다.
    if (ChargingSlot == SlotA || ChargingSlot == SlotB)
    {
        CancelCharge();
    }

    EquippedSkills.Swap(SlotA, SlotB);

    // 쿨타임도 같이 옮긴다. 안 그러면 스킬을 쓰고 자리를 바꾸는 것이
    // 쿨타임 초기화 수단이 된다.
    CooldownEndTime.Swap(SlotA, SlotB);

    OnSkillSlotsChanged.Broadcast();
}

void UGJSkillComponent::LogSkillInfo() const
{
    AGJCharacter* Character = GetOwnerCharacter();
    UWorld* World = GetWorld();
    if (!Character || !World)
    {
        return;
    }

    const float Now = World->GetTimeSeconds();

    UE_LOG(LogTemp, Log, TEXT("=== 스킬 상태 (MP %.0f, 스킬공격력 %.1f) ==="),
        Character->GetCurrentMP(), Character->GetSkillPower());

    for (int32 i = 0; i < EquippedSkills.Num(); i++)
    {
        const float Remaining = FMath::Max(CooldownEndTime[i] - Now, 0.f);
        UE_LOG(LogTemp, Log, TEXT("  슬롯 %d (%s): %s / 쿨 %.1fs%s"),
            i, *GetSlotKeyLabel(i).ToString(),
            EquippedSkills[i].IsNone() ? TEXT("(비어 있음)") : *EquippedSkills[i].ToString(),
            Remaining,
            (ChargingSlot == i) ? TEXT(" / 차징 중") : TEXT(""));
    }
}
