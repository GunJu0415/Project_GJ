#include "GJPlayerHUDWidget.h"
#include "Components/ProgressBar.h"

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
