#include "GJInventoryWidget.h"
#include "GJInventorySlotWidget.h"
#include "GJInventoryComponent.h"
#include "GJWeaponSlotWidget.h"
#include "GJCharacter.h"
#include "Components/UniformGridPanel.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"

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

    // 2페이지(무기 페이지)용 - 인벤토리 컴포넌트의 소유 액터에서 캐릭터를 얻어 무기 슬롯도 함께 갱신
    if (OwningCharacter)
    {
        OwningCharacter->OnWeaponSlotsChanged.RemoveDynamic(this, &UGJInventoryWidget::RefreshWeaponSlots);
    }

    OwningCharacter = BoundInventory ? Cast<AGJCharacter>(BoundInventory->GetOwner()) : nullptr;

    if (OwningCharacter)
    {
        OwningCharacter->OnWeaponSlotsChanged.AddDynamic(this, &UGJInventoryWidget::RefreshWeaponSlots);
    }

    RefreshWeaponSlots();
}

void UGJInventoryWidget::RefreshWeaponSlots()
{
    if (WeaponSlotWidget1)
    {
        WeaponSlotWidget1->SetSlotData(0, OwningCharacter);
    }
    if (WeaponSlotWidget2)
    {
        WeaponSlotWidget2->SetSlotData(1, OwningCharacter);
    }
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

void UGJInventoryWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    // 포커스 기반(NativeOnPreviewKeyDown)이 아니라 전역 프리-인풋 리스너로 Tab을 감지함 - 자세한
    // 이유는 헤더의 주석 참고(인벤토리 바깥 클릭 후 포커스가 빠져나가면 포커스 기반 방식은 Tab이
    // 안 먹혔음). 이 리스너는 위젯이 생성될 때 한 번만 등록되고, 실제 동작은
    // HandleGlobalPreviewKeyDown 안에서 IsInViewport()로 걸러서 열려있을 때만 반응함.
    GlobalKeyListenerHandle = FSlateApplication::Get().OnApplicationPreInputKeyDownListener().AddUObject(this, &UGJInventoryWidget::HandleGlobalPreviewKeyDown);
}

void UGJInventoryWidget::BeginDestroy()
{
    if (GlobalKeyListenerHandle.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(GlobalKeyListenerHandle);
        GlobalKeyListenerHandle.Reset();
    }

    Super::BeginDestroy();
}

void UGJInventoryWidget::HandleGlobalPreviewKeyDown(const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() != EKeys::Tab || !IsInViewport())
    {
        return;
    }

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
}
