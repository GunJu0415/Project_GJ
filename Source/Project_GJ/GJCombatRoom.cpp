#include "GJCombatRoom.h"
#include "GJRoomSpawnPointComponent.h"
#include "GJEnemyCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

void AGJCombatRoom::PopulateRoom()
{
    if (!RoomSpawnTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: RoomSpawnTable이 비어 있습니다."), *GetName());
        return;
    }

    const FRoomSpawnData* Row = RoomSpawnTable->FindRow<FRoomSpawnData>(SpawnRowName, TEXT("PopulateRoom"), false);
    if (!Row)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: '%s' 행을 찾지 못했습니다."),
            *GetName(), *SpawnRowName.ToString());
        return;
    }

    SpawnEnemies(*Row);
}

void AGJCombatRoom::SpawnEnemies(const FRoomSpawnData& Row)
{
    UWorld* World = GetWorld();
    if (!World || Row.EnemyPool.Num() == 0)
    {
        return;
    }

    TArray<UGJRoomSpawnPointComponent*> Points = GatherPoints(ESpawnPointType::Enemy);
    int32 Count = FMath::RandRange(Row.MinEnemies, Row.MaxEnemies);
    PrepareSpawnPoints(Points, Count);

    FActorSpawnParameters Params;
    // 스폰 자리가 살짝 겹쳐도 스폰은 되게 한다. 안 그러면 조용히 아무것도 안 나온다.
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < Count; i++)
    {
        const TSubclassOf<AGJEnemyCharacter> EnemyClass = Row.EnemyPool[FMath::RandRange(0, Row.EnemyPool.Num() - 1)];
        if (!EnemyClass)
        {
            continue;
        }

        AGJEnemyCharacter* Enemy = World->SpawnActor<AGJEnemyCharacter>(
            EnemyClass,
            Points[i]->GetComponentLocation(),
            Points[i]->GetComponentRotation(),
            Params);

        RegisterSpawnedEnemy(Enemy);
    }

    // 뽑은 수와 실제로 등록된 수를 같이 찍는다. AliveEnemies.Num()만 찍으면
    // "3을 뽑았다"와 "5를 뽑았는데 2마리가 스폰에 실패했다"가 구분되지 않는다.
    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s: 적 %d/%d마리 스폰 (행 '%s', 범위 %d~%d)"),
        *GetName(), AliveEnemies.Num(), Count, *SpawnRowName.ToString(), Row.MinEnemies, Row.MaxEnemies);

    if (AliveEnemies.Num() < Count)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: 적 %d마리가 스폰에 실패했습니다."),
            *GetName(), Count - AliveEnemies.Num());
    }
}

TArray<UGJRoomSpawnPointComponent*> AGJCombatRoom::GatherPoints(ESpawnPointType Type) const
{
    TArray<UGJRoomSpawnPointComponent*> All;
    GetComponents<UGJRoomSpawnPointComponent>(All);

    TArray<UGJRoomSpawnPointComponent*> Result;
    for (UGJRoomSpawnPointComponent* Point : All)
    {
        if (Point && Point->PointType == Type)
        {
            Result.Add(Point);
        }
    }

    return Result;
}

void AGJCombatRoom::PrepareSpawnPoints(TArray<UGJRoomSpawnPointComponent*>& Points, int32& InOutCount)
{
    // 테이블이 점보다 많은 수를 요구할 수 있다. 자르지 않으면 인덱스가 넘친다.
    InOutCount = FMath::Clamp(InOutCount, 0, Points.Num());

    // 섞지 않으면 항상 앞쪽 점만 쓰여서 배치가 매번 같아진다.
    for (int32 i = Points.Num() - 1; i > 0; i--)
    {
        Points.Swap(i, FMath::RandRange(0, i));
    }
}
