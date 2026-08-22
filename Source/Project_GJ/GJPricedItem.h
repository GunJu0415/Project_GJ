#pragma once

#include "CoreMinimal.h"
#include "GJItemBase.h"
#include "GJPricedItem.generated.h"

// 대가를 치러야 보상을 주는 아이템의 베이스. 지불 수단이 무엇이냐만 서브클래스가 정하고,
// "낼 수 있나 -> 보상 -> 지불 -> 사라짐" 흐름은 여기서 한 번만 짠다.
// 나중에 화폐가 생기면 AGJShopItem이 이 자리에 그대로 들어와 CanAfford/PayPrice만 다르게 구현한다.
UCLASS(Abstract)
class PROJECT_GJ_API AGJPricedItem : public AGJItemBase
{
    GENERATED_BODY()

protected:
    virtual void PickUp(AGJCharacter* Picker) override;

    // 지금 이 캐릭터가 대가를 낼 수 있는가. 베이스는 항상 true (지불 수단이 없으니 막을 것도 없다).
    virtual bool CanAfford(const AGJCharacter* Buyer) const;

    // 대가를 실제로 차감한다. CanAfford가 true였을 때만 불린다.
    virtual void PayPrice(AGJCharacter* Buyer);

    // 보상을 지급한다. 기본 구현은 카드 선택 한 번.
    // false를 돌려주면 지불도 하지 않고 아이템도 남는다 - 돈만 받고 못 주는 상황을 막는다.
    virtual bool GrantReward(AGJCharacter* Buyer);

    // 로그와 (나중에) 가격표 UI에 쓸 설명. "최대 체력 20" 같은 형태.
    virtual FText GetPriceText() const;

    // 왜 못 내는지. 가격표는 지불 수단만 말하지 못 내는 이유는 말하지 못한다 -
    // 모자란 게 무엇인지는 지불 수단을 아는 쪽만 말할 수 있다.
    virtual FText GetAffordFailText(const AGJCharacter* Buyer) const;
};
