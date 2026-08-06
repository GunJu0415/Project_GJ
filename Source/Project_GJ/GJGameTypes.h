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

    // 연사 속도 제어: 이 시간(초)마다 최대 1발까지 발사 가능 (예: 0.1 = 초당 10발)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    float FireInterval = 0.1f;

    // 탄창 용량. 총알 자체는 무제한이지만 이 발수를 다 쏘면 재장전(R)이 필요함.
    // 밸런스 요소: 강한 무기일수록 이 값을 작게 잡아 재장전을 자주 하도록 유도.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    int32 MagazineSize = 30;

    // 재장전 소요 시간(초). ReloadMontageAsset이 지정되어 있으면 몽타주 재생이 우선되고,
    // 몽타주가 없을 때는 이 값만큼 타이머로 대체 재생됨.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    float ReloadTime = 1.5f;

    // 에셋 정보도 여기에 통합
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    USkeletalMesh* WeaponMeshAsset;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UAnimMontage* AttackMontageAsset;

    // 재장전 몽타주 (아직 없다면 비워두면 됨 - 비어있으면 ReloadTime 타이머로 대체됨)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UAnimMontage* ReloadMontageAsset;
};

// -----------------------------------------
// 적(잡몹) 스탯 데이터 테이블 구조체
// -----------------------------------------
USTRUCT(BlueprintType)
struct FEnemyStat : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 50.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackDamage = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackRange = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float DetectionRange = 800.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackCooldown = 1.5f;

    // 추적 이동 속도 (CharacterMovement의 MaxWalkSpeed에 적용됨)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MoveSpeed = 300.f;
};
