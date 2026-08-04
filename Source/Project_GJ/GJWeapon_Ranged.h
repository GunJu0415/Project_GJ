#pragma once

#include "CoreMinimal.h"
#include "GJWeaponBase.h"
#include "GJWeapon_Ranged.generated.h"

class AGJProjectile;

UCLASS()
class PROJECT_GJ_API AGJWeapon_Ranged : public AGJWeaponBase
{
    GENERATED_BODY()

public:
    AGJWeapon_Ranged();

    // 캐릭터의 애님 노티파이에서 호출할 공격 함수
    virtual void FireWeapon();

protected:
    virtual void BeginPlay() override;

    // 오브젝트 풀 생성 함수
    void CreateProjectilePool();

    // 풀에서 쉬고 있는(비활성화된) 총알을 찾는 함수
    AGJProjectile* GetAvailableProjectile();

protected:
    // 발사할 투사체 블루프린트 클래스
    UPROPERTY(EditDefaultsOnly, Category = "Pooling")
    TSubclassOf<AGJProjectile> ProjectileClass;

    // 생성해둘 총알 개수
    UPROPERTY(EditDefaultsOnly, Category = "Pooling")
    int32 PoolSize = 30;

    // 총알을 담아둘 배열 (Object Pool)
    UPROPERTY()
    TArray<AGJProjectile*> ProjectilePool;
};