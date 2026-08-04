#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GJBaseCharacter.generated.h"

class UCharacterStateComponent;
class UMotionWarpingComponent;
class UAbilitySystemComponent;

UCLASS()
class PROJECT_GJ_API AGJBaseCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGJBaseCharacter();

protected:
    virtual void BeginPlay() override;

public:    
    virtual void Tick(float DeltaTime) override;

    // ==========================================
    // 플레이어와 적이 공통으로 사용하는 컴포넌트
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCharacterStateComponent* StateComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UMotionWarpingComponent* MotionWarpingComponent;

    // 향후 GAS(어빌리티 시스템) 도입 시 사용할 공통 인터페이스
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const;
};