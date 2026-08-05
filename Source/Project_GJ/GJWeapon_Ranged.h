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

    virtual void Fire() override;

    void CreateProjectilePool();
    class AGJProjectile* GetAvailableProjectile();

protected:
    virtual void BeginPlay() override;

public:
    //  부모에서 이사 온 풀링 관련 변수들
    UPROPERTY(EditDefaultsOnly, Category = "Pooling")
    TSubclassOf<AGJProjectile> ProjectileClass;

    UPROPERTY(EditDefaultsOnly, Category = "Pooling")
    int32 PoolSize = 30;

    UPROPERTY()
    TArray<AGJProjectile*> ProjectilePool;
};
