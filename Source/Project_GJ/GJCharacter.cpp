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

    // ==========================================
    // [디버그용 로그 및 시각화 추가]
    // ==========================================

    // 1. Output Log에 Actor 회전값과 Controller 회전값 찍어보기
    //UE_LOG(LogTemp, Warning, TEXT("Actor Rotation : %s"), *GetActorRotation().ToString());

    //if (GetController())
    //{
    //    UE_LOG(LogTemp, Warning, TEXT("Control Rotation : %s"), *GetControlRotation().ToString());
    //}

    // 2. 캐릭터 위치에 좌표계 그리기
    // 빨간선(X축)이 마우스를 따라 돌아가면 액터가 도는 것이고, 안 돌면 Mesh만 도는 것입니다.
    //DrawDebugCoordinateSystem(
    //    GetWorld(),
    //    GetActorLocation(),
    //    GetActorRotation(),
    //    150.0f, // 선 길이 (잘 보이게 150으로 설정)
    //    false,
    //    0.0f,
    //    0,
    //    3.0f  // 선 두께
    //);
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

        // 구르기 키 바인딩 (Started: 키를 누르는 순간 1회 발생)
        if (RollAction)
        {
            EnhancedInput->BindAction(RollAction, ETriggerEvent::Started, this, &AGJCharacter::PerformRoll);
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

void AGJCharacter::PerformRoll()
{
    if (StateComponent->GetState() != ECharacterState::Idle)
    {
        return;
    }

    if (RollMontage == nullptr)
    {
        UE_LOG(LogTemp, Error, TEXT("RollMontage가 할당되지 않았습니다!"));
        return;
    }

    StateComponent->SetState(ECharacterState::Rolling);

    // ==========================================
    // 1. 몽타주 재생 속도 조절
    // ==========================================
    // PlayAnimMontage의 두 번째 매개변수가 PlayRate(재생 속도)입니다.
    // 1.5f로 설정하면 1.5배 빠르게 재생됩니다. 원하는 속도로 맞춰보세요.
    float PlayRate = 1.5f;
    PlayAnimMontage(RollMontage, PlayRate);

    // ==========================================
    // 2. 몽타주 종료 델리게이트 바인딩 (Idle로 복구)
    // ==========================================
    UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();
    if (AnimInstance)
    {
        // 델리게이트 객체 생성 및 우리 함수 연결
        FOnMontageEnded EndDelegate;
        EndDelegate.BindUObject(this, &AGJCharacter::OnRollMontageEnded);

        // 몽타주가 끝날 때 이 델리게이트를 실행하도록 애니메이션 인스턴스에 예약
        AnimInstance->Montage_SetEndDelegate(EndDelegate, RollMontage);
    }

    // 캐릭터 밀어내기 (속도가 빨라진 만큼 체공 시간이 짧아지므로 밀어내는 힘을 2000.f 등으로 늘려야 할 수도 있습니다)
    //FVector ForwardDir = GetActorForwardVector();
   // LaunchCharacter(ForwardDir * 1500.f, true, true);
}
// 몽타주가 끝나거나, 다른 애니메이션에 의해 끊겼을 때(Interrupted) 자동 실행됨
void AGJCharacter::OnRollMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // 혹시라도 다른 몽타주가 끝난 것이 아니라, '구르기 몽타주'가 끝난 것이 맞는지 확인
    if (Montage == RollMontage)
    {
        // 상태를 다시 Idle로 복구
        StateComponent->SetState(ECharacterState::Idle);
    }
}
UAbilitySystemComponent* AGJCharacter::GetAbilitySystemComponent() const
{
    return nullptr;
}