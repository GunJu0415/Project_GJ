#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "GJProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECT_GJ_API AGJProjectile : public AActor
{
    GENERATED_BODY()

public:
    AGJProjectile();

    // 풀(Pool)에서 꺼내서 발사할 때 호출하는 함수. 데미지/속도/사거리를 매번 새로 받아서 세팅합니다.
    // InScale: 액터 전체 크기 배율. 콜리전과 메시가 같이 커진다(스킬 차징용, 총알은 1.0).
    // InPierceCount: 추가로 관통하는 적 수. 0=1명만, 1=2명, -1=무한.
    void FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange,
                         float InScale = 1.f, int32 InPierceCount = 0);

    // 충돌하거나 수명이 다했을 때 풀(Pool)로 반환(숨김)하는 함수
    void Deactivate();

    // 현재 사용 중인 총알인지 확인
    bool IsActive() const { return bIsActive; }

protected:
    virtual void BeginPlay() override;

    // 타격 판정 (총알이 어딘가에 맞았을 때)
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

    // 관통 구체는 폰을 블록하지 않고 통과하므로 Hit이 아니라 Overlap으로 들어온다.
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // Hit과 Overlap 양쪽에서 부르는 공통 타격 처리. 데미지를 주고 관통을 소모한다.
    void HandleTouch(AActor* OtherActor);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* CollisionComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* MeshComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement")
    UProjectileMovementComponent* ProjectileMovement;

private:
    bool bIsActive;
    float Damage;

    // 남은 관통 수. -1은 무한이며 감소시키지 않는다.
    int32 RemainingPierce = 0;

    // 이번 발사에서 이미 때린 대상. 큰 구체는 한 적의 콜리전 안에 여러 프레임 머물기 때문에
    // 이게 없으면 같은 적을 프레임마다 재타격한다.
    UPROPERTY()
    TSet<AActor*> HitActors;

    // 사거리만큼 날아가면 자동으로 Deactivate 시키는 타이머
    FTimerHandle RangeTimerHandle;
};
