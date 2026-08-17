#include "GJPlayerHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "GJSkillIconWidget.h"

void UGJPlayerHUDWidget::UpdateHP(float CurrentHP, float MaxHP)
{
    if (!HPBar)
    {
        return;
    }

    const float Percent = (MaxHP > 0.f) ? FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f) : 0.f;
    HPBar->SetPercent(Percent);
}

void UGJPlayerHUDWidget::UpdateMP(float CurrentMP, float MaxMP)
{
    if (!MPBar)
    {
        return;
    }

    const float Percent = (MaxMP > 0.f) ? FMath::Clamp(CurrentMP / MaxMP, 0.f, 1.f) : 0.f;
    MPBar->SetPercent(Percent);
}

void UGJPlayerHUDWidget::UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level)
{
    if (EXPBar)
    {
        // RequiredEXP가 0이면(데이터 미입력 등) 나누기 0이 되므로 가득 찬 것으로 표시한다
        const float Percent = (RequiredEXP > 0.f)
            ? FMath::Clamp(CurrentEXP / RequiredEXP, 0.f, 1.f)
            : 1.f;
        EXPBar->SetPercent(Percent);
    }

    if (LevelText)
    {
        LevelText->SetText(FText::AsNumber(Level));
    }
}

void UGJPlayerHUDWidget::InitializeSkillIcons(AGJCharacter* InCharacter)
{
    if (SkillIcon1) { SkillIcon1->SetSlotData(0, InCharacter); }
    if (SkillIcon2) { SkillIcon2->SetSlotData(1, InCharacter); }
    if (SkillIcon3) { SkillIcon3->SetSlotData(2, InCharacter); }
}
