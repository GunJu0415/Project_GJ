# Project_GJ 개발 가이드 (코드 리뷰용)

> 대상: 처음 이 코드베이스를 리뷰하는 사람. "어디에 뭐가 있고, 뭘 하는 함수인지" 빠르게 파악하는 용도.
> 기준: UE 5.8, C++ 우선 + 얇은 블루프린트 레이어. 탑다운 카메라 런앤건/핵앤슬래시. 솔로 개발.
> 마지막 갱신 시점: 2026-08-07 세션 기준 (적 AI, MP 시스템, HP/MP HUD까지 반영됨)

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

⚠️ **인코딩 주의**: `Source/Project_GJ/*.cpp/.h`의 기존 한글 주석 중 일부는 **CP949(EUC-KR)**로 저장되어 있음 (초기 파일들). UTF-8 기준 툴로 열면 깨져 보일 수 있는데 파일 자체는 CP949로는 정상. 이 문서는 UTF-8로 작성됨.

---

## 1. 클래스 계층 한눈에 보기

```
AGJBaseCharacter (abstract, ACharacter + IAbilitySystemInterface)
├─ AGJCharacter        — 플레이어
└─ AGJEnemyCharacter    — 적(잡몹)

AGJWeaponBase (abstract, AActor)
└─ AGJWeapon_Ranged     — 원거리 무기 (현재 유일하게 구현된 무기 타입)

AGJItemBase (abstract, AActor, IGJInteractable 구현)
└─ AGJItem              — 데이터 테이블 기반 범용 습득 아이템 (BP_Item_XXX로 파생시켜 사용)

IGJInteractable (UInterface) — 상호작용 가능한 모든 것의 공통 인터페이스 (아이템, 나중에 문/버튼 등)

AGJProjectile (AActor)  — 오브젝트 풀링되는 총알

UCharacterStateComponent (UActorComponent) — Idle/Attack/Dead 등 상태 머신
UGJInventoryComponent (UActorComponent) — 플레이어 인벤토리 (AGJCharacter에 부착, UI 없음)

AI (플레이어 아님, 적 전용)
├─ AGJEnemyAIController (AAIController)
├─ UBTService_UpdateTarget (UBTService)
└─ UBTTask_MeleeAttack (UBTTaskNode)

UI (UUserWidget)
├─ UGJHealthBarWidget   — 적 머리 위 체력바 (월드 위젯 컴포넌트)
└─ UGJPlayerHUDWidget   — 좌상단 플레이어 HP/MP 바 (뷰포트 HUD)

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
- `Move()` — 이동 입력 바인딩
- `UpdateCharacterRotation()` — 마우스 커서를 캐릭터의 지면 평면에 투영해서 그 방향으로 회전
- `UpdateCameraOffset()` / `ApplyCameraOffset()` — 마우스가 화면 중심에서 먼 쪽으로 카메라를 살짝 당겨주는 연출

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

**무기 장착**
- `EquipWeapon()` — `DefaultWeaponClass` 스폰 후 메시의 `WeaponSocket`에 부착
- `GetEquippedWeapon()` — UI 등에서 무기 캐스팅해서 쓰라고 열어둔 getter

**HUD (둘 다 캐릭터가 직접 `CreateWidget`+`AddToViewport`, GameMode 관여 없음 — 레벨 무관하게 동작)**
- `AmmoWidgetClass` / `AmmoWidgetInstance` — 좌하단 탄약 "n/m" 표시 (`WBP_AmmoUI`)
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

단순 상태 머신. `ECharacterState { Idle, Rolling, Attacking, Hit, Dead, Reloading, Dashing, Dodge, Attack }`

- `SetState(NewState)` / `GetState()` — 값이 같으면 무시, 다르면 `OnStateChanged` 브로드캐스트
- ⚠️ **주의**: `Rolling`/`Dodge`, `Attacking`/`Attack`처럼 거의 중복인 값이 있음(점진적으로 추가되며 생긴 잔재). 실제 코드가 어떤 걸 쓰는지 grep해서 확인할 것 (플레이어는 `Dodge`/`Attack`/`Reloading`/`Dead`를 씀)
- `Hit` 상태로 자동 복귀하는 타이머는 없음. `Dodge`→`Idle`(몽타주 종료 시)과 `Dead`(터미널)만 명시적으로 처리됨

---

## 3. 인벤토리

`GJInventoryComponent.h/.cpp` (신규) + `GJGameTypes.h`의 `FItemData`

`UGJInventoryComponent`는 `AGJCharacter`에 붙는 `UActorComponent`. **UI는 아직 없음** — 버튼/위젯은 나중에 이 컴포넌트의 함수/델리게이트에 연결하면 됨 (`AGJCharacter::GetInventoryComponent()`로 접근).

| 멤버/함수 | 설명 |
|---|---|
| `ItemDataTable` | `FItemData` 행을 담은 DataTable (예: DT_ItemData). 행 이름이 곧 아이템 ID(FName)로 쓰임 |
| `MaxSlots` | 인벤토리 칸 최대 개수 (기본 20). 같은 아이템이 스택돼도 한 칸으로 침 |
| `Items` (`TArray<FInventorySlot>`) | 현재 보유 슬롯 목록. `FInventorySlot { FName ItemID; int32 Quantity; }` |
| `AddItem(ItemID, Quantity)` | 기존 스택의 여유 공간부터 채우고, 남으면 `MaxSlots` 한도 내에서 새 슬롯 생성. 요청 수량을 다 못 넣으면(칸 부족) 들어간 만큼만 반영하고 false 반환 |
| `RemoveItem(ItemID, Quantity)` | 보유 수량이 부족하면 아무것도 빼지 않고 false 반환(원자적). 뒤쪽 슬롯부터 소모, 0이 되면 슬롯 제거 |
| `GetItemCount(ItemID)` / `HasItem(ItemID, Quantity)` | 여러 슬롯에 나뉘어 있어도 합산해서 조회 |
| `OnInventoryChanged` (델리게이트) | 추가/제거로 내용이 바뀔 때마다 브로드캐스트. 다른 위젯들과 동일하게 폴링 없이 UI 바인딩용으로 씀 |

`FItemData`(GJGameTypes.h)는 `DisplayName`, `ItemType`, `MaxStackSize`, `SellPrice`, `BuyPrice`, `HealAmount`, `ManaRecoverAmount`, `bPersistAcrossRuns`, `Icon`, `ItemMeshAsset`를 가짐 (아래 8절 스키마 표 참고).

### 3.1 상호작용 시스템 + 인벤토리 아이템 액터

`GJInteractable.h/.cpp` + `GJItemBase.h/.cpp` (abstract) + `GJItem.h/.cpp`

아이템은 **겹치기만 해서는 안 주워짐** — 상호작용 범위(콜리전) 안에서 상호작용 입력(IA_Interact, 플레이어가 직접 만든 에셋)을 눌러야 습득됨. 나중에 문/버튼 등 다른 상호작용 대상도 같은 인터페이스로 추가할 수 있도록 설계됨.

| 클래스 | 역할 |
|---|---|
| `IGJInteractable` (UInterface) | `Interact(AGJCharacter* Interactor)` 하나만 선언. 아이템/문/버튼 등 상호작용 가능한 모든 액터가 구현. "지금 정말 범위 안인지"는 각 구현부가 스스로 판단 |
| `AGJItemBase` (abstract, IGJInteractable 구현) | `ItemMesh`(비주얼, 콜리전 없음) + **물리 충돌용** `CollisionComp`(Sphere 반경 50, Trigger — 지금은 상호작용 판정에 안 쓰고, 나중에 아이템이 땅에 굴러다니는 등 실제 충돌/물리가 필요해지면 그대로 활용) + **상호작용 범위용** `InteractionCollision`(Sphere 반경 150, Trigger — 물리 충돌과 분리되어 있어서 이 범위만 넓혀도 아이템의 실제 충돌 크기엔 영향 없음). `ItemDataHandle`로 `FItemData` 행을 읽어 `ItemStat`에 캐시(OnConstruction). `Interact_Implementation()`이 `InteractionCollision->IsOverlappingActor(Interactor)`로 실제 범위 안인지 확인한 뒤에만 virtual `PickUp(AGJCharacter*)` 호출 (베이스는 빈 구현) |
| `AGJItem` | `PickUp()` 오버라이드 — Picker의 `InventoryComponent->AddItem(GetItemID(), Quantity)` 호출, 전부 들어갔을 때만 `Destroy()` (칸이 모자라면 필드에 남음) |

플레이어 쪽은 `AGJCharacter::InteractInputPressed()`가 `GetCapsuleComponent()->GetOverlappingActors()`로 지금 겹쳐있는 액터 중 `IGJInteractable`을 구현한 첫 번째 대상을 찾아 `Interact()`를 호출함(이후 문/버튼도 이 경로 그대로 재사용 가능). `InteractAction` UPROPERTY에 IA_Interact 에셋을 할당해야 동작함.

`Quantity`(기본 1)는 이 액터 하나를 주웠을 때 실제로 지급되는 개수 — 같은 종류를 여러 개 쌓아 놓지 않고 "포션 5개" 같은 픽업 하나로 표현하고 싶을 때 씀. 인벤토리 자체의 스택(같은 아이템 여러 개 보유)은 `UGJInventoryComponent`가 처리. BP로 뺄 때는 `AGJItem`을 부모로 `BP_Item_XXX`를 만들어 메시/ItemDataHandle/Quantity만 채우면 됨.

---

## 4. 무기 / 투사체

### 3.1 `AGJWeaponBase` (GJWeaponBase.h/.cpp) — 추상

- `WeaponDataHandle` → `OnConstruction()`에서 `FWeaponStat` 행을 읽어와 `WeaponStat`/`AttackMontage`/`ReloadMontage`/무기 메시를 채움 (에디터에서 즉시 반영됨)
- `GetWeaponStat()` / `GetAttackMontage()` / `GetReloadMontage()` / `GetWeaponMesh()` — getter
- `Fire()` (virtual) — 베이스는 빈 구현. 서브클래스가 오버라이드

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
| `OnAmmoChanged` (델리게이트) | `(int32 CurrentAmmo, int32 MaxAmmo)`. UI가 폴링 없이 바인딩 |

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

두 위젯 다 **C++ 베이스 클래스 + `BindWidget`** 패턴을 씀 (블루프린트 이벤트 그래프에 로직을 두지 않음 — UMG 이벤트 그래프 자동화 작업 중 문제가 있었던 이력이 있어서, 값 갱신 로직은 전부 C++ `UFUNCTION(BlueprintCallable)`로 두고 디자이너에서는 이름만 맞는 위젯을 배치하면 되게 함).

| 클래스 | 파생 WBP | BindWidget 이름 | 갱신 함수 | 위치/방식 |
|---|---|---|---|---|
| `UGJHealthBarWidget` | `WBP_EnemyHealthBar` | `HealthProgressBar` | `UpdateHealth(Current, Max)` | 적 머리 위, `UWidgetComponent` (Screen space) |
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` | `UpdateHP(Current,Max)` / `UpdateMP(Current,Max)` | 좌상단, `AddToViewport()` |

`WBP_AmmoUI`는 (아직) 이 패턴이 아니라 자체 이벤트 그래프로 `AGJWeapon_Ranged::OnAmmoChanged`를 직접 바인딩하는 방식.

> UMG 관련 작업 시 주의: 새 C++ 위젯 클래스를 만들면 **먼저 컴파일(라이브 코딩)** 해서 리플렉션에 등록시킨 다음에 `CreateWidgetBlueprint`로 그 클래스를 부모로 하는 WBP를 만들어야 함.

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
| `DisplayName` | — | FText. 나중에 UI 붙일 때 씀 |
| `ItemType` | Misc | `EItemType`: Consumable/Equipment/Material/Quest/Misc |
| `MaxStackSize` | 99 | 한 슬롯에 최대로 겹쳐 쌓일 수 있는 개수 (1이면 스택 불가) |
| `SellPrice` | 0 | 상점에 팔 때 받는 금액 |
| `BuyPrice` | 0 | 상점에서 살 때 지불하는 금액 |
| `HealAmount` | 0 | 사용 시 회복되는 HP량 (0이면 없음) |
| `ManaRecoverAmount` | 0 | 사용 시 회복되는 MP량 (0이면 없음) |
| `bPersistAcrossRuns` | false | true면 새 회차(런) 시작해도 인벤토리에서 안 사라짐(영구 재화/장비 등). false면 회차가 바뀌면 사라짐 |
| `Icon` | — | UTexture2D. 인벤토리 UI 등에 쓸 2D 아이콘 |
| `ItemMeshAsset` | — | UStaticMesh. AGJItemBase가 OnConstruction에서 자동으로 ItemMesh에 반영 |

---

## 9. 알려진 갭 / TODO (리뷰 시 참고)

- GAS(GameplayAbilitySystem) 모듈은 링크만 되어 있고 실제로 안 씀 (`GetAbilitySystemComponent()`가 항상 null)
- 근접 콤보(`Attack1`/`Attack2`...)는 몽타주 섹션 점프 로직만 있고 **히트 판정이 없음** — 현재 원거리만 실전 사용 중
- `MyGJWeaponBase`는 빈 미사용 스텁
- 적 `DeathMontage`는 전용 애님이 없어서 사격 몽타주(`MM_Rifle_Fire_Montage`)를 임시로 재사용 중
- `ECharacterState`에 `Rolling`/`Dodge`, `Attacking`/`Attack` 중복 값 존재 (정리 필요할 수 있음)
- `OnDeath()` BlueprintImplementableEvent는 비어있음 (사망 연출 미구현)
- 인벤토리(`UGJInventoryComponent`)는 데이터/함수만 있고 UI(버튼, 슬롯 위젯)가 없음. `DT_ItemData` 테이블도 아직 안 만들어짐
- `AGJItem`은 "줍기"까지만 구현됨 — `FItemData`의 `HealAmount`/`ManaRecoverAmount`를 실제로 캐릭터에 적용하는 "사용(Use)" 로직은 아직 없음 (인벤토리에서 아이템을 꺼내 쓰는 흐름 자체가 미구현)
- `FItemData.bPersistAcrossRuns`는 데이터 필드만 있고, 실제로 회차(런) 전환 시 인벤토리를 정리/유지하는 세이브·로드나 런 전환 시스템 자체가 아직 없어서 이 값을 읽어서 처리하는 코드는 없음

---

## 10. 빌드/워크플로 메모

- 에디터는 보통 라이브 코딩 켜진 채로 열려있음 → 코드 수정 후 에디터에서 **Ctrl+Alt+F11**
- USTRUCT 레이아웃 변경(필드 추가/이름 변경)을 라이브 코딩으로 여러 번 하면, 그 구조체를 참조하는 UMG 블루프린트 그래프(`Break WeaponStat` 등)의 핀 타입이 깨질 수 있음 → 증상: "정확히 일치하는 구조체만 호환" 컴파일 에러 → **에디터 완전 재시작**(재빌드 불필요, 껐다 켜기만)으로 대부분 해결됨
- PCH 생성 중 `C1076`/`C3859` 에러는 그 순간 시스템 메모리 부족 때문(코드 문제 아님) — 메모리 여유 있는 상태에서 재시도
