#include "GJSkillSlotWidget.h"
#include "GJCharacter.h"
#include "GJSkillComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UGJSkillSlotWidget::SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter)
{
    SlotIndex = InSlotIndex;
    OwningCharacter = InOwningCharacter;

    if (KeyText)
    {
        KeyText->SetText(UGJSkillComponent::GetSlotKeyLabel(SlotIndex));
    }

    if (!IconImage || !OwningCharacter)
    {
        return;
    }

    UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent();
    const FSkillData* Skill = Skills ? Skills->FindSkill(Skills->GetSkillInSlot(SlotIndex)) : nullptr;

    if (!Skill || !Skill->Icon)
    {
        IconImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    IconImage->SetBrushFromTexture(Skill->Icon);
    IconImage->SetVisibility(ESlateVisibility::Visible);
}

FReply UGJSkillSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    // 빈 칸은 끌 게 없다.
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !OwningCharacter)
    {
        return FReply::Unhandled();
    }

    UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent();
    if (!Skills || Skills->GetSkillInSlot(SlotIndex).IsNone())
    {
        return FReply::Unhandled();
    }

    return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

void UGJSkillSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
    UGJSkillSlotDragOp* DragOp = NewObject<UGJSkillSlotDragOp>(this);
    DragOp->SourceSlotIndex = SlotIndex;
    OutOperation = DragOp;
}

bool UGJSkillSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
    UGJSkillSlotDragOp* DragOp = Cast<UGJSkillSlotDragOp>(InOperation);
    if (!DragOp || !OwningCharacter)
    {
        return false;
    }

    UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent();
    if (!Skills)
    {
        return false;
    }

    // 교환 결과는 OnSkillSlotsChanged로 방송되고, 인벤토리 위젯이 그걸 받아 칸을 다시 그린다.
    // 여기서 직접 SetSlotData를 부르면 출발 칸이 안 갱신된다.
    Skills->SwapSkillSlots(DragOp->SourceSlotIndex, SlotIndex);
    return true;
}
