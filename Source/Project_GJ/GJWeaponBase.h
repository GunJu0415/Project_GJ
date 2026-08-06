#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJGameTypes.h"
#include "GJWeaponBase.generated.h"

UCLASS(Abstract)
class PROJECT_GJ_API AGJWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AGJWeaponBase();
    virtual void OnConstruction(const FTransform& Transform) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Data")
    FDataTableRowHandle WeaponDataHandle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Stat")
    FWeaponStat WeaponStat;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* AttackMontage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* ReloadMontage;

public:
    FORCEINLINE FWeaponStat GetWeaponStat() const { return WeaponStat; }
    FORCEINLINE UAnimMontage* GetAttackMontage() const { return AttackMontage; }
    FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
    FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

    // 캐릭터가 호출할 사격 함수 (선언)
    virtual void Fire();

};
