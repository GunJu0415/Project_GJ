#pragma once

#include "CoreMinimal.h"
#include "GJBaseCharacter.h" // 부모 클래스 포함
#include "GJEnemyCharacter.generated.h"

UCLASS()
class PROJECT_GJ_API AGJEnemyCharacter : public AGJBaseCharacter
{
    GENERATED_BODY()

public:
    AGJEnemyCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
};