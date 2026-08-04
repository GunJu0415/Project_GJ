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
#include "DrawDebugHelpers.h" 
#include "Engine/DataTable.h" 
#include "GJWeaponBase.h"      

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

    CurrentLevel = 1;

    // [신규] 콤보 변수 초기화
    CurrentComboCount = 0;
    bHasNextComboInput = false;
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
        // [수정] 몽타주 종료 콜백 연결
        AnimInstance->OnMontageEnded.AddDynamic(this, &AGJCharacter::OnMontageEndedEvent);
    }

    UpdateCharacterStat(CurrentLevel);
    EquipWeapon();
}

void AGJCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

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

    bIsMouseInsideViewport = PC->GetMousePosition(CurrentMouseX, CurrentMouseY);

    if (CurrentMouseX < 0.f || CurrentMouseX > ViewportSizeX ||
        CurrentMouseY < 0.f || CurrentMouseY > ViewportSizeY)
    {
        bIsMouseInsideViewport = false;
    }
}

void AGJCharacter::UpdateCharacterRotation()
{
    if (StateComponent && StateComponent->GetState() == ECharacterState::Dodge)
    {
        return;
    }

    float DeltaTime = GetWorld()->GetDeltaSeconds();
    float RotationSpeed = 30.f;

    if (!bIsMouseInsideViewport)
    {
        if (!GetActorRotation().Equals(LastValidRotation, 0.1f))
        {
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
                FRotator SmoothRotation = FMath::RInterpTo(GetActorRotation(), LastValidRotation, DeltaTime, RotationSpeed);
                SetActorRotation(SmoothRotation);
            }
        }
    }
}

void AGJCharacter::UpdateCameraOffset(float DeltaTime)
{
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

    CurrentWorldOffset = FMath::VInterpTo(CurrentWorldOffset, DesiredWorldOffset, DeltaTime, CameraOffsetInterpSpeed);
}

void AGJCharacter::ApplyCameraOffset()
{
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

        if (DodgeAction)
        {
            EnhancedInput->BindAction(DodgeAction, ETriggerEvent::Started, this, &AGJCharacter::PerformDodge);
        }

        // [신규] 공격 입력 바인딩
        if (AttackAction)
        {
            EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &AGJCharacter::AttackInputPressed);
        }
    }
}

void AGJCharacter::Move(const FInputActionValue& Value)
{
    // 공격 중일 때는 이동 차단 (원한다면 제거 가능)
    if (StateComponent && StateComponent->GetState() == ECharacterState::Attack) return;

    MoveInput = Value.Get<FVector2D>();
    const FVector2D Movement = Value.Get<FVector2D>();
    if (Controller == nullptr) return;

    AddMovementInput(FVector::ForwardVector, Movement.Y);
    AddMovementInput(FVector::RightVector, Movement.X);
}

// ==========================================
// [신규] 콤보 공격 구현부
// ==========================================
void AGJCharacter::AttackInputPressed()
{
    if (!EquippedWeapon || !StateComponent) return;

    // 구르기 중에는 공격 불가
    if (StateComponent->GetState() == ECharacterState::Dodge) return;

    UAnimMontage* WeaponMontage = EquippedWeapon->GetAttackMontage();
    if (!WeaponMontage) return;

    // 현재 공격 상태가 아니면 1타 시작
    if (StateComponent->GetState() != ECharacterState::Attack)
    {
        StateComponent->SetState(ECharacterState::Attack);
        CurrentComboCount = 1;
        bHasNextComboInput = false;

        PlayAnimMontage(WeaponMontage);

        // 몽타주 내의 "Attack1" 섹션으로 이동 재생
        FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboCount));
        if (WeaponMontage->IsValidSectionName(SectionName))
        {
            GetMesh()->GetAnimInstance()->Montage_JumpToSection(SectionName, WeaponMontage);
        }
    }
    else
    {
        // 공격 모션 도중 클릭했다면 다음 콤보 예약
        bHasNextComboInput = true;
    }
}

void AGJCharacter::AdvanceCombo()
{
    // 예약된 입력이 있고, 무기가 존재할 때
    if (bHasNextComboInput && EquippedWeapon)
    {
        UAnimMontage* WeaponMontage = EquippedWeapon->GetAttackMontage();
        if (WeaponMontage)
        {
            CurrentComboCount++;
            bHasNextComboInput = false; // 예약 소모

            // Attack2, Attack3 등 다음 섹션 이름 동적 생성
            FName NextSection = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboCount));

            // 해당 섹션이 존재하면 점프해서 재생 이어나감
            if (WeaponMontage->IsValidSectionName(NextSection))
            {
                GetMesh()->GetAnimInstance()->Montage_JumpToSection(NextSection, WeaponMontage);
                return; // 성공적으로 콤보가 이어지면 종료
            }
        }
    }

    // 예약된 입력이 없거나 더 이상 섹션이 없으면 콤보 종료 처리
    ResetCombo();
}

void AGJCharacter::ResetCombo()
{
    CurrentComboCount = 0;
    bHasNextComboInput = false;

    if (StateComponent && StateComponent->GetState() == ECharacterState::Attack)
    {
        StateComponent->SetState(ECharacterState::Idle);
    }
}

void AGJCharacter::PerformFire()
{
    if (EquippedWeapon)
    {
        // 향후 무기 베이스에 ExecuteAttack() 가상함수를 만들면 형변환 없이 호출 가능합니다.
        // 현재는 무기 베이스를 Ranged나 Melee로 적절히 캐스팅해서 사용합니다.
        // ex) RangedWeapon->Fire();
    }
}

// ==========================================
// 기존 로직들
// ==========================================
void AGJCharacter::PerformDodge()
{
    if (MoveInput.IsNearlyZero()) return;
    if (!StateComponent || StateComponent->GetState() != ECharacterState::Idle) return;

    FVector WorldDirection(MoveInput.Y, MoveInput.X, 0.f);
    WorldDirection.Normalize();

    FVector Local = GetActorTransform().InverseTransformVectorNoScale(WorldDirection);
    Local.Normalize();

    float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
    Angle = FRotator::NormalizeAxis(Angle);

    UAnimMontage* Montage = nullptr;
    FVector FacingDirection = WorldDirection;
    float DodgeDistance = 500.f;

    if (Angle >= -22.5f && Angle < 22.5f) { Montage = DodgeForwardMontage; }
    else if (Angle >= 22.5f && Angle < 67.5f) { Montage = DodgeForwardMontage; }
    else if (Angle >= 67.5f && Angle < 112.5f) { FacingDirection = GetActorForwardVector(); Montage = DodgeRightMontage; }
    else if (Angle >= 112.5f && Angle < 157.5f) { FacingDirection = -WorldDirection; Montage = DodgeBackwardMontage; }
    else if (Angle >= 157.5f || Angle < -157.5f) { FacingDirection = -WorldDirection; Montage = DodgeBackwardMontage; }
    else if (Angle >= -157.5f && Angle < -112.5f) { FacingDirection = -WorldDirection; Montage = DodgeBackwardMontage; }
    else if (Angle >= -112.5f && Angle < -67.5f) { FacingDirection = GetActorForwardVector(); Montage = DodgeLeftMontage; }
    else { Montage = DodgeForwardMontage; }

    FRotator TargetRotation = FacingDirection.Rotation();
    SetActorRotation(TargetRotation);
    LastValidRotation = TargetRotation;

    if (MotionWarpingComponent)
    {
        MotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(FName("DodgeTarget"), GetActorLocation() + WorldDirection * DodgeDistance);
    }

    StateComponent->SetState(ECharacterState::Dodge);
    PlayAnimMontage(Montage);
}

void AGJCharacter::OnMontageEndedEvent(UAnimMontage* Montage, bool bInterrupted)
{
    if (StateComponent)
    {
        // 1. 회피 몽타주가 끝났을 때
        if (Montage == DodgeForwardMontage || Montage == DodgeBackwardMontage ||
            Montage == DodgeLeftMontage || Montage == DodgeRightMontage)
        {
            if (StateComponent->GetState() == ECharacterState::Dodge)
            {
                StateComponent->SetState(ECharacterState::Idle);
            }
        }
        // 2. 무기 공격 몽타주가 끝났을 때 (또는 끊겼을 때)
        else if (EquippedWeapon && Montage == EquippedWeapon->GetAttackMontage())
        {
            ResetCombo();
        }
    }
}

void AGJCharacter::UpdateCharacterStat(int32 NewLevel)
{
    CurrentLevel = NewLevel;
    if (CharacterStatTable)
    {
        FString RowName = FString::FromInt(CurrentLevel);
        FCharacterStat* RowData = CharacterStatTable->FindRow<FCharacterStat>(FName(*RowName), TEXT("UpdateCharacterStat"));

        if (RowData)
        {
            CurrentCharacterStat = *RowData;
        }
    }
}

void AGJCharacter::EquipWeapon()
{
    if (DefaultWeaponClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;

        EquippedWeapon = GetWorld()->SpawnActor<AGJWeaponBase>(DefaultWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

        if (EquippedWeapon)
        {
            EquippedWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(TEXT("WeaponSocket")));
        }
    }
}