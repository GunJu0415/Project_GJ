#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GJGameTypes.generated.h" // 이름 맞춰주기

// FCardData가 TSubclassOf로만 참조하므로 전방 선언으로 충분하다.
// 여기서 GJWeaponBase.h를 include하면 GJWeaponBase.h가 GJGameTypes.h를 다시 include해서
// 순환이 된다.
class AGJWeaponBase;

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

    // 스킬 데미지에만 쓰이는 공격력. BaseAttackPower와 나눈 이유는 평타 특화와
    // 스킬 특화 빌드를 갈라놓기 위해서다 - 하나로 합치면 카드가 항상 양쪽을 같이 올린다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float SkillPower = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float RequiredEXP = 100.0f;

    // 받는 데미지를 경감시킨다. 체감형이라 아무리 올려도 무적이 되지 않음 (100이면 50% 경감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    // 캐릭터 이동 속도. 지금까지 플레이어는 이 값을 어디서도 설정하지 않아 엔진 기본값(600)을
    // 그대로 썼는데, 기본값을 600으로 맞춰 두었으므로 기존 플레이 감각은 바뀌지 않는다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MoveSpeed = 600.f;

    // 스킬 쿨타임 감소. 적용 대상이 될 스킬 시스템이 아직 없어서 지금은 어디에도 연결되지 않는다.
    // 나중에 스탯을 또 추가하면 데이터 테이블 마이그레이션을 두 번 해야 하므로 미리 자리를 잡아둔 것.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CooldownReduction = 0.f;

    // 치명타 확률. 0.0~1.0 범위다 (0.25 = 25%). 퍼센트 정수가 아님에 주의.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    // 치명타가 터졌을 때 데미지 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
};

// -----------------------------------------
// 스탯 보너스 (카드/버프가 더하는 값)
// -----------------------------------------

// 전부 0에서 시작하는 스탯 값 묶음.
// FCharacterStat을 재사용하지 않는 이유: 그쪽 기본값이 MaxHP=100, MoveSpeed=600,
// CritMultiplier=2라서 "보너스 없음"을 표현할 수 없다. 보너스 구조체는 기본 생성했을 때
// 아무 효과가 없어야 한다.
// FCharacterStat의 9개 필드를 전부 미러링한다 - 일부만 지원하면 "이 스탯은 왜 카드로 못
// 올리지?"라는 비대칭이 생기고, 나중에 필드를 추가하면 USTRUCT 레이아웃이 바뀌어 그때는
// 이 구조체를 쓰는 데이터 테이블(M2.6의 DT_CardData)까지 영향을 받는다.
USTRUCT(BlueprintType)
struct FStatValues
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxMP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float SkillPower = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float RequiredEXP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MoveSpeed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CooldownReduction = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 0.f;

    // 모디파이어를 합칠 때 쓴다. 필드별 단순 덧셈이다.
    FStatValues& operator+=(const FStatValues& Other);
};

// 카드/버프 하나가 주는 효과.
// Percent는 1.0이 아니라 0에서 시작하는 증가율이다(0.15 = +15%). 이 선택이 두 가지를
// 공짜로 만든다 - 기본 생성한 모디파이어가 무효과가 되고, 모디파이어를 합치는 게 그냥
// 필드 덧셈이 된다.
// 증가율은 곱하지 않고 합산한다: +15% 두 장이면 1.30이지 1.3225가 아니다. 합산이
// 밸런싱이 예측 가능하고, 카드를 많이 먹었을 때 지수적으로 터지지 않는다.
USTRUCT(BlueprintType)
struct FStatModifier
{
    GENERATED_BODY()

    // 가산 (+5 체력)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Add;

    // 증가율 (0.15 = +15%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Percent;
};

// -----------------------------------------
// 카드 (레벨업 선택지)
// -----------------------------------------

UENUM(BlueprintType)
enum class ECardEffectType : uint8
{
    // StatEffect를 AddStatBonus로 넘긴다
    StatBonus   UMETA(DisplayName = "스탯 보너스"),
    // WeaponClass를 스폰해서 지급한다
    GrantWeapon UMETA(DisplayName = "무기 획득"),
    // 미구현 - 스킬 시스템(M2.7)이 생기기 전까지는 골라도 경고만 찍힌다
    Ability     UMETA(DisplayName = "능력 획득 (미구현)")
};

// 카드 한 장의 정의. 행 이름이 곧 카드 ID다.
USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    UTexture2D* Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    ECardEffectType EffectType = ECardEffectType::StatBonus;

    // EffectType == StatBonus일 때만 쓰인다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FStatModifier StatEffect;

    // EffectType == GrantWeapon일 때만 쓰인다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    TSubclassOf<AGJWeaponBase> WeaponClass;

    // 이 카드가 속한 트리/계열 (예: Tree.Fire, Weapon.Gun). 여러 개 달아도 된다.
    // 플레이어가 타고 있는 트리의 카드가 더 자주 뜨게 하는 데 쓰인다
    // (UGJCardComponent::TagWeightMultipliers).
    // FName이 아니라 게임플레이 태그인 이유는 계층 때문이다 - Tree.Fire 배율 하나가
    // Tree.Fire.Shotgun 카드까지 자동으로 밀어준다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FGameplayTagContainer CardTags;

    // false면 한 번 고른 뒤 풀에서 영구 제외된다(무기나 고유 효과용).
    // true면 여러 번 등장할 수 있어 "같은 카드를 쌓아 빌드를 밀어붙이는" 플레이가 가능하다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    bool bStackable = true;

    // 가중 랜덤의 가중치. 0 이하면 절대 안 뽑힌다(카드를 임시로 끄는 용도로도 쓸 수 있다).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    float Weight = 1.f;
};

// 선택지 위젯에 넘기는 표시용 데이터.
// 위젯이 FCardData를 직접 알면 무기 교체 화면을 따로 만들어야 하므로, 표시에 필요한
// 것만 담은 이 구조체로 한 겹 끊는다.
USTRUCT(BlueprintType)
struct FGJChoiceEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    UTexture2D* Icon = nullptr;
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

    // 인벤토리 무기 페이지 등 UI에 표시할 2D 아이콘
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UTexture2D* WeaponIcon = nullptr;

    // 이 무기로 교체(스왑)할 때 재생할 몽타주. 비어있으면 몽타주 없이 즉시 교체됨.
    // 전용 스왑 애님이 아직 없으면 임시로 아무 몽타주나 넣어서 써도 됨 - 무기마다 다른 스왑 모션을
    // 주고 싶으면 각 행에 별도 에셋을, 원거리/근접처럼 묶어서 공유하고 싶으면 같은 에셋을 여러 행에
    // 지정하면 됨(코드에서 무기 종류를 구분하지 않고 데이터로만 제어)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asset")
    UAnimMontage* SwapMontageAsset = nullptr;
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

    // 받는 데미지를 경감시킨다. 적마다 단단함을 다르게 줄 수 있다 (100이면 50% 경감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    // 치명타 확률. 0.0~1.0 범위다 (0.25 = 25%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;

    // 이 적을 죽였을 때 플레이어가 얻는 경험치. 적 레벨 같은 다른 값에서 유도하지 않고
    // 적마다 명시한다 - 유도하면 "좀 더 단단하게" 같은 밸런스 조정이 성장 속도까지 같이
    // 바꿔버리고, "체력만 많은 샌드백"이나 "약한데 빠른 견제형" 같은 적을 표현할 수 없다.
    // float인 이유: 비교 대상인 FCharacterStat::RequiredEXP가 float이라 파이프라인을 통일한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float ExpReward = 10.f;
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
