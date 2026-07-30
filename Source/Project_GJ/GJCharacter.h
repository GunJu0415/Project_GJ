#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h" // 향상된 입력 값 구조체
#include "GJCharacter.generated.h"

class UAbilitySystemComponent;
class UInputMappingContext;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;

UCLASS()
class PROJECT_GJ_API AGJCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

	// ... 기존 GAS 코드는 그대로 유지 ...
public:
	AGJCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// 탑다운 카메라를 매달아줄 스프링암
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	// 화면을 비출 실제 카메라
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* TopDownCameraComponent;
protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:
	/* ---------------- 향상된 입력 (Enhanced Input) ---------------- */

	// 키 매핑 컨텍스트 (어떤 상황에서 어떤 키 묶음을 쓸 것인가)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	// 이동 액션 (WASD 또는 좌측 스틱)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	// 조준 액션 (마우스 위치 또는 우측 스틱)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 실제 이동 로직이 들어갈 함수
	void Move(const FInputActionValue& Value);

	// 실제 조준 로직이 들어갈 함수
	void Look(const FInputActionValue& Value);
};