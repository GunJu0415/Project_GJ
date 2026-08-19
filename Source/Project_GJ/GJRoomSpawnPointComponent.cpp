#include "GJRoomSpawnPointComponent.h"

UGJRoomSpawnPointComponent::UGJRoomSpawnPointComponent()
{
    // 자리 표시일 뿐이라 틱할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;
}
