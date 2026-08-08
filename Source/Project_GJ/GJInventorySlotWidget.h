#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "GJInventorySlotWidget.generated.h"

class UImage;
class UTextBlock;
class UGJInventoryComponent;

// 드래그 중인 슬롯의 출발 인덱스를 담아 나르는 용도
UCLASS()
class PROJECT_GJ_API UGJInventorySlotDragOp : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Inventory")
    int32 SourceSlotIndex = -1;
};

// 인벤토리 그리드의 칸 하나. 아이콘 표시 + 마우스로 집어서 다른 칸에 놓는(드래그 앤 드롭) 동작을
// 전부 C++에서 처리함(블루프린트 이벤트 그래프 작업 없이 디자이너에서 위젯 배치만 하면 됨).
UCLASS()
class PROJECT_GJ_API UGJInventorySlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 이 슬롯이 담당할 인벤토리 칸 번호와 대상 인벤토리를 세팅하고 아이콘/수량을 갱신함
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void SetSlotData(int32 InSlotIndex, UGJInventoryComponent* InInventory);

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    // 디자이너에서 이 이름과 똑같은 Image를 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    // 선택 사항 - 수량 표시용 텍스트 (스택 1개면 자동으로 숨겨짐)
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* QuantityText;

    int32 SlotIndex = -1;

    UPROPERTY()
    UGJInventoryComponent* OwningInventory;
};
