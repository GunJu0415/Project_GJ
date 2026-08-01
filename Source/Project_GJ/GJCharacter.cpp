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

    // 상태 컴포넌트 생성 및 부착
    StateComponent = CreateDefaultSubobject<UCharacterStateComponent>(TEXT("StateComponent"));
    // 모션 워핑 컴포넌트 생성
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));
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
    float RotationSpeed = 45.f; // [튜닝 포인트] 이 수치를 조절해 회전 속도 결정 (높을수록 빠름)

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

    if (!StateComponent || StateComponent->GetState() != ECharacterState::Idle)
    {
        return;
    }

    FVector2D Input = MoveInput.GetSafeNormal();

    //---------------------------------------
    // MoveInput -> 월드 방향
    //---------------------------------------
    FVector WorldDirection(
        Input.Y,
        Input.X,
        0.f);

    WorldDirection.Normalize();

    //---------------------------------------
    // MoveInput -> 애니메이션 방향 판정 (올바른 축 비교)
    //---------------------------------------
    UAnimMontage* Montage = nullptr;
    float DodgeDistance = 500.f;
    const float ForwardBackwardRatio = 1.2f;

    // 앞뒤(Input.Y)가 좌우(Input.X)보다 우세한가?
    if (FMath::Abs(Input.Y) * ForwardBackwardRatio >= FMath::Abs(Input.X))
    {
        if (Input.Y > 0.f)
        {
            DodgeType = EDodgeType::Forward;
            Montage = DodgeForwardMontage;
        }
        else
        {
            DodgeType = EDodgeType::Backward;
            Montage = DodgeBackwardMontage;
        }
    }
    else
    {
        if (Input.X > 0.f)
        {
            DodgeType = EDodgeType::Right;
            Montage = DodgeRightMontage;
        }
        else
        {
            DodgeType = EDodgeType::Left;
            Montage = DodgeLeftMontage;
        }
    }

    //---------------------------------------
    // 회전 및 이동 방향 설정 (회피 종료 후 시선 튀는 현상 방지 포함)
    //---------------------------------------
    FRotator TargetRotation = GetActorRotation();

    switch (DodgeType)
    {
    case EDodgeType::Forward:
        TargetRotation = WorldDirection.Rotation();
        SetActorRotation(TargetRotation);
        break;

    case EDodgeType::Backward:
        TargetRotation = (-WorldDirection).Rotation();
        SetActorRotation(TargetRotation);
        // 뒷점프 시 뒤로 밀려나도록 월드 방향 반전
        WorldDirection = WorldDirection;
        break;

    case EDodgeType::Left:
        // 캐릭터의 현재 시선 기준 좌측 방향 유지 (시선 튐 방지)
        TargetRotation = GetActorRotation();
        WorldDirection = -GetActorRightVector();
        break;

    case EDodgeType::Right:
        // 캐릭터의 현재 시선 기준 우측 방향 유지 (시선 튐 방지)
        TargetRotation = GetActorRotation();
        WorldDirection = GetActorRightVector();
        break;
    }

    // 회피 중에 시선이 마우스로 튀지 않도록 LastValidRotation도 함께 동기화
    LastValidRotation = TargetRotation;

    //---------------------------------------
    // Motion Warp
    //---------------------------------------
    if (MotionWarpingComponent)
    {
        FVector WarpTarget =
            GetActorLocation() +
            WorldDirection * DodgeDistance;

        MotionWarpingComponent->AddOrUpdateWarpTargetFromLocationAndRotation(
            TEXT("DodgeTarget"),
            WarpTarget,
            TargetRotation);
    }

    //---------------------------------------
    // 상태 변경 및 재생
    //---------------------------------------
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

UAbilitySystemComponent* AGJCharacter::GetAbilitySystemComponent() const
{
    return nullptr;
}