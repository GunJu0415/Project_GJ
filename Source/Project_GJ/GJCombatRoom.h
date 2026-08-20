#pragma once

#include "CoreMinimal.h"
#include "GJRoomBase.h"
#include "GJGameTypes.h"
#include "GJCombatRoom.generated.h"

class UDataTable;
class UGJRoomSpawnPointComponent;

// 데이터 테이블 행대로 방을 채우는 방. 전투방/보물방/시작방이 전부 이 클래스이고
// 행만 다르다. 방의 모양은 이 클래스를 상속한 BP가 정한다.
UCLASS()
class PROJECT_GJ_API AGJCombatRoom : public AGJRoomBase
{
    GENERATED_BODY()

protected:
    virtual void PopulateRoom() override;

    void SpawnEnemies(const FRoomSpawnData& Row);
    void SpawnItems(const FRoomSpawnData& Row);
    void SpawnChest(const FRoomSpawnData& Row);

    // 이 방의 스폰 포인트 중 해당 용도인 것만 모은다.
    TArray<UGJRoomSpawnPointComponent*> GatherPoints(ESpawnPointType Type) const;

    // 개수를 점 개수로 자르고 점 배열을 섞는다. 스폰 전에 반드시 거친다.
    static void PrepareSpawnPoints(TArray<UGJRoomSpawnPointComponent*>& Points, int32& InOutCount);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    UDataTable* RoomSpawnTable;
};
