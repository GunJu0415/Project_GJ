#include "GJCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

AGJCharacter::AGJCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // ==========================================
    // 1. 캐릭터 기본 무브먼트 설정
    // ==========================================

    // 마우스 방향으로 캐릭터가 직접 회전하므로, 컨트롤러의 Yaw 회전이나 이동 방향으로의 자동 회전은 끕니다.
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);

    // ==========================================
    // 2. 카메라 붐 (스프링암) 설정
    // ==========================================
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());

    // 탑다운 시점을 위해 절대 회전값을 사용하고 길이를 800, 각도를 -60도로 고정합니다.
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->TargetArmLength = 800.f;
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));

    // 탑다운 게임에서는 카메라가 벽과 충돌해 앞으로 당겨지는 현상을 방지해야 합니다.
    CameraBoom->bDoCollisionTest = false;

    // 카메라가 캐릭터를 부드럽게 쫓아오게 하는 Lag 기능 활성화
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 7.0f;

    // ==========================================
    // 3. 카메라 설정
    // ==========================================
    TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCameraComponent->SetupAttachment(CameraBoom);
    TopDownCameraComponent->bUsePawnControlRotation = false;
}

void AGJCharacter::BeginPlay()
{
    Super::BeginPlay();

    // 시작 시 캐릭터가 바라보는 방향을 유효한 기본값으로 저장
    LastValidRotation = GetActorRotation();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        // Enhanced Input System 연결
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }

        // 탑다운 게임에 맞게 마우스 커서 표시 및 클릭/마우스오버 이벤트 활성화
        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
}

void AGJCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 매 프레임 마우스 처리와 카메라 오프셋을 동기화하여 실행
    ProcessMouseBehavior(DeltaTime);
}

void AGJCharacter::ProcessMouseBehavior(float DeltaTime)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !GEngine || !GEngine->GameViewport) return;

    int32 ViewportSizeX, ViewportSizeY;
    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);

    if (ViewportSizeX > 0 && ViewportSizeY > 0)
    {
        float MouseX, MouseY;
        bool bIsMouseInside = PC->GetMousePosition(MouseX, MouseY);

        if (MouseX <= 5.f || MouseX >= (ViewportSizeX - 5.f) ||
            MouseY <= 5.f || MouseY >= (ViewportSizeY - 5.f))
        {
            bIsMouseInside = false;
        }

        if (bIsMouseInside)
        {
            // ==== A. 캐릭터 회전 목표값 갱신 ====
            FVector WorldLocation, WorldDirection;
            if (PC->DeprojectScreenPositionToWorld(MouseX, MouseY, WorldLocation, WorldDirection))
            {
                FVector PlaneOrigin = GetActorLocation();
                FVector PlaneNormal = FVector::UpVector;
                FVector Intersection = FMath::LinePlaneIntersection(
                    WorldLocation,
                    WorldLocation + (WorldDirection * 10000.f),
                    PlaneOrigin,
                    PlaneNormal);

                FVector LookDirection = Intersection - GetActorLocation();
                LookDirection.Z = 0.f;

                if (!LookDirection.IsNearlyZero())
                {
                    // 마우스가 화면 안에 있을 때만 '마지막 유효 회전값'을 갱신합니다.
                    LastValidRotation = LookDirection.Rotation();
                }
            }

            // ==== B. 카메라 오프셋 목표값 계산 ====
            FVector2D ViewportCenter(ViewportSizeX / 2.f, ViewportSizeY / 2.f);
            FVector2D MouseDir((MouseX - ViewportCenter.X) / ViewportCenter.X, (MouseY - ViewportCenter.Y) / ViewportCenter.Y);
            float DistanceFromCenter = MouseDir.Size();

            if (DistanceFromCenter > CameraOffsetDeadzone)
            {
                float MappedDistance = FMath::Clamp((DistanceFromCenter - CameraOffsetDeadzone) / (1.f - CameraOffsetDeadzone), 0.f, 1.f);
                MappedDistance = FMath::InterpEaseInOut(0.f, 1.f, MappedDistance, 2.f);

                FVector MouseWorldDir(-MouseDir.Y, MouseDir.X, 0.f);
                MouseWorldDir.Normalize();
                DesiredWorldOffset = MouseWorldDir * (MappedDistance * MaxCameraOffset);
            }
            else
            {
                DesiredWorldOffset = FVector::ZeroVector;
            }
        }
    }

    // ==========================================
    // [해결된 부분] 회전 적용 위치 변경
    // ==========================================
    // 기존에는 if (bIsMouseInside) 안에서만 회전시켰지만, 이제는 밖으로 뺐습니다.
    // 마우스가 밖으로 나가서 갱신이 멈추더라도, 매 프레임 무조건 LastValidRotation 값으로 강제 세팅합니다.
    // 이렇게 하면 블루프린트나 무브먼트 컴포넌트가 캐릭터를 맘대로 돌리는 것을 원천 차단합니다.
    SetActorRotation(LastValidRotation);

    // ==========================================
    // 카메라 위치 실제 적용
    // ==========================================
    CurrentWorldOffset = FMath::VInterpTo(CurrentWorldOffset, DesiredWorldOffset, DeltaTime, CameraOffsetInterpSpeed);
    CameraBoom->SetRelativeLocation(GetActorRotation().UnrotateVector(CurrentWorldOffset));
}

void AGJCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
        {
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AGJCharacter::Move);
        }
    }
}

void AGJCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Movement = Value.Get<FVector2D>();
    if (Controller == nullptr) return;

    // 카메라나 캐릭터 회전에 관계없이, 입력 방향대로 절대적인 월드 이동을 수행합니다.
    const FVector Forward = FVector::ForwardVector;
    const FVector Right = FVector::RightVector;

    AddMovementInput(Forward, Movement.Y);
    AddMovementInput(Right, Movement.X);
}

UAbilitySystemComponent* AGJCharacter::GetAbilitySystemComponent() const
{
    return nullptr;
}