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
#include "GJPlayerHUDWidget.h"
#include "GJInventoryComponent.h"
#include "GJCardComponent.h"
#include "GJInteractable.h"
#include "GJInventoryWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GJGameMode.h"

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

    InventoryComponent = CreateDefaultSubobject<UGJInventoryComponent>(TEXT("InventoryComponent"));
    CardComponent = CreateDefaultSubobject<UGJCardComponent>(TEXT("CardComponent"));

    // 무기 슬롯 2칸(0번/1번) - 처음엔 둘 다 빈 슬롯
    WeaponSlots.Init(nullptr, 2);

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

        // 게임오버 화면에서 건 FInputModeUIOnly는 뷰포트 클라이언트에 저장되는데, 뷰포트는
        // 월드보다 오래 살기 때문에 OpenLevel로 레벨을 갈아끼워도 "게임 입력 무시" 상태가
        // 그대로 따라온다. 그러면 허브에 도착해도 WASD가 전혀 안 먹고 마우스만 움직인다.
        // 캐릭터가 스폰될 때마다 게임 입력 모드로 되돌려서, 어느 레벨이든 항상 정상 상태로 시작하게 함.
        FInputModeGameOnly InputMode;
        InputMode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(InputMode);
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

    // 좌측 상단 HP/MP 바 HUD 자동 생성 및 화면 표시
    if (PlayerHUDWidgetClass)
    {
        if (APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            PlayerHUDWidgetInstance = CreateWidget<UGJPlayerHUDWidget>(PC, PlayerHUDWidgetClass);
            if (PlayerHUDWidgetInstance)
            {
                PlayerHUDWidgetInstance->AddToViewport();
            }
        }
    }

    OnDamaged.AddDynamic(this, &AGJCharacter::OnHPChanged);
    UpdatePlayerHUD();
}

void AGJCharacter::HandleDeath()
{
    Super::HandleDeath();

    // 죽는 순간 연사 중이었다면 즉시 멈춤 (그렇지 않으면 Tick에서 TryAutoFire가 계속 호출됨 -
    // Dead 상태 체크로 어차피 막히긴 하지만, 아예 호출 자체를 멈추는 게 더 깔끔함)
    bIsAutoFiring = false;

    // 런 종료 흐름(회차 카운트, 화면, 레벨 이동)은 전부 게임 모드가 담당한다.
    // 캐릭터는 "죽었다"는 사실만 알리고 게임 흐름은 알지 못한다.
    if (AGJGameMode* GJGameMode = Cast<AGJGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GJGameMode->OnPlayerDied();
    }
    else
    {
        // 다른 게임 모드를 쓰는 레벨에서 죽어도 크래시하지 않도록 로그만 남긴다
        UE_LOG(LogTemp, Warning, TEXT("HandleDeath: GameMode is not AGJGameMode. The run end flow will not run in this level."));
    }
}

// ==========================================
// [신규] 인벤토리 열기/닫기
// ==========================================
void AGJCharacter::ToggleInventory()
{
    if (!InventoryWidgetClass) return;

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) return;

    // 위젯은 처음 열 때 1회만 생성해서 재사용함
    if (!InventoryWidgetInstance)
    {
        InventoryWidgetInstance = CreateWidget<UGJInventoryWidget>(PC, InventoryWidgetClass);
        if (InventoryWidgetInstance)
        {
            InventoryWidgetInstance->InitializeInventory(InventoryComponent);
        }
    }

    if (!InventoryWidgetInstance) return;

    if (InventoryWidgetInstance->IsInViewport())
    {
        // 닫기: 위젯 제거 + 일시정지 해제 + 입력을 다시 게임으로만 돌림
        InventoryWidgetInstance->RemoveFromParent();
        UGameplayStatics::SetGamePaused(this, false);

        // FInputModeGameOnly 기본값(bConsumeCaptureMouseDown=true)은 마우스 캡처를 다시 잡는 그 클릭을
        // "캡처용으로만" 소모하고 게임 입력으로는 넘기지 않음 - 그래서 인벤토리를 닫은 직후의 클릭이
        // 간헐적으로 씹혔던 것. false로 줘서 캡처를 다시 잡는 클릭도 그대로 게임 입력으로 전달되게 함.
        FInputModeGameOnly InputMode;
        InputMode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(InputMode);
    }
    else
    {
        // 열기: 위젯 표시 + 일시정지(SetGamePaused는 PlayerController::SetPause를 거는 것뿐이라
        // 매 프레임 비용이 드는 방식이 아니라 액터 Tick 스케줄 자체를 건너뛰게 하는, 사실상 공짜에 가까운 방법임.
        // UI/Slate는 월드 Tick과 별개라 정지 중에도 계속 반응함)
        InventoryWidgetInstance->AddToViewport();
        UGameplayStatics::SetGamePaused(this, true);

        // 입력 모드가 UI로 바뀌는 과정에서 마우스 버튼 "뗌(release)" 이벤트가 캐릭터한테
        // 안 들어가는 경우가 있어서, 열 때 연사 중이었다면 강제로 꺼줌 - 안 그러면 인벤토리를
        // 닫고 Tick이 다시 돌 때 bIsAutoFiring이 true로 박혀있어서 무한 연사가 됨
        bIsAutoFiring = false;

        // 위젯에 키보드 포커스를 줌 - Tab을 "닫기"로 처리하는 건 이제 이 위젯의
        // NativeOnPreviewKeyDown(GJInventoryWidget.cpp)이 직접 담당함(포커스가 있어야 키 이벤트가 위젯으로 들어옴).
        // 일시정지 중에는 캐릭터가 물고 있는 Enhanced Input 액션 평가가 안정적으로 안 들어올 수 있어서,
        // 게임 로직과 무관한 UI 레이어(Slate 키 이벤트)에서 직접 처리하는 쪽이 더 확실함.
        //
        // GameAndUI 모드는 UI 패널 바깥(인벤토리 밖) 클릭이 게임 뷰포트로 그대로 흘러가는데, 그
        // 클릭이 키보드 포커스를 뷰포트 쪽으로 가져가버려서 그 다음부턴 Tab이 이 위젯까지 아예
        // 안 왔음(어차피 열려있는 동안은 게임이 일시정지 상태라 뒤쪽 게임 클릭이 의미도 없음).
        // UIOnly로 완전히 UI에만 입력을 묶어서 포커스가 위젯 밖으로 새지 않게 함.
        FInputModeUIOnly InputMode;
        InputMode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
        PC->SetInputMode(InputMode);
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
    if (StateComponent && (StateComponent->GetState() == ECharacterState::Dodge || StateComponent->GetState() == ECharacterState::Dead))
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
            // 방향 키를 뗐을 때 MoveInput을 0으로 되돌림 (안 그러면 마지막 방향이 계속 남아있음)
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &AGJCharacter::MoveInputReleased);
            EnhancedInput->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AGJCharacter::MoveInputReleased);
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

        // [신규] 상호작용 입력 바인딩 (아이템 습득 / 나중에 문·버튼 등에도 재사용)
        if (InteractAction)
        {
            EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &AGJCharacter::InteractInputPressed);
        }

        // [신규] 인벤토리 열기/닫기 입력 바인딩 (Tab)
        if (InventoryToggleAction)
        {
            EnhancedInput->BindAction(InventoryToggleAction, ETriggerEvent::Started, this, &AGJCharacter::ToggleInventory);
        }

        // [신규] 무기 슬롯 스왑 입력 바인딩 (1번/2번 키)
        if (WeaponSlot1Action)
        {
            EnhancedInput->BindAction(WeaponSlot1Action, ETriggerEvent::Started, this, &AGJCharacter::SwapToWeaponSlot1);
        }
        if (WeaponSlot2Action)
        {
            EnhancedInput->BindAction(WeaponSlot2Action, ETriggerEvent::Started, this, &AGJCharacter::SwapToWeaponSlot2);
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

void AGJCharacter::MoveInputReleased()
{
    MoveInput = FVector2D::ZeroVector;
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
    if (StateComponent->GetState() == ECharacterState::Dead) return;
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
    if (StateComponent->GetState() == ECharacterState::Dead) return;
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

    // 꽉 채우는 데 필요한 발수 (CanReload()를 통과했으므로 항상 1발 이상)
    const int32 BulletsNeeded = RangedWeapon->GetWeaponStat().MagazineSize - RangedWeapon->GetCurrentAmmo();
    const float MPCostPerAmmo = RangedWeapon->GetWeaponStat().MPCostPerAmmo;

    // MP가 허락하는 한도까지만 채움 - 꼭 탄창을 다 채울 MP가 없어도 되고, 있는 만큼만 리필됨
    // (MPCostPerAmmo가 0이면 MP 소모 없는 무기이므로 항상 꽉 채움)
    const int32 BulletsToRefill = (MPCostPerAmmo > 0.f)
        ? FMath::Min(BulletsNeeded, FMath::FloorToInt(CurrentMP / MPCostPerAmmo))
        : BulletsNeeded;

    // MP가 1발 채울 만큼도 없으면 재장전 자체를 시작하지 않음
    if (BulletsToRefill <= 0) return;

    const float MPCost = MPCostPerAmmo * BulletsToRefill;
    CurrentMP = FMath::Clamp(CurrentMP - MPCost, 0.f, MaxMP);
    UpdatePlayerHUD();

    RangedWeapon->StartReload(BulletsToRefill);
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
// [신규] 상호작용 (아이템 습득 등)
// ==========================================
void AGJCharacter::InteractInputPressed()
{
    TArray<AActor*> OverlappingActors;
    GetCapsuleComponent()->GetOverlappingActors(OverlappingActors);

    for (AActor* Actor : OverlappingActors)
    {
        if (Actor && Actor->Implements<UGJInteractable>())
        {
            IGJInteractable::Execute_Interact(Actor, this);
            return; // 가장 먼저 찾은 대상 하나만 상호작용
        }
    }
}

// ==========================================
// 기존 로직들
// ==========================================
void AGJCharacter::PerformDodge()
{
    if (!StateComponent || StateComponent->GetState() != ECharacterState::Idle) return;

    FVector WorldDirection;
    if (MoveInput.IsNearlyZero())
    {
        // 이동 방향 입력이 없으면 캐릭터가 지금 바라보고 있는 방향으로 닷지함
        WorldDirection = GetActorForwardVector();
    }
    else
    {
        WorldDirection = FVector(MoveInput.Y, MoveInput.X, 0.f);
        WorldDirection.Normalize();
    }

    const float DodgeDistance = 500.f;

    // 경로 중간에 턱/오르막처럼 높이가 있는 지형이 있으면 모션 워핑과 무브먼트 컴포넌트가
    // 서로 다투며 캐릭터가 튕겨나가는 문제가 있었음 - 닷지 시작 전에 캡슐을 목적지까지 그대로
    // (높이 변화 없이) 스윕해서, 막혀있으면 걸린 지점 바로 앞까지만 워프 거리를 줄임.
    // (예전엔 아예 닷지 자체를 취소했는데, 그러면 벽을 등지고 있을 때 아예 회피가 안 나가버려서
    // - 이동은 못 해도 최소한 그 방향 회피 동작/무적 프레임은 나가도록 "취소" 대신 "거리 축소"로 변경)
    float ActualDodgeDistance = DodgeDistance;
    {
        // 캡슐을 캐릭터 발밑 높이 그대로 스윕하면 CMC가 원래 알아서 잘 밟고 올라가는 작은 단차/
        // 완만한 오르막까지도 전부 "막힘"으로 잡혀서 닷지가 씹혔음. 실제로 튀는 버그는 CMC가
        // 자동으로 못 올라가는(MaxStepHeight보다 높은) 진짜 턱/벽에서만 났으므로, 스윕 시작 높이를
        // "밟고 올라갈 수 있는 높이" 만큼 들어올려서 그 이하 높이의 지형은 애초에 안 걸리게 함
        const UCharacterMovementComponent* MoveComp = GetCharacterMovement();
        const float StepUpTolerance = (MoveComp ? MoveComp->MaxStepHeight : 45.f) * 0.9f;

        const FVector SweepStart = GetActorLocation() + FVector(0.f, 0.f, StepUpTolerance);
        const FVector SweepEnd = SweepStart + WorldDirection * DodgeDistance;

        FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
            GetCapsuleComponent()->GetScaledCapsuleRadius(),
            GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

        FCollisionQueryParams SweepParams;
        SweepParams.AddIgnoredActor(this);

        // ECC_Pawn 채널로 스윕하면 근처에 있는 다른 폰(적)이나 날아가는 총알(WorldDynamic)까지
        // "막힘"으로 잡혀서 닷지가 애먼 타이밍에 씹혔음 - 오브젝트 타입이 WorldStatic(정적 지형)인
        // 것만 걸리도록 해서 환경(벽/턱)에만 반응하게 함
        FCollisionObjectQueryParams ObjectParams;
        ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);

        FHitResult SweepHit;
        const bool bSweepHit = GetWorld()->SweepSingleByObjectType(SweepHit, SweepStart, SweepEnd, FQuat::Identity, ObjectParams, CapsuleShape, SweepParams);

        // bStartPenetrating이 true인 히트는 "스윕 시작 지점에서 이미 겹쳐있던 것" - 캐릭터 캡슐은
        // 원래 서 있는 바닥과 항상 거의 맞닿아 있어서, 이걸 걸러내지 않으면 평지에서도 매번
        // 바닥 자체가 "막힘"으로 잡혀 닷지가 거의 항상 씹히는 문제가 있었음
        if (bSweepHit && !SweepHit.bStartPenetrating)
        {
            // 걸린 표면이 "걸어서 오를 수 있는 경사"(오르막)인지 판단 - 오르막은 CMC가 원래
            // 스텝업 없이도 그냥 걸어 올라가는 지형이라 튕김 버그와 무관함. 부딪힌 지점의 법선이
            // 거의 수직(=진짜 벽/못 오르는 턱)일 때만 거리를 줄이고, 걸을 수 있는 경사면이면
            // 걸린 걸로 치지 않고 원래 거리 그대로 통과시킴
            const float WalkableFloorAngle = MoveComp ? MoveComp->GetWalkableFloorAngle() : 44.7f;
            const float HitSurfaceAngle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(SweepHit.Normal.Z, -1.f, 1.f)));

            if (HitSurfaceAngle > WalkableFloorAngle)
            {
                // 걸린 지점보다 살짝 안쪽까지만 - 모션 워핑 목표 지점이 막힌 지형 너머(도달 불가능한
                // 곳)로 잡히지 않게 해서, 튕겨나가는 원인 자체를 없앰
                ActualDodgeDistance = FMath::Max(SweepHit.Distance - 10.f, 0.f);
            }
        }
    }

    FVector Local = GetActorTransform().InverseTransformVectorNoScale(WorldDirection);
    Local.Normalize();

    float Angle = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
    Angle = FRotator::NormalizeAxis(Angle);

    UAnimMontage* Montage = nullptr;
    FVector FacingDirection = WorldDirection;

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
        FVector WarpLocation = GetActorLocation() + WorldDirection * ActualDodgeDistance;

        // 목적지에 오르막/턱이 있으면 시작 지점 높이를 그대로 워프 타깃 Z로 쓰는 게 실제 바닥과 어긋나서,
        // 무브먼트 컴포넌트의 바닥/계단 보정과 모션 워핑이 서로 다른 높이로 캐릭터를 끌어당기며
        // 충돌 - 움찔거리다가 튕겨나가는 원인이 됨. 목적지 위아래로 바닥을 트레이스해서 Z를 보정.
        const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        const FVector TraceStart = WarpLocation + FVector(0.f, 0.f, CapsuleHalfHeight + 100.f);
        const FVector TraceEnd = WarpLocation - FVector(0.f, 0.f, CapsuleHalfHeight + 200.f);

        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        FHitResult HitResult;
        if (GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
        {
            WarpLocation.Z = HitResult.ImpactPoint.Z + CapsuleHalfHeight;
        }

        MotionWarpingComponent->AddOrUpdateWarpTargetFromLocation(FName("DodgeTarget"), WarpLocation);
    }

    // 구르는 도중에 다른 폰(적)과 부딪히면, 모션 워핑이 계속 목표 지점으로 끌어당기는 것과
    // 무브먼트 컴포넌트의 충돌 보정(밀어내기)이 서로 다투면서 지형 턱에서와 같은 튕김 현상이 났음 -
    // 사전 스윕은 시작 시점 기준이라 구르는 도중 움직이는 적까지는 못 막으므로, 닷지 중엔 아예
    // Pawn 채널 충돌을 무시하게 해서 부딪히지 않고 지나치게 함 (닷지 무적 프레임과도 자연스럽게 맞음)
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

    StateComponent->SetState(ECharacterState::Dodge);
    PlayAnimMontage(Montage);
}

void AGJCharacter::OnMontageEndedEvent(UAnimMontage* Montage, bool bInterrupted)
{
    if (!StateComponent)
    {
        return;
    }

    // 예전엔 몽타주 "에셋 자체"로만 무슨 몽타주가 끝났는지 구분했는데, 스왑 몽타주 자리에 임시로
    // 기존 몽타주(예: 닷지 몽타주)를 재사용하면 그 에셋 하나가 여러 상태에서 동시에 쓰이게 되어
    // 엉뚱한 분기로 빠지는 문제가 있었음(예: 스왑 중에 끝났는데 닷지 분기로 처리되면서
    // "GetState() == Dodge" 검사에 걸려 WeaponSwap 상태가 영원히 안 풀림). 지금은 "그때 실제로
    // 어떤 상태였는지"를 먼저 보고 분기해서, 몽타주 에셋이 겹쳐도 안전하게 동작함.
    switch (StateComponent->GetState())
    {
    case ECharacterState::Dodge:
        // 닷지 동안 무시해뒀던 Pawn 충돌을 원래대로 복구 (몽타주가 끊겼을 때도 반드시 복구되어야 함)
        GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
        StateComponent->SetState(ECharacterState::Idle);
        break;

    case ECharacterState::WeaponSwap:
        // 실제 무기 교체는 SwapToWeaponSlot에서 이미 즉시 처리했으므로, 여기서는 입력을 막고
        // 있던 WeaponSwap 상태만 풀어줌
        StateComponent->SetState(ECharacterState::Idle);
        break;

    case ECharacterState::Attack:
        if (EquippedWeapon && Montage == EquippedWeapon->GetAttackMontage())
        {
            ResetCombo();
        }
        break;

    case ECharacterState::Reloading:
        if (EquippedWeapon && Montage == EquippedWeapon->GetReloadMontage())
        {
            CompleteReload();
        }
        break;

    default:
        break;
    }
}

void AGJCharacter::UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull)
{
    CurrentLevel = NewLevel;

    if (CharacterStatTable)
    {
        FString RowName = FString::FromInt(CurrentLevel);
        FCharacterStat* RowData = CharacterStatTable->FindRow<FCharacterStat>(FName(*RowName), TEXT("UpdateCharacterStat"));

        if (RowData)
        {
            // 테이블 원본만 갱신한다. 실효값 계산은 RecalculateStats 한 곳에서만 한다.
            BaseStat = *RowData;
        }
    }

    // 행이 없거나 테이블이 비어 있어도 호출한다 - 그래야 HUD 갱신과 하한 처리가
    // 어느 경로에서든 똑같이 걸린다.
    RecalculateStats(bRestoreToFull);
}

void AGJCharacter::RecalculateStats(bool bRestoreToFull)
{
    // 실효값 = (테이블값 + 가산) x (1 + 증가율)
    // 람다 하나로 9개 스탯을 같은 규칙으로 계산한다 - 규칙이 바뀌면 여기만 고친다.
    auto Combine = [](float Base, float Add, float Percent)
    {
        return (Base + Add) * (1.f + Percent);
    };

    FCharacterStat& S = CurrentCharacterStat;

    // MaxHP/MaxMP가 0이 되면 HUD의 Current/Max가 0으로 나누고, 최대 체력 0은 즉사다.
    S.MaxHP = FMath::Max(Combine(BaseStat.MaxHP, StatBonus.Add.MaxHP, StatBonus.Percent.MaxHP), 1.f);
    S.MaxMP = FMath::Max(Combine(BaseStat.MaxMP, StatBonus.Add.MaxMP, StatBonus.Percent.MaxMP), 1.f);

    // RequiredEXP가 0 이하가 되면 AddEXP의 while 가드(RequiredEXP > 0)에 걸려 레벨업이
    // 조용히 멈춘다. 크래시가 아니라 아무 일도 안 일어나서 원인 추적이 어려운 종류다.
    S.RequiredEXP = FMath::Max(Combine(BaseStat.RequiredEXP, StatBonus.Add.RequiredEXP, StatBonus.Percent.RequiredEXP), 1.f);

    // 공격력이 -100 아래로 가면 데미지 공식(무기데미지 x (1 + 공격력/100))이 음수를 내고,
    // TakeDamage의 CurrentHP -= 음수가 맞은 쪽을 회복시킨다. 치명타 배율도 같은 이유다.
    S.BaseAttackPower = FMath::Max(Combine(BaseStat.BaseAttackPower, StatBonus.Add.BaseAttackPower, StatBonus.Percent.BaseAttackPower), 0.f);
    // 음수 클램프가 필요한 이유는 BaseAttackPower와 같다 - 음수 데미지는 적을 치료한다.
    S.SkillPower = FMath::Max(Combine(BaseStat.SkillPower, StatBonus.Add.SkillPower, StatBonus.Percent.SkillPower), 0.f);
    S.CritMultiplier  = FMath::Max(Combine(BaseStat.CritMultiplier,  StatBonus.Add.CritMultiplier,  StatBonus.Percent.CritMultiplier),  0.f);

    S.MoveSpeed = FMath::Max(Combine(BaseStat.MoveSpeed, StatBonus.Add.MoveSpeed, StatBonus.Percent.MoveSpeed), 0.f);

    // 치명타 확률에 상한은 두지 않는다 - 1.0을 넘기면 항상 치명타인데, 그건 빌드가
    // 도달하려는 목표지 버그가 아니다.
    S.CritChance = FMath::Max(Combine(BaseStat.CritChance, StatBonus.Add.CritChance, StatBonus.Percent.CritChance), 0.f);

    // Defense는 하한을 걸지 않는다 - UGJCombatStatics::ApplyDefense가 이미 FMath::Max(Defense, 0)을
    // 한다. 같은 방어를 두 곳에 두면 나중에 한쪽만 고치게 된다.
    S.Defense = Combine(BaseStat.Defense, StatBonus.Add.Defense, StatBonus.Percent.Defense);

    // 아직 아무도 읽지 않는다. 스킬 시스템이 생기면 그쪽에서 범위를 정한다.
    S.CooldownReduction = Combine(BaseStat.CooldownReduction, StatBonus.Add.CooldownReduction, StatBonus.Percent.CooldownReduction);

    // 최대치가 오른 만큼만 현재값에 더한다(bRestoreToFull=false).
    // 레벨업과 "+5 최대 체력" 카드가 같은 이 한 줄을 지나므로, 카드가 현재 체력도 함께
    // 올려주는 동작이 따로 짤 것 없이 나온다.
    //
    // 하한이 0이 아니라 1인 이유: 스탯 변화는 데미지가 아니다. "최대 체력 -20%, 공격력 +30%"
    // 같은 리스크/리턴 카드를 체력이 낮을 때 고르면 현재 체력이 0으로 떨어지는데, 사망 판정은
    // TakeDamage 안에만 있어서 죽지는 않고 IsDead()만 true가 되는 좀비 상태가 된다.
    // 카드 한 장에 즉사하지도, 살아있는데 죽은 것으로 보이지도 않게 최소 1을 남긴다.
    // 이미 죽어있었다면 0을 유지해야 하므로 그때만 하한이 0이다.
    const float OldMaxHP = MaxHP;
    MaxHP = S.MaxHP;
    const float MinHP = (CurrentHP > 0.f) ? 1.f : 0.f;
    CurrentHP = bRestoreToFull ? MaxHP : FMath::Clamp(CurrentHP + (MaxHP - OldMaxHP), MinHP, MaxHP);

    const float OldMaxMP = MaxMP;
    MaxMP = S.MaxMP;
    CurrentMP = bRestoreToFull ? MaxMP : FMath::Clamp(CurrentMP + (MaxMP - OldMaxMP), 0.f, MaxMP);

    // TakeDamage와 GJWeapon_Ranged::Fire가 이 멤버들을 직접 읽으므로 실효값을 밀어 넣는다.
    Defense        = S.Defense;
    CritChance     = S.CritChance;
    CritMultiplier = S.CritMultiplier;

    GetCharacterMovement()->MaxWalkSpeed = S.MoveSpeed;

    UpdatePlayerHUD();
}

bool AGJCharacter::IsMaxLevel() const
{
    if (!CharacterStatTable)
    {
        // 테이블이 없으면 성장 자체가 불가능하다. 만렙으로 취급해서 AddEXP가 무한 루프에
        // 빠지지 않게 한다.
        return true;
    }

    const FString NextRowName = FString::FromInt(CurrentLevel + 1);

    // 세 번째 인자(bWarnIfRowMissing)에 false를 넘긴다 - 여기서 행이 없는 건 오류가 아니라
    // "만렙"이라는 정상 결과다. 기본값(true)으로 두면 만렙 도달 후 적을 죽일 때마다 경고가 쌓인다.
    const FCharacterStat* NextRow = CharacterStatTable->FindRow<FCharacterStat>(
        FName(*NextRowName), TEXT("IsMaxLevel"), false);

    return NextRow == nullptr;
}

void AGJCharacter::AddEXP(float Amount)
{
    if (Amount <= 0.f || IsMaxLevel())
    {
        return;
    }

    CurrentEXP += Amount;

    // 한 번에 여러 레벨이 오를 수 있다(경험치가 큰 보스 등). 매 반복마다 그 시점 레벨의
    // RequiredEXP를 빼므로 초과분이 정확히 다음 레벨로 이월된다.
    // RequiredEXP가 0 이하인 행이 있으면 무한 루프가 되므로 조건에 함께 둔다.
    while (CurrentCharacterStat.RequiredEXP > 0.f && CurrentEXP >= CurrentCharacterStat.RequiredEXP)
    {
        if (IsMaxLevel())
        {
            break;
        }

        CurrentEXP -= CurrentCharacterStat.RequiredEXP;
        LevelUp();  // 여기서 CurrentCharacterStat이 다음 레벨 값으로 갱신된다
    }

    if (IsMaxLevel())
    {
        // 만렙에서는 더 쌓아둘 곳이 없다. 0으로 두면 경험치 바가 빈 채로 남아서 "아직 더 오를
        // 수 있다"로 보이므로, 가득 찬 상태로 고정한다.
        CurrentEXP = CurrentCharacterStat.RequiredEXP;
    }

    UpdatePlayerHUD();
}

void AGJCharacter::LevelUp()
{
    // 레벨업은 회복이 아니다 - 최대치 증가분만 현재 HP/MP에 반영된다
    UpdateCharacterStat(CurrentLevel + 1, /*bRestoreToFull=*/false);

    UE_LOG(LogTemp, Log, TEXT("LevelUp! Level=%d, HP=%.0f/%.0f, NextRequiredEXP=%.0f"),
        CurrentLevel, CurrentHP, MaxHP, CurrentCharacterStat.RequiredEXP);

    // 아직 구독자가 없다. 레벨업 시 카드 3장을 띄우는 선택 시스템이 여기에 붙는다.
    OnLevelUp.Broadcast(CurrentLevel);
}

void AGJCharacter::AddStatBonus(const FStatModifier& Delta)
{
    StatBonus.Add += Delta.Add;
    StatBonus.Percent += Delta.Percent;

    // 카드는 회복이 아니다 - 최대치 증가분만 현재 HP/MP에 반영된다.
    // ("+5 최대 체력" 카드가 현재 체력도 +5 시키는 건 RecalculateStats가 처리한다)
    RecalculateStats(/*bRestoreToFull=*/false);
}

void AGJCharacter::GJDrawCards()
{
    if (!CardComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJDrawCards: CardComponent가 없습니다."));
        return;
    }

    CardComponent->GJDrawCards();
}

void AGJCharacter::GJSetTagWeight(const FString& TagName, float Multiplier)
{
    if (!CardComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSetTagWeight: CardComponent가 없습니다."));
        return;
    }

    // 두 번째 인자가 false면 등록되지 않은 태그일 때 경고 없이 빈 태그를 돌려준다.
    // 오타를 조용히 넘기지 않으려고 직접 확인하고 메시지를 찍는다.
    const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
    if (!Tag.IsValid())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("GJSetTagWeight: '%s'는 등록된 게임플레이 태그가 아닙니다. Config/DefaultGameplayTags.ini를 확인하세요."),
            *TagName);
        return;
    }

    CardComponent->SetTagWeightMultiplier(Tag, Multiplier);
    UE_LOG(LogTemp, Log, TEXT("GJSetTagWeight: %s -> x%.2f"), *Tag.ToString(), Multiplier);
}

void AGJCharacter::GJShowCards()
{
    if (!CardComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: CardComponent가 없습니다."));
        return;
    }

    CardComponent->GJShowCards();
}

bool AGJCharacter::ConsumeMP(float Amount)
{
    if (Amount <= 0.f)
    {
        return true;
    }

    if (CurrentMP < Amount)
    {
        return false;
    }

    CurrentMP = FMath::Clamp(CurrentMP - Amount, 0.f, MaxMP);
    UpdatePlayerHUD();
    return true;
}

void AGJCharacter::GJAddBonus(const FString& StatName, float AddValue, float PercentValue)
{
    FStatModifier Delta;

    // 스탯 이름을 해당 필드로 매핑한다. 대소문자는 구분하지 않는다.
    // 포인터-투-멤버를 쓰면 Add와 Percent 양쪽에 같은 필드를 지정하는 걸 한 줄로 쓸 수 있다.
    auto TryApply = [&](const TCHAR* Name, float FStatValues::* Member) -> bool
    {
        if (!StatName.Equals(Name, ESearchCase::IgnoreCase))
        {
            return false;
        }
        Delta.Add.*Member = AddValue;
        Delta.Percent.*Member = PercentValue;
        return true;
    };

    // 스탯이 늘어나면 여기에도 한 줄 추가해야 한다. 컴파일러가 안 잡아주는 지점이다.
    const bool bMatched =
        TryApply(TEXT("MaxHP"),             &FStatValues::MaxHP)             ||
        TryApply(TEXT("MaxMP"),             &FStatValues::MaxMP)             ||
        TryApply(TEXT("BaseAttackPower"),   &FStatValues::BaseAttackPower)   ||
        TryApply(TEXT("SkillPower"),        &FStatValues::SkillPower)        ||
        TryApply(TEXT("RequiredEXP"),       &FStatValues::RequiredEXP)       ||
        TryApply(TEXT("Defense"),           &FStatValues::Defense)           ||
        TryApply(TEXT("MoveSpeed"),         &FStatValues::MoveSpeed)         ||
        TryApply(TEXT("CooldownReduction"), &FStatValues::CooldownReduction) ||
        TryApply(TEXT("CritChance"),        &FStatValues::CritChance)        ||
        TryApply(TEXT("CritMultiplier"),    &FStatValues::CritMultiplier);

    if (!bMatched)
    {
        // 조용히 무시하면 오타를 쳤을 때 "보너스가 안 먹네"로 오인해서 없는 버그를 쫓게 된다.
        UE_LOG(LogTemp, Warning,
            TEXT("GJAddBonus: 알 수 없는 스탯 '%s'. 사용 가능: MaxHP, MaxMP, BaseAttackPower, SkillPower, RequiredEXP, Defense, MoveSpeed, CooldownReduction, CritChance, CritMultiplier"),
            *StatName);
        return;
    }

    AddStatBonus(Delta);

    UE_LOG(LogTemp, Log,
        TEXT("GJAddBonus: %s (가산 %.2f, 증가율 %.0f%%) -> HP=%.0f/%.0f, 공격력=%.1f, 스킬공격력=%.1f, 방어력=%.1f, 치명타=%.2f/x%.2f, 이동속도=%.0f, RequiredEXP=%.0f"),
        *StatName, AddValue, PercentValue * 100.f,
        CurrentHP, MaxHP,
        CurrentCharacterStat.BaseAttackPower, CurrentCharacterStat.SkillPower, Defense,
        CritChance, CritMultiplier,
        CurrentCharacterStat.MoveSpeed, CurrentCharacterStat.RequiredEXP);
}

void AGJCharacter::ApplyConsumableEffect(float HealAmount, float ManaAmount)
{
    CurrentHP = FMath::Clamp(CurrentHP + HealAmount, 0.f, MaxHP);
    CurrentMP = FMath::Clamp(CurrentMP + ManaAmount, 0.f, MaxMP);
    UpdatePlayerHUD();
}

void AGJCharacter::OnHPChanged(float DamageAmount, AActor* DamageCauser)
{
    UpdatePlayerHUD();
}

void AGJCharacter::UpdatePlayerHUD()
{
    if (!PlayerHUDWidgetInstance)
    {
        return;
    }

    PlayerHUDWidgetInstance->UpdateHP(CurrentHP, MaxHP);
    PlayerHUDWidgetInstance->UpdateMP(CurrentMP, MaxMP);
    PlayerHUDWidgetInstance->UpdateEXP(CurrentEXP, CurrentCharacterStat.RequiredEXP, CurrentLevel);
}

void AGJCharacter::EquipWeapon()
{
    if (DefaultWeaponClass)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = this;
        SpawnParams.Instigator = this;

        AGJWeaponBase* StartingWeapon = GetWorld()->SpawnActor<AGJWeaponBase>(DefaultWeaponClass, GetActorLocation(), GetActorRotation(), SpawnParams);

        if (StartingWeapon)
        {
            // 시작 무기는 필드에 놓인 픽업이 아니므로 처음부터 상호작용 판정을 꺼둠
            StartingWeapon->OnPickedUp(this);
            WeaponSlots[0] = StartingWeapon;
            CommitWeaponSwap(0);
        }
    }
}

AGJWeaponBase* AGJCharacter::GetWeaponInSlot(int32 SlotIndex) const
{
    return WeaponSlots.IsValidIndex(SlotIndex) ? WeaponSlots[SlotIndex] : nullptr;
}

void AGJCharacter::CommitWeaponSwap(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex) || !WeaponSlots[SlotIndex])
    {
        return;
    }

    if (EquippedWeapon && EquippedWeapon != WeaponSlots[SlotIndex])
    {
        // 비활성화되는 무기는 버려지는 게 아니라 그냥 손에서 치워짐(숨김) - 슬롯엔 계속 남아있음
        EquippedWeapon->SetActorHiddenInGame(true);

        // 탄약 UI가 더 이상 이 무기의 발사/재장전에 반응하지 않도록 구독 해제
        if (AGJWeapon_Ranged* OldRanged = Cast<AGJWeapon_Ranged>(EquippedWeapon))
        {
            OldRanged->OnAmmoChanged.RemoveDynamic(this, &AGJCharacter::HandleActiveWeaponAmmoChanged);
        }
    }

    CurrentWeaponSlotIndex = SlotIndex;
    EquippedWeapon = WeaponSlots[SlotIndex];

    EquippedWeapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, FName(TEXT("WeaponSocket")));
    EquippedWeapon->SetActorHiddenInGame(false);

    // 새로 손에 든 무기가 원거리 무기라면 탄약 UI가 이 무기의 발사/재장전을 구독하게 하고,
    // 스왑 직후 다음 발사/재장전을 기다리지 않고 즉시 현재 탄약으로 한 번 갱신해줌
    if (AGJWeapon_Ranged* NewRanged = Cast<AGJWeapon_Ranged>(EquippedWeapon))
    {
        NewRanged->OnAmmoChanged.AddUniqueDynamic(this, &AGJCharacter::HandleActiveWeaponAmmoChanged);
        OnActiveWeaponAmmoChanged.Broadcast(NewRanged->GetCurrentAmmo(), NewRanged->GetWeaponStat().MagazineSize);
    }
    else
    {
        // 근접 무기 등 원거리 무기가 아니면 탄약 UI를 0/0으로 알려서 이전 무기의 탄약이 남아
        // 표시되지 않게 함
        OnActiveWeaponAmmoChanged.Broadcast(0, 0);
    }
}

void AGJCharacter::HandleActiveWeaponAmmoChanged(int32 CurrentAmmo, int32 MaxAmmo)
{
    OnActiveWeaponAmmoChanged.Broadcast(CurrentAmmo, MaxAmmo);
}

void AGJCharacter::DropWeapon(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex))
    {
        return;
    }

    if (AGJWeaponBase* Weapon = WeaponSlots[SlotIndex])
    {
        WeaponSlots[SlotIndex] = nullptr;

        // 지금 손에 든 무기를 떨어뜨리는 거라면, CommitWeaponSwap이 "이전 무기"로 착각해서
        // 방금 필드에 되돌려놓은 무기를 다시 숨겨버리지 않도록 먼저 참조를 끊어둠
        if (EquippedWeapon == Weapon)
        {
            EquippedWeapon = nullptr;
        }

        Weapon->OnDropped(GetActorLocation() + GetActorForwardVector() * 100.f);
    }
}

bool AGJCharacter::PickUpWeapon(AGJWeaponBase* NewWeapon)
{
    if (!NewWeapon)
    {
        return false;
    }

    int32 TargetSlot = INDEX_NONE;
    bool bReplacedActiveSlot = false;
    if (!WeaponSlots[0])
    {
        TargetSlot = 0;
    }
    else if (!WeaponSlots[1])
    {
        TargetSlot = 1;
    }
    else
    {
        // 두 슬롯이 다 찼으면 현재 활성 슬롯의 무기를 필드에 떨어뜨리고 그 자리를 새 무기로 채움
        DropWeapon(CurrentWeaponSlotIndex);
        TargetSlot = CurrentWeaponSlotIndex;
        bReplacedActiveSlot = true;
    }

    NewWeapon->OnPickedUp(this);
    WeaponSlots[TargetSlot] = NewWeapon;

    // 방금 든 무기 자리를 대체했거나(안 그러면 손에 아무것도 안 들려있게 됨), 애초에 아무 무기도
    // 없었을 때만 자동으로 손에 장착함. 그 외(빈 슬롯에 새로 채워진 경우)엔 자동 장착하지 않고
    // 그냥 슬롯에만 채워둠 - 1/2번 키나 무기 페이지 클릭으로 직접 바꿔 껴야 함
    if (bReplacedActiveSlot || !EquippedWeapon)
    {
        CommitWeaponSwap(TargetSlot);
    }
    else
    {
        NewWeapon->SetActorHiddenInGame(true);
    }

    OnWeaponSlotsChanged.Broadcast();
    return true;
}

bool AGJCharacter::ReplaceWeaponInSlot(int32 SlotIndex, AGJWeaponBase* NewWeapon)
{
    if (!NewWeapon || !WeaponSlots.IsValidIndex(SlotIndex))
    {
        return false;
    }

    // 순서가 중요하다. 먼저 그 슬롯을 비워야 PickUpWeapon의 "빈 슬롯을 먼저 채운다" 로직이
    // 정확히 그 자리를 고른다. 순서를 뒤집으면 PickUpWeapon이 슬롯이 꽉 찬 것으로 보고
    // 현재 활성 무기를 떨어뜨려서, 플레이어가 고른 것과 다른 무기가 사라진다.
    DropWeapon(SlotIndex);
    return PickUpWeapon(NewWeapon);
}

void AGJCharacter::SwapToWeaponSlot(int32 SlotIndex)
{
    if (!WeaponSlots.IsValidIndex(SlotIndex) || !WeaponSlots[SlotIndex])
    {
        return; // 빈 슬롯으로는 전환할 수 없음
    }

    if (SlotIndex == CurrentWeaponSlotIndex)
    {
        return; // 이미 사용 중인 무기
    }

    if (!StateComponent)
    {
        return;
    }

    const ECharacterState CurState = StateComponent->GetState();
    if (CurState == ECharacterState::Dead || CurState == ECharacterState::Attack ||
        CurState == ECharacterState::Reloading || CurState == ECharacterState::Dodge ||
        CurState == ECharacterState::WeaponSwap)
    {
        return;
    }

    UAnimMontage* SwapMontage = WeaponSlots[SlotIndex]->GetSwapMontage();

    CommitWeaponSwap(SlotIndex);
    OnWeaponSlotsChanged.Broadcast();

    if (SwapMontage)
    {
        // PlayAnimMontage는 몽타주가 캐릭터 스켈레톤과 안 맞는 등 실제로 재생을 못 시키면 0을
        // 반환함 - 이때 그냥 WeaponSwap 상태로 잠가버리면 몽타주가 끝났다는 콜백이 영원히 안 와서
        // 캐릭터가 그 상태에 계속 갇혀버림(공격/닷지/스왑이 전부 막힘). 실제로 재생에 성공했을
        // 때만 상태를 잠그고, 실패하면 (몽타주가 없을 때처럼) 그냥 즉시 교체로 취급함.
        const float PlayLength = PlayAnimMontage(SwapMontage);
        if (PlayLength > 0.f)
        {
            StateComponent->SetState(ECharacterState::WeaponSwap);
        }
    }
}

bool AGJCharacter::SwapWeaponSlots(int32 IndexA, int32 IndexB)
{
    if (!WeaponSlots.IsValidIndex(IndexA) || !WeaponSlots.IsValidIndex(IndexB) || IndexA == IndexB)
    {
        return false;
    }

    WeaponSlots.Swap(IndexA, IndexB);

    // 배열상의 자리만 바뀌었을 뿐 실제로 손에 들고 있는 무기(EquippedWeapon)는 그대로이므로,
    // 활성 슬롯 번호도 그 무기를 따라 옮겨줘야 함(안 그러면 활성 슬롯 표시가 엉뚱한 칸에 남음)
    if (EquippedWeapon == WeaponSlots[IndexA])
    {
        CurrentWeaponSlotIndex = IndexA;
    }
    else if (EquippedWeapon == WeaponSlots[IndexB])
    {
        CurrentWeaponSlotIndex = IndexB;
    }

    OnWeaponSlotsChanged.Broadcast();
    return true;
}

void AGJCharacter::SwapToWeaponSlot1()
{
    SwapToWeaponSlot(0);
}

void AGJCharacter::SwapToWeaponSlot2()
{
    SwapToWeaponSlot(1);
}
