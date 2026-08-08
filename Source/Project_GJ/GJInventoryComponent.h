#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GJInventoryComponent.generated.h"

// 인벤토리 한 칸. ItemHandle이 비어있으면(IsNull) 빈 칸을 의미함.
// 아이템의 테이블+행 이름을 그대로 들고 있어서, 인벤토리 컴포넌트 자체는 어떤 데이터 테이블을 쓰는지
// 몰라도 되고(UI에서 아이콘 등을 표시할 때도 이 핸들 하나로 FItemData를 바로 조회 가능함).
USTRUCT(BlueprintType)
struct FInventorySlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    FDataTableRowHandle ItemHandle;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
    int32 Quantity = 0;
};

// 인벤토리 내용이 바뀔 때마다(추가/제거/스왑) 브로드캐스트 - UI에서 폴링 없이 바인딩해서 쓰기 위함
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChangedSignature);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGJInventoryComponent();

    // 인벤토리 슬롯(칸) 최대 개수. Items 배열이 BeginPlay에서 이 크기로 고정되고,
    // 빈 칸도 항상 배열 안에 존재함(그리드 UI가 배열 인덱스 = 그리드 위치로 1:1 대응시킬 수 있도록)
    UPROPERTY(EditAnywhere, Category = "Inventory")
    int32 MaxSlots = 20;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    TArray<FInventorySlot> Items;

    UPROPERTY(BlueprintAssignable, Category = "Inventory")
    FOnInventoryChangedSignature OnInventoryChanged;

    // 아이템 추가. 기존 스택의 여유 공간부터 채우고, 남으면 빈 칸에 순서대로 채움.
    // 뭔가 실제로 추가됐다면 아이템 ID 순으로 자동 정렬됨(빈 칸은 항상 뒤로 밀림).
    // 요청한 수량을 전부 넣지 못했으면 false를 반환함(넣을 수 있었던 만큼은 그대로 반영됨).
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool AddItem(const FDataTableRowHandle& ItemHandle, int32 Quantity);

    // 아이템 제거. 보유 수량이 부족하면 아무것도 빼지 않고 false 반환(원자적 처리).
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool RemoveItem(FName ItemID, int32 Quantity);

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetItemCount(FName ItemID) const;

    UFUNCTION(BlueprintPure, Category = "Inventory")
    bool HasItem(FName ItemID, int32 Quantity = 1) const;

    // 두 슬롯의 내용을 서로 바꿈 (인벤토리 UI에서 아이콘을 드래그해서 다른 칸에 놓을 때 사용).
    // 정렬을 다시 하지 않음 - 다음에 아이템을 주울 때까지는 수동으로 옮긴 위치가 유지됨.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool SwapSlots(int32 IndexA, int32 IndexB);

    // 슬롯의 아이템을 사용함 (인벤토리 UI에서 더블클릭 시 호출). EItemType::Consumable인
    // 아이템만 사용 가능하고(장비/재료/퀘스트 아이템은 더블클릭해도 아무 일도 안 일어남),
    // 실제 회복 효과 적용은 소유 액터(AGJCharacter)에게 위임한 뒤 수량을 1 줄임.
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    bool UseItem(int32 SlotIndex);

protected:
    virtual void BeginPlay() override;

    void SortItems();
};
