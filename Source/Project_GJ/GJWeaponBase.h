#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJGameTypes.h" // ★ 분리된 공통 타입 헤더(구조체) 인클루드
#include "GJWeaponBase.generated.h"

UCLASS(Abstract)
class PROJECT_GJ_API AGJWeaponBase : public AActor
{
    GENERATED_BODY()

public:
    AGJWeaponBase();

    // 에디터에서 데이터 테이블 변경 시 즉각 반영하기 위한 함수
    virtual void OnConstruction(const FTransform& Transform) override;


protected:
    // 무기의 최상위 루트 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;

    // 무기 외형을 담당할 스켈레탈 메시 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USkeletalMeshComponent* WeaponMesh;

    // [데이터 연동] 블루프린트에서 데이터 테이블과 Row Name을 지정할 핸들
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon Data")
    FDataTableRowHandle WeaponDataHandle;

    // 실제 게임 구동 및 에디터에서 테이블로부터 읽어와 저장할 스탯 구조체
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Stat")
    FWeaponStat WeaponStat;

    // 데이터 테이블에서 읽어온 공격 몽타주 캐싱
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Animation")
    UAnimMontage* AttackMontage;

public:
    // 외부(캐릭터 클래스 등)에서 무기의 스탯이나 애니메이션을 가져갈 때 사용할 Getter 함수들
    FORCEINLINE FWeaponStat GetWeaponStat() const { return WeaponStat; }
    FORCEINLINE UAnimMontage* GetAttackMontage() const { return AttackMontage; }
    FORCEINLINE USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }
};