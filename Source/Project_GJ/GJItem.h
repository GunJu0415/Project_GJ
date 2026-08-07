#pragma once

#include "CoreMinimal.h"
#include "GJItemBase.h"
#include "GJItem.generated.h"

// 데이터 테이블 기반 범용 아이템. 상호작용 범위 안에서 상호작용 키를 누르면 인벤토리에 들어가고 자신은 사라짐
// (인벤토리가 꽉 차서 요청한 개수를 다 못 넣으면 필드에 그대로 남아있음).
// BP_Item_XXX 형태로 블루프린트화해서 메시/ItemDataHandle/Quantity만 바꿔가며 여러 아이템을 만들면 됨.
UCLASS()
class PROJECT_GJ_API AGJItem : public AGJItemBase
{
    GENERATED_BODY()

protected:
    virtual void PickUp(AGJCharacter* Picker) override;
};
