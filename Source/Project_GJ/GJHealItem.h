#pragma once

#include "CoreMinimal.h"
#include "GJItemBase.h"
#include "GJHealItem.generated.h"

// 줍는 즉시 효과가 나는 회복 아이템. 인벤토리를 거치지 않고 바로 HP/MP를 채우고 사라진다.
// 회복량은 데이터 테이블(FItemData)의 HealAmount/ManaRecoverAmount를 그대로 쓴다 -
// 소모품으로 인벤토리에 넣어 쓰는 경로와 같은 값이라 두 벌로 관리되지 않는다.
// 회복방은 이 아이템을 ItemPool에 넣은 DT_RoomSpawn 행 하나로 성립한다(방 클래스는 안 늘어난다).
UCLASS()
class PROJECT_GJ_API AGJHealItem : public AGJItemBase
{
    GENERATED_BODY()

protected:
    virtual void PickUp(AGJCharacter* Picker) override;
};
