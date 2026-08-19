#pragma once

#include "CoreMinimal.h"
#include "GJCombatRoom.h"
#include "GJBoxRoom.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class USceneComponent;
class UGJRoomExitComponent;

// 파라미터로 바닥과 벽을 만들어내는 그레이박스 방. 아트가 없어도 즉시 플레이된다.
//
// 모양을 하드코딩하지 않고 크기·문 폭을 받는 이유: 하드코딩하면 방 모양마다 C++
// 클래스가 생겨서 "모양은 BP, 역할은 데이터"라는 축이 깨진다. 파라미터면 새 모양이
// 새 클래스가 아니라 새 값이 된다.
//
// 실제 아트가 들어간 방은 여전히 AGJCombatRoom을 상속한 BP로 만든다. 둘은 공존한다.
UCLASS()
class PROJECT_GJ_API AGJBoxRoom : public AGJCombatRoom
{
    GENERATED_BODY()

public:
    AGJBoxRoom();

    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    // 벽 안쪽 치수. 스폰 포인트가 이 안에 들어와야 한다.
    UPROPERTY(EditAnywhere, Category = "Room|Box")
    FVector2D InteriorSize = FVector2D(1400.f, 1400.f);

    UPROPERTY(EditAnywhere, Category = "Room|Box")
    float WallHeight = 400.f;

    UPROPERTY(EditAnywhere, Category = "Room|Box")
    float WallThickness = 40.f;

    UPROPERTY(EditAnywhere, Category = "Room|Box")
    float FloorThickness = 20.f;

    // 북쪽(+Y) 벽에 이만큼 구멍을 낸다. 그 자리에 출구 컴포넌트가 들어간다.
    UPROPERTY(EditAnywhere, Category = "Room|Box")
    float DoorWidth = 300.f;

    // 기본값은 엔진 기본 큐브(100 유닛). 다른 메시를 쓰면 100 유닛 정육면체여야 한다.
    UPROPERTY(EditAnywhere, Category = "Room|Box")
    UStaticMesh* CubeMesh;

    void RebuildGeometry();

    // 출구 컴포넌트 하나와 그 자식 블로커를 만든다. 블로커는 출구의 자식이라
    // 출구를 열고 닫으면 함께 사라지고 나타난다.
    void AddDoorway(const FVector& Location, const FRotator& Rotation);

    // Parent가 null이면 루트에 붙인다. Size는 월드 유닛이고 스케일 변환은 내부에서 한다.
    UStaticMeshComponent* AddBox(FName ComponentName, const FVector& Size, const FVector& Location,
                                 USceneComponent* Parent = nullptr);

    // 다시 만들 때 이전 것을 지우려고 들고 있는다. 안 그러면 편집할 때마다 쌓인다.
    // 출구 컴포넌트도 같이 담기므로 USceneComponent로 받는다.
    UPROPERTY()
    TArray<USceneComponent*> GeneratedParts;
};
