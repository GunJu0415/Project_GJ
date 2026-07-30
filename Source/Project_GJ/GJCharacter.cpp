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

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 720.f, 0.f);

    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->SetUsingAbsoluteRotation(true);
    CameraBoom->TargetArmLength = 800.f;
    CameraBoom->SetRelativeRotation(FRotator(-60.f, 0.f, 0.f));
    CameraBoom->bDoCollisionTest = false;
    CameraBoom->bEnableCameraLag = true;
    CameraBoom->CameraLagSpeed = 7.0f;

    TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
    TopDownCameraComponent->SetupAttachment(CameraBoom);
    TopDownCameraComponent->bUsePawnControlRotation = false;
}

void AGJCharacter::BeginPlay()
{
    Super::BeginPlay();
    LastValidRotation = GetActorRotation();

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }

        PC->bShowMouseCursor = true;
        PC->bEnableClickEvents = true;
        PC->bEnableMouseOverEvents = true;
    }
}

void AGJCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // [피드백 3 반영] 4단계로 역할을 명확히 분리하여 가독성과 유지보수성 극대화
    UpdateMouseState();
    UpdateCharacterRotation();
    UpdateCameraOffset(DeltaTime);
    ApplyCameraOffset();
}

void AGJCharacter::UpdateMouseState()
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC || !GEngine || !GEngine->GameViewport) return;

    PC->GetViewportSize(ViewportSizeX, ViewportSizeY);
    if (ViewportSizeX <= 0 || ViewportSizeY <= 0) return;

    // 마우스 좌표를 가져오는데 성공했는지 1차 판정
    bIsMouseInsideViewport = PC->GetMousePosition(CurrentMouseX, CurrentMouseY);

    // [피드백 1 반영] 5픽셀 하드코딩 마진 제거, 화면 바깥으로 아예 나갔을 때만 false 처리
    if (CurrentMouseX < 0.f || CurrentMouseX > ViewportSizeX ||
        CurrentMouseY < 0.f || CurrentMouseY > ViewportSizeY)
    {
        bIsMouseInsideViewport = false;
    }
}

void AGJCharacter::UpdateCharacterRotation()
{
    // 마우스가 화면 밖이면 LastValidRotation만 유지하고 리턴
    if (!bIsMouseInsideViewport)
    {
        // [피드백 4 반영] 현재 회전값과 목표 회전값이 다를 때만 SetActorRotation 호출 (최적화)
        if (!GetActorRotation().Equals(LastValidRotation, 0.1f))
        {
            SetActorRotation(LastValidRotation);
        }
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    FVector WorldLocation, WorldDirection;
    if (PC->DeprojectScreenPositionToWorld(CurrentMouseX, CurrentMouseY, WorldLocation, WorldDirection))
    {
        FVector PlaneOrigin = GetActorLocation();
        FVector PlaneNormal = FVector::UpVector;

        // [피드백 2 반영] 넉넉한 레이캐스트 거리 확보 (10만 유닛)
        FVector Intersection = FMath::LinePlaneIntersection(
            WorldLocation,
            WorldLocation + (WorldDirection * 100000.f),
            PlaneOrigin,
            PlaneNormal);

        FVector LookDirection = Intersection - GetActorLocation();
        LookDirection.Z = 0.f;

        if (!LookDirection.IsNearlyZero())
        {
            LastValidRotation = LookDirection.Rotation();

            // [피드백 4 반영] 회전 최적화
            if (!GetActorRotation().Equals(LastValidRotation, 0.1f))
            {
                SetActorRotation(LastValidRotation);
            }
        }
    }
}

void AGJCharacter::UpdateCameraOffset(float DeltaTime)
{
    // [피드백 5 반영] 마우스가 화면 안에 있을 때만 DesiredWorldOffset 갱신
    if (bIsMouseInsideViewport)
    {
        FVector2D ViewportCenter(ViewportSizeX / 2.f, ViewportSizeY / 2.f);
        FVector2D MouseDir((CurrentMouseX - ViewportCenter.X) / ViewportCenter.X, (CurrentMouseY - ViewportCenter.Y) / ViewportCenter.Y);
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

    // 마우스가 화면 밖에 있더라도 VInterpTo는 실행되어, 
    // 나가는 순간 카메라가 뚝 끊기지 않고 부드럽게 감속하며 정지합니다.
    CurrentWorldOffset = FMath::VInterpTo(CurrentWorldOffset, DesiredWorldOffset, DeltaTime, CameraOffsetInterpSpeed);
}

void AGJCharacter::ApplyCameraOffset()
{
    // [피드백 6 유지] 부모(캐릭터)의 회전과 무관하게 카메라를 월드 기준으로 이동시키기 위한 필수 수학 연산입니다.
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

    AddMovementInput(FVector::ForwardVector, Movement.Y);
    AddMovementInput(FVector::RightVector, Movement.X);
}

UAbilitySystemComponent* AGJCharacter::GetAbilitySystemComponent() const
{
    return nullptr;
}