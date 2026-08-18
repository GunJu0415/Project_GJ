#include "GJSkillIconWidget.h"
#include "GJCharacter.h"
#include "GJSkillComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Materials/MaterialInstanceDynamic.h"

void UGJSkillIconWidget::SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter)
{
    // 재사용될 수 있으므로 이전 구독을 먼저 끊는다.
    if (OwningCharacter)
    {
        if (UGJSkillComponent* OldSkills = OwningCharacter->GetSkillComponent())
        {
            OldSkills->OnSkillSlotsChanged.RemoveDynamic(this, &UGJSkillIconWidget::RefreshSkill);
        }
    }

    SlotIndex = InSlotIndex;
    OwningCharacter = InOwningCharacter;

    if (OwningCharacter)
    {
        if (UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent())
        {
            Skills->OnSkillSlotsChanged.AddDynamic(this, &UGJSkillIconWidget::RefreshSkill);
        }
    }

    if (KeyText)
    {
        KeyText->SetText(UGJSkillComponent::GetSlotKeyLabel(SlotIndex));
    }

    // 방사형 머티리얼을 쓰는 경우에만 MID를 만든다. ProgressBar 방식이면 null로 남는다.
    if (CooldownImage && !CooldownMID)
    {
        CooldownMID = CooldownImage->GetDynamicMaterial();
    }

    // 슬롯이 바뀌었으니 다음 틱에서 색을 무조건 다시 칠하게 한다.
    LastAffordState = INDEX_NONE;

    RefreshSkill();
}

void UGJSkillIconWidget::NativeDestruct()
{
    // 위젯이 사라진 뒤에도 델리게이트가 남아 있으면 죽은 객체를 부른다.
    if (OwningCharacter)
    {
        if (UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent())
        {
            Skills->OnSkillSlotsChanged.RemoveDynamic(this, &UGJSkillIconWidget::RefreshSkill);
        }
    }

    Super::NativeDestruct();
}

void UGJSkillIconWidget::RefreshSkill()
{
    if (!IconImage || !OwningCharacter)
    {
        return;
    }

    UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent();
    const FSkillData* Skill = Skills ? Skills->FindSkill(Skills->GetSkillInSlot(SlotIndex)) : nullptr;

    // 빈 슬롯이면 아이콘만 숨긴다. 키 라벨은 남겨서 "이 자리가 비어 있다"를 보여준다.
    if (!Skill || !Skill->Icon)
    {
        IconImage->SetVisibility(ESlateVisibility::Hidden);
        return;
    }

    IconImage->SetBrushFromTexture(Skill->Icon);
    IconImage->SetVisibility(ESlateVisibility::Visible);
}

void UGJSkillIconWidget::SetCooldownRatio(float Ratio)
{
    if (CooldownBar)
    {
        CooldownBar->SetPercent(Ratio);
        // 쿨타임이 0이면 덮개를 아예 숨긴다. 0%짜리 바가 얇게 남아 보이는 걸 막는다.
        CooldownBar->SetVisibility(Ratio > 0.f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
    }

    if (CooldownMID)
    {
        CooldownMID->SetScalarParameterValue(CooldownParamName, Ratio);
    }
}

void UGJSkillIconWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!OwningCharacter)
    {
        return;
    }

    if (UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent())
    {
        SetCooldownRatio(Skills->GetCooldownRatio(SlotIndex));
    }

    UpdateAffordTint();
}

void UGJSkillIconWidget::UpdateAffordTint()
{
    if (!IconImage || !OwningCharacter)
    {
        return;
    }

    const UGJSkillComponent* Skills = OwningCharacter->GetSkillComponent();
    if (!Skills)
    {
        return;
    }

    const int32 AffordState = Skills->HasEnoughMP(SlotIndex) ? 1 : 0;
    if (AffordState == LastAffordState)
    {
        return;
    }

    LastAffordState = AffordState;
    IconImage->SetColorAndOpacity(AffordState == 1 ? AffordableTint : UnaffordableTint);
}
