#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingComponent.h"
#include "GJBaseCharacter.h"
#include "InputActionValue.h"
#include "GJGameTypes.h"
#include "TimerManager.h"
#include "GJCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputComponent;
class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
class UDataTable;
class AGJWeaponBase;
class UCharacterStateComponent;
class UUserWidget;
class UGJPlayerHUDWidget;
class UGJInventoryComponent;
class UGJCardComponent;
class UGJSkillComponent;
class UGJInventoryWidget;

enum class EDodgeType
{
    Forward,
    Backward,
    Left,
    Right
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponSlotsChangedSignature);

// 지금 손에 든 무기가 원거리 무기일 때, 그 무기의 탄약이 바뀔 때마다(발사/재장전) 또는 무기
// 자체가 바뀔 때(스왑)마다 브로드캐스트됨. 탄약 UI는 특정 무기 인스턴스의 OnAmmoChanged에
// 직접 바인딩하지 않고 이 델리게이트 하나만 구독하면 됨 - 어떤 무기로 스왑되든 재바인딩은
// AGJCharacter::CommitWeaponSwap이 알아서 처리함.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveWeaponAmmoChangedSignature, int32, CurrentAmmo, int32, MaxAmmo);

// 레벨이 오를 때마다 브로드캐스트됨. 지금은 구독자가 없지만, 레벨업 시 카드 3장이 떠서 하나를
// 고르는 선택 시스템이 붙을 자리다 - 그때 이 델리게이트 하나만 구독하면 된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUpSignature, int32, NewLevel);

UCLASS()
class PROJECT_GJ_API AGJCharacter : public AGJBaseCharacter
{
    GENERATED_BODY()

public:
    AGJCharacter();

protected:
    virtual void BeginPlay() override;
    virtual void HandleDeath() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(BlueprintReadOnly, Category = "Animation")
    FVector2D MoveInput;

protected:
    void UpdateMouseState();
    void UpdateCharacterRotation();
    void UpdateCameraOffset(float DeltaTime);
    void ApplyCameraOffset();

    void Move(const FInputActionValue& Value);

    // 이동 입력이 없어질 때(키를 뗄 때) MoveInput을 0으로 되돌림 - 안 그러면 마지막으로 눌렀던
    // 방향이 계속 남아있어서, 방향 입력 없이 닷지할 때 엉뚱하게 그 방향으로 닷지되는 문제가 있었음
    void MoveInputReleased();

protected:
    EDodgeType DodgeType;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCameraComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DodgeAction;

    // ==========================================
    // [신규] 콤보 공격 입력
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* AttackAction;

    // ==========================================
    // [신규] 재장전 입력 (R키). 에디터에서 IA_Reload 에셋을 만들어 R키에 매핑한 뒤
    // BP_GJCharacter 디테일 패널에서 이 프로퍼티에 할당해야 동작함.
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ReloadAction;

    // ==========================================
    // [신규] 상호작용 입력 (아이템 습득 / 나중에 문·버튼 등에도 재사용).
    // BP_GJCharacter 디테일 패널에서 만들어둔 IA_Interact를 할당해야 동작함.
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InteractAction;

    // ==========================================
    // [신규] 인벤토리 열기/닫기 입력 (Tab). 에디터에서 만들어둔 IA를 할당해야 동작함.
    // ==========================================
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* InventoryToggleAction;

    // 카메라 오프셋 설정 변수
    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float MaxCameraOffset = 250.f;

    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float CameraOffsetInterpSpeed = 2.7f;

    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float CameraOffsetDeadzone = 0.3f;

    bool bIsMouseInsideViewport;
    float CurrentMouseX;
    float CurrentMouseY;
    int32 ViewportSizeX;
    int32 ViewportSizeY;

    FVector CurrentWorldOffset;
    FVector DesiredWorldOffset;
    FRotator LastValidRotation;

public:
    FORCEINLINE UCharacterStateComponent* GetStateComponent() const { return StateComponent; }

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DodgeForwardMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DodgeRightMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DodgeLeftMontage;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* DodgeBackwardMontage;

    void PerformDodge();

    // [수정] 범용 몽타주 종료 처리 함수로 이름 변경
    UFUNCTION()
    void OnMontageEndedEvent(UAnimMontage* Montage, bool bInterrupted);

    // ==========================================
    // [신규] 콤보 공격 관련 변수 및 함수
    // ==========================================
    int32 CurrentComboCount;
    bool bHasNextComboInput;

    void AttackInputPressed();

    // 원거리 무기 연사 시도 (Tick에서 매 프레임 호출됨 - 실제 발사 간격 판단은 무기의 Fire() 내부 쿨다운이 담당)
    void TryAutoFire();

    // 공격 입력을 뗐을 때 (연사 정지용)
    void AttackInputReleased();

    // 공격 버튼을 누르고 있는 동안 true - Tick에서 이 값을 보고 TryAutoFire()를 계속 시도함
    bool bIsAutoFiring = false;

    // ==========================================
    // [신규] 재장전 관련 함수 및 타이머
    // ==========================================
    void ReloadInputPressed();

    // 재장전 종료 처리 (몽타주 종료 콜백 / 몽타주가 없을 때는 타이머 콜백에서 공통으로 호출)
    void CompleteReload();

    // ReloadMontageAsset이 비어있을 때 ReloadTime만큼 대체 재생하는 타이머
    FTimerHandle ReloadTimerHandle;

    // ==========================================
    // [신규] 상호작용 (아이템 습득 등)
    // ==========================================
    // 상호작용 범위(콜리전) 안에 있는 IGJInteractable 구현 액터를 찾아 Interact()를 호출함
    void InteractInputPressed();

public:
    // 애니메이션 노티파이에서 호출할 브릿지 함수들
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AdvanceCombo();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetCombo();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void PerformFire();

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Stat")
    UDataTable* CharacterStatTable;

    // --- 스탯 3층 구조 ---
    // 아래 세 멤버는 각자 쓰는 주체가 하나씩만 있다. 이 규칙이 깨지면 보너스가 조용히 사라진다.

    // (1) 테이블 원본. UpdateCharacterStat만 쓴다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat BaseStat;

    // (2) 카드/버프가 누적한 보너스. AddStatBonus만 쓴다.
    // 런마다 캐릭터가 새로 스폰되면서 기본 생성되므로 초기화 코드가 따로 없다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FStatModifier StatBonus;

    // (3) 실효값 = (BaseStat + StatBonus.Add) x (1 + StatBonus.Percent).
    // RecalculateStats만 쓰고, AddEXP/UpdatePlayerHUD/GetAttackPower가 읽는다.
    // 예전에는 이 멤버가 테이블 원본이었다 - 의미가 "실효값"으로 바뀐 것이므로,
    // 여기에 직접 대입하는 코드를 새로 만들면 안 된다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat CurrentCharacterStat;

    // MP - 재장전할 때마다 소비됨 (소비량 = 실제로 채워지는 발수 x 장착 무기의 WeaponStat.MPCostPerAmmo)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    float MaxMP = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    float CurrentMP = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    int32 CurrentLevel;

    // 현재 레벨에서 쌓은 경험치. 누적 총량이 아니라 "이번 레벨의 진행도"다 -
    // 레벨업할 때 CurrentCharacterStat.RequiredEXP만큼 빼고 초과분을 이월한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level")
    float CurrentEXP = 0.f;

    // 다음 레벨의 스탯을 적용하고 OnLevelUp을 쏜다. AddEXP 내부에서만 호출된다.
    void LevelUp();

    // bRestoreToFull=true면 HP/MP를 가득 채운다(스폰/리스폰용, 기존 동작 그대로).
    // false면 최대치가 오른 만큼만 현재값에 더한다(레벨업용) - 레벨업이 완전 회복 수단이 되면
    // "위험할 때 잡몹 하나 잡기"가 최고의 회복법이 되어 체력 관리 긴장이 사라진다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true);

    // BaseStat과 StatBonus를 합쳐 CurrentCharacterStat과 전투 스탯 멤버들을 다시 계산한다.
    // 실효값을 쓰는 유일한 지점이다 - 계산 규칙을 바꾸거나 모디파이어를 목록 기반으로
    // 갈아끼우게 되면 고칠 곳이 이 함수 하나다.
    void RecalculateStats(bool bRestoreToFull);

public:
    // 소비 아이템 사용 시 HP/MP 회복 적용 (인벤토리 컴포넌트의 UseItem에서 호출됨)
    UFUNCTION(BlueprintCallable, Category = "Item")
    void ApplyConsumableEffect(float HealAmount, float ManaAmount);

    // 무기가 발사 시 공격력 배율을 계산할 때 읽는다 (CurrentCharacterStat이 protected라 getter가 필요함)
    // 보너스가 실린 실효값이다. 예전 이름이 GetBaseAttackPower였는데, 반환값이 더 이상
    // "기본값"이 아니게 되어 이름이 거짓이 되므로 개명했다 - 그대로 뒀다면 나중에 누군가
    // "보너스 이전 값이 필요하다"며 이 함수를 써서 조용히 틀린 계산을 하게 된다.
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }

    // 스킬 데미지 계산에 쓰인다. 실효값(테이블 + 보너스)이다.
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetSkillPower() const { return CurrentCharacterStat.SkillPower; }

    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetCurrentMP() const { return CurrentMP; }

    // MP가 충분하면 차감하고 true, 부족하면 아무것도 안 하고 false.
    // HUD 갱신까지 여기서 하는 이유: 호출자마다 UpdatePlayerHUD를 기억하게 하면 언젠가 빠뜨려서
    // "MP는 줄었는데 바는 그대로"가 된다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    bool ConsumeMP(float Amount);

    // 경험치를 더한다. 적 처치가 주 경로지만 퀘스트/상자 등 다른 소스가 생겨도 이 입구를 쓴다.
    // 한 번의 호출로 여러 레벨이 오를 수 있다(초과분은 다음 레벨로 이월됨).
    UFUNCTION(BlueprintCallable, Category = "Level")
    void AddEXP(float Amount);

    // DT_CharacterStat에 다음 레벨 행이 없으면 만렙이다. 상한을 코드 상수로 두지 않으므로
    // 테이블에 행을 추가하는 것만으로 만렙이 늘어난다.
    UFUNCTION(BlueprintPure, Category = "Level")
    bool IsMaxLevel() const;

    // 레벨업 시점. 아직 구독자가 없다 - 카드 선택 시스템이 여기 붙는다.
    UPROPERTY(BlueprintAssignable, Category = "Level")
    FOnLevelUpSignature OnLevelUp;

    // 카드/버프가 준 스탯 보너스를 누적한다. 개별 제거는 지원하지 않는다 - 런마다 캐릭터가
    // 새로 스폰되면서 StatBonus가 기본 생성되므로 초기화가 필요 없기 때문이다. 나중에
    // 시간제 버프가 필요해지면 목록 기반으로 바꾸되, 그때도 실효값을 읽는 코드는 안 바뀐다.
    UFUNCTION(BlueprintCallable, Category = "Stat")
    void AddStatBonus(const FStatModifier& Delta);

    // 개발용 콘솔 명령. 카드 시스템 없이 보너스를 시험한다.
    // 예) GJAddBonus MaxHP 5 0              -> 최대 체력 +5
    //     GJAddBonus BaseAttackPower 0 0.15 -> 공격력 +15%
    // 카드가 생긴 뒤에도 "이 조합이면 어떻게 되나"를 카드 없이 시험할 수 있어 남겨둔다.
    UFUNCTION(Exec)
    void GJAddBonus(const FString& StatName, float AddValue, float PercentValue);

    // 아래 두 개는 CardComponent로 그대로 넘기기만 한다.
    // 컴포넌트에 UFUNCTION(Exec)를 달아도 콘솔이 못 찾는다 - 명령 라우팅이 폰까지만 내려오고
    // 소유 컴포넌트까지는 안 들어가는 경우가 있어서, 폰에 창구를 만들어 준다.
    // 예) GJDrawCards                  -> 지금 풀에서 뽑히는 카드를 로그로 출력
    UFUNCTION(Exec)
    void GJDrawCards();

    // 예) GJSetTagWeight Tree.Offense 5 -> 공격 트리 카드가 5배 잘 나오게
    //     GJSetTagWeight Tree.Offense 1 -> 원래대로
    UFUNCTION(Exec)
    void GJSetTagWeight(const FString& TagName, float Multiplier);

    // 예) GJShowCards -> 레벨업 없이 카드 화면만 띄운다
    UFUNCTION(Exec)
    void GJShowCards();

    // 예) GJEquipSkill Skill_Fireball     -> 첫 빈 슬롯에 장착
    //     GJEquipSkill Skill_Fireball 1   -> 슬롯 1(Q)에 강제 장착
    UFUNCTION(Exec)
    void GJEquipSkill(const FString& SkillId, int32 SlotIndex = -1);

    // 슬롯별 장착 스킬, 쿨타임 잔량, MP, 차징 상태를 로그로 출력
    UFUNCTION(Exec)
    void GJSkillInfo();

protected:

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TSubclassOf<AGJWeaponBase> DefaultWeaponClass;

    // 지금 손에 들려 있는(공격/재장전에 실제로 쓰이는) 무기 - WeaponSlots[CurrentWeaponSlotIndex]를 가리킴
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AGJWeaponBase* EquippedWeapon;

    // 동시에 소지(장착)할 수 있는 무기 슬롯 (0번/1번, 1·2번 키로 스왑). 무기는 데이터 테이블 스택이
    // 아니라 각각 별개의 액터 인스턴스를 그대로 들고 있음 - 같은 무기를 두 개 주워도 서로 다른 오브젝트.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    TArray<AGJWeaponBase*> WeaponSlots;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    int32 CurrentWeaponSlotIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* WeaponSlot1Action;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* WeaponSlot2Action;

    void SwapToWeaponSlot1();
    void SwapToWeaponSlot2();

    void EquipWeapon();

    // WeaponSlots[SlotIndex]를 실제로 손(WeaponSocket)에 부착해 활성 무기로 바꿈 - 이전에 들고 있던
    // 무기는 버려지지 않고 숨겨진 채 계속 그 슬롯에 남아있음(스왑으로 다시 꺼낼 수 있음)
    void CommitWeaponSwap(int32 SlotIndex);

    // 무기 슬롯 2개가 이미 다 찼을 때 새 무기를 주우면, 현재 활성 슬롯의 무기를 필드에 떨어뜨리고
    // 그 자리를 비움 (파괴하지 않음 - 다시 주울 수 있음)
    void DropWeapon(int32 SlotIndex);

    // 지금 손에 든 원거리 무기의 OnAmmoChanged를 그대로 OnActiveWeaponAmmoChanged로 흘려보냄
    // (탄약 UI가 무기 인스턴스가 아니라 캐릭터의 델리게이트 하나만 구독하면 되게 하기 위함)
    UFUNCTION()
    void HandleActiveWeaponAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo);

public:
    // UI(UMG) 등에서 현재 장착 무기를 가져다 쓰기 위한 getter (예: 탄약 표시하려면 여기서 받아 GJWeapon_Ranged로 캐스팅)
    UFUNCTION(BlueprintPure, Category = "Weapon")
    AGJWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

    UFUNCTION(BlueprintPure, Category = "Weapon")
    AGJWeaponBase* GetWeaponInSlot(int32 SlotIndex) const;

    UFUNCTION(BlueprintPure, Category = "Weapon")
    int32 GetCurrentWeaponSlotIndex() const { return CurrentWeaponSlotIndex; }

    // 필드에 놓인 무기(AGJWeaponBase)가 E로 상호작용됐을 때 호출됨 - 빈 슬롯을 찾아 장착하고,
    // 슬롯이 다 찼으면 현재 활성 슬롯의 무기를 필드에 떨어뜨린 뒤 그 자리를 새 무기로 채움
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool PickUpWeapon(AGJWeaponBase* NewWeapon);

    // 지정한 슬롯의 무기를 버리고 그 자리에 새 무기를 넣는다.
    // 카드로 무기를 받을 때 "어느 무기를 버릴지" 고른 결과를 적용하는 경로다.
    // DropWeapon을 그냥 public으로 여는 대신 이 함수를 두는 이유: "버리고 넣는" 두 동작의
    // 순서가 맞아야 플레이어가 고른 슬롯에 정확히 들어가는데, 그 순서를 호출자마다
    // 기억하게 하면 언젠가 틀린다.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool ReplaceWeaponInSlot(int32 SlotIndex, AGJWeaponBase* NewWeapon);

    // 연사 상태를 강제로 해제한다. 모달 UI를 열 때 필요하다 - 입력 모드가 UI로 바뀌면
    // 마우스 "뗌" 이벤트가 캐릭터에 안 들어와서 bIsAutoFiring이 켜진 채로 굳고,
    // UI를 닫고 Tick이 다시 돌 때 무한 연사가 된다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopAutoFire() { bIsAutoFiring = false; }

    // 지정한 슬롯(0/1)의 무기로 전환 - 1·2번 키 입력과 인벤토리 무기 페이지 클릭 양쪽에서 호출됨.
    // 무기의 SwapMontage가 있으면 그 몽타주를 재생하는 동안 WeaponSwap 상태로 다른 입력을 막음.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    void SwapToWeaponSlot(int32 SlotIndex);

    // 두 무기 슬롯의 내용을 서로 바꿈 (무기 페이지에서 아이콘을 드래그해서 자리를 바꿀 때 사용).
    // SwapToWeaponSlot과 달리 "어느 무기를 손에 들지"는 안 바꾸고 슬롯 배치(1번/2번)만 바꿈.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool SwapWeaponSlots(int32 IndexA, int32 IndexB);

    // 무기 슬롯이 바뀔 때(습득/스왑/드랍)마다 브로드캐스트 - 인벤토리 무기 페이지 UI 갱신용
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnWeaponSlotsChangedSignature OnWeaponSlotsChanged;

    // 지금 활성화된 무기의 탄약 정보 - 탄약 UI는 이 델리게이트 하나만 구독하면 어떤 무기로
    // 스왑되든 항상 정확한 값을 받음 (자세한 설명은 델리게이트 타입 선언부 주석 참고)
    UPROPERTY(BlueprintAssignable, Category = "Weapon")
    FOnActiveWeaponAmmoChangedSignature OnActiveWeaponAmmoChanged;

protected:
    // 탄약 UI 등 HUD 위젯 클래스. BP_GJCharacter 디테일 패널에서 WBP_AmmoUI 같은 위젯 블루프린트를 할당하면
    // BeginPlay에서 자동으로 생성되어 화면에 뜸.
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UUserWidget> AmmoWidgetClass;

    UPROPERTY()
    UUserWidget* AmmoWidgetInstance;

    // 좌측 상단 HP/MP 바 HUD. BP_GJCharacter 디테일 패널에서 WBP_PlayerHUD 같은 위젯 블루프린트를 할당하면
    // BeginPlay에서 자동으로 생성되어 화면에 뜸 (레벨과 무관하게 캐릭터가 스폰되는 곳이면 어디서든 동작함)
    UPROPERTY(EditDefaultsOnly, Category = "UI")
    TSubclassOf<UGJPlayerHUDWidget> PlayerHUDWidgetClass;

    UPROPERTY()
    UGJPlayerHUDWidget* PlayerHUDWidgetInstance;

    // OnDamaged 델리게이트에 바인딩되는 핸들러 - HP 바를 최신 HP로 갱신함
    UFUNCTION()
    void OnHPChanged(float DamageAmount, AActor* DamageCauser);

    void UpdatePlayerHUD();

    // 인벤토리 데이터/로직 (버튼 등은 이 컴포넌트에 직접 연결해서 쓰면 됨)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UGJInventoryComponent* InventoryComponent;

    // 레벨업 카드 선택 (OnLevelUp을 구독해서 알아서 동작함 - 캐릭터는 카드를 모른다)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card")
    UGJCardComponent* CardComponent;

    // 액티브 스킬 (슬롯/쿨타임/차징 전부 여기 - 캐릭터는 입력만 넘긴다)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    UGJSkillComponent* SkillComponent;

    // 스킬 입력 액션. BP_GJCharacter 디테일 패널에서 IA_Skill1/2/3을 할당해야 동작한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill1Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill2Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill3Action;

    // 슬롯 번호를 넘기기만 하는 얇은 래퍼. BindAction이 인자 있는 함수를 못 받아서 필요하다.
    void Skill1Pressed();
    void Skill1Released();
    void Skill2Pressed();
    void Skill2Released();
    void Skill3Pressed();
    void Skill3Released();

    // 인벤토리 그리드 UI. BP_GJCharacter 디테일 패널에서 WBP_Inventory 같은 위젯 블루프린트를 할당해야 함
    UPROPERTY(EditDefaultsOnly, Category = "Inventory")
    TSubclassOf<UGJInventoryWidget> InventoryWidgetClass;

    UPROPERTY()
    UGJInventoryWidget* InventoryWidgetInstance;

public:
    // Tab 입력에 바인딩됨 - 인벤토리 위젯을 열고 닫으면서 게임을 일시정지/재개함.
    // public인 이유: 인벤토리 위젯이 자기 자신의 키 입력 처리(NativeOnKeyDown)에서 Tab을 감지해
    // 직접 이 함수를 호출해서 닫음 (자세한 이유는 .cpp 주석 참고)
    UFUNCTION(BlueprintCallable, Category = "Inventory")
    void ToggleInventory();

    FORCEINLINE UGJInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
};
