#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GJGameTypes.h"
#include "GJSkillComponent.generated.h"

class AGJCharacter;
class AGJProjectile;

// 스킬 슬롯 수. 우클릭 / Q / R 세 개다.
#define GJ_SKILL_SLOT_COUNT 3

// 슬롯 내용(어느 칸에 무슨 스킬)이 바뀌었다. HUD와 인벤토리가 구독한다.
// 쿨타임은 여기 안 태운다 - 매 프레임 바뀌는 값이라 델리게이트로 밀면 방송만 하다 끝난다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillSlotsChangedSignature);

// UPROPERTY TMap은 값으로 TArray를 직접 담지 못해서 한 겹 감싼다.
USTRUCT()
struct FGJProjectilePool
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<AGJProjectile*> Projectiles;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGJSkillComponent();

    // 입력 전달. 캐릭터가 IA_Skill1/2/3의 Started/Completed에서 부른다.
    void OnSkillPressed(int32 SlotIndex);
    void OnSkillReleased(int32 SlotIndex);

    // 빈 슬롯에 장착한다. 슬롯이 다 찼으면 false를 돌려주고 아무것도 안 한다 -
    // 호출자(카드 컴포넌트)가 그때 교체 선택지를 띄운다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool EquipSkill(FName SkillId);

    // 지정한 슬롯을 덮어쓴다. 교체 선택 결과를 적용하는 경로다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void EquipSkillInSlot(int32 SlotIndex, FName SkillId);

    // 차징 중이면 true. 캐릭터의 입력 핸들러가 이걸 보고 다른 동작을 막는다.
    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsCharging() const { return ChargingSlot != INDEX_NONE; }

    // 차징을 버린다. MP도 쿨타임도 소모하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void CancelCharge();

    // 슬롯에 장착된 스킬 ID. 비었으면 NAME_None.
    UFUNCTION(BlueprintPure, Category = "Skill")
    FName GetSkillInSlot(int32 SlotIndex) const;

    // 테이블에서 스킬 정의를 찾는다. 없으면 nullptr.
    const FSkillData* FindSkill(FName SkillId) const;

    // 슬롯 3개의 상태를 로그로 출력한다. AGJCharacter의 GJSkillInfo가 부른다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void LogSkillInfo() const;

    UPROPERTY(BlueprintAssignable, Category = "Skill")
    FOnSkillSlotsChangedSignature OnSkillSlotsChanged;

    // 0 = 준비됨, 1 = 방금 썼음. 빈 슬롯이나 쿨타임 0인 스킬도 0.
    // UI가 매 프레임 물어본다.
    UFUNCTION(BlueprintPure, Category = "Skill")
    float GetCooldownRatio(int32 SlotIndex) const;

    // 두 슬롯의 스킬과 쿨타임을 함께 맞바꾼다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void SwapSkillSlots(int32 SlotA, int32 SlotB);

    // 슬롯 번호에 대응하는 키 이름. 지금까지 이 문자열이 여러 곳에 하드코딩돼 있었다.
    UFUNCTION(BlueprintPure, Category = "Skill")
    static FText GetSlotKeyLabel(int32 SlotIndex);

protected:
    virtual void BeginPlay() override;

    // 스킬 정의 테이블 (DT_SkillData). 비어 있으면 스킬 시스템 전체가 조용히 꺼진다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    UDataTable* SkillTable;

    // ProjectileClass가 비어 있는 스킬이 쓸 기본 구체 (BP_GJSkillProjectile)
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    TSubclassOf<AGJProjectile> DefaultProjectileClass;

    // 캐릭터 기준 발사 위치 오프셋 (X=전방, Y=우측, Z=상방).
    // 무기의 MuzzleSocket을 안 쓰는 이유: 스킬은 맨손이어도 나가야 한다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    FVector MuzzleOffset = FVector(60.f, 0.f, 40.f);

    // 구체 클래스마다 만드는 풀의 최대 크기. 스킬은 쿨타임이 있어 동시에 떠 있는 수가
    // 무기(30)보다 훨씬 적다. 모자라면 그 발사만 무시된다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    int32 PoolSizePerClass = 10;

    // 슬롯별 장착 스킬 ID. 생성자에서 GJ_SKILL_SLOT_COUNT개로 채운다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    TArray<FName> EquippedSkills;

    // 슬롯별 쿨타임이 끝나는 월드 시각. 지금 시각보다 작으면 사용 가능.
    TArray<float> CooldownEndTime;

    // 차징을 시작한 월드 시각. ChargingSlot이 INDEX_NONE이면 의미 없다.
    float ChargeStartTime = 0.f;

    // 지금 차징 중인 슬롯. INDEX_NONE이면 차징 안 함.
    int32 ChargingSlot = INDEX_NONE;

    // 구체 클래스별 풀. 스킬마다 비주얼이 다를 수 있어 하나로 못 묶는다.
    UPROPERTY()
    TMap<TSubclassOf<AGJProjectile>, FGJProjectilePool> ProjectilePools;

    AGJCharacter* GetOwnerCharacter() const;

    // 비활성 구체를 꺼내온다. 없으면 풀 크기 한도 내에서 새로 스폰한다.
    AGJProjectile* GetPooledProjectile(TSubclassOf<AGJProjectile> ProjClass);

    // 실제 발사. ChargeRatio는 0~1이다.
    void FireSkill(int32 SlotIndex, const FSkillData& Skill, float ChargeRatio);
};
