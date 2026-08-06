#include "BTService_UpdateTarget.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "GJEnemyCharacter.h"
#include "Kismet/GameplayStatics.h"

UBTService_UpdateTarget::UBTService_UpdateTarget()
{
    NodeName = TEXT("Update Target");
    Interval = 0.2f;
}

void UBTService_UpdateTarget::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

    AAIController* AIController = OwnerComp.GetAIOwner();
    if (!AIController)
    {
        return;
    }

    AGJEnemyCharacter* Enemy = Cast<AGJEnemyCharacter>(AIController->GetPawn());
    if (!Enemy)
    {
        return;
    }

    UBlackboardComponent* Blackboard = OwnerComp.GetBlackboardComponent();
    if (!Blackboard)
    {
        return;
    }

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(Enemy, 0);
    if (!PlayerPawn)
    {
        Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
        Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, false);
        return;
    }

    const float Distance = FVector::Dist(Enemy->GetActorLocation(), PlayerPawn->GetActorLocation());

    if (Distance <= Enemy->GetDetectionRange())
    {
        Blackboard->SetValueAsObject(TargetActorKey.SelectedKeyName, PlayerPawn);
    }
    else
    {
        Blackboard->ClearValue(TargetActorKey.SelectedKeyName);
    }

    Blackboard->SetValueAsBool(IsInAttackRangeKey.SelectedKeyName, Distance <= Enemy->GetAttackRange());
}
