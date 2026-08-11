# Project_GJ 개발 가이드 (코드 리뷰용)

> 대상: 처음 이 코드베이스를 리뷰하는 사람. "어디에 뭐가 있고, 뭘 하는 함수인지" 빠르게 파악하는 용도.
> 기준: UE 5.8, C++ 우선 + 얇은 블루프린트 레이어. 탑다운 카메라 런앤건/핵앤슬래시. 솔로 개발.
> 마지막 갱신 시점: 2026-08-08 세션 기준 (닷지 버그 수정, 무기 2슬롯 스왑/필드 픽업 시스템, 탄약 UI 델리게이트 재설계, 인벤토리 UI(그리드+무기 페이지) 구현까지 반영됨)

---

## 0. 폴더 구조 — 뭐가 진짜고 뭐가 안 쓰는 코드인가

| 위치 | 상태 |
|---|---|
| `Source/Project_GJ/*.h .cpp` (루트) | **실제 게임 로직.** 캐릭터, 무기, 투사체, UI, 데이터 타입 |
| `Source/Project_GJ/AI/*` | **실제 게임 로직.** 적 AI (AIController, BT Service/Task) |
| `Source/Project_GJ/Variant_Strategy/`, `Variant_TwinStick/` | **안 씀.** Epic "Top Down" 템플릿 샘플 잔재. 참고하지 말 것 |
| `Project_GJCharacter`, `Project_GJGameMode`, `Project_GJPlayerController` (접두사 `Project_GJ`, `GJ`가 아님 주의) | **안 씀.** 프로젝트 생성 시 Epic이 자동 생성한 abstract 베이스 스캘폴딩. 실제로는 `AGJCharacter`/`AGJGameMode`/`AGJPlayerController`가 쓰임 |
| `MyGJWeaponBase` | **안 씀.** `AGJWeaponBase`의 빈 미사용 서브클래스 스텁 |
| `Content/GJ/` | **실제 콘텐츠** (블루프린트, 데이터 테이블, 애니메이션, UI 위젯) |
| `Content/Variant_Strategy/`, `Variant_TwinStick/` | **안 씀.** 위와 동일한 템플릿 잔재 |

⚠️ **인코딩 주의**: `Source/Project_GJ/*.cpp/.h`의 기존 한글 주석 중 일부는 **CP949(EUC-KR)**로 저장되어 있음 (초기 파일들, 예: `CharacterStateComponent.h`). UTF-8 기준 툴로 열면 깨져 보일 수 있는데 파일 자체는 CP949로는 정상. 이 문서는 UTF-8로 작성됨.

---

## 1. 클래스 계층 한눈에 보기

```
AGJBaseCharacter (abstract, ACharacter + IAbilitySystemInterface)
├─ AGJCharacter        — 플레이어
└─ AGJEnemyCharacter    — 적(잡몹)

AGJWeaponBase (abstract, AActor, IGJInteractable 구현)
└─ AGJWeapon_Ranged     — 원거리 무기 (현재 유일하게 구현된 무기 타입)

AGJItemBase (abstract, AActor, IGJInteractable 구현)
└─ AGJItem              — 데이터 테이블 기반 범용 습득 아이템 (BP_Item_XXX로 파생시켜 사용)

IGJInteractable (UInterface) — 상호작용 가능한 모든 것의 공통 인터페이스
                                (소비 아이템, 필드에 놓인 무기, 나중에 문/버튼 등)

AGJProjectile (AActor)  — 오브젝트 풀링되는 총알

UCharacterStateComponent (UActorComponent) — Idle/Attack/Dodge/WeaponSwap 등 상태 머신
UGJInventoryComponent (UActorComponent) — 플레이어 인벤토리 (AGJCharacter에 부착)

AI (플레이어 아님, 적 전용)
├─ AGJEnemyAIController (AAIController)
├─ UBTService_UpdateTarget (UBTService)
└─ UBTTask_MeleeAttack (UBTTaskNode)

UI (UUserWidget)
├─ UGJHealthBarWidget     — 적 머리 위 체력바 (월드 위젯 컴포넌트)
├─ UGJPlayerHUDWidget     — 좌상단 플레이어 HP/MP 바 (뷰포트 HUD)
├─ UGJInventoryWidget     — 인벤토리 창 (아이템 그리드 페이지 + 무기 페이지, 탭 전환)
├─ UGJInventorySlotWidget — 인벤토리 그리드 한 칸 (드래그로 자리 교체, 더블클릭으로 소비 아이템 사용)
└─ UGJWeaponSlotWidget    — 무기 페이지 한 칸 (클릭으로 장착 전환, 드래그로 1/2번 자리 교체)

데이터 (GJGameTypes.h, USTRUCT : FTableRowBase)
├─ FCharacterStat  — DT_CharacterStat
├─ FWeaponStat     — DT_WeaponStat
├─ FEnemyStat      — DT_EnemyStat
└─ FItemData       — 인벤토리 아이템 정의 (예: DT_ItemData)
```

---

## 2. 캐릭터 계층

### 2.1 `AGJBaseCharacter` (GJBaseCharacter.h/.cpp) — 추상 베이스

플레이어/적이 공통으로 쓰는 HP·데미지·상태컴포넌트를 여기서 관리.

| 멤버/함수 | 설명 |
|---|---|
| `StateComponent`, `MotionWarpingComponent` | 공통 컴포넌트 (생성자에서 CreateDefaultSubobject) |
| `MaxHP`, `CurrentHP` | 체력. `EditDefaultsOnly` MaxHP, 나머지는 `TakeDamage`가 관리 |
| `IsDead()` | `CurrentHP <= 0` |
| `TakeDamage(...)` (override) | `Super::TakeDamage` 호출 → `CurrentHP` 감소 → `OnDamaged` 브로드캐스트 → 0 이하면 `HandleDeath()` |
| `OnDamaged` (델리게이트) | `(float DamageAmount, AActor* DamageCauser)`. UI 등에서 폴링 없이 바인딩해서 씀 |
| `OnDeath()` (BlueprintImplementableEvent) | 사망 연출/VFX용 훅. 아직 아무것도 바인딩 안 됨 |
| `HandleDeath()` (virtual) | 상태를 `Dead`로, **캡슐+메시 콜리전을 NoCollision으로 끔**(사망 후 총알 판정 완전 무시), 이동 비활성화, `OnDeath()` 호출 |

> GAS(GameplayAbilitySystem)는 모듈만 링크되어 있고 `GetAbilitySystemComponent()`는 항상 `nullptr` 반환. 아직 실제로 안 쓰임.

### 2.2 `AGJCharacter` (GJCharacter.h/.cpp) — 플레이어

`BP_GJCharacter`로 블루프린트화됨. Enhanced Input 기반, 마우스 조준.

**이동/카메라**
- `Move()` — 이동 입력 바인딩. `MoveInput`(BlueprintReadOnly)을 항상 raw 입력값으로 갱신(실제 이동 성공 여부와 무관 — 벽에 막혀 못 움직여도 "입력 방향"은 정확히 유지됨, 닷지 방향 판단에 씀)
- `MoveInputReleased()` — 이동 키를 뗄 때 `MoveInput`을 0으로 되돌림 (Completed/Canceled 트리거에 바인딩. 안 하면 마지막 입력 방향이 계속 남아 방향 입력 없이 닷지할 때 엉뚱한 방향으로 나감)
- `UpdateCharacterRotation()` — 마우스 커서를 캐릭터의 지면 평면에 투영해서 그 방향으로 회전
- `UpdateCameraOffset()` / `ApplyCameraOffset()` — 마우스가 화면 중심에서 먼 쪽으로 카메라를 살짝 당겨주는 연출

**회피(닷지)**
- `PerformDodge()` — `Idle` 상태에서만 시작. 입력 방향 없으면(`MoveInput`이 거의 0) 캐릭터가 지금 보고 있는 방향으로, 있으면 그 방향으로 회피
- **경로 사전 체크**: 목적지까지 캡슐을 그대로 스윕해서 막혀있는지 미리 확인함
  - 오브젝트 타입을 `ECC_WorldStatic`만 검사(`SweepSingleByObjectType`) — 채널 스윕(`ECC_Pawn`)을 썼을 때는 근처 적/날아가는 총알까지 "막힘"으로 잡혀 닷지가 애먼 타이밍에 씹히는 문제가 있었음
  - 스윕 시작 높이를 `MaxStepHeight * 0.9`만큼 들어올림 → CMC가 원래 자연스럽게 밟고 올라가는 낮은 단차는 걸리지 않음
  - 걸린 지점의 표면 법선이 `WalkableFloorAngle`(걸어서 오를 수 있는 경사각) 이내면 막힌 걸로 안 침 — 오르막을 억지로 막지 않기 위함
  - 그래도 진짜로 못 넘는 벽/턱에 걸리면 **닷지 자체를 취소하지 않고** 걸린 지점 바로 앞까지만 거리를 줄임 — 벽을 등지고 있어도 최소한 방향 전환 애니메이션은 나가게 하려는 의도 (예전엔 완전 취소였는데, 그러면 벽 옆에서 아예 회피가 안 나가는 문제가 있었음)
  - `bStartPenetrating`인 히트(스윕 시작점에서 이미 겹쳐있던 것 = 서 있는 바닥 자체)는 무시함 — 안 그러면 평지에서도 매번 바닥이 "막힘"으로 잡혀 닷지가 거의 항상 씹힘
- **모션 워핑 타깃 Z 보정**: 목적지 위아래로 라인트레이스해서 실제 바닥 높이로 워프 타깃 Z를 잡음 — 시작 지점 높이를 그대로 쓰면 오르막/턱에서 모션 워핑과 무브먼트 컴포넌트의 바닥 보정이 서로 다른 높이를 잡아당기며 캐릭터가 움찔거리다 튕겨나가는 버그가 있었음
- **닷지 중 Pawn 충돌 무시**: 캡슐의 `ECC_Pawn` 채널 응답을 `Dodge` 상태 진입 시 `Ignore`로 바꿔서, 구르는 도중 움직이는 적과 부딪혀 튕기는 것도 방지함(사전 스윕은 시작 시점 기준이라 구르는 도중엔 못 막음). 몽타주 종료 시 `Block`으로 복구. 결과적으로 닷지 무적 프레임과도 자연스럽게 맞음
- `OnMontageEndedEvent(Montage, bInterrupted)` — **"그 순간 실제 상태가 뭐였는지"로 먼저 분기**(`switch(StateComponent->GetState())`), 몽타주 에셋 identity로 분기하지 않음. 스왑 몽타주 자리에 임시로 기존 몽타주(예: 닷지 몽타주)를 재사용해도 안전하게 동작함 (에셋 identity로만 분기했을 때는, 같은 에셋을 여러 상태에서 재사용하면 엉뚱한 분기로 빠져서 `WeaponSwap` 같은 상태가 영원히 안 풀리는 버그가 있었음)
  - `Dodge` → Pawn 충돌 복구 + `Idle`
  - `WeaponSwap` → `Idle` (실제 무기 교체 자체는 `SwapToWeaponSlot`에서 이미 즉시 처리됨, 여기선 입력 잠금만 풀어줌)
  - `Attack` → 몽타주가 무기의 `AttackMontage`와 일치할 때만 `ResetCombo()`
  - `Reloading` → 몽타주가 무기의 `ReloadMontage`와 일치할 때만 `CompleteReload()`

**전투 — 원거리(현재 장착 무기 기준)**
- `AttackInputPressed()` — 장착 무기가 `AGJWeapon_Ranged`면: 몽타주 1회 재생(비주얼용) + `Fire()` 즉시 호출 + `bIsAutoFiring = true`
- `TryAutoFire()` — `Tick()`에서 `bIsAutoFiring`일 때 매 프레임 호출. 실제 발사 간격 판단은 무기 내부 쿨다운(`FireInterval`) 하나로만 함 (타이머+쿨다운 이중 판정을 피해서 연사 편차 문제를 해결한 이력 있음)
- `AttackInputReleased()` — `bIsAutoFiring = false`
- 무기가 근접 타입이면(현재 미사용 경로) `CurrentComboCount`로 `Attack1`/`Attack2`... 몽타주 섹션을 점프하는 콤보 로직 있음 (`AdvanceCombo`/`ResetCombo`, AnimNotify에서 호출)

**재장전 + MP**
- `ReloadInputPressed()` — 근접 무기면 무시. `CanReload()` 통과 시:
  1. 꽉 채우는 데 필요한 발수(`BulletsNeeded`) 계산
  2. **MP가 허락하는 한도까지만** 채움 (`CurrentMP / MPCostPerAmmo`로 발수 역산) → 부분 재장전 가능
  3. MP를 소모한 만큼 차감하고 `RangedWeapon->StartReload(BulletsToRefill)` 호출
  4. 재장전 몽타주 있으면 재생(끝나면 `OnMontageEndedEvent`→`CompleteReload`), 없으면 `ReloadTime` 타이머로 대체
- `CompleteReload()` — 무기의 `FinishReload()` 호출 + 상태를 `Idle`로 복귀

**스탯 / 레벨**
- `UpdateCharacterStat(NewLevel)` — `DT_CharacterStat`에서 레벨(행 이름) 조회 → `MaxHP/CurrentHP`, `MaxMP/CurrentMP` 갱신 → `UpdatePlayerHUD()` 호출

**무기 장착 / 스왑 (2슬롯 듀얼 웰드)**

플레이어는 무기 슬롯을 **2개**(`WeaponSlots[0]`/`[1]`, 1·2번 키로 전환) 가짐. 무기는 데이터 테이블 스택이 아니라 **각각 별개의 액터 인스턴스**를 그대로 들고 있음 — 같은 무기를 두 개 주워도 서로 다른 오브젝트로 취급.

| 함수/멤버 | 설명 |
|---|---|
| `EquipWeapon()` | `BeginPlay`에서 호출. `DefaultWeaponClass` 스폰 → `OnPickedUp()`으로 필드 픽업 판정 꺼둠 → `WeaponSlots[0]`에 채우고 `CommitWeaponSwap(0)`으로 즉시 장착 |
| `PickUpWeapon(NewWeapon)` | 필드에 놓인 무기(`AGJWeaponBase`, `IGJInteractable` 구현체)를 E로 상호작용하면 `Interact_Implementation`에서 호출됨. 빈 슬롯 우선으로 채우고, 둘 다 차있으면 현재 활성 슬롯 무기를 `DropWeapon()`으로 필드에 되돌린 뒤 그 자리를 새 무기로 채움. **자동 장착은 (a) 방금 활성 슬롯을 대체했을 때, (b) 원래 아무 무기도 없었을 때만** 일어남 — 빈 슬롯에 그냥 채워진 경우엔 자동 장착 안 하고 숨겨서 슬롯에만 넣어둠(1/2번 키나 무기 페이지 클릭으로 직접 꺼내 써야 함) |
| `SwapToWeaponSlot(SlotIndex)` | 1·2번 키(`WeaponSlot1Action`/`WeaponSlot2Action`) 또는 무기 페이지 클릭에서 호출. `Dead`/`Attack`/`Reloading`/`Dodge`/`WeaponSwap` 상태면 무시. 무기에 `SwapMontage`가 있으면 `PlayAnimMontage`의 **반환값(재생 성공 여부)을 확인한 뒤에만** `WeaponSwap` 상태로 잠금 — 몽타주가 스켈레톤과 안 맞아 재생 자체가 실패해도 상태에 영구히 갇히지 않도록 하기 위함 |
| `SwapWeaponSlots(IndexA, IndexB)` | 무기 페이지에서 아이콘을 드래그해서 두 슬롯 위치만 바꿈(장착 상태는 안 바뀜). 배열을 스왑한 뒤 활성 슬롯 인덱스도 실제 들고 있는 무기(`EquippedWeapon`)를 따라 재계산함 |
| `CommitWeaponSwap(SlotIndex)` | 실제로 `WeaponSocket`에 부착/이전 무기 숨김을 처리하는 내부 함수. 원거리 무기면 탄약 델리게이트 재바인딩도 여기서 처리 (아래 참고) |
| `DropWeapon(SlotIndex)` | 슬롯을 비우고 무기를 캐릭터 앞 100유닛 지점에 `OnDropped()`로 필드 픽업 상태로 되돌림 (파괴 안 함 — 다시 주울 수 있음) |
| `GetWeaponInSlot(SlotIndex)` / `GetCurrentWeaponSlotIndex()` | UI용 getter |
| `OnWeaponSlotsChanged` (델리게이트, 파라미터 없음) | 슬롯 내용이 바뀔 때마다(습득/스왑/드랍) 브로드캐스트. 무기 페이지 UI 갱신용 |
| `OnActiveWeaponAmmoChanged` (델리게이트, `int32 CurrentAmmo, int32 MaxAmmo`) | **탄약 UI가 구독해야 할 진짜 소스.** `CommitWeaponSwap`이 무기가 바뀔 때마다 이전 무기의 `OnAmmoChanged` 구독을 해제하고 새 무기 걸로 재구독한 뒤 즉시 현재 탄약을 한 번 흘려줌 — UI는 이 델리게이트 하나만 구독하면 어떤 무기로 스왑되든 항상 정확한 값을 받고, 무기 인스턴스별로 직접 재바인딩할 필요가 없음 (`WBP_AmmoUI`가 이걸 구독 — 7절 참고) |
| `GetEquippedWeapon()` | 지금 손에 든(=`WeaponSlots[CurrentWeaponSlotIndex]`) 무기 getter |

**아이템 사용**
- `ApplyConsumableEffect(HealAmount, ManaAmount)` — `UGJInventoryComponent::UseItem()`이 호출. `CurrentHP`/`CurrentMP`에 클램프 적용 후 `UpdatePlayerHUD()`

**인벤토리 UI**
- `ToggleInventory()` — Tab에 바인딩(Enhanced Input `InventoryToggleAction`). 처음 열 때 위젯 1회 생성 후 재사용
  - 열기: `AddToViewport()` + `SetGamePaused(true)` + `FInputModeUIOnly`(포커스를 인벤토리 위젯에 고정). 예전엔 `FInputModeGameAndUI`를 썼는데, 그 모드는 인벤토리 패널 바깥 클릭이 게임 뷰포트로 그대로 흘러가면서 키보드 포커스까지 뷰포트로 뺏겨 Tab이 안 먹히는 문제가 있었음(인벤토리가 열려있는 동안은 게임이 멈춰있어서 뒤쪽 클릭이 의미도 없으므로 `UIOnly`로 완전히 묶음)
  - 닫기: `RemoveFromParent()` + `SetGamePaused(false)` + `FInputModeGameOnly`(`bConsumeCaptureMouseDown=false` — 기본값(true)이면 닫은 직후 마우스 캡처를 다시 잡는 그 클릭 자체가 게임 입력으로 안 넘어가서 간헐적으로 씹혔음)
- `GetInventoryComponent()` — public getter

**HUD** (둘 다 캐릭터가 직접 `CreateWidget`+`AddToViewport`, GameMode 관여 없음 — 레벨 무관하게 동작)
- `AmmoWidgetClass` / `AmmoWidgetInstance` — 좌하단 탄약 "n/m" 표시 (`WBP_AmmoUI`, `AGJCharacter::OnActiveWeaponAmmoChanged` 구독 — 7절 참고)
- `PlayerHUDWidgetClass` / `PlayerHUDWidgetInstance` — 좌상단 HP/MP 바 (`WBP_PlayerHUD`)
- `OnHPChanged()` — `OnDamaged`에 바인딩, HUD 갱신
- `UpdatePlayerHUD()` — `PlayerHUDWidgetInstance->UpdateHP/UpdateMP` 호출 (위젯 없으면 조용히 무시)

### 2.3 `AGJEnemyCharacter` (GJEnemyCharacter.h/.cpp) — 적(잡몹)

`BP_GJEnemyCharacter`로 블루프린트화됨. AI 컨트롤러가 자동 빙의(`AutoPossessAI`).

**스탯**
- `EnemyDataHandle` (`FDataTableRowHandle`) — `DT_EnemyStat` 참조. 비어있으면 헤더에 박힌 기본값 사용
- `ApplyEnemyStat()` — `BeginPlay`에서 호출, 테이블 값으로 `MaxHP/AttackDamage/AttackRange/DetectionRange/AttackCooldown/AttackWindup/MoveSpeed` 덮어씀

**공격 (실제 판단은 BT가 호출)**
- `GetDetectionRange()` / `GetAttackRange()` — BT Service가 읽어서 블랙보드 갱신
- `PerformAttack()` — BT Task가 호출. 쿨다운 체크 → 플레이어 조회 → 회전 → 몽타주 재생 → **즉시 데미지를 넣지 않고** `AttackWindup`초 후 `ApplyAttackDamage()`를 타이머로 예약
- `ApplyAttackDamage()` — 선딜레이 종료 시점에 실제 판정. 그 사이 플레이어가 사거리(+50 여유) 밖으로 나갔으면 헛스윙 처리

**사망**
- `HandleDeath()` (override) — `Super::HandleDeath()` 후: 대기 중이던 공격 타이머 취소 → AI `BrainComponent->StopLogic()` + `StopMovement()`로 AI 완전 정지 → `DeathMontage` 재생(전용 데스 애님이 없어서 생성자에서 `MM_Rifle_Fire_Montage`를 `ConstructorHelpers::FObjectFinder`로 기본값으로 채워둠 — 나중에 진짜 데스 애님 생기면 BP 디테일 패널에서 교체) → 체력바 위젯 숨김 → `DestroyDelay`(기본 2초) 후 `DestroySelf()`
- `DestroySelf()` — `Destroy()`

**체력바 (머리 위, `UWidgetComponent`, `EWidgetSpace::Screen`이라 탑다운 카메라 각도 무관하게 항상 화면 정면)**
- `HealthBarWidgetComponent` — BP 컴포넌트 디테일에서 Widget Class를 `WBP_EnemyHealthBar`로 지정해야 표시됨
- `OnHealthChanged()` / `UpdateHealthBarWidget()` — `OnDamaged` 바인딩, 맞을 때마다 갱신

### 2.4 `UCharacterStateComponent` (CharacterStateComponent.h/.cpp)

단순 상태 머신. `ECharacterState { Idle, Rolling, Attacking, Hit, Dead, Reloading, Dashing, Dodge, Attack, WeaponSwap }`

- `SetState(NewState)` / `GetState()` — 값이 같으면 무시, 다르면 `OnStateChanged` 브로드캐스트
- `WeaponSwap` — 무기 교체 몽타주(`AGJWeaponBase::SwapMontage`)가 재생되는 동안 다른 입력(공격/재장전/닷지/스왑)을 막기 위한 상태. `AGJCharacter::SwapToWeaponSlot()`이 몽타주 재생에 성공했을 때만 진입시키고, `OnMontageEndedEvent`가 종료 시 `Idle`로 되돌림
- ⚠️ **주의**: `Rolling`/`Dodge`, `Attacking`/`Attack`처럼 거의 중복인 값이 있음(점진적으로 추가되며 생긴 잔재). 실제 코드가 어떤 걸 쓰는지 grep해서 확인할 것 (플레이어는 `Dodge`/`Attack`/`Reloading`/`WeaponSwap`/`Dead`를 씀)
- `Hit` 상태로 자동 복귀하는 타이머는 없음. `Dodge`/`WeaponSwap`→`Idle`(몽타주 종료 시)과 `Dead`(터미널)만 명시적으로 처리됨

---

## 3. 인벤토리

`GJInventoryComponent.h/.cpp` + `GJGameTypes.h`의 `FItemData`

`UGJInventoryComponent`는 `AGJCharacter`에 붙는 `UActorComponent`. UI(`UGJInventoryWidget`/`UGJInventorySlotWidget`, 3.2절)에서 이 컴포넌트의 함수/델리게이트에 연결되어 있음 (`AGJCharacter::GetInventoryComponent()`로 접근).

| 멤버/함수 | 설명 |
|---|---|
| `MaxSlots` | 인벤토리 칸 최대 개수 (기본 20). 빈 칸도 항상 배열 안에 존재함(그리드 UI가 배열 인덱스를 그대로 그리드 위치로 씀) |
| `Items` (`TArray<FInventorySlot>`) | 현재 보유 슬롯 목록. `FInventorySlot { FDataTableRowHandle ItemHandle; int32 Quantity; }` — 아이템의 테이블+행 이름을 그대로 들고 있어서, 컴포넌트 자체는 어떤 데이터 테이블을 쓰는지 몰라도 됨 |
| `AddItem(const FDataTableRowHandle& ItemHandle, int32 Quantity)` | 기존 스택의 여유 공간부터 채우고, 남으면 `MaxSlots` 한도 내에서 빈 칸에 채움. 뭔가 실제로 추가됐으면 아이템 ID순으로 자동 정렬. 요청 수량을 다 못 넣으면(칸 부족) 들어간 만큼만 반영하고 false 반환 |
| `RemoveItem(FName ItemID, int32 Quantity)` | 보유 수량이 부족하면 아무것도 빼지 않고 false 반환(원자적 처리). 뒤쪽 슬롯부터 소모, 0이 되면 슬롯을 빈 칸으로 되돌림 |
| `GetItemCount(ItemID)` / `HasItem(ItemID, Quantity)` | 여러 슬롯에 나뉘어 있어도 합산해서 조회 |
| `SwapSlots(IndexA, IndexB)` | 인벤토리 UI에서 아이콘을 드래그해서 다른 칸에 놓을 때 사용. 정렬을 다시 하지 않음 |
| `UseItem(SlotIndex)` | 인벤토리 UI에서 더블클릭 시 호출. `EItemType::Consumable`인 아이템만 사용 가능(장비/재료/퀘스트는 아무 일도 안 일어남). `AGJCharacter::ApplyConsumableEffect()`로 회복 효과를 위임한 뒤 수량을 1 줄임(0 되면 빈 칸으로) |
| `OnInventoryChanged` (델리게이트) | 추가/제거/스왑/사용으로 내용이 바뀔 때마다 브로드캐스트. UI가 폴링 없이 바인딩 |

`FItemData`(GJGameTypes.h) 스키마는 8절 표 참고.

### 3.1 상호작용 시스템 + 인벤토리 아이템 액터

`GJInteractable.h/.cpp` + `GJItemBase.h/.cpp` (abstract) + `GJItem.h/.cpp`

아이템은 **겹치기만 해서는 안 주워짐** — 상호작용 범위(콜리전) 안에서 상호작용 입력(IA_Interact, 플레이어가 직접 만든 에셋)을 눌러야 습득됨. 무기(4.1절)도 같은 인터페이스로 필드에서 주울 수 있음 — 문/버튼 등 다른 상호작용 대상도 이 구조 그대로 추가 가능.

| 클래스 | 역할 |
|---|---|
| `IGJInteractable` (UInterface) | `Interact(AGJCharacter* Interactor)` 하나만 선언. "지금 정말 범위 안인지"는 각 구현부가 스스로 판단 |
| `AGJItemBase` (abstract, IGJInteractable 구현) | `ItemMesh`(비주얼, 콜리전 없음) + **물리 충돌용** `CollisionComp`(Sphere 반경 50, Trigger — 지금은 상호작용 판정에 안 쓰고, 나중에 아이템이 땅에 굴러다니는 등 실제 충돌/물리가 필요해지면 그대로 활용) + **상호작용 범위용** `InteractionCollision`(Sphere 반경 150, Trigger — 물리 충돌과 분리되어 있어서 이 범위만 넓혀도 아이템의 실제 충돌 크기엔 영향 없음). `ItemDataHandle`로 `FItemData` 행을 읽어 `ItemStat`에 캐시(OnConstruction). `Interact_Implementation()`이 `InteractionCollision->IsOverlappingActor(Interactor)`로 실제 범위 안인지 확인한 뒤에만 virtual `PickUp(AGJCharacter*)` 호출 (베이스는 빈 구현) |
| `AGJItem` | `PickUp()` 오버라이드 — Picker의 `InventoryComponent->AddItem(ItemDataHandle, Quantity)` 호출, 전부 들어갔을 때만 `Destroy()` (칸이 모자라면 필드에 남음) |

플레이어 쪽은 `AGJCharacter::InteractInputPressed()`가 `GetCapsuleComponent()->GetOverlappingActors()`로 지금 겹쳐있는 액터 중 `IGJInteractable`을 구현한 첫 번째 대상을 찾아 `Interact()`를 호출함. `InteractAction` UPROPERTY에 IA_Interact 에셋을 할당해야 동작함.

`Quantity`(기본 1)는 이 액터 하나를 주웠을 때 실제로 지급되는 개수. BP로 뺄 때는 `AGJItem`을 부모로 `BP_Item_XXX`를 만들어 메시/ItemDataHandle/Quantity만 채우면 됨.

### 3.2 인벤토리 UI (`UGJInventoryWidget` / `UGJInventorySlotWidget` / `UGJWeaponSlotWidget`)

인벤토리 창은 **1페이지(아이템 그리드)** + **2페이지(무기 슬롯 0/1번)**를 `WidgetSwitcher`로 감싸고 탭 버튼(`Tab1Button`/`Tab2Button`)으로 전환하는 구조. 페이지 레이아웃 자체는 `WBP_Inventory`에서 구성(UMG MCP 툴로 작업함).

| 클래스 | 역할 |
|---|---|
| `UGJInventorySlotWidget` | 그리드 한 칸. `SetSlotData(index, inventoryComponent)`로 세팅. `BindWidget: IconImage`, `BindWidgetOptional: QuantityText`(스택 1개면 자동 숨김). 클릭+드래그로 `OwningInventory->SwapSlots()`, 더블클릭으로 `OwningInventory->UseItem()` |
| `UGJInventoryWidget` | 인벤토리 창 전체. `InitializeInventory(inventoryComponent)`를 열 때 1회 호출 — `RefreshGrid()`(아이템 그리드, `OnInventoryChanged` 구독)와 `RefreshWeaponSlots()`(무기 페이지 2칸, 캐릭터의 `OnWeaponSlotsChanged` 구독)를 둘 다 갱신함. `BindWidget: GridPanel`, `BindWidgetOptional: WeaponSlotWidget1/WeaponSlotWidget2` |
| `UGJWeaponSlotWidget` | 무기 페이지 한 칸(0번/1번). `SetSlotData(slotIndex, character)`로 세팅. `bIsActiveSlot`(BlueprintReadOnly)로 지금 손에 든 무기인지 표시 가능. 순수 클릭(드래그 없음)이면 `SwapToWeaponSlot()`으로 장착 전환, 드래그해서 다른 칸에 놓으면 `SwapWeaponSlots()`으로 자리만 교체(장착 상태 유지) — 클릭/드래그 구분은 `NativeOnMouseButtonDown`에서 `DetectDrag`만 걸고, 실제 교체 액션은 드래그가 감지 안 됐을 때만 `NativeOnMouseButtonUp`에서 수행하는 방식으로 함 |

**Tab으로 닫기 — 포커스에 의존하지 않는 전역 리스너**: `UGJInventoryWidget`은 `NativeOnInitialized()`에서 `FSlateApplication::Get().OnApplicationPreInputKeyDownListener()`에 핸들러를 등록해서 Tab 키를 감지함(포커스가 어디에 있든 항상 통지받음). 처음엔 `NativeOnPreviewKeyDown`(포커스 기반)으로 했었는데, 탭 버튼처럼 포커스를 받을 수 있는 자식 위젯이 있으면 인벤토리 바깥 클릭으로 포커스가 위젯 트리 밖으로 나갔을 때 Tab이 아예 안 들어오는 문제가 있어서 바꿈. 리스너 해제는 `NativeDestruct()`가 아니라 **`BeginDestroy()`**(UObject 레벨)에서 함 — `NativeDestruct`는 위젯이 진짜로 파괴될 때가 아니라 `RemoveFromParent()`(=닫을 때)마다 불려서, 거기서 해제하면 처음 닫을 때 리스너가 없어지고 재등록도 안 돼(`NativeOnInitialized`는 생성 시 1회뿐) 두 번째 닫기부터 Tab이 안 먹히는 버그가 있었음.

---

## 4. 무기 / 투사체

### 4.1 `AGJWeaponBase` (GJWeaponBase.h/.cpp) — 추상, `IGJInteractable` 구현

무기는 `AGJCharacter::EquipWeapon()`으로 스폰되는 시작 무기이자, **필드에 배치해두면 그대로 E로 주울 수 있는 픽업 액터이기도 함**. 습득되면 같은 액터 인스턴스가 그대로 캐릭터 손에 부착되는 것이라 새 오브젝트를 스폰하지 않음(무기는 스택 안 됨, 같은 종류여도 각각 별개 인스턴스).

| 멤버/함수 | 설명 |
|---|---|
| `WeaponDataHandle` | `OnConstruction()`에서 `FWeaponStat` 행을 읽어와 `WeaponStat`/`AttackMontage`/`ReloadMontage`/`SwapMontage`/무기 메시를 채움 (에디터에서 즉시 반영됨) |
| `InteractionCollision` (Sphere, 반경 150, Trigger) | 필드에 놓여 있을 때 상호작용(습득) 판정용 — `AGJItemBase`의 것과 동일한 방식. 장착되면 `OnPickedUp()`에서 꺼버림 |
| `Interact_Implementation(Interactor)` | `IGJInteractable` 구현 — `Interactor->PickUpWeapon(this)` 호출 |
| `OnPickedUp(NewOwner)` | 습득(장착)됐을 때 호출. `InteractionCollision`을 `NoCollision`으로 끄고, `Owner`/`Instigator`를 주운 캐릭터로 세팅(레벨에 미리 배치해둔 무기는 스폰 시 Instigator가 없어서, 이걸 안 해주면 `Fire()`에서 `GetInstigator()`가 nullptr이라 크래시남) |
| `OnDropped(DropLocation)` | 무기 슬롯이 꽉 차서 밀려날 때 필드로 되돌릴 때 호출. 지정 위치로 옮기고 `InteractionCollision`을 다시 `Trigger`로 켜서 다시 주울 수 있게 함 |
| `GetWeaponStat()` / `GetAttackMontage()` / `GetReloadMontage()` / `GetSwapMontage()` / `GetWeaponMesh()` | getter |
| `Fire()` (virtual) | 베이스는 빈 구현. 서브클래스가 오버라이드 |

무기 슬롯/스왑 로직 자체(2개 슬롯, 픽업 규칙, 스왑 몽타주 재생 등)는 `AGJCharacter`가 소유함 — 2.2절 "무기 장착 / 스왑" 참고.

### 4.2 `AGJWeapon_Ranged` (GJWeapon_Ranged.h/.cpp)

`BP_GJWeapon_Ranged`. **오브젝트 풀링**으로 투사체 관리 (`PoolSize` 기본 30 — `WeaponStat.MagazineSize`와는 별개 개념, 우연히 기본값이 같을 뿐).

| 함수 | 설명 |
|---|---|
| `CreateProjectilePool()` | `BeginPlay`에서 `PoolSize`만큼 `AGJProjectile` 미리 스폰 |
| `GetAvailableProjectile()` | 풀에서 `!IsActive()`인 총알 하나 찾아 반환 (없으면 nullptr) |
| `Fire()` | 재장전 중이면 무시 → `FireInterval` 쿨다운 체크 → `CurrentAmmo <= 0`이면 무시 → 풀에서 꺼내 `FireInDirection()` 호출 → `CurrentAmmo--` → `OnAmmoChanged` 브로드캐스트 |
| `CanReload()` | 재장전 중이 아니고 탄창이 꽉 안 찼을 때 true |
| `StartReload(int32 InBulletsToRefill)` | `bIsReloading = true`, 채울 발수를 `PendingRefillAmount`에 저장 (부분 재장전 지원용) |
| `FinishReload()` | `CurrentAmmo += PendingRefillAmount` (클램프), `OnAmmoChanged` 브로드캐스트 |
| `OnAmmoChanged` (델리게이트) | `(int32 CurrentAmmo, int32 MaxAmmo)`. **직접 UI가 바인딩하지 않음** — `AGJCharacter::CommitWeaponSwap`이 구독해서 `OnActiveWeaponAmmoChanged`로 중계함 (2.2/7절 참고) |

### 4.3 `AGJProjectile` (GJProjectile.h/.cpp)

- `CollisionComp`(Sphere, `BlockAllDynamic` 프로필) + `MeshComp`(Static, 콜리전 없음) + `ProjectileMovement`
- `FireInDirection(dir, damage, speed, range)` — 활성화(숨김 해제, 콜리전/틱 켬), 속도 세팅, `range/speed` 시간 후 자동 `Deactivate()` 타이머
- `Deactivate()` — 숨기고 콜리전/틱/속도 끔 (풀로 반환, `Destroy()` 안 씀)
- `OnHit()` — 자신/발사자가 아닌 `AGJBaseCharacter`에 맞으면 `UGameplayStatics::ApplyDamage` 호출 → `Deactivate()`

---

## 5. 데미지 파이프라인

엔진 표준 `AActor::TakeDamage`를 그대로 씀 (GAS 아님).

```
AGJProjectile::OnHit()
  → UGameplayStatics::ApplyDamage(HitCharacter, ...)
  → AGJBaseCharacter::TakeDamage() [override]
      → CurrentHP -= ActualDamage
      → OnDamaged.Broadcast()   ← UI(체력바/HUD)가 여길 구독
      → CurrentHP <= 0 이면 HandleDeath()
```

- **원거리 투사체만** 데미지를 줌. 근접 콤보(`Attack1`/`Attack2`... 몽타주 섹션)는 아직 히트 판정(트레이스/콜리전)이 전혀 없음
- `AnimNotify_GameplayEvent`에 `EGameplayNotifyType::MeleeHit` 케이스가 있지만 스텁 상태(`// Character->PerformMeleeHit();`)이고, 이 노티파이는 `AGJCharacter`로만 캐스트하므로 지금 붙여도 적한텐 안 먹음
- `AnimNotify_GameplayEvent`에서 실제로 동작하는 건 `Fire` 케이스뿐 (`Character->PerformFire()` 호출 — 다만 현재 발사 로직은 노티파이가 아니라 `Fire()` 직접 호출/Tick 폴링으로 가고 있어서 이 노티파이 자체는 사실상 레거시에 가까움)

---

## 6. 적 AI (비헤이비어 트리)

C++ 쪽은 BT/BB **에셋 자체는 만들 수 없음** (MCP 툴셋이 읽기 전용) — `BB_GJEnemy`/`BT_GJEnemy`/`BP_GJEnemyAIController`는 에디터에서 직접 구성됨. C++은 그 트리가 호출하는 서비스/태스크 노드만 제공.

```
AGJEnemyAIController::OnPossess()
  → RunBehaviorTree(BehaviorTreeAsset)

UBTService_UpdateTarget (0.2초 간격)
  → 플레이어와의 거리 계산
  → DetectionRange 안이면 블랙보드 TargetActor = 플레이어, 아니면 비움
  → AttackRange 안이면 블랙보드 IsInAttackRange = true

UBTTask_MeleeAttack
  → AGJEnemyCharacter::PerformAttack() 호출 (쿨다운/판정은 그 함수 내부에서)
```

BT 트리 구조 자체(Selector로 IsInAttackRange 분기해서 MoveTo vs MeleeAttack 고르는 부분)는 에디터에서 짜여진 것이라 여기 문서에는 없음 — 리뷰 시 `BT_GJEnemy` 에셋을 직접 열어봐야 함.

---

## 7. UI / 위젯

체력바/HP·MP 바는 **C++ 베이스 클래스 + `BindWidget`** 패턴을 씀 (블루프린트 이벤트 그래프에 로직을 두지 않음 — UMG 이벤트 그래프 자동화 작업 중 문제가 있었던 이력이 있어서, 값 갱신 로직은 전부 C++ `UFUNCTION(BlueprintCallable)`로 두고 디자이너에서는 이름만 맞는 위젯을 배치하면 되게 함).

| 클래스 | 파생 WBP | BindWidget 이름 | 갱신 함수 | 위치/방식 |
|---|---|---|---|---|
| `UGJHealthBarWidget` | `WBP_EnemyHealthBar` | `HealthProgressBar` | `UpdateHealth(Current, Max)` | 적 머리 위, `UWidgetComponent` (Screen space) |
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` | `UpdateHP(Current,Max)` / `UpdateMP(Current,Max)` | 좌상단, `AddToViewport()` |

인벤토리/무기 페이지 UI(`UGJInventoryWidget`, `UGJInventorySlotWidget`, `UGJWeaponSlotWidget`)는 3.2절 참고.

`WBP_AmmoUI`는 위 두 개와 달리 **자체 이벤트 그래프**로 동작함(값 갱신 함수는 C++이 아니라 BP `UpdateAmmoText(CurrentAmmo, MaxAmmo)` 함수). Construct 시점에 `GetOwningPlayerPawn()`→`AGJCharacter`로 캐스트한 뒤 **`AGJCharacter::OnActiveWeaponAmmoChanged`를 구독**하고 초기값을 한 번 그려줌. 예전에는 그 시점의 `EquippedWeapon`을 직접 캐스팅해서 `AGJWeapon_Ranged::OnAmmoChanged`에 바인딩하는 방식이었는데, 그러면 무기를 스왑해도 재바인딩이 안 돼서 탄약 표시가 예전 무기 것에 고정되는 버그가 있었음 — 캐릭터 델리게이트로 옮기면서 해결(2.2절 "무기 장착/스왑" 참고).

> UMG 관련 작업 시 주의:
> - 새 C++ 위젯 클래스를 만들면 **먼저 컴파일(라이브 코딩)** 해서 리플렉션에 등록시킨 다음에 `CreateWidgetBlueprint`로 그 클래스를 부모로 하는 WBP를 만들어야 함
> - `AddWidget` 등에 위젯 블루프린트 클래스를 넘길 때는 에셋 경로가 아니라 **`_C` 서픽스가 붙은 생성 클래스 경로**를 써야 함 (예: `/Game/GJ/UI/WBP_WeaponSlot.WBP_WeaponSlot_C`)
> - `AssignXXX`(델리게이트 바인드) 계열 노드를 `create_node`/`get_node_type_pins`로 만들면 **매번 새 CustomEvent 스텁을 자동으로 같이 생성**함 — 기존 커스텀 이벤트를 재사용하려면 자동 생성된 CustomEvent를 지우고 델리게이트 핀을 기존 이벤트의 `OutputDelegate`에 수동으로 연결해야 함

---

## 8. 데이터 테이블 스키마 (`GJGameTypes.h`)

### `FCharacterStat` — `DT_CharacterStat` (행 이름 = 레벨 숫자)
| 필드 | 기본값 | 설명 |
|---|---|---|
| `MaxHP` | 100 | |
| `MaxMP` | 50 | 재장전 시 소모됨 |
| `BaseAttackPower` | 10 | |
| `RequiredEXP` | 100 | |

### `FWeaponStat` — `DT_WeaponStat`
| 필드 | 기본값 | 설명 |
|---|---|---|
| `BaseDamage` | 15 | |
| `AttackSpeedRate` | 1.0 | (현재 코드에서 직접 참조 안 됨 — `FireInterval`이 실질적 연사 속도 담당) |
| `ProjectileSpeed` | 3000 | |
| `Range` | 2000 | 이 거리 도달 시 투사체 자동 비활성화 |
| `FireInterval` | 0.1 | 이 시간(초)마다 최대 1발 |
| `MagazineSize` | 30 | 탄창 용량. **`AGJWeapon_Ranged::PoolSize`(오브젝트 풀 크기, 별개 개념)와 헷갈리지 말 것** |
| `ReloadTime` | 1.5 | `ReloadMontageAsset` 없을 때 대체 타이머 |
| `MPCostPerAmmo` | 1 | 총알 1발당 MP 소모. 재장전 시 `실제로 채워지는 발수 x 이 값`만큼 소모 (부분 재장전 가능) |
| `WeaponMeshAsset`, `AttackMontageAsset`, `ReloadMontageAsset` | — | 에셋 참조 |
| `WeaponIcon` | — | UTexture2D. 무기 페이지 UI(`UGJWeaponSlotWidget`)에 표시 |
| `SwapMontageAsset` | — | 이 무기로 교체(스왑)할 때 재생할 몽타주. 비어있으면 몽타주 없이 즉시 교체됨. 무기마다 다르게/원거리·근접 공유/전부 동일하게 쓰고 싶으면 여러 행에 같은 에셋을 지정하면 됨(코드에서 무기 종류를 구분하지 않고 데이터로만 제어) |

### `FEnemyStat` — `DT_EnemyStat`
| 필드 | 기본값 | 설명 |
|---|---|---|
| `MaxHP` | 50 | |
| `AttackDamage` | 10 | |
| `AttackRange` | 150 | |
| `DetectionRange` | 800 | |
| `AttackCooldown` | 1.5 | |
| `MoveSpeed` | 300 | `CharacterMovement.MaxWalkSpeed`에 적용 |
| `AttackWindup` | 0.3 | 공격 결정~실제 데미지 판정까지 선딜레이 |

### `FItemData` — 예: `DT_ItemData` (행 이름 = 아이템 ID)
| 필드 | 기본값 | 설명 |
|---|---|---|
| `DisplayName` | — | FText |
| `ItemType` | Misc | `EItemType`: Consumable/Equipment/Material/Quest/Misc. **`Consumable`만 인벤토리에서 더블클릭으로 사용 가능**(`UGJInventoryComponent::UseItem`) |
| `MaxStackSize` | 99 | 한 슬롯에 최대로 겹쳐 쌓일 수 있는 개수 (1이면 스택 불가) |
| `SellPrice` | 0 | 상점에 팔 때 받는 금액 |
| `BuyPrice` | 0 | 상점에서 살 때 지불하는 금액 |
| `HealAmount` | 0 | 사용 시 회복되는 HP량 (`ApplyConsumableEffect`가 적용) |
| `ManaRecoverAmount` | 0 | 사용 시 회복되는 MP량 (`ApplyConsumableEffect`가 적용) |
| `bPersistAcrossRuns` | false | true면 새 회차(런)를 시작해도 인벤토리에서 사라지지 않음(예: 영구 재화/장비). false면 회차가 바뀌면 사라짐 |
| `Icon` | — | UTexture2D. 인벤토리 그리드 UI에 씀 |
| `ItemMeshAsset` | — | UStaticMesh. `AGJItemBase`가 OnConstruction에서 자동으로 반영 |

---

## 9. 알려진 갭 / TODO (리뷰 시 참고)

- GAS(GameplayAbilitySystem) 모듈은 링크만 되어 있고 실제로 안 씀 (`GetAbilitySystemComponent()`가 항상 null)
- 근접 콤보(`Attack1`/`Attack2`...)는 몽타주 섹션 점프 로직만 있고 **히트 판정이 없음** — 현재 원거리만 실전 사용 중
- **근접 무기 클래스 자체가 아직 없음** — `AGJWeaponBase`를 직접 쓰는 근접 무기 서브클래스가 필요해지면 그때 추가. 무기 슬롯/스왑/픽업 시스템 자체는 무기 타입과 무관하게 동작하도록 설계돼 있음(`AGJWeapon_Ranged`가 아니어도 `AGJWeaponBase` 서브클래스면 다 됨)
- `MyGJWeaponBase`는 빈 미사용 스텁
- 적 `DeathMontage`는 전용 애님이 없어서 사격 몽타주(`MM_Rifle_Fire_Montage`)를 임시로 재사용 중
- `ECharacterState`에 `Rolling`/`Dodge`, `Attacking`/`Attack` 중복 값 존재 (정리 필요할 수 있음)
- `OnDeath()` BlueprintImplementableEvent는 비어있음 (사망 연출 미구현)
- `DropWeapon()`의 드랍 위치는 캐릭터 정면 100유닛 고정 — 바닥 스냅이나 다른 무기/장애물과 겹침 방지 처리는 없음
- 무기 스왑 몽타주(`SwapMontageAsset`)는 데이터 테이블에서 아직 안 채웠을 수 있음 — 비어있으면 그냥 즉시 교체(정상 동작). 테스트용으로 다른 용도 몽타주(닷지 등)를 임시로 재사용해도 `OnMontageEndedEvent`가 상태 기준으로 분기해서 안전하지만, 실제 전용 스왑 애님이 생기면 교체 필요
- `FItemData.bPersistAcrossRuns`는 데이터 필드만 있고, 실제로 회차(런) 전환 시 인벤토리를 정리/유지하는 세이브·로드나 런 전환 시스템 자체가 아직 없어서 이 값을 읽어서 처리하는 코드는 없음

---

## 10. 빌드/워크플로 메모

- 에디터는 보통 라이브 코딩 켜진 채로 열려있음 → 코드 수정 후 에디터에서 **Ctrl+Alt+F11**
- **완전히 새로운 UCLASS 파일**(새 `.h`/`.cpp` 쌍)은 라이브 코딩만으로는 못 받는 경우가 있음 — 그럴 땐 에디터를 닫고 `Build.bat`으로 전체 빌드하거나, 라이브 코딩 컴파일 후 위젯 블루프린트 부모 클래스 목록에 새 클래스가 안 뜨면 에디터 재시작
- USTRUCT 레이아웃 변경(필드 추가/이름 변경)을 라이브 코딩으로 여러 번 하면, 그 구조체를 참조하는 UMG 블루프린트 그래프(`Break WeaponStat` 등)의 핀 타입이 깨질 수 있음 → 증상: "정확히 일치하는 구조체만 호환" 컴파일 에러 → **에디터 완전 재시작**(재빌드 불필요, 껐다 켜기만)으로 대부분 해결됨
- PCH 생성 중 `C1076`/`C3859` 에러는 그 순간 시스템 메모리 부족 때문(코드 문제 아님) — 메모리 여유 있는 상태에서 재시도
- UMG 위젯 트리/그래프를 MCP로 직접 조작할 때 자주 걸리는 함정은 7절 마지막 노트 참고
