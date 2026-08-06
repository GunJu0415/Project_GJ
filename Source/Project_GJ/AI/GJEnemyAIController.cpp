#include "GJEnemyAIController.h"
#include "BehaviorTree/BehaviorTree.h"

AGJEnemyAIController::AGJEnemyAIController()
{
}

void AGJEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (BehaviorTreeAsset)
    {
        RunBehaviorTree(BehaviorTreeAsset);
    }
}
