#include "GJCardWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UGJCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SelectButton)
    {
        // 위젯이 재사용되는 경우를 대비해 중복 바인딩을 막는다.
        SelectButton->OnClicked.RemoveDynamic(this, &UGJCardWidget::HandleButtonClicked);
        SelectButton->OnClicked.AddDynamic(this, &UGJCardWidget::HandleButtonClicked);
    }
}

void UGJCardWidget::Setup(int32 InChoiceIndex, const FGJChoiceEntry& Entry)
{
    ChoiceIndex = InChoiceIndex;

    if (NameText)
    {
        NameText->SetText(Entry.DisplayName);
    }
    if (DescText)
    {
        DescText->SetText(Entry.Description);
    }
    if (IconImage)
    {
        if (Entry.Icon)
        {
            IconImage->SetBrushFromTexture(Entry.Icon);
            IconImage->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            // 아이콘이 없는 카드도 있을 수 있다. 빈 브러시를 그리면 흰 사각형이 남으므로 숨긴다.
            IconImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void UGJCardWidget::HandleButtonClicked()
{
    OnCardClicked.Broadcast(ChoiceIndex);
}
