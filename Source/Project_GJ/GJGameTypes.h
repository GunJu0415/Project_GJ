#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GJGameTypes.generated.h" // 이름 맞춰주기

// -----------------------------------------
// 캐릭터 스탯 데이터 테이블 구조체
// -----------------------------------------
USTRUCT(BlueprintType)
struct FCharacterStat : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float RequiredEXP = 100.0f;
};

// -----------------------------------------
// 무기 스탯 데이터 테이블 구조체
// -----------------------------------------
USTRUCT(BlueprintType)
struct FWeaponStat : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseDamage = 15.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackSpeedRate = 1.0f;

    // 총알이 날아가는 속도 (직선 비행, 초당 유닛)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    float ProjectileSpeed = 3000.f;

    // 총알의 최대 사거리 (이만큼 날아가면 자동으로 비활성화되어 풀로 돌아감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    float Range = 2000.f;

    // 에셋 정보도 여기에 통합
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    USkeletalMesh* WeaponMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UAnimMontage* AttackMontageAsset;
};
