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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat CurrentCharacterStat;

    // MP - 재장전할 때마다 소비됨 (소비량 = 실제로 채워지는 발수 x 장착 무기의 WeaponStat.MPCostPerAmmo)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    float MaxMP = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    float CurrentMP = 50.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    int32 CurrentLevel;

    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel);

public:
    // 소비 아이템 사용 시 HP/MP 회복 적용 (인벤토리 컴포넌트의 UseItem에서 호출됨)
    UFUNCTION(BlueprintCallable, Category = "Item")
    void ApplyConsumableEffect(float HealAmount, float ManaAmount);

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
