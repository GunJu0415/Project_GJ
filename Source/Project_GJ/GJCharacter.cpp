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
#include "GJWeapon_Ranged.h"
#include "GJWeaponBase.h"
#include "Blueprint/UserWidget.h"

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

    // 탄약 UI 등 HUD 위젯 자동 생성 및 화면 표시
    if (AmmoWidgetClass)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            AmmoWidgetInstance = CreateWidget<UUserWidget>(PC, AmmoWidgetClass);
            if (AmmoWidgetInstance)
            {
                AmmoWidgetInstance->AddToViewport();
            }
        }
    }
}

void AGJCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UpdateMouseState();
    UpdateCharacterRotation();
    UpdateCameraOffset(DeltaTime);
    ApplyCameraOffset();

    if (bIsAutoFiring)
    {
        TryAutoFire();
    }
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
            // 원거리 무기 연사(꾹 누르고 있는 동안 계속 발사)를 멈추기 위한 입력 해제 바인딩
            EnhancedInput->BindAction(AttackAction, ETriggerEvent::Completed, this, &AGJCharacter::AttackInputReleased);
            EnhancedInput->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AGJCharacter::AttackInputReleased);
        }

        // [신규] 재장전 입력 바인딩 (R키)
        if (ReloadAction)
        {
            EnhancedInput->BindAction(ReloadAction, ETriggerEvent::Started, this, &AGJCharacter::ReloadInputPressed);
        }
    }
}

void AGJCharacter::Move(const FInputActionValue& Value)
{
    // 공격 중일 때는 이동 차단 (원한다면 제거 가능)
    //if (StateComponent && StateComponent->GetState() == ECharacterState::Attack) return;

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
    // 1. 입력 바인딩 확인 (클릭 시 노란색 글씨가 뜨는지 확인)
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("1. Attack Input Received!"));

    if (!EquippedWeapon)
    {
        // 무기 스폰 또는 장착 실패 - 여기서 반환 안 하면 바로 아래에서 EquippedWeapon을 널 상태로 참조해서 크래시남
        //if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: EquippedWeapon is NULL!"));
        return;
    }

    if (!StateComponent) return;
    if (StateComponent->GetState() == ECharacterState::Dodge) return;
    if (StateComponent->GetState() == ECharacterState::Reloading) return;

    UAnimMontage* WeaponMontage = EquippedWeapon->GetAttackMontage();
    if (!WeaponMontage)
    {
        // 무기 BP나 데이터 테이블에 몽타주 에셋 할당이 안 됨
        //if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, TEXT("ERROR: WeaponMontage is NULL! Check Weapon BP or DataTable."));
        return;
    }

    // 원거리 무기는 콤보 시스템을 타지 않음. 몽타주는 누를 때 한 번만 재생(비주얼용)하고,
    // 실제 발사는 몽타주 노티파이에 맡기지 않고 Tick에서 매 프레임 시도함.
    // (노티파이에 맡기면 재생이 FireInterval보다 자주 끊길 경우 노티파이 지점에 도달하지 못해 발사가 안 되는 문제가 있었고,
    //  타이머로 별도 스케줄링하면 타이머 자체의 프레임 단위 오차 + Fire() 내부 쿨다운이라는 두 타이밍 소스가 겹쳐서 편차가 생겼음.
    //  Tick에서 계속 시도하고 실제 발사 간격은 Fire() 내부 쿨다운 하나로만 판단하면 소스가 하나로 줄어 훨씬 균일함)
    if (AGJWeapon_Ranged* RangedWeapon = Cast<AGJWeapon_Ranged>(EquippedWeapon))
    {
        PlayAnimMontage(WeaponMontage);
        RangedWeapon->Fire();
        bIsAutoFiring = true;
        return;
    }

    // 2. 최종 재생 단계 도달 확인 (초록색 글씨가 뜨면 코드상으로는 정상 재생된 것임)
    //if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("2. Playing Montage Success!"));

    // 현재 공격 상태가 아니면 1타 시작
    if (StateComponent->GetState() != ECharacterState::Attack)
    {
        StateComponent->SetState(ECharacterState::Attack);
        CurrentComboCount = 1;
        bHasNextComboInput = false;

        PlayAnimMontage(WeaponMontage);

        FName SectionName = FName(*FString::Printf(TEXT("Attack%d"), CurrentComboCount));
        if (WeaponMontage->IsValidSectionName(SectionName))
        {
            GetMesh()->GetAnimInstance()->Montage_JumpToSection(SectionName, WeaponMontage);
        }
    }
    else
    {
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
    GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("Perform Come"));

    if (EquippedWeapon)
    {
        // 1. 장착된 무기(GJWeaponBase)를 원거리 무기(GJWeapon_Ranged)로 형변환합니다.
        AGJWeaponBase* RangedWeapon = Cast<AGJWeaponBase>(EquippedWeapon);

        if (RangedWeapon)
        {
            // 2. 드디어 무기의 사격 함수를 호출합니다!
            RangedWeapon->Fire();
        }
        else
        {
            UE_LOG(LogTemp, Error, TEXT("EquippedWeapon is not a RangedWeapon!"));
        }
    }
}

void AGJCharacter::TryAutoFire()
{
    if (!EquippedWeapon || !StateComponent) return;
    if (StateComponent->GetState() == ECharacterState::Reloading) return;

    if (AGJWeapon_Ranged* RangedWeapon = Cast<AGJWeapon_Ranged>(EquippedWeapon))
    {
        RangedWeapon->Fire();
    }
}

void AGJCharacter::AttackInputReleased()
{
    bIsAutoFiring = false;
}

// ==========================================
// [신규] 재장전 구현부
// ==========================================
void AGJCharacter::ReloadInputPressed()
{
    if (!EquippedWeapon || !StateComponent) return;
    if (StateComponent->GetState() == ECharacterState::Dead) return;
    if (StateComponent->GetState() == ECharacterState::Reloading) return;

    // 근접 무기에는 재장전 개념이 없으므로 원거리 무기일 때만 처리
    AGJWeapon_Ranged* RangedWeapon = Cast<AGJWeapon_Ranged>(EquippedWeapon);
    if (!RangedWeapon || !RangedWeapon->CanReload()) return;

    RangedWeapon->StartReload();
    StateComponent->SetState(ECharacterState::Reloading);

    UAnimMontage* ReloadMontage = RangedWeapon->GetReloadMontage();
    if (ReloadMontage)
    {
        PlayAnimMontage(ReloadMontage);
        // 재생이 끝나면 OnMontageEndedEvent에서 CompleteReload()를 호출함
    }
    else
    {
        // 아직 재장전 몽타주가 없으므로 무기 데이터의 ReloadTime만큼 타이머로 대체
        GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &AGJCharacter::CompleteReload, RangedWeapon->GetWeaponStat().ReloadTime, false);
    }
}

void AGJCharacter::CompleteReload()
{
    GetWorldTimerManager().ClearTimer(ReloadTimerHandle);

    if (AGJWeapon_Ranged* RangedWeapon = Cast<AGJWeapon_Ranged>(EquippedWeapon))
    {
        RangedWeapon->FinishReload();
    }

    if (StateComponent && StateComponent->GetState() == ECharacterState::Reloading)
    {
        StateComponent->SetState(ECharacterState::Idle);
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
        // 3. 재장전 몽타주가 끝났을 때 (또는 끊겼을 때)
        else if (EquippedWeapon && Montage == EquippedWeapon->GetReloadMontage())
        {
            CompleteReload();
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

            MaxHP = CurrentCharacterStat.MaxHP;
            CurrentHP = MaxHP;
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
