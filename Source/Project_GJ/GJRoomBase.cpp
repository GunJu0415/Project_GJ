#include "GJRoomBase.h"
#include "GJBaseCharacter.h"
#include "GJEnemyCharacter.h"
// CreateDefaultSubobject<USceneComponent>에 완전한 타입이 필요하다.
// Actor.h가 전방 선언만 주는 경우가 있어 명시적으로 넣는다.
#include "Components/SceneComponent.h"

AGJRoomBase::AGJRoomBase()
{
    // 방은 아무것도 매 프레임 하지 않는다.
    PrimaryActorTick.bCanEverTick = false;

    // 바닥과 벽은 BP에서 붙인다. C++은 붙일 자리만 준다.
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot"));
}

void AGJRoomBase::BeginPlay()
{
    Super::BeginPlay();

    PopulateRoom();
    CheckClearedAfterPopulate();
}

void AGJRoomBase::RegisterSpawnedEnemy(AGJEnemyCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    Enemy->OnCharacterDied.AddDynamic(this, &AGJRoomBase::OnSpawnedEnemyDied);
    AliveEnemies.Add(Enemy);
}

void AGJRoomBase::CheckClearedAfterPopulate()
{
    // 적이 0마리인 방(보물방, 시작방)은 처음부터 열려 있어야 한다.
    // 이걸 빼먹으면 문이 안 열린 채로 굳는다.
    if (AliveEnemies.Num() == 0 && !bCleared)
    {
        HandleRoomCleared();
    }
}

void AGJRoomBase::OnSpawnedEnemyDied(AGJBaseCharacter* DeadCharacter)
{
    AliveEnemies.Remove(Cast<AGJEnemyCharacter>(DeadCharacter));

    if (AliveEnemies.Num() == 0 && !bCleared)
    {
        HandleRoomCleared();
    }
}

void AGJRoomBase::HandleRoomCleared()
{
    bCleared = true;

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s 클리어"), *GetName());

    OnRoomCleared.Broadcast(this);
}
