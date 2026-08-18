#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJInventoryWidget.generated.h"

class UUniformGridPanel;
class UGJInventorySlotWidget;
class UGJInventoryComponent;
class UGJWeaponSlotWidget;
class UGJSkillSlotWidget;
class AGJCharacter;

// 인벤토리 격자 전체 패널. 큰 사각형(GridPanel을 담는 배경) 안에 슬롯 위젯이 격자로 채워짐.
// 2페이지(무기 페이지)는 WeaponSlotWidget1/2 두 칸으로 구성됨 - 실제로 1페이지(GridPanel)와
// 2페이지(무기 슬롯)를 오가는 탭/스위처 배치는 디자이너에서 만들면 됨(둘 다 이 위젯의 자식으로 두면
// InitializeInventory 한 번으로 둘 다 갱신됨).
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

    // 무기 페이지(슬롯 0/1) 아이콘/활성 표시를 최신 상태로 갱신
    UFUNCTION()
    void RefreshWeaponSlots();

    // 스킬 페이지(슬롯 0/1/2) 아이콘을 최신 상태로 갱신
    UFUNCTION()
    void RefreshSkillSlots();

    virtual void NativeOnInitialized() override;

    // 진짜로 이 위젯 오브젝트가 파괴될 때(레벨 전환 등)만 리스너를 해제하기 위함 - 처음엔
    // NativeDestruct에서 해제했는데, NativeDestruct는 위젯이 실제로 파괴될 때가 아니라
    // RemoveFromParent()(=인벤토리를 닫을 때)마다 호출되는 거라서, 인벤토리를 첫 번째로 닫는
    // 순간 리스너가 해제돼버리고 NativeOnInitialized는 위젯 생성 시 딱 한 번만 도니까 다시
    // 등록도 안 돼서 두 번째부터는 Tab이 아예 안 먹혔음.
    virtual void BeginDestroy() override;

    // Tab으로 닫는 처리를 전역 프리-인풋 리스너로 함 (포커스에 의존하지 않음). 처음엔
    // NativeOnPreviewKeyDown으로 했었는데, 그건 이 위젯(또는 그 자손)이 실제로 키보드 포커스를
    // 갖고 있어야만 호출됨 - 인벤토리 바깥(빈 캔버스 영역)을 클릭하면 포커스가 이 위젯 트리 밖으로
    // 빠져나가서 Tab이 안 먹혔고, 그 다음 Tab 입력이 포커스를 다시 이 위젯 쪽으로 되돌리는 데
    // 소모되어 버려서 두 번 눌러야 닫히는 문제가 있었음. 전역 리스너는 포커스가 어디에 있든
    // 상관없이 모든 키 입력을 미리 통지받아서 이 문제 자체가 없음.
    void HandleGlobalPreviewKeyDown(const FKeyEvent& InKeyEvent);

    FDelegateHandle GlobalKeyListenerHandle;

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

    // ==========================================
    // 2페이지: 무기 페이지 (무기 슬롯 0번/1번)
    // ==========================================
    // 디자이너에서 이 이름과 똑같은 이름으로 UGJWeaponSlotWidget을 2개 배치해야 자동으로 바인딩됨
    // (선택 사항 - 안 만들면 무기 페이지 없이 기존 그리드 페이지만 동작함)
    UPROPERTY(meta = (BindWidgetOptional))
    UGJWeaponSlotWidget* WeaponSlotWidget1;

    UPROPERTY(meta = (BindWidgetOptional))
    UGJWeaponSlotWidget* WeaponSlotWidget2;

    // ==========================================
    // 3페이지: 스킬 페이지 (슬롯 0/1/2 = 우클릭/Q/F)
    // ==========================================
    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillSlotWidget* SkillSlotWidget1;

    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillSlotWidget* SkillSlotWidget2;

    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillSlotWidget* SkillSlotWidget3;

    UPROPERTY()
    AGJCharacter* OwningCharacter;
};
