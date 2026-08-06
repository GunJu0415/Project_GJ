#include "BTTask_MeleeAttack.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "AIController.h"
#include "GJEnemyCharacter.h"

UBTTask_MeleeAttack::UBTTask_MeleeAttack()
{
    NodeName = TEXT("Melee Attack");
}

EBTNodeResult::Type UBTTask_MeleeAttack::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return EBTNodeResult::Failed;
    }

    AGJEnemyCharacter* Enemy = Cast<AGJEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        return EBTNodeResult::Failed;
    }

    Enemy->PerformAttack();
    return EBTNodeResult::Succeeded;
}
