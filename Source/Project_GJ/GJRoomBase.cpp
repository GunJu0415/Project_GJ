#include "GJRoomBase.h"
#include "GJBaseCharacter.h"
#include "GJEnemyCharacter.h"
// CreateDefaultSubobject<USceneComponent>에 완전한 타입이 필요하다.
// Actor.h가 전방 선언만 주는 경우가 있어 명시적으로 넣는다.
#include "Components/SceneComponent.h"
#include "GJRoomExitComponent.h"

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

    // 채우기보다 먼저 막는다. 적 0마리 방은 채우기 끝에 즉시 클리어되면서 다시 열리는데,
    // 순서가 반대면 열린 뒤에 막혀서 영구히 갇힌다.
    if (ShouldBlockExits())
    {
        SetExitsBlocked(true);
    }

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

    SetExitsBlocked(false);

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s 클리어 - 출구 개방"), *GetName());

    OnRoomCleared.Broadcast(this);
}

void AGJRoomBase::SetExitsBlocked(bool bBlocked)
{
    TArray<UGJRoomExitComponent*> Exits;
    GetComponents<UGJRoomExitComponent>(Exits);

    for (UGJRoomExitComponent* Exit : Exits)
    {
        if (Exit)
        {
            Exit->SetBlocked(bBlocked);
        }
    }
}
