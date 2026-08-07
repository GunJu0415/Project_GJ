#pragma once

#include "CoreMinimal.h"
#include "GJWeaponBase.h"
#include "GJWeapon_Ranged.generated.h"

class AGJProjectile;

// 탄약이 바뀔 때마다(발사/재장전 완료) 브로드캐스트 - UI(UMG)에서 폴링 없이 바인딩해서 쓰기 위함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAmmoChangedSignature, int32, CurrentAmmo, int32, MaxAmmo);

UCLASS()
class PROJECT_GJ_API AGJWeapon_Ranged : public AGJWeaponBase
{
    GENERATED_BODY()

public:
    AGJWeapon_Ranged();

    virtual void Fire() override;

    UPROPERTY(BlueprintAssignable, Category = "Weapon|Ammo")
    FOnAmmoChangedSignature OnAmmoChanged;

    void CreateProjectilePool();
    class AGJProjectile* GetAvailableProjectile();

    // 재장전 가능 여부 (재장전 중이 아니고, 탄창이 가득 차있지 않을 때)
    UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
    bool CanReload() const;

    // 재장전 시작 (탄창을 채우는 시점은 FinishReload에서 처리 - 몽타주/타이머 종료 후 호출됨)
    // InBulletsToRefill: 실제로 채워질 발수. MP가 부족하면 필요한 발수보다 적게 넘어올 수 있음(부분 재장전).
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    void StartReload(int32 InBulletsToRefill);

    // 재장전 완료 처리 (탄창을 최대치로 채움)
    UFUNCTION(BlueprintCallable, Category = "Weapon|Ammo")
    void FinishReload();

    UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
    bool IsReloading() const { return bIsReloading; }

    UFUNCTION(BlueprintPure, Category = "Weapon|Ammo")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

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

protected:
    // 현재 탄창에 남은 탄약 수 (WeaponStat.MagazineSize로 초기화됨)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
    int32 CurrentAmmo = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
    bool bIsReloading = false;

    // 이번 재장전에서 실제로 채워질 발수 (MP가 허락하는 한도까지만 - StartReload에서 세팅, FinishReload에서 사용 후 초기화)
    int32 PendingRefillAmount = 0;

    // 마지막으로 발사한 시각 (WeaponStat.FireInterval과 비교해 연사 속도를 제한하는 데 사용)
    float LastFireTime = -100.f;
};
