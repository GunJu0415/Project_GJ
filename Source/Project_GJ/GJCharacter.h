#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingComponent.h"
#include "GJBaseCharacter.h"
#include "InputActionValue.h"
#include "GJGameTypes.h" 
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

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    int32 CurrentLevel;

    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel);

    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    TSubclassOf<AGJWeaponBase> DefaultWeaponClass;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon")
    AGJWeaponBase* EquippedWeapon;

    void EquipWeapon();
};