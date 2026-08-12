#include "GJCombatStatics.h"

float UGJCombatStatics::CalculateOutgoingDamage(
    float BaseDamage,
    float AttackPower,
    float CritChance,
    float CritMultiplier,
    bool& bOutWasCritical)
{
    // 공격력은 배율로 들어간다 - 센 무기일수록 성장이 더 크게 돌아와서 무기 선택에 의미가 생긴다
    const float PowerScaledDamage = BaseDamage * (1.f + AttackPower / 100.f);

    // FMath::FRand()는 0.0~1.0을 돌려주므로 CritChance와 직접 비교하면 된다
    bOutWasCritical = (CritChance > 0.f) && (FMath::FRand() < CritChance);

    const float FinalMultiplier = bOutWasCritical ? CritMultiplier : 1.f;

    return PowerScaledDamage * FinalMultiplier;
}

float UGJCombatStatics::ApplyDefense(float IncomingDamage, float Defense)
{
    if (IncomingDamage <= 0.f)
    {
        return 0.f;
    }

    // 음수 방어력이 들어오면 데미지가 증폭되어버리므로 0으로 막는다
    const float SafeDefense = FMath::Max(Defense, 0.f);

    // 체감형 경감: 방어력 100마다 "체력이 1배씩 더 있는" 효과.
    // 분모가 항상 100 이상이라 0으로 나눌 일이 없다.
    const float Mitigated = IncomingDamage * (100.f / (100.f + SafeDefense));

    // 방어력이 극단적으로 높을 때 데미지가 0에 수렴해 사실상 무적이 되는 것을 막는다.
    // 다만 하한을 그냥 1로 두면 0.5짜리 약한 공격이 오히려 1로 늘어나는 역전이 생기므로,
    // 들어온 데미지 자체가 1보다 작으면 그 값을 하한으로 쓴다. 방어력은 어떤 경우에도 데미지를 늘리지 않는다.
    const float MinimumDamage = FMath::Min(1.f, IncomingDamage);

    return FMath::Max(Mitigated, MinimumDamage);
}
