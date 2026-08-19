#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GJGameTypes.h"
#include "GJRoomSpawnPointComponent.generated.h"

// 방 안에서 무언가가 스폰될 자리. 방 BP에 원하는 만큼 꽂고 용도를 드롭다운으로 정한다.
// 용도별로 컴포넌트를 셋으로 나누지 않는 이유: 그러면 점의 용도를 바꿀 때 컴포넌트를
// 지우고 다시 만들어야 한다. 문자열 태그가 아니라 전용 타입인 이유: 오타가 컴파일 타임에 걸린다.
UCLASS(ClassGroup = (GJ), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJRoomSpawnPointComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UGJRoomSpawnPointComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
    ESpawnPointType PointType = ESpawnPointType::Enemy;
};
