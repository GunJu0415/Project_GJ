#include "GJCharacter.h"
#include "AbilitySystemComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CharacterStateComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h" // 디버그 라인 출력을 위해 추가

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

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->OnMontageEnded.AddDynamic(this, &AGJCharacter::OnDodgeMontageEnded);
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
    // ==========================================
    // 1. 대시(회피) 중에는 시선 처리를 멈춰서 궤적이 휘는 것을 방지
    // ==========================================
    if (StateComponent && StateComponent->GetState() == ECharacterState::Dodge)
    {
        return;
    }

    // ==========================================
    // 2. 부드러운 회전을 위한 세팅 (RInterpTo 활용)
    // ==========================================
    float DeltaTime = GetWorld()->GetDeltaSeconds(); // 헤더 수정 없이 델타 타임 가져오기
    float RotationSpeed = 30.f; // [튜닝 포인트] 이 수치를 조절해 회전 속도 결정 (높을수록 빠름)

    // 마우스가 화면 밖이면 LastValidRotation으로 부드럽게 회전
    if (!bIsMouseInsideViewport)
    {
        if (!GetActorRotation().Equals(LastValidRotation, 0.1f))
        {
            // [핵심 변경] 순간이동하듯 돌지 않고, 프레임에 맞춰 부드럽게 회전
            FRotator SmoothRotation = FMath::RInterpTo(GetActorRotation(), LastValidRotation, DeltaTime, RotationSpeed);
            SetActorRotation(SmoothRotation);
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

            if (!GetActorRotation().Equals(LastValidRotation, 0.1f))
            {
                // [핵심 변경] 마우스가 가리키는 방향으로 부드럽게 보간하며 회전
                FRotator SmoothRotation = FMath::RInterpTo(GetActorRotation(), LastValidRotation, DeltaTime, RotationSpeed);
                SetActorRotation(SmoothRotation);
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

        // 회피(Dodge) 키 바인딩 (Started: 키를 누르는 순간 1회 발생)
        if (DodgeAction)
        {
            EnhancedInput->BindAction(DodgeAction, ETriggerEvent::Started, this, &AGJCharacter::PerformDodge);
        }
    }
}

void AGJCharacter::Move(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>();

    const FVector2D Movement = Value.Get<FVector2D>();
    if (Controller == nullptr) return;

    AddMovementInput(FVector::ForwardVector, Movement.Y);
    AddMovementInput(FVector::RightVector, Movement.X);
}

void AGJCharacter::PerformDodge()
{
    if (MoveInput.IsNearlyZero())
    {
        return;
    }

    // [에러 수정] GetCurrentState -> GetState() 및 null 체크 추가
    if (!StateComponent || StateComponent->GetState() != ECharacterState::Idle)
    {
        return;
    }

    //----------------------------------------
    // 입력 → 월드 방향
    //----------------------------------------
    FVector WorldDirection(
        MoveInput.Y,
        MoveInput.X,
        0.f);

    WorldDirection.Normalize();

    //----------------------------------------
    // 월드 → 캐릭터 기준
    //----------------------------------------
    FVector Local = GetActorTransform().InverseTransformVectorNoScale(WorldDirection);
    Local.Normalize();

    //----------------------------------------
    // 각도 계산
    //----------------------------------------
    float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
    Angle = FRotator::NormalizeAxis(Angle);

    //----------------------------------------
    // 판정
    //----------------------------------------
    UAnimMontage* Montage = nullptr;
    FVector FacingDirection = WorldDirection;

    // [에러 수정] DodgeDistance 선언 누락 복구
    float DodgeDistance = 500.f;

    if (Angle >= -22.5f && Angle < 22.5f)
    {
        // Forward
        Montage = DodgeForwardMontage;
    }
    else if (Angle >= 22.5f && Angle < 67.5f)
    {
        // Forward Right
        Montage = DodgeForwardMontage;
    }
    else if (Angle >= 67.5f && Angle < 112.5f)
    {
        // Right
        // [로직 수정] 사이드스텝은 현재 바라보는 방향을 유지해야 제대로 우측으로 이동함
        FacingDirection = GetActorForwardVector();
        Montage = DodgeRightMontage;
    }
    else if (Angle >= 112.5f && Angle < 157.5f)
    {
        // Back Right
        FacingDirection = -WorldDirection;
        Montage = DodgeBackwardMontage;
    }
    else if (Angle >= 157.5f || Angle < -157.5f)
    {
        // Back
        FacingDirection = -WorldDirection;
        Montage = DodgeBackwardMontage;
    }
    else if (Angle >= -157.5f && Angle < -112.5f)
    {
        // Back Left
        FacingDirection = -WorldDirection;
        Montage = DodgeBackwardMontage;
    }
    else if (Angle >= -112.5f && Angle < -67.5f)
    {
        // Left
        // [로직 수정] 사이드스텝은 현재 바라보는 방향을 유지
        FacingDirection = GetActorForwardVector();
        Montage = DodgeLeftMontage;
    }
    else
    {
        // Forward Left
        Montage = DodgeForwardMontage;
    }

    //----------------------------------------
    // 회전
    //----------------------------------------
    FRotator TargetRotation = FacingDirection.Rotation();
    SetActorRotation(TargetRotation);

    // [핵심 수정] 대시 직후 시선 튀는 현상 방지용 동기화
    LastValidRotation = TargetRotation;

    //----------------------------------------
    // Motion Warp
    //----------------------------------------
    if (MotionWarpingComponent)
    {
        // [에러 수정] TEXT 매크로 대신 FName 명시적 사용
        MotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(
            FName("DodgeTarget"),
            GetActorLocation() + WorldDirection * DodgeDistance);
    }

    //----------------------------------------
    // 상태
    //----------------------------------------
    // [에러 수정] ECharacterState::Dodging -> ECharacterState::Dodge
    StateComponent->SetState(ECharacterState::Dodge);

    PlayAnimMontage(Montage);
}

// 몽타주가 끝나거나, 다른 애니메이션에 의해 끊겼을 때(Interrupted) 자동 실행됨
void AGJCharacter::OnDodgeMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage == DodgeForwardMontage ||
        Montage == DodgeBackwardMontage ||
        Montage == DodgeLeftMontage ||
        Montage == DodgeRightMontage)
    {
        // 애니메이션 종료 시 상태를 되돌리는 로직
        if (StateComponent)
        {
            StateComponent->SetState(ECharacterState::Idle);
        }
    }
}
