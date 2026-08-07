#include "GJHealthBarWidget.h"
#include "Components/ProgressBar.h"

void UGJHealthBarWidget::UpdateHealth(float CurrentHP, float MaxHP)
{
    if (!HealthProgressBar)
    {
        return;
    }

    const float Percent = (MaxHP > 0.f) ? FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f) : 0.f;
    HealthProgressBar->SetPercent(Percent);
}
