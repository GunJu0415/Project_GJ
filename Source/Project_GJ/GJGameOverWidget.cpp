#include "GJGameOverWidget.h"
#include "GJGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UGJGameOverWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (ReturnToHubButton)
    {
        ReturnToHubButton->OnClicked.AddDynamic(this, &UGJGameOverWidget::OnReturnToHubClicked);
    }
}

void UGJGameOverWidget::SetRunCount(int32 RunCount)
{
    if (RunCountText)
    {
        RunCountText->SetText(FText::AsNumber(RunCount));
    }
}

void UGJGameOverWidget::OnReturnToHubClicked()
{
    // 회차 증가는 이미 사망 시점(AGJGameMode::OnPlayerDied)에서 끝났다.
    // 이 버튼은 이동만 담당한다.
    if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
    {
        GJGameInstance->ReturnToHub();
    }
}
