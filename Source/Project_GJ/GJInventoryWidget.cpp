#include "GJInventoryWidget.h"
#include "GJInventorySlotWidget.h"
#include "GJInventoryComponent.h"
#include "GJCharacter.h"
#include "Components/UniformGridPanel.h"
#include "Containers/Ticker.h"

void UGJInventoryWidget::InitializeInventory(UGJInventoryComponent* InInventory)
{
    if (BoundInventory)
    {
        BoundInventory->OnInventoryChanged.RemoveDynamic(this, &UGJInventoryWidget::RefreshGrid);
    }

    BoundInventory = InInventory;

    if (BoundInventory)
    {
        BoundInventory->OnInventoryChanged.AddDynamic(this, &UGJInventoryWidget::RefreshGrid);
    }

    RefreshGrid();
}

void UGJInventoryWidget::RefreshGrid()
{
    if (!GridPanel || !SlotWidgetClass || !BoundInventory || Columns <= 0)
    {
        return;
    }

    GridPanel->ClearChildren();
    GridPanel->SetSlotPadding(FMargin(CellPadding));

    for (int32 i = 0; i < BoundInventory->Items.Num(); i++)
    {
        UGJInventorySlotWidget* SlotWidget = CreateWidget<UGJInventorySlotWidget>(this, SlotWidgetClass);
        if (!SlotWidget)
        {
            continue;
        }

        SlotWidget->SetSlotData(i, BoundInventory);

        const int32 Row = i / Columns;
        const int32 Col = i % Columns;
        GridPanel->AddChildToUniformGrid(SlotWidget, Row, Col);
    }
}

FReply UGJInventoryWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Tab)
    {
        if (AGJCharacter* Character = Cast<AGJCharacter>(GetOwningPlayerPawn()))
        {
            // 지금 이 위젯 자신의 키 입력 처리 도중에 곧바로 위젯을 제거하고 입력 모드를 바꾸면
            // Slate가 그 프레임에 포커스를 게임 뷰포트로 제대로 못 돌려주고, 이후 게임 입력이
            // 전부 안 먹는 문제가 생김(자기 자신의 이벤트 처리 중에 자기 자신을 건드리는 전형적인 함정).
            // 월드 타이머(SetTimerForNextTick)는 일시정지 중엔 안 돌아서 대신 일시정지와 무관하게
            // 도는 엔진 코어 티커로 한 프레임만 미뤄서 처리함.
            TWeakObjectPtr<AGJCharacter> WeakCharacter = Character;
            FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakCharacter](float) -> bool
            {
                if (WeakCharacter.IsValid())
                {
                    WeakCharacter->ToggleInventory();
                }
                return false; // 한 번만 실행하고 스스로 제거됨
            }));
        }
        return FReply::Handled();
    }

    return FReply::Unhandled();
}
