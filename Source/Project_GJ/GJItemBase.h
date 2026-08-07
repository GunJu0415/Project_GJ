#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJGameTypes.h"
#include "GJInteractable.h"
#include "GJItemBase.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AGJCharacter;

// 월드에 배치되는 아이템 액터의 공통 베이스. 데이터 테이블(FItemData) 기반으로 스탯을 읽어오고,
// InteractionCollision 범위 안에서 상호작용 입력(Interact)이 들어오면 PickUp()을 호출함.
// (겹치기만 해서는 자동으로 습득되지 않음 - 반드시 상호작용 키를 눌러야 함)
UCLASS(Abstract)
class PROJECT_GJ_API AGJItemBase : public AActor, public IGJInteractable
{
    GENERATED_BODY()

public:
    AGJItemBase();

    virtual void OnConstruction(const FTransform& Transform) override;

    // IGJInteractable - Interactor가 실제로 InteractionCollision 범위 안에 있을 때만 PickUp()을 호출함
    virtual void Interact_Implementation(AGJCharacter* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ItemMesh;

    // 아이템 자체의 물리적인 충돌 판정용 (작게 유지 - 나중에 아이템이 땅에 굴러다니는 등
    // 실제 충돌/물리가 필요해지면 이 컴포넌트를 그대로 활용하면 됨). 지금은 상호작용 판정에 안 씀.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionComp;

    // 상호작용(습득) 가능한 범위 - CollisionComp보다 넉넉하게 큼. 캐릭터가 이 범위 안에 있을 때
    // 상호작용 입력을 누르면 습득됨. 물리 충돌과 분리되어 있어서 이 범위를 넓혀도 아이템의
    // 실제 충돌 크기에는 영향이 없음.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionCollision;

    // 아이템 데이터 테이블 행 참조. 행 이름이 인벤토리에서 쓰는 아이템 ID(FName)와 동일하게 취급됨.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Item")
    FDataTableRowHandle ItemDataHandle;

    // 이 액터를 주웠을 때 실제로 지급되는 개수 (한 번에 여러 개를 주울 수 있도록)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 Quantity = 1;

    // OnConstruction에서 ItemDataHandle로부터 읽어온 스탯 캐시 (에디터에서 즉시 확인 가능)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
    FItemData ItemStat;

    // 실제 습득 처리 (베이스는 빈 구현 - 서브클래스가 오버라이드)
    virtual void PickUp(AGJCharacter* Picker);

public:
    FORCEINLINE FItemData GetItemStat() const { return ItemStat; }
    FORCEINLINE FName GetItemID() const { return ItemDataHandle.RowName; }
    FORCEINLINE int32 GetQuantity() const { return Quantity; }
};
