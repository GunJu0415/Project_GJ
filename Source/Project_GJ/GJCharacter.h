#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GJCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputComponent;
class UAbilitySystemComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class PROJECT_GJ_API AGJCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AGJCharacter();

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // 어빌리티 시스템 (현재는 임시로 nullptr 반환)
    UAbilitySystemComponent* GetAbilitySystemComponent() const;

protected:
    /*
     * [핵심] 마우스 좌표를 기반으로 한 캐릭터 회전 및 카메라 오프셋 이동을 통합 처리합니다.
     * 마우스가 창 밖으로 나가는 예외 상황까지 완벽하게 제어합니다.
     */
    void ProcessMouseBehavior(float DeltaTime);

    // 이동 처리 함수
    void Move(const FInputActionValue& Value);

protected:
    // ==========================================
    // 컴포넌트 & 입력 시스템
    // ==========================================
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    USpringArmComponent* CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
    UCameraComponent* TopDownCameraComponent;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;

    // ==========================================
    // 카메라 오프셋 설정 변수
    // ==========================================

    // 마우스가 화면 끝에 닿았을 때 카메라가 캐릭터로부터 멀어지는 최대 거리
    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float MaxCameraOffset = 300.f;

    // 카메라 오프셋 이동의 부드러움 정도 (높을수록 마우스를 빨리 따라감)
    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float CameraOffsetInterpSpeed = 5.f;

    // 화면 중앙에서 마우스가 어느 정도 벗어나야 카메라가 움직이기 시작할지 결정하는 데드존 (0.0 ~ 1.0)
    UPROPERTY(EditAnywhere, Category = "Camera|Offset")
    float CameraOffsetDeadzone = 0.2f;

    // ==========================================
    // 내부 상태 저장용 변수
    // ==========================================

    // 카메라의 현재 오프셋 위치 (보간용)
    FVector CurrentWorldOffset;

    // 마우스 좌표를 바탕으로 계산된 카메라 오프셋의 최종 목표 위치
    FVector DesiredWorldOffset;

    // 마우스가 창 밖으로 나갔을 때를 대비해 마지막으로 바라본 유효한 방향을 저장
    FRotator LastValidRotation;
};