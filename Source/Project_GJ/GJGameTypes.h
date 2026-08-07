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

    // 최대 MP. 재장전 시 무기 데이터 테이블의 MPCostPerAmmo x 채워지는 발수만큼 소비됨.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxMP = 50.0f;

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

    // 총알 1발당 소비되는 MP. 재장전 시 실제로 채워지는 발수(MagazineSize - 남은 탄약)만큼 곱해져 소비됨.
    // 플레이어의 MaxMP/CurrentMP는 DT_CharacterStat(FCharacterStat)에서 관리됨.
    // 밸런스 요소: 강한 무기일수록 이 값을 크게 잡아 재장전 남발을 막을 수 있음.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponStat")
    float MPCostPerAmmo = 1.f;

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

    // 공격 판정(데미지 적용)까지의 선딜레이(초). 재생 중인 몽타주의 실제 타격 타이밍에 맞춰 조절.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float AttackWindup = 0.3f;
};

// -----------------------------------------
// 아이템 종류
// -----------------------------------------
UENUM(BlueprintType)
enum class EItemType : uint8
{
    Consumable,     // 소비형 (포션 등 - 사용하면 사라짐)
    Equipment,      // 장비형
    Material,       // 재료
    Quest,          // 퀘스트 아이템
    Misc            // 기타
};

// -----------------------------------------
// 아이템 데이터 테이블 구조체 (행 이름 = 아이템 ID로 사용)
// -----------------------------------------
USTRUCT(BlueprintType)
struct FItemData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    EItemType ItemType = EItemType::Misc;

    // 한 슬롯에 최대로 겹쳐 쌓일 수 있는 개수 (1이면 스택 불가 아이템)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 MaxStackSize = 99;

    // 상점에 팔 때 받는 금액
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 SellPrice = 0;

    // 상점에서 살 때 지불하는 금액
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    int32 BuyPrice = 0;

    // 사용 시 회복되는 HP량 (0이면 HP 회복 없음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    float HealAmount = 0.f;

    // 사용 시 회복되는 MP량 (0이면 MP 회복 없음)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    float ManaRecoverAmount = 0.f;

    // true면 새 회차(런)를 시작해도 인벤토리에서 사라지지 않음(예: 영구 재화/장비).
    // false면 회차가 바뀌면 사라짐(예: 일반 소모품/재료) - 기본값
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
    bool bPersistAcrossRuns = false;

    // 인벤토리 UI 등에 쓸 아이템 아이콘(2D 이미지)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UTexture2D* Icon = nullptr;

    // 월드에 배치될 때 쓸 스태틱 메시. AGJItemBase가 OnConstruction에서 자동으로 반영함
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UStaticMesh* ItemMeshAsset = nullptr;
};
