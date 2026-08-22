#include "GJDevilItem.h"
#include "GJCharacter.h"

bool AGJDevilItem::CanAfford(const AGJCharacter* Buyer) const
{
    return Buyer && (Buyer->GetMaxHP() - MaxHPCost) >= MinMaxHPAfterPurchase;
}

void AGJDevilItem::PayPrice(AGJCharacter* Buyer)
{
    if (!Buyer)
    {
        return;
    }

    // MaxHP를 직접 깎지 않고 스탯 보너스에 음수를 얹는다. 직접 대입하면 다음 레벨업 때
    // RecalculateStats가 BaseStat + StatBonus로 다시 계산하면서 지운다 - 대가를 치렀는데
    // 레벨업 한 번에 돌려받는 꼴이 된다.
    // 최대 체력이 내려갈 때 현재 체력을 같이 내리는 것과 0이 되지 않게 막는 것은
    // RecalculateStats가 이미 한다(리스크 카드에서 같은 문제를 풀었다).
    FStatModifier Cost;
    Cost.Add.MaxHP = -MaxHPCost;
    Buyer->AddStatBonus(Cost);
}

FText AGJDevilItem::GetPriceText() const
{
    return FText::FromString(FString::Printf(TEXT("최대 체력 %.0f"), MaxHPCost));
}

FText AGJDevilItem::GetAffordFailText(const AGJCharacter* Buyer) const
{
    const float CurrentMaxHP = Buyer ? Buyer->GetMaxHP() : 0.f;
    return FText::FromString(FString::Printf(
        TEXT("최대 체력 %.0f - %.0f = %.0f 이(가) 하한 %.0f 미만입니다"),
        CurrentMaxHP, MaxHPCost, CurrentMaxHP - MaxHPCost, MinMaxHPAfterPurchase));
}
