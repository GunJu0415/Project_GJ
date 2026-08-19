#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJRoomBase.generated.h"

class AGJBaseCharacter;
class AGJEnemyCharacter;
class AGJRoomBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomClearedSignature, AGJRoomBase*, Room);

// 방 한 칸의 공통 베이스. 무엇을 채울지는 서브클래스가 정하고, 이 클래스는
// 전멸 추적과 출구 제어만 한다.
//
// 방 종류마다 클래스를 만들지 않는다: 시작방/전투방/보물방은 채우고, 전멸을 세고,
// 문을 여는 일이 똑같고 값만 다르다. 그건 데이터지 동작이 아니다. 동작이 실제로
// 다른 것은 보스방 하나뿐이라(문을 여는 게 아니라 스테이지를 넘긴다) 훅만 열어둔다.
UCLASS(Abstract)
class PROJECT_GJ_API AGJRoomBase : public AActor
{
    GENERATED_BODY()

public:
    AGJRoomBase();

    // 지연 스폰에서 BeginPlay 전에 역할을 정한다. Task B의 던전 생성기가 쓴다:
    // SpawnActorDeferred -> SetSpawnRow -> FinishSpawning.
    UFUNCTION(BlueprintCallable, Category = "Room")
    void SetSpawnRow(FName RowName) { SpawnRowName = RowName; }

    UPROPERTY(BlueprintAssignable, Category = "Room")
    FOnRoomClearedSignature OnRoomCleared;

    UFUNCTION(BlueprintPure, Category = "Room")
    bool IsCleared() const { return bCleared; }

protected:
    virtual void BeginPlay() override;

    // 무엇을 채울지. 베이스는 아무것도 안 한다.
    virtual void PopulateRoom() {}

    // 클리어됐을 때 무엇을 할지. 보스방은 여기서 스테이지를 넘긴다.
    virtual void HandleRoomCleared();

    // 문을 막을지. 통로처럼 막을 일이 없는 방은 false로 오버라이드한다.
    virtual bool ShouldBlockExits() const { return true; }

    // 서브클래스가 적을 스폰한 뒤 이걸로 등록한다. 전멸 추적은 베이스가 한다.
    void RegisterSpawnedEnemy(AGJEnemyCharacter* Enemy);

    // 채우기가 끝난 뒤 반드시 부른다. 적이 0마리면 즉시 클리어로 보낸다.
    void CheckClearedAfterPopulate();

    // 이 방의 모든 UGJRoomExitComponent를 한꺼번에 막거나 연다.
    void SetExitsBlocked(bool bBlocked);

    UFUNCTION()
    void OnSpawnedEnemyDied(AGJBaseCharacter* DeadCharacter);

    // 행 이름 = 방의 역할. 손으로 배치할 때는 디테일 패널에서, 생성기가 놓을 때는
    // SetSpawnRow로 정해진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    FName SpawnRowName;

    UPROPERTY()
    TArray<AGJEnemyCharacter*> AliveEnemies;

    bool bCleared = false;
};
