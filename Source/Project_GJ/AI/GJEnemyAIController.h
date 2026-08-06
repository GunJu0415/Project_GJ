#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GJEnemyAIController.generated.h"

class UBehaviorTree;

// 기본 잡몹(GJEnemyCharacter)이 빙의하는 AI 컨트롤러.
// BehaviorTreeAsset을 에디터에서 할당하면 빙의 시 자동으로 해당 비헤이비어 트리를 실행함.
UCLASS()
class PROJECT_GJ_API AGJEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:
    AGJEnemyAIController();

protected:
    virtual void OnPossess(APawn* InPawn) override;

    // 이 컨트롤러가 실행할 비헤이비어 트리 (에디터에서 BT_GJEnemy 같은 에셋을 만들어 할당해야 함)
    UPROPERTY(EditDefaultsOnly, Category = "AI")
    UBehaviorTree* BehaviorTreeAsset;
};
