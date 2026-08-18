#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/DragDropOperation.h"
#include "GJSkillSlotWidget.generated.h"

class UImage;
class UTextBlock;
class AGJCharacter;

// 드래그 중인 스킬 칸의 출발 인덱스를 담아 나른다 (GJWeaponSlotWidget과 같은 패턴)
UCLASS()
class PROJECT_GJ_API UGJSkillSlotDragOp : public UDragDropOperation
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Skill")
    int32 SourceSlotIndex = -1;
};

// 인벤토리 스킬 페이지의 칸 하나. 드래그해서 다른 칸에 놓으면 두 슬롯의 스킬이
// (쿨타임까지) 서로 바뀐다 - 즉 어느 키에 둘지를 바꾼다.
// 클릭에는 아무 동작도 없다: 무기와 달리 스킬 셋은 항상 세 개가 다 활성이라
// "장착"이라는 개념이 없다.
UCLASS()
class PROJECT_GJ_API UGJSkillSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter);

protected:
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
    virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* KeyText;

    int32 SlotIndex = -1;

    UPROPERTY()
    AGJCharacter* OwningCharacter;
};
