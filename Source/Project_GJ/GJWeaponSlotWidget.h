#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "GJWeaponSlotWidget.generated.h"

class UImage;
class AGJCharacter;

// 드래그 중인 무기 슬롯의 출발 인덱스를 담아 나르는 용도 (GJInventorySlotWidget의 드래그 앤 드롭과 동일한 패턴)
UCLASS()
class PROJECT_GJ_API UGJWeaponSlotDragOp : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Weapon")
    int32 SourceSlotIndex = -1;
};

// 인벤토리 2번째 페이지(무기 페이지)의 칸 하나. 캐릭터의 무기 슬롯(0번/1번) 중 하나를 가리키며,
// 그냥 클릭하면 그 슬롯의 무기로 스왑(장착)하고, 드래그해서 다른 무기 슬롯 위에 놓으면 두 슬롯의
// 자리를 서로 바꿈(장착 상태는 안 바뀜). 디자이너에서 이 위젯을 2개 배치해 각각
// SetSlotData(0, ...)/SetSlotData(1, ...)로 세팅하면 됨.
UCLASS()
class PROJECT_GJ_API UGJWeaponSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 이 슬롯이 담당할 무기 슬롯 번호(0/1)와 소유 캐릭터를 세팅하고 아이콘을 갱신함
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter);

protected:
    // 드래그 감지만 걸어둠(클릭으로 취급되는 액션은 여기서 바로 하지 않음) - 실제로 드래그 없이
    // 뗐을 때만 NativeOnMouseButtonUp에서 무기 교체를 처리함
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    // 드래그가 감지되지 않은 순수 클릭이었을 때만 호출됨(드래그가 실제로 시작됐다면 그쪽 드롭
    // 처리로 끝나고 여기는 호출되지 않음) - 이 슬롯의 무기로 교체(장착)함
    virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    // 디자이너에서 이 이름과 똑같은 Image를 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    // 지금 실제로 손에 들려 있는(활성) 슬롯인지 여부 - SetSlotData가 호출될 때마다 갱신됨.
    // 디자이너에서 이 값을 참조해 테두리 강조 등을 주면 됨(블루프린트에서 바인딩해서 사용).
    UPROPERTY(BlueprintReadOnly, Category = "Weapon")
    bool bIsActiveSlot = false;

    int32 SlotIndex = -1;

    UPROPERTY()
    AGJCharacter* OwningCharacter;
};
