#include "GJCardSelectWidget.h"
#include "GJCardWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UGJCardSelectWidget::ShowChoices(const TArray<FGJChoiceEntry>& Choices)
{
    if (!CardContainer || !CardWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJCardSelectWidget: CardContainer 또는 CardWidgetClass가 비어있습니다."));
        return;
    }

    // 같은 위젯 인스턴스를 재사용하므로 이전 선택지를 먼저 지운다.
    // 안 지우면 카드가 계속 옆으로 쌓인다(연속 레벨업에서 바로 드러난다).
    CardContainer->ClearChildren();

    for (int32 i = 0; i < Choices.Num(); i++)
    {
        UGJCardWidget* CardWidget = CreateWidget<UGJCardWidget>(this, CardWidgetClass);
        if (!CardWidget)
        {
            continue;
        }

        CardWidget->Setup(i, Choices[i]);
        CardWidget->OnCardClicked.AddDynamic(this, &UGJCardSelectWidget::HandleCardClicked);

        UHorizontalBoxSlot* BoxSlot = CardContainer->AddChildToHorizontalBox(CardWidget);
        if (BoxSlot)
        {
            BoxSlot->SetPadding(FMargin(12.f, 0.f));
        }
    }
}

void UGJCardSelectWidget::HandleCardClicked(int32 ChoiceIndex)
{
    OnChoiceSelected.Broadcast(ChoiceIndex);
}
