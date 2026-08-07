#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJInventoryWidget.generated.h"

class UUniformGridPanel;
class UGJInventorySlotWidget;
class UGJInventoryComponent;

// 인벤토리 격자 전체 패널. 큰 사각형(GridPanel을 담는 배경) 안에 슬롯 위젯이 격자로 채워짐.
UCLASS()
class PROJECT_GJ_API UGJInventoryWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 캐릭터가 자신의 InventoryComponent를 넘겨 이 위젯을 세팅함 (열릴 때 1회 호출하면 됨)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void InitializeInventory(UGJInventoryComponent* InInventory);

protected:
    UFUNCTION()
    void RefreshGrid();

    // Tab을 직접 감지해서 닫음 (일시정지 중 Enhanced Input 액션 평가에 기대지 않고,
    // 포커스를 받은 이 위젯이 Slate 키 이벤트 레벨에서 바로 처리하는 게 훨씬 확실함)
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    // 디자이너에서 이 이름과 똑같은 Uniform Grid Panel을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UUniformGridPanel* GridPanel;

    // 격자 한 칸으로 쓸 위젯 클래스 (UGJInventorySlotWidget을 상속한 WBP를 만들어 할당)
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    TSubclassOf<UGJInventorySlotWidget> SlotWidgetClass;

    // 한 줄에 몇 칸씩 배치할지 (인벤토리 MaxSlots를 이 값으로 나눠 줄이 정해짐)
    UPROPERTY(EditAnywhere, Category = "Inventory")
    int32 Columns = 5;

    // 칸 사이 간격(px) - 슬롯끼리 붙어서 하나의 사각형처럼 보이지 않도록
    UPROPERTY(EditAnywhere, Category = "Inventory")
    float CellPadding = 4.f;

    UPROPERTY()
    UGJInventoryComponent* BoundInventory;
};
