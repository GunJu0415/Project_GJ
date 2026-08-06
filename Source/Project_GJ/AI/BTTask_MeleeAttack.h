#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_MeleeAttack.generated.h"

// GJEnemyCharacter::PerformAttack()을 호출하는 태스크 (쿨다운/데미지 판단은 그쪽에서 처리함)
UCLASS()
class PROJECT_GJ_API UBTTask_MeleeAttack : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_MeleeAttack();

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
