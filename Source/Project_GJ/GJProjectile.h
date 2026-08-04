#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECT_GJ_API AGJProjectile : public AActor
{
    GENERATED_BODY()

public:
    AGJProjectile();

    // 풀(Pool)에서 꺼내서 발사할 때 호출하는 함수
    void FireInDirection(const FVector& ShootDirection);

    // 충돌하거나 수명이 다했을 때 풀(Pool)로 반환(숨김)하는 함수
    void Deactivate();

    // 현재 사용 중인 총알인지 확인
    bool IsActive() const { return bIsActive; }

protected:
    virtual void BeginPlay() override;

    // 타격 판정 (총알이 어딘가에 맞았을 때)
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    UProjectileMovementComponent* ProjectileMovement;

private:
    bool bIsActive;
};