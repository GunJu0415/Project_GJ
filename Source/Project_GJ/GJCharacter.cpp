#include "GJCharacter.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"      // 향상된 입력 컴포넌트
#include "InputAction.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputSubsystems.h"     // 향상된 입력 서브시스템

// ... 기존 생성자 및 GAS 코드는 그대로 유지 ...

AGJCharacter::AGJCharacter()
{
	// 매 프레임마다 Tick 함수를 호출할지 여부입니다. (필요 없으면 false로 꺼두는 것이 성능에 좋습니다)
	PrimaryActorTick.bCanEverTick = true;

	PrimaryActorTick.bCanEverTick = true;

	// 1. 스프링암(CameraBoom) 생성 및 세팅
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	// 탑다운 뷰의 핵심: 캐릭터가 회전해도 카메라는 같이 돌지 않고 절대 방향(월드)을 유지하게 합니다.
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.f; // 카메라와의 거리
	CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f)); // 카메라를 아래로 60도 꺾음
	CameraBoom->bDoCollisionTest = false; // 카메라가 벽에 부딪혔을 때 줌인되는 현상 방지

	// 2. 탑다운 카메라 생성 및 스프링암 끝에 부착
	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false; // 카메라는 스프링암의 방향만 따라감
}

void AGJCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 플레이어 컨트롤러를 가져와서 향상된 입력 로컬 서브시스템에 매핑 컨텍스트를 추가합니다.
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// 우선순위 0으로 기본 매핑 컨텍스트 추가
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void AGJCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 기존 UInputComponent를 향상된 입력 컴포넌트로 캐스팅
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		// MoveAction이 트리거될 때(키를 누르고 있을 때) Move 함수 호출
		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGJCharacter::Move);
		}

		// LookAction이 트리거될 때 Look 함수 호출
		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AGJCharacter::Look);
		}
	}
}

void AGJCharacter::Move(const FInputActionValue& Value)
{
	// 에디터에서 Axis2D(Vector2D)로 설정할 예정이므로 Vector2D로 가져옵니다.
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// 탑다운 시점이므로, 카메라 방향과 무관하게 절대적인 월드 좌표축(X, Y)을 기준으로 이동합니다.
		const FVector ForwardDirection = FVector(1.0f, 0.0f, 0.0f); // 월드 X축 (앞/뒤)
		const FVector RightDirection = FVector(0.0f, 1.0f, 0.0f);   // 월드 Y축 (좌/우)

		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void AGJCharacter::Look(const FInputActionValue& Value)
{
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// 마우스 커서 위치를 향해 캐릭터를 회전시키는 로직은 
	// 이후 PlayerController의 GetHitResultUnderCursor를 활용해 별도로 구현할 예정입니다.
	// 우선 축 입력값만 받아옵니다.
}


UAbilitySystemComponent* AGJCharacter::GetAbilitySystemComponent() const
{
	// 나중에 진짜 AbilitySystemComponent 변수를 만들면 그것을 반환하면 됩니다.
	// 지금은 껍데기만 만들어 둡니다. (또는 이미 구현해 두신 코드가 있다면 유지!)
	return nullptr;
}