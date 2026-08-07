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

enum class EDodgeType
{
    Forward,
    Backward,
    Left,
    Right
};

UCLASS()
class PROJECT_GJ_API AGJCharacter : public AGJBaseCharacter
{
    GENERATED_BODY()

public:
    AGJCharacter();

protected:
    virtual void BeginPlay() override;

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

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TSubclassOf<AGJWeaponBase> DefaultWeaponClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AGJWeaponBase* EquippedWeapon;

    void EquipWeapon();

public:
    // UI(UMG) 등에서 현재 장착 무기를 가져다 쓰기 위한 getter (예: 탄약 표시하려면 여기서 받아 GJWeapon_Ranged로 캐스팅)
    UFUNCTION(BlueprintPure, Category = "Weapon")
    AGJWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

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
};
