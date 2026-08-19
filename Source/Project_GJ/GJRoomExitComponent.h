#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GJRoomExitComponent.generated.h"

// 방의 출구. 문을 막는 메시와 콜리전은 이 컴포넌트의 자식으로 붙이고,
// C++은 자식 전체의 표시와 콜리전만 토글한다.
//
// Task A에서는 막고 여는 데만 쓰이지만 Task B가 이 위에 세워진다. 던전 생성기는
// 출구의 위치와 전방 방향(GetForwardVector)을 알아야 다음 방을 이어붙일 수 있다.
// 전방(+X)이 방 바깥을 향하도록 배치한다.
UCLASS(ClassGroup = (GJ), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJRoomExitComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UGJRoomExitComponent();

    // 막을 때 자식을 보이고 충돌하게, 열 때 숨기고 통과하게 한다.
    UFUNCTION(BlueprintCallable, Category = "Room")
    void SetBlocked(bool bBlocked);
};
