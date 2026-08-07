#include "GJItem.h"
#include "GJCharacter.h"
#include "GJInventoryComponent.h"

void AGJItem::PickUp(AGJCharacter* Picker)
{
    if (!Picker)
    {
        return;
    }

    UGJInventoryComponent* Inventory = Picker->GetInventoryComponent();
    if (!Inventory)
    {
        return;
    }

    // 인벤토리에 요청한 개수를 전부 넣을 수 있었을 때만 필드에서 사라짐 (칸이 부족하면 남겨둠)
    if (Inventory->AddItem(ItemDataHandle, Quantity))
    {
        Destroy();
    }
}
