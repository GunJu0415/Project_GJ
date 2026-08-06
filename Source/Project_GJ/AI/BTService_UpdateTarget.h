#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BTService_UpdateTarget.generated.h"

// 주기적으로 플레이어와의 거리를 확인해서 블랙보드의 TargetActor / IsInAttackRange 키를 갱신함.
// - GJEnemyCharacter::GetDetectionRange() 안이면 TargetActor에 플레이어를 채움 (밖이면 비움)
// - GJEnemyCharacter::GetAttackRange() 안이면 IsInAttackRange를 true로 설정
UCLASS()
class PROJECT_GJ_API UBTService_UpdateTarget : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_UpdateTarget();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector TargetActorKey;

    UPROPERTY(EditAnywhere, Category = "Blackboard")
    FBlackboardKeySelector IsInAttackRangeKey;
};
