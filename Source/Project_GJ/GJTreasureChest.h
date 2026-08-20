#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJInteractable.h"
#include "GJTreasureChest.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AGJCharacter;

// 보물 상자. E로 한 번만 열리고 내용물을 바닥에 뿌린다.
//
// 인벤토리에 직접 넣지 않는 이유: 인벤토리가 꽉 찼을 때 아이템이 증발한다.
// 바닥에 떨어뜨리면 기존 습득 흐름(AGJItem::PickUp - 칸이 모자라면 필드에 남음)을
// 그대로 타서 새 경로가 하나도 안 생긴다.
UCLASS()
class PROJECT_GJ_API AGJTreasureChest : public AActor, public IGJInteractable
{
    GENERATED_BODY()

public:
    AGJTreasureChest();

    // IGJInteractable - 실제로 범위 안일 때만 열린다 (AGJItemBase와 같은 패턴)
    virtual void Interact_Implementation(AGJCharacter* Interactor) override;

protected:
    // 여는 연출은 BP가 맡는다. 상자 메시와 애니메이션은 에디터 작업이다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Chest")
    void OnChestOpened();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ChestMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionCollision;

    // 열었을 때 바닥에 뿌릴 것. AGJItem BP도, 무기 BP도 들어간다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    TArray<TSubclassOf<AActor>> Contents;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    int32 MinDrops = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    int32 MaxDrops = 2;

    // 뿌릴 반경. 같은 자리에 겹치면 하나만 있는 것처럼 보인다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    float DropRadius = 120.f;

    bool bOpened = false;
};
