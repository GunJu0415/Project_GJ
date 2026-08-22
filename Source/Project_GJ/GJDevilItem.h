#pragma once

#include "CoreMinimal.h"
#include "GJPricedItem.h"
#include "GJDevilItem.generated.h"

// 최대 체력을 팔아 카드 선택을 사는 아이템 (아이작의 악마방). 보급방은 이 아이템을
// ItemPool에 넣은 DT_RoomSpawn 행 하나로 성립한다.
UCLASS()
class PROJECT_GJ_API AGJDevilItem : public AGJPricedItem
{
    GENERATED_BODY()

protected:
    virtual bool CanAfford(const AGJCharacter* Buyer) const override;
    virtual void PayPrice(AGJCharacter* Buyer) override;
    virtual FText GetPriceText() const override;
    virtual FText GetAffordFailText(const AGJCharacter* Buyer) const override;

    // 가격을 FItemData가 아니라 액터에 두는 이유: 가격은 아이템 종류의 성질이 아니라
    // 이 배치의 성질이다. 같은 아이템이 악마방에선 비싸고 다른 데선 쌀 수 있다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Price")
    float MaxHPCost = 20.f;

    // 지불 후 남아야 하는 최대 체력의 하한. RecalculateStats가 최대 체력을 최소 1로
    // 클램프하긴 하지만, 최대 체력 1은 게임이 성립하지 않는 상태다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Price")
    float MinMaxHPAfterPurchase = 20.f;
};
