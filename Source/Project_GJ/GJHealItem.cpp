#include "GJHealItem.h"
#include "GJCharacter.h"

void AGJHealItem::PickUp(AGJCharacter* Picker)
{
    if (!Picker)
    {
        return;
    }

    // 회복량이 둘 다 0이면 주워도 아무 일이 없다. 조용히 사라지면 데이터 미입력을
    // 플레이 중에 알아챌 방법이 없으므로 경고만 남기고 습득 자체는 진행한다.
    if (ItemStat.HealAmount <= 0.f && ItemStat.ManaRecoverAmount <= 0.f)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ITEM] %s: 회복량이 0입니다 (행 '%s'의 HealAmount/ManaRecoverAmount 확인)"),
            *GetName(), *ItemDataHandle.RowName.ToString());
    }

    Picker->ApplyConsumableEffect(ItemStat.HealAmount, ItemStat.ManaRecoverAmount);

    UE_LOG(LogTemp, Log, TEXT("[ITEM] %s 습득 - HP +%.0f, MP +%.0f"),
        *GetName(), ItemStat.HealAmount, ItemStat.ManaRecoverAmount);

    Destroy();
}
