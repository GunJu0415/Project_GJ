#include "GJInventorySlotWidget.h"
#include "GJInventoryComponent.h"
#include "GJGameTypes.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGJInventorySlotWidget::SetSlotData(int32 InSlotIndex, UGJInventoryComponent* InInventory)
{
    SlotIndex = InSlotIndex;
    OwningInventory = InInventory;

    if (!IconImage || !OwningInventory || !OwningInventory->Items.IsValidIndex(SlotIndex))
    {
        return;
    }

    const FInventorySlot& SlotData = OwningInventory->Items[SlotIndex];

    if (SlotData.ItemHandle.IsNull())
    {
        IconImage->SetVisibility(ESlateVisibility::Hidden);
        if (QuantityText)
        {
            QuantityText->SetVisibility(ESlateVisibility::Hidden);
        }
        return;
    }

    if (FItemData* ItemData = SlotData.ItemHandle.GetRow<FItemData>(TEXT("InventorySlotWidget")))
    {
        if (ItemData->Icon)
        {
            IconImage->SetBrushFromTexture(ItemData->Icon);
        }
        IconImage->SetVisibility(ESlateVisibility::Visible);
    }

    if (QuantityText)
    {
        // 아이템이 있는 칸이면 개수를 항상 표시 (1개여도 표시)
        QuantityText->SetVisibility(ESlateVisibility::Visible);
        QuantityText->SetText(FText::AsNumber(SlotData.Quantity));
    }
}

FReply UGJInventorySlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    const bool bHasItem = OwningInventory && OwningInventory->Items.IsValidIndex(SlotIndex) &&
        !OwningInventory->Items[SlotIndex].ItemHandle.IsNull();

    if (bHasItem && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }

    return FReply::Unhandled();
}

FReply UGJInventorySlotWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OwningInventory)
    {
        // 소비형 아이템이 아니면 UseItem 내부에서 아무 일도 안 일어나고 false만 반환됨
        if (OwningInventory->UseItem(SlotIndex))
        {
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

void UGJInventorySlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UGJInventorySlotDragOp* DragOp = NewObject<UGJInventorySlotDragOp>(this);
    DragOp->SourceSlotIndex = SlotIndex;
    OutOperation = DragOp;
}

bool UGJInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (UGJInventorySlotDragOp* DragOp = Cast<UGJInventorySlotDragOp>(InOperation))
    {
        if (OwningInventory)
        {
            return OwningInventory->SwapSlots(DragOp->SourceSlotIndex, SlotIndex);
        }
    }

    return false;
}
