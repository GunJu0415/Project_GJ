#include "GJInventoryComponent.h"
#include "GJGameTypes.h"
#include "GJCharacter.h"

UGJInventoryComponent::UGJInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGJInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // 빈 칸을 포함해 항상 MaxSlots 크기로 고정 - 그리드 UI가 배열 인덱스를 그대로 그리드 위치로 씀
    Items.SetNum(MaxSlots);
}

bool UGJInventoryComponent::AddItem(const FDataTableRowHandle& ItemHandle, int32 Quantity)
{
    if (Quantity <= 0 || ItemHandle.IsNull())
    {
        return false;
    }

    FItemData* ItemData = ItemHandle.GetRow<FItemData>(TEXT("AddItem"));
    if (!ItemData)
    {
        return false;
    }

    const FName ItemID = ItemHandle.RowName;
    int32 Remaining = Quantity;

    // 1. 기존 스택 중 여유 공간이 있는 칸부터 채움
    for (FInventorySlot& Slot : Items)
    {
        if (Remaining <= 0)
        {
            break;
        }

        if (Slot.ItemHandle.RowName == ItemID && !Slot.ItemHandle.IsNull() && Slot.Quantity < ItemData->MaxStackSize)
        {
            const int32 SpaceInSlot = ItemData->MaxStackSize - Slot.Quantity;
            const int32 AmountToAdd = FMath::Min(SpaceInSlot, Remaining);
            Slot.Quantity += AmountToAdd;
            Remaining -= AmountToAdd;
        }
    }

    // 2. 그래도 남았으면 빈 칸에 순서대로 채움
    for (FInventorySlot& Slot : Items)
    {
        if (Remaining <= 0)
        {
            break;
        }

        if (Slot.ItemHandle.IsNull())
        {
            const int32 AmountToAdd = FMath::Min(ItemData->MaxStackSize, Remaining);
            Slot.ItemHandle = ItemHandle;
            Slot.Quantity = AmountToAdd;
            Remaining -= AmountToAdd;
        }
    }

    // 뭔가는 실제로 추가됐다면(=Remaining이 원래 요청보다 줄었다면) 정렬 후 알림
    if (Remaining < Quantity)
    {
        SortItems();
        OnInventoryChanged.Broadcast();
    }

    return Remaining <= 0;
}

bool UGJInventoryComponent::RemoveItem(FName ItemID, int32 Quantity)
{
    if (Quantity <= 0 || GetItemCount(ItemID) < Quantity)
    {
        return false;
    }

    int32 Remaining = Quantity;
    for (int32 i = Items.Num() - 1; i >= 0 && Remaining > 0; --i)
    {
        FInventorySlot& Slot = Items[i];
        if (Slot.ItemHandle.RowName != ItemID || Slot.ItemHandle.IsNull())
        {
            continue;
        }

        const int32 AmountToRemove = FMath::Min(Slot.Quantity, Remaining);
        Slot.Quantity -= AmountToRemove;
        Remaining -= AmountToRemove;

        if (Slot.Quantity <= 0)
        {
            // 칸 자체를 배열에서 지우지 않고 빈 칸으로만 되돌림 (그리드 위치가 밀리지 않도록)
            Slot.ItemHandle = FDataTableRowHandle();
            Slot.Quantity = 0;
        }
    }

    OnInventoryChanged.Broadcast();
    return true;
}

int32 UGJInventoryComponent::GetItemCount(FName ItemID) const
{
    int32 Total = 0;
    for (const FInventorySlot& Slot : Items)
    {
        if (!Slot.ItemHandle.IsNull() && Slot.ItemHandle.RowName == ItemID)
        {
            Total += Slot.Quantity;
        }
    }

    return Total;
}

bool UGJInventoryComponent::HasItem(FName ItemID, int32 Quantity) const
{
    return GetItemCount(ItemID) >= Quantity;
}

bool UGJInventoryComponent::SwapSlots(int32 IndexA, int32 IndexB)
{
    if (!Items.IsValidIndex(IndexA) || !Items.IsValidIndex(IndexB) || IndexA == IndexB)
    {
        return false;
    }

    Items.Swap(IndexA, IndexB);
    OnInventoryChanged.Broadcast();
    return true;
}

bool UGJInventoryComponent::UseItem(int32 SlotIndex)
{
    if (!Items.IsValidIndex(SlotIndex))
    {
        return false;
    }

    FInventorySlot& Slot = Items[SlotIndex];
    if (Slot.ItemHandle.IsNull() || Slot.Quantity <= 0)
    {
        return false;
    }

    FItemData* ItemData = Slot.ItemHandle.GetRow<FItemData>(TEXT("UseItem"));
    if (!ItemData || ItemData->ItemType != EItemType::Consumable)
    {
        return false; // 소비형 아이템이 아니면 더블클릭으로 사용할 수 없음
    }

    if (AGJCharacter* OwningCharacter = Cast<AGJCharacter>(GetOwner()))
    {
        OwningCharacter->ApplyConsumableEffect(ItemData->HealAmount, ItemData->ManaRecoverAmount);
    }

    Slot.Quantity--;
    if (Slot.Quantity <= 0)
    {
        // 칸 자체를 배열에서 지우지 않고 빈 칸으로만 되돌림 (그리드 위치가 밀리지 않도록)
        Slot.ItemHandle = FDataTableRowHandle();
        Slot.Quantity = 0;
    }

    OnInventoryChanged.Broadcast();
    return true;
}

void UGJInventoryComponent::SortItems()
{
    Items.Sort([](const FInventorySlot& A, const FInventorySlot& B)
    {
        const bool bAEmpty = A.ItemHandle.IsNull();
        const bool bBEmpty = B.ItemHandle.IsNull();

        if (bAEmpty != bBEmpty)
        {
            return !bAEmpty; // 채워진 칸이 빈 칸보다 앞으로
        }
        if (bAEmpty)
        {
            return false; // 둘 다 비었으면 순서 상관없음
        }

        return A.ItemHandle.RowName.LexicalLess(B.ItemHandle.RowName);
    });
}
