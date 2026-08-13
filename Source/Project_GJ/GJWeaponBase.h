#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJGameTypes.h"
#include "GJInteractable.h"
#include "GJWeaponBase.generated.h"

class USphereComponent;
class AGJCharacter;

// 무기는 AGJCharacter::EquipWeapon()으로 스폰되는 시작 무기이자, 필드에 배치해두면 그대로
// E(상호작용)로 주울 수 있는 픽업 액터이기도 함(IGJInteractable 구현) - 습득되면 같은 액터
// 인스턴스가 그대로 캐릭터 손에 부착되는 것이라 새 오브젝트를 스폰하지 않음(무기는 스택되지 않고
// 같은 종류여도 각각 별개의 액터 인스턴스로 취급됨).
UCLASS(Abstract)
class PROJECT_GJ_API AGJWeaponBase : public AActor, public IGJInteractable
{
    GENERATED_BODY()

public:
    AGJWeaponBase();
    virtual void OnConstruction(const FTransform& Transform) override;

    // IGJInteractable - 필드에 놓인 무기를 E로 상호작용하면 캐릭터의 무기 슬롯에 장착됨
    virtual void Interact_Implementation(AGJCharacter* Interactor) override;

    // 습득(장착)되었을 때 호출 - 더 이상 필드 픽업으로 잡히지 않도록 상호작용 콜리전을 끄고,
    // Owner/Instigator를 주운 캐릭터로 세팅함(레벨에 미리 배치해둔 무기는 스폰 시 Instigator가
    // 없어서, 이걸 안 해주면 Fire()에서 GetInstigator()가 nullptr이라 크래시남)
    void OnPickedUp(AGJCharacter* NewOwner);

    // 무기 슬롯이 꽉 차서 밀려날 때 필드에 다시 떨어뜨릴 때 호출 - 지정된 위치로 옮기고
    // 상호작용 콜리전을 다시 켜서(다시 주울 수 있게) 필드 픽업 상태로 되돌림
    void OnDropped(const FVector& DropLocation);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    // 필드에 놓여 있을 때 상호작용(습득) 판정용 범위 - AGJItemBase의 InteractionCollision과
    // 동일한 방식("Trigger" 프로필). 장착되고 나면 OnPickedUp()에서 꺼버림.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionCollision;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Data")
    FDataTableRowHandle WeaponDataHandle;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Stat")
    FWeaponStat WeaponStat;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* AttackMontage;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* ReloadMontage;

    // 이 무기로 교체(스왑)할 때 재생할 몽타주 (임시로 아무 몽타주나 넣어서 써도 됨 - 비어있으면
    // 몽타주 없이 즉시 교체됨)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* SwapMontage;

public:
    FORCEINLINE FWeaponStat GetWeaponStat() const { return WeaponStat; }

    // 무기 교체 UI에 표시할 이름. FWeaponStat에는 이름 필드가 없고 데이터 테이블의
    // 행 이름이 곧 무기 ID라서 그걸 그대로 쓴다.
    UFUNCTION(BlueprintPure, Category = "Weapon Stat")
    FName GetWeaponRowName() const { return WeaponDataHandle.RowName; }

    FORCEINLINE UAnimMontage* GetAttackMontage() const { return AttackMontage; }
    FORCEINLINE UAnimMontage* GetReloadMontage() const { return ReloadMontage; }
    FORCEINLINE UAnimMontage* GetSwapMontage() const { return SwapMontage; }
    FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

    // 캐릭터가 호출할 사격 함수 (선언)
    virtual void Fire();

};
