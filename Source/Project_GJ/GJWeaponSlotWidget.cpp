#include "GJWeaponSlotWidget.h"
#include "GJCharacter.h"
#include "GJWeaponBase.h"
#include "GJGameTypes.h"
#include "Components/Image.h"

void UGJWeaponSlotWidget::SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter)
{
    SlotIndex = InSlotIndex;
    OwningCharacter = InOwningCharacter;

    if (!IconImage || !OwningCharacter)
    {
        return;
    }

    bIsActiveSlot = (OwningCharacter->GetCurrentWeaponSlotIndex() == SlotIndex);

    AGJWeaponBase* Weapon = OwningCharacter->GetWeaponInSlot(SlotIndex);
    if (!Weapon)
    {
        IconImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    if (UTexture2D* Icon = Weapon->GetWeaponStat().WeaponIcon)
    {
        IconImage->SetBrushFromTexture(Icon);
    }
    IconImage->SetVisibility(ESlateVisibility::Visible);
}

FReply UGJWeaponSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OwningCharacter && OwningCharacter->GetWeaponInSlot(SlotIndex))
    {
        return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
    }

    return FReply::Unhandled();
}

FReply UGJWeaponSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && OwningCharacter)
    {
        OwningCharacter->SwapToWeaponSlot(SlotIndex);
        return FReply::Handled();
    }

    return FReply::Unhandled();
}

void UGJWeaponSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UGJWeaponSlotDragOp* DragOp = NewObject<UGJWeaponSlotDragOp>(this);
    DragOp->SourceSlotIndex = SlotIndex;
    OutOperation = DragOp;
}

bool UGJWeaponSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    if (UGJWeaponSlotDragOp* DragOp = Cast<UGJWeaponSlotDragOp>(InOperation))
    {
        if (OwningCharacter)
        {
            return OwningCharacter->SwapWeaponSlots(DragOp->SourceSlotIndex, SlotIndex);
        }
    }

    return false;
}
