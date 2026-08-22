#include "GJPricedItem.h"
#include "GJCharacter.h"
#include "GJCardComponent.h"

void AGJPricedItem::PickUp(AGJCharacter* Picker)
{
    if (!Picker)
    {
        return;
    }

    // 못 낼 때 조용히 넘어가지 않는다. 아무 반응 없이 아이템이 그대로 있으면
    // 플레이어는 상호작용이 고장 난 줄 안다.
    if (!CanAfford(Picker))
    {
        UE_LOG(LogTemp, Log, TEXT("[PRICE] %s: 지불 불가 - %s"),
            *GetName(), *GetAffordFailText(Picker).ToString());
        return;
    }

    // 지불보다 먼저 지급한다. 순서가 반대면 보상 지급이 실패했을 때 대가만 날린다.
    if (!GrantReward(Picker))
    {
        UE_LOG(LogTemp, Warning, TEXT("[PRICE] %s: 보상을 지급하지 못해 대가를 청구하지 않습니다."), *GetName());
        return;
    }

    PayPrice(Picker);

    UE_LOG(LogTemp, Log, TEXT("[PRICE] %s 구매 완료 (대가 %s)"),
        *GetName(), *GetPriceText().ToString());

    Destroy();
}

bool AGJPricedItem::CanAfford(const AGJCharacter* Buyer) const
{
    return Buyer != nullptr;
}

void AGJPricedItem::PayPrice(AGJCharacter* Buyer)
{
    // 베이스는 무료다. 지불 수단은 서브클래스가 정한다.
}

bool AGJPricedItem::GrantReward(AGJCharacter* Buyer)
{
    UGJCardComponent* Cards = Buyer ? Buyer->GetCardComponent() : nullptr;
    if (!Cards)
    {
        return false;
    }

    // 화면이 이미 떠 있어도 대기열에 쌓이므로, 레벨업과 겹쳐도 하나가 사라지지 않는다.
    Cards->GrantCardChoice();
    return true;
}

FText AGJPricedItem::GetPriceText() const
{
    return FText::FromString(TEXT("무료"));
}

FText AGJPricedItem::GetAffordFailText(const AGJCharacter* Buyer) const
{
    return FText::Format(FText::FromString(TEXT("대가 {0}을(를) 낼 수 없습니다")), GetPriceText());
}
