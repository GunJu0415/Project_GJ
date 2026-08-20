# Project_GJ 개발 가이드 (코드 리뷰용)

> 대상: 처음 이 코드베이스를 리뷰하는 사람. "어디에 뭐가 있고, 뭘 하는 함수인지" 빠르게 파악하는 용도.
> 기준: UE 5.8, C++ 우선 + 얇은 블루프린트 레이어. 탑다운 카메라 런앤건/핵앤슬래시. 솔로 개발.
> 마지막 갱신 시점: 2026-08-08 세션 기준 (M1 런 루프까지 반영됨 — 닷지 버그 수정, 무기 2슬롯 스왑/필드 픽업, 탄약 UI 델리게이트 재설계, 인벤토리 UI, 로그라이트 회차 루프)

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

⚠️ **인코딩 주의**: 초기 파일들(`CharacterStateComponent.h`, `GJGameInstance.*`, `GJGameMode.cpp` 등)의 한글 주석이 깨져 보인다. 2026-08-08에 실제 바이트를 확인한 결과, **이미 손실 변환이 일어난 상태**다 — 대부분 `EF BF BD`(U+FFFD 대체 문자)로 바뀌어 있어 **원래 한글은 복구 불가**이고, 일부 CP949 바이트 쌍만 살아남아 섞여 있다. 즉 "CP949로 열면 정상"이 아니라 그냥 깨진 것이다.

실무상 영향: 파일은 사실상 UTF-8로 취급되고 컴파일에 문제가 없으므로, **새 주석은 UTF-8 한글로 그냥 쓰면 된다.** 다만 깨진 옛 주석 줄은 의미를 알 수 없으므로 함부로 지우지 말고, 해당 코드를 실제로 손볼 때 새로 다시 쓰는 편이 낫다. 이 문서는 UTF-8로 작성됨.

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
                                (소비 아이템, 필드에 놓인 무기, 허브의 런 시작 포탈, 나중에 문/버튼 등)

AGJRunPortal (AActor, IGJInteractable 구현) — 허브의 런 시작 포탈

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
├─ UGJWeaponSlotWidget    — 무기 페이지 한 칸 (클릭으로 장착 전환, 드래그로 1/2번 자리 교체)
└─ UGJGameOverWidget      — 런 종료 시 뜨는 게임오버 화면

UGJCombatStatics (UBlueprintFunctionLibrary) — 데미지 공식 단일 소스

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

**스탯 / 레벨 / 경험치**

| 함수/멤버 | 설명 |
|---|---|
| `UpdateCharacterStat(NewLevel, bRestoreToFull=true)` | `DT_CharacterStat`에서 레벨(행 이름) 조회 → `MaxHP`/`MaxMP`, 방어력/치명타/이동속도 갱신 → `UpdatePlayerHUD()` 호출. **`bRestoreToFull=true`면 HP/MP를 가득 채우고**(스폰/리스폰), **`false`면 최대치 증가분만 현재값에 더한다**(레벨업). 기본값이 `true`라 기존 호출부는 동작이 안 바뀜 |
| `AddEXP(Amount)` | 경험치 누적 입구. `float`이며 적 처치 외 다른 소스가 생겨도 여기를 쓴다. 내부 `while` 루프라 **한 번의 호출로 여러 레벨**이 오를 수 있고 초과분은 다음 레벨로 이월됨. `RequiredEXP <= 0`인 행이 있어도 무한 루프에 빠지지 않도록 루프 조건에 방어가 들어있음 |
| `LevelUp()` | `UpdateCharacterStat(CurrentLevel+1, false)` 호출 + `UE_LOG` + `OnLevelUp` 브로드캐스트. `AddEXP` 내부에서만 호출됨 |
| `IsMaxLevel()` | `DT_CharacterStat`에 **다음 레벨 행이 없으면 만렙**. 상한 상수가 코드에 없으므로 **테이블에 행을 추가하면 코드 수정 없이 만렙이 늘어남**. `FindRow`의 `bWarnIfRowMissing`에 `false`를 넘김 — 여기서 행이 없는 건 오류가 아니라 정상 결과라 경고를 쌓으면 안 되기 때문 |
| `CurrentEXP` | **누적 총량이 아니라 "이번 레벨의 진행도".** 레벨업 시 `RequiredEXP`만큼 빼고 이월. 만렙에 도달하면 `RequiredEXP` 값으로 고정되어 바가 가득 찬 상태로 표시됨 |
| `OnLevelUp` | `BlueprintAssignable`, `(int32 NewLevel)`. **아직 구독자 없음** — 레벨업 시 카드 3장을 띄우는 선택 시스템이 붙을 자리 |

**경험치는 런마다 초기화된다.** 죽으면 레벨이 리로드되면서 캐릭터가 새로 스폰되고 `BeginPlay`가 `CurrentLevel=1`로 시작하므로, 초기화 코드가 따로 없다. 회차를 넘어 남는 성장은 이 시스템이 아니라 **M6 영구 특성**(별도 재화 + 별도 저장소)이 담당하며, 그래서 `CurrentEXP`/`CurrentLevel`은 **어떤 세이브 경로에도 들어가지 않는다.**

**레벨업은 회복이 아니다.** 최대치가 오른 만큼만 현재 HP/MP에 더해진다(체력 30/100 → 최대 120이 되면 50/120). 풀 회복시키면 "위험할 때 잡몹 하나 잡기"가 최고의 회복 수단이 되어 체력 관리 긴장이 사라진다.

**스탯은 3층 구조다.** 카드/버프가 준 보너스가 레벨업에 지워지지 않게 하기 위한 것이다.

| 층 | 멤버 | 쓰는 주체 | 읽는 쪽 |
|---|---|---|---|
| 테이블 원본 | `BaseStat` | `UpdateCharacterStat`만 | `RecalculateStats` |
| 보너스 누적 | `StatBonus` (`FStatModifier`) | `AddStatBonus`만 | `RecalculateStats` |
| **실효값** | `CurrentCharacterStat` | **`RecalculateStats`만** | `AddEXP`, `UpdatePlayerHUD`, `GetAttackPower` 등 전부 |

실효값 계산은 `실효값 = (테이블값 + Add) x (1 + Percent)`이며, `Percent`는 1.0이 아니라 **0에서 시작하는 증가율**이다(0.15 = +15%). 그래야 기본 생성한 `FStatModifier`가 무효과가 되고 모디파이어 합치기가 필드 덧셈이 된다. 증가율은 **곱하지 않고 합산**한다 — `+15%` 두 장이면 1.30이지 1.3225가 아니다.

`RecalculateStats`는 계산 후 **하한을 건다**: `MaxHP`/`MaxMP`/`RequiredEXP`는 최소 1, `BaseAttackPower`/`SkillPower`/`CritMultiplier`/`MoveSpeed`/`CritChance`는 최소 0. `RequiredEXP`가 0 이하가 되면 `AddEXP`의 루프 가드에 걸려 **레벨업이 조용히 멈추고**, 공격력이 -100 아래로 가면 데미지가 음수가 되어 **맞은 쪽을 회복시킨다.** `Defense`는 `ApplyDefense`가 이미 하한을 걸므로 여기선 안 건다. `CritChance`에 상한은 없다 — 1.0 초과는 빌드의 목표지 버그가 아니다.

**현재 HP에도 하한 1이 걸린다**(살아있던 경우에 한해). 최대 체력이 줄면 그 감소분이 현재 체력에 반영되는데, 0까지 떨어지면 사망 판정이 `TakeDamage` 안에만 있어서 **죽지는 않고 `IsDead()`만 true가 되는 좀비 상태**가 된다. "최대 체력 -20%, 공격력 +30%" 같은 리스크/리턴 카드를 체력 낮을 때 고르면 실제로 밟는 경로다. **스탯 변화는 데미지가 아니므로 죽이지 않는다** — 죽는 건 `TakeDamage`만 시킨다.

> `RecalculateStats`가 실효값을 쓰는 **유일한 지점**이라는 게 이 구조의 전부다. `CurrentCharacterStat`이나 `MaxHP`/`Defense`/`CritChance`/`CritMultiplier`/`MaxWalkSpeed`에 다른 곳에서 직접 대입하면 그 순간 보너스가 조용히 사라진다.

**개발용 콘솔 명령**: `GJAddBonus <스탯이름> <가산> <증가율>` (예: `GJAddBonus MaxHP 5 0`, `GJAddBonus BaseAttackPower 0 0.15`). 카드 없이 보너스를 시험한다. 이름이 틀리면 사용 가능한 목록을 경고로 찍는다. `UFUNCTION(Exec)`이라 플레이어가 조종 중인 폰에서만 먹는다.

**보너스도 런마다 초기화된다.** 캐릭터가 새로 스폰되면서 `StatBonus`가 기본 생성되므로 초기화 코드가 없다. EXP와 같은 메커니즘이다.

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

### 3.2 인벤토리 UI (`UGJInventoryWidget` / `UGJInventorySlotWidget` / `UGJWeaponSlotWidget` / `UGJSkillSlotWidget`)

인벤토리 창은 **1페이지(아이템 그리드)** + **2페이지(무기 슬롯 0/1번)** + **3페이지(스킬 슬롯 0/1/2번)**를 `WidgetSwitcher`로 감싸고 탭 버튼(`Tab1Button`/`Tab2Button`/`Tab3Button`)으로 전환하는 구조. 페이지 레이아웃 자체는 `WBP_Inventory`에서 구성(UMG MCP 툴로 작업함). 각 탭 버튼의 `OnClicked`가 `SetActiveWidgetIndex(PageSwitcher, N)`에 연결된 이벤트 그래프 노드 세 벌이 전부다.

| 클래스 | 역할 |
|---|---|
| `UGJInventorySlotWidget` | 그리드 한 칸. `SetSlotData(index, inventoryComponent)`로 세팅. `BindWidget: IconImage`, `BindWidgetOptional: QuantityText`(스택 1개면 자동 숨김). 클릭+드래그로 `OwningInventory->SwapSlots()`, 더블클릭으로 `OwningInventory->UseItem()` |
| `UGJInventoryWidget` | 인벤토리 창 전체. `InitializeInventory(inventoryComponent)`를 열 때 1회 호출 — `RefreshGrid()`(아이템 그리드, `OnInventoryChanged` 구독)와 `RefreshWeaponSlots()`(무기 페이지 2칸, 캐릭터의 `OnWeaponSlotsChanged` 구독)를 둘 다 갱신함. `BindWidget: GridPanel`, `BindWidgetOptional: WeaponSlotWidget1/2`, `SkillSlotWidget1/2/3` |
| `UGJWeaponSlotWidget` | 무기 페이지 한 칸(0번/1번). `SetSlotData(slotIndex, character)`로 세팅. `bIsActiveSlot`(BlueprintReadOnly)로 지금 손에 든 무기인지 표시 가능. 순수 클릭(드래그 없음)이면 `SwapToWeaponSlot()`으로 장착 전환, 드래그해서 다른 칸에 놓으면 `SwapWeaponSlots()`으로 자리만 교체(장착 상태 유지) — 클릭/드래그 구분은 `NativeOnMouseButtonDown`에서 `DetectDrag`만 걸고, 실제 교체 액션은 드래그가 감지 안 됐을 때만 `NativeOnMouseButtonUp`에서 수행하는 방식으로 함 |
| `UGJSkillSlotWidget` | 스킬 페이지 한 칸(0/1/2 = 우클릭/Q/F). `SetSlotData(slotIndex, character)`로 세팅. **클릭에는 아무 동작도 없다** — 무기 칸은 클릭이 장착이지만 스킬은 세 개가 항상 활성이라 "장착"이라는 개념이 없다. 드래그해서 다른 칸에 놓으면 `SwapSkillSlots()`로 **쿨타임까지 함께** 자리를 바꾼다(= 어느 키에 둘지를 바꾼다). 드롭 처리는 `SetSlotData`를 직접 부르지 않고 `SwapSkillSlots`만 부른다 — 갱신은 `OnSkillSlotsChanged` 방송을 `UGJInventoryWidget`이 받아서 하는데, 여기서 직접 그리면 도착 칸만 갱신되고 **출발 칸이 남는다** |

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

> `Fire()`는 풀에서 꺼낸 총알에 **매 발사마다 `SetInstigator(GetInstigator())`를 다시 건다.** 총알 풀은 무기의 `BeginPlay`에서 만들어지는데, 필드에 놓여 있다가 주운 무기는 그 시점에 주인이 없어서 풀 전체가 인스티게이터 `nullptr`로 굳어버린다(`OnPickedUp`의 `SetInstigator`는 **무기 액터에만** 걸린다). 그 상태로는 `AGJProjectile::OnHit`이 넘기는 가해자 컨트롤러가 null이라 **적 처치 경험치가 아무에게도 안 가고**, `OtherActor != GetInstigator()` 자기 피격 방지도 동작하지 않는다. `EquipWeapon()`으로 스폰되는 시작 무기만 우연히 정상이었던 문제다.

### 4.3 `AGJProjectile` (GJProjectile.h/.cpp)

- `CollisionComp`(Sphere, `BlockAllDynamic` 프로필) + `MeshComp`(Static, 콜리전 없음) + `ProjectileMovement`
- `FireInDirection(dir, damage, speed, range)` — 활성화(숨김 해제, 콜리전/틱 켬), 속도 세팅, `range/speed` 시간 후 자동 `Deactivate()` 타이머
- `Deactivate()` — 숨기고 콜리전/틱/속도 끔 (풀로 반환, `Destroy()` 안 씀)
- `OnHit()` — 자신/발사자가 아닌 `AGJBaseCharacter`에 맞으면 `UGameplayStatics::ApplyDamage` 호출 → `Deactivate()`

---

## 5. 데미지 파이프라인

엔진 표준 `AActor::TakeDamage`를 그대로 씀 (GAS 아님).

```
[공격 측] 무기 발사 / 적 근접 공격
  → UGJCombatStatics::CalculateOutgoingDamage(무기데미지, 공격력, 치명타확률, 치명타배율)
      = 무기데미지 x (1 + 공격력/100) x 치명타배율
  → UGameplayStatics::ApplyDamage(대상, 계산된 데미지)

[방어 측] AGJBaseCharacter::TakeDamage() [override]
  → UGJCombatStatics::ApplyDefense(받은 데미지, 내 Defense)
      = 데미지 x 100/(100 + 방어력)     ← 방어력은 여기 한 곳에서만
  → CurrentHP 차감
  → OnDamaged.Broadcast(경감 후 데미지)   ← UI(체력바/HUD)가 여길 구독
  → CurrentHP <= 0 이면
       LastDamageInstigator = EventInstigator   ← 가해자를 기억(경험치 지급용)
       HandleDeath()
```

**공식이 사는 곳**: `UGJCombatStatics`(`GJCombatStatics.h/.cpp`, `UBlueprintFunctionLibrary`). 공격 계산과 방어 경감이 서로 다른 지점에서 호출되지만 **공식 자체는 이 파일 하나에만** 있다 — 밸런스 조정 시 여기만 보면 된다.

**방어력은 체감형**이다(`100/(100+방어력)`). 방어력 100마다 "체력이 1배씩 더 있는" 효과이고, 아무리 올려도 100% 무효화에 도달하지 않는다.

**방어력은 `TakeDamage` 한 곳에서만 적용된다.** 따라서 앞으로 어떤 데미지 소스가 추가되어도(장판, 도트, 폭발, 근접 히트 판정) 경감이 자동으로 걸린다. 공격자가 최종값까지 계산하는 방식은 소스가 늘 때마다 방어력 적용을 빠뜨릴 위험이 있어 채택하지 않았다.

**최소 데미지 하한**: 방어력이 극단적으로 높아도 데미지가 0에 수렴해 사실상 무적이 되지 않도록 하한을 둔다. 단 하한값은 `min(1.0, 들어온 데미지)`라, 원래 1보다 약한 공격이 방어력을 거치며 오히려 세지는 역전은 생기지 않는다.

**적은 공격력 배율을 쓰지 않는다** — `EnemyStat.AttackDamage`가 이미 최종 공격력이라 `AttackPower`에 0을 넘긴다. 치명타는 적도 굴린다.

**치명타 여부는 대상에게 전달되지 않는다.** `CalculateOutgoingDamage`가 `bOutWasCritical`을 돌려주지만 공격자 쪽에서만 알 수 있다. 치명타 데미지 폰트 같은 UI를 붙이려면 커스텀 `FDamageEvent`가 필요하다.

**적 처치 경험치는 `TakeDamage`가 기억한 가해자로 지급된다.** `HandleDeath()`에는 가해자 정보가 전혀 없어서(인자도 없고 멤버로도 안 남음), `AGJBaseCharacter::TakeDamage`가 사망이 확정되는 시점에 `LastDamageInstigator`(`TWeakObjectPtr<AController>`)로 기억해 둔다. `AGJEnemyCharacter::HandleDeath()`가 이걸 읽어 `GetPawn()` → `AGJCharacter` 캐스팅에 성공하면 `AddEXP(ExpReward)`를 호출한다. 약참조인 이유는 적이 죽고 `DestroyDelay`(기본 2초) 뒤에 파괴되므로 그 사이 컨트롤러가 먼저 사라질 수 있기 때문이다. 캐스팅이 실패하면(적끼리 죽임, 환경 사망) 아무에게도 주지 않는다 — 여기서는 "받을 사람이 없다"가 정답이다.

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

## 6.5 런 루프 (로그라이트 회차 구조)

플레이어가 죽으면 런이 끝나고 허브로 돌아가며, 허브의 포탈로 새 런을 시작한다.

| 클래스 | 책임 | 수명 |
|---|---|---|
| `UGJGameInstance` | `RebirthCount`(누적 도전 횟수), 레벨 전환 실행 | 앱 전체 (레벨 전환에도 생존) |
| `AGJGameMode` | 사망 감지, 딜레이, 게임오버 화면, 입력 전환 | 레벨마다 새로 생성 |
| `AGJRunPortal` | 허브에서 새 런 시작 (`IGJInteractable`) | 레벨 소속 |
| `UGJGameOverWidget` | 게임오버 화면 (`ReturnToHubButton`, `RunCountText`) | 위젯 |

```
[전투 레벨] HP 0 → HandleDeath() → AGJGameMode::OnPlayerDied()
  → EndRun() (RebirthCount++) + DeathToGameOverDelay(기본 2초) 타이머
  → ShowGameOverScreen() → 위젯 표시 + FInputModeUIOnly
  → "허브로 돌아가기" 클릭 → ReturnToHub() → OpenLevel(HubLevelName)

[허브] 포탈에 상호작용(E) → StartNewRun() → OpenLevel(CombatLevelName)
```

**설계 규칙**

- **레벨 경로는 `Config/DefaultGame.ini`**의 `[/Script/Project_GJ.GJGameInstance]` 섹션에서 지정한다. `Config` 프로퍼티라 블루프린트 서브클래스를 만들 필요가 없다(현재 `DefaultEngine.ini`가 네이티브 클래스를 직접 가리킴).
- **회차 카운트는 사망 시점 한 곳(`OnPlayerDied`)에서만** 증가한다. 게임오버 위젯을 거치든, 위젯 미할당 폴백으로 바로 이동하든 중복되지 않는다. 그래서 위젯이 표시하는 값이 곧 "방금 끝난 도전의 번호"가 된다.
- **인벤토리/무기를 명시적으로 초기화하지 않는다.** 레벨을 다시 여는 순간 캐릭터가 새로 스폰되어 자동 초기화된다(2회 반복 플레이로 검증됨).
- **소프트락 방지**: 레벨 이름이 비어 있으면 `OpenLevel`을 호출하지 않고 에러 로그만 남긴다. `GameOverWidgetClass`가 비어 있으면 화면 없이 곧바로 허브로 보낸다.
- `RebirthCount`는 설계상 세이브에 저장되어 누적되는 값이지만 **저장은 아직 미구현**이라 앱을 종료하면 0으로 돌아간다(M4에서 해결).

> ⚠️ **`FInputModeUIOnly`는 레벨 전환 후에도 남는다.** `SetInputMode`는 월드보다 오래 사는 `UGameViewportClient`에 설정을 걸기 때문에, 게임오버 화면에서 UI 전용 모드로 바꾸면 `OpenLevel` 이후 새 레벨에서도 게임 입력이 통째로 무시된다(마우스만 움직이는 증상). 그래서 `AGJCharacter::BeginPlay()`에서 캐릭터가 스폰될 때마다 `FInputModeGameOnly`로 되돌린다.

---

## 6.7 카드 선택 시스템 (레벨업 선택지)

레벨이 오르면 카드 3장이 뜨고 하나를 고른다. 로직 전부가 `UGJCardComponent`(`GJCardComponent.h/.cpp`)에 있고, `AGJCharacter`는 생성자에서 컴포넌트를 붙이고 `OnLevelUp`을 쏘기만 한다 — **캐릭터는 카드를 모른다.**

| 요소 | 설명 |
|---|---|
| `DT_CardData` (`FCardData`) | 행 이름 = 카드 ID. 이름/설명/아이콘 + `EffectType` + 페이로드(`StatEffect` 또는 `WeaponClass`) + `CardTags` + `bStackable` + `Weight` |
| 뽑기 | 가중 랜덤 **비복원** 추출. 한 장 뽑을 때마다 **총합을 다시 계산**한다 — 안 그러면 빠진 카드의 가중치가 구간에 남아 뽑기가 한쪽으로 몰린다 |
| 제외 조건 | `Weight <= 0`, 이미 먹은 `bStackable=false` 카드, `WeaponClass`가 빈 무기 카드, 효과가 전부 0인 스탯 카드. **무기 슬롯이 꽉 찼다는 이유로는 안 거른다** |
| 장수 | `GetDrawCount()` = `NumCardsToDraw` + `BonusCardSlots`(영구 특성용) + `ExtraCardChance` 판정 1장. 최소 1장 보장 |
| 대기열 | `PendingChoices` 카운터. 킬 한 번에 레벨 2→5가 실제로 일어나므로(`AddEXP`의 `while` 루프) 화면을 한 번만 띄우면 보상 3번을 잃는다 |
| 효과 적용 | `StatBonus` → `AddStatBonus`, `GrantWeapon` → 스폰 후 `PickUpWeapon`(슬롯이 차 있으면 교체 선택), `Ability` → `EquipSkill`(슬롯이 차 있으면 교체 선택, 6.8절) |

**위젯은 카드를 모른다.** `UGJCardSelectWidget`은 `FGJChoiceEntry`(이름/설명/아이콘) 목록을 받아 늘어놓고 **선택 인덱스**만 돌려준다. 인덱스의 의미는 컴포넌트가 `EGJChoiceMode`로 해석한다 — `Card`면 뽑힌 카드 목록의 위치, `WeaponReplace`면 버릴 무기 슬롯, `SkillReplace`면 버릴 스킬 슬롯. **덕분에 교체용 위젯이 따로 없다.**

**무기 슬롯이 꽉 찬 상태에서 무기 카드를 고르면** 같은 위젯에 지금 든 무기 2개를 넣어 "어느 걸 버릴지" 묻고, `AGJCharacter::ReplaceWeaponInSlot`으로 적용한다. 버린 무기는 `DropWeapon`을 거쳐 바닥에 떨어지므로 다시 주울 수 있다. **이 단계에서는 대기열을 줄이지 않는다** — 먼저 줄이면 연속 레벨업 중에 카드 한 장이 통째로 증발한다. 또한 스택 불가 무기 카드는 **이 분기 안에서 따로 `TakenCards`에 기록**한다. 공통 기록 지점은 함수 맨 아래에 있는데 교체 분기는 그 전에 빠져나가므로, 안 그러면 같은 무기 카드가 계속 다시 뜬다.

### 태그로 트리 밀어주기

카드마다 `FGameplayTagContainer CardTags`(예: `Tree.Offense`, `Weapon.Gun`)가 붙고, 컴포넌트의 `TagWeightMultipliers`에 등록된 배율이 뽑기 가중치에 곱해진다(`GetEffectiveWeight`). 플레이어가 타고 있는 트리의 카드를 더 자주 띄우기 위한 장치다.

- **`FName`이 아니라 게임플레이 태그인 이유는 계층**이다. `Tree.Fire` 배율 하나가 `Tree.Fire.Shotgun` 카드까지 자동으로 밀어준다(`HasTag`는 계층 매칭).
- **배율 순회는 카드 태그가 아니라 맵 항목 기준**이다. 카드 기준으로 돌면 `Tree.Fire`와 `Tree.Fire.Shotgun`을 둘 다 가진 카드가 같은 배율을 두 번 먹는다.
- **`Weight <= 0` 필터는 테이블 원본값으로 판정**한다. 꺼둔 카드가 배율 때문에 되살아나면 안 된다.
- 배율이 겹쳐 곱해지므로 `MaxTagWeightMultiplier`(기본 5배)로 상한을 둔다. 한 카드가 풀을 독점하는 것을 막는다.
- 배율을 올리는 주체(직업 카드든, 먹은 카드 누적이든)는 전부 `SetTagWeightMultiplier` 하나로 들어와야 한다. 경로가 둘로 갈리면 배율이 어떻게 합쳐지는지 아무도 모르게 된다.

태그는 `Config/DefaultGameplayTags.ini`에 등록되어 있다(`Tree.Offense/Defense/Agility/Fire`, `Weapon.Gun/Melee`). **이 파일은 에디터 시작 시점에만 읽히므로, 태그를 추가하면 라이브 코딩이 아니라 에디터 재시작이 필요하다.**

> **소프트락 방지**: 위젯 클래스가 비어 있거나 생성에 실패하면 **일시정지를 걸지 않고** 경고 후 대기열을 비운다. 화면은 안 뜨는데 게임만 멈추는 상태가 가장 추적하기 어렵다. 무기 교체 화면이 실패한 경우도 마찬가지로, 그 상태에 빠지느니 슬롯 선택을 포기하고 기본 규칙(활성 슬롯 교체)으로 지급한다 — 안 그러면 카드 3장이 뜬 채 모드만 바뀌어 세 번째 카드를 누르면 없는 슬롯 2번을 교체하려 든다.

일시정지·입력 모드는 인벤토리 모달과 같은 패턴이다(2.2절 `ToggleInventory` 참고): 열 때 `SetGamePaused(true)` + `FInputModeUIOnly` + `SetWidgetToFocus` + `StopAutoFire()`, 닫을 때 `SetGamePaused(false)` + `FInputModeGameOnly` + `SetConsumeCaptureMouseDown(false)`. 인벤토리와 달리 **닫기 키가 없다** — 반드시 한 장 골라야 넘어간다.

**개발용 콘솔 명령**: `GJDrawCards`(뽑기 결과를 로그로만), `GJShowCards`(레벨업 없이 화면만 띄움), `GJSetTagWeight <태그> <배율>`(트리 밀어주기 시험). 셋 다 **`AGJCharacter`의 `UFUNCTION(Exec)`**이고 몸통은 `UGJCardComponent`에 있다 — 컴포넌트에 직접 `Exec`를 달면 콘솔이 `Command not recognized`를 낸다(실제로 겪음).

**카드도 런마다 초기화된다.** 컴포넌트가 캐릭터와 함께 새로 만들어지므로 `TakenCards`와 `TagWeightMultipliers`가 비워진다. EXP·스탯 보너스와 같은 메커니즘이다.

---

## 6.8 액티브 스킬 시스템

우클릭을 누르고 있으면 차징되고, 떼면 구체가 날아간다. 누른 시간에 비례해 구체가 커지고 데미지가 오른다. 로직 전부가 `UGJSkillComponent`(`GJSkillComponent.h/.cpp`)에 있고 **캐릭터는 입력만 넘긴다.**

**슬롯 3개 = 키 3개**: 슬롯 0/1/2가 각각 **우클릭 / Q / F**(`IMC_GJ`의 `IA_Skill1/2/3`). 슬롯 번호와 키가 1:1이라 "스킬 슬롯을 고른다"는 곧 "어느 키에 놓을지 고른다"는 뜻이다.

| 요소 | 설명 |
|---|---|
| `DT_SkillData` (`FSkillData`) | 행 이름 = 스킬 ID |
| 차징 | `배율 = 1 + (MaxChargeMultiplier - 1) × clamp(경과/ChargeTime, 0, 1)`. **크기와 데미지에 함께** 곱해진다 |
| `ChargeTime <= 0` | 차징 없음 — **누르는 순간** 발사(배율 1.0). 뗄 때까지 기다리면 차징도 없는데 손을 떼야 나가는 이상한 감각이 된다 |
| MP·쿨타임 | **떼는 순간 고정값**. 차징률에 비례시키면 약한 구체 연타가 최적해가 되는데, 그 균형을 잡으려면 쿨타임까지 같이 조정해야 해서 변수가 둘로 는다 |
| 데미지 | `CalculateOutgoingDamage(BaseDamage × 배율, **SkillPower**, CritChance, CritMultiplier)` — 평타의 `BaseAttackPower`와 **별개 스탯**이다. 치명타는 공유 |
| 획득 | `Ability` 카드의 `SkillId`로만. 빈 슬롯이 있으면 첫 빈 슬롯, 다 찼으면 **버릴 스킬을 고르는 2단계 화면**(6.7절의 무기 교체와 같은 위젯) |

### 틱을 쓰지 않는다

차징 경과와 쿨타임 잔량을 매 프레임 깎지 않고 `GetWorld()->GetTimeSeconds()` **비교**로 구한다. `ChargeStartTime`, `CooldownEndTime[3]`만 들고 있으면 된다.

부수 효과가 하나 좋다: **일시정지된 월드에서는 시간이 안 흐르므로 카드 화면이 떠 있는 동안 차징이 몰래 차오르지 않는다.**

### `ECharacterState`에 `Charging`을 넣지 않은 이유

`ECharacterState`는 값을 하나만 갖는 단일 상태인데 **차징은 이동·대기와 동시에 성립**한다. 넣으면 차징이 끝났을 때 무엇으로 되돌릴지 알 수 없다 — `Idle`로 돌리면 차징 중 시작한 회피가 아직 안 끝났을 때 `Dodge`를 덮어써서 회피가 상태를 잃는다. 이 enum에 이미 있는 `Rolling`/`Dodge`, `Attacking`/`Attack` 중복도 같은 식으로 생긴 흔적으로 보인다.

대신 `UGJSkillComponent::IsCharging()`을 입력 핸들러가 묻는다.

### 차징 중 제약

| 동작 | 차징 중 |
|---|---|
| 이동 | O (**속도 감소 없음**) |
| 회피(Shift) | O → **차징 취소** |
| 인벤토리(Tab) | O → **차징 취소 후 열림** |
| 좌클릭 평타 / 재장전(R) / 무기 스왑(1·2) | X (입력 무시) |

회피와 인벤토리를 다르게 다룬 이유: 회피는 전투 행동이라 "강한 한 방을 포기하고 회피를 쓴다"는 판단이 성립하지만, 인벤토리는 전투 행동이 아니라 하던 걸 그만두고 메뉴를 보는 조작이다. 눌렀는데 아무 반응이 없으면 입력이 씹힌 것으로 느껴진다.

> ⚠️ **모달을 여는 경로에서는 차징을 반드시 먼저 끈다.** 입력 모드가 `UIOnly`로 바뀌는 순간부터 마우스 "뗌"이 캐릭터에 안 들어와서 차징이 눌린 채 굳고, **UI를 닫는 순간 최대 차징으로 발사된다.** `ToggleInventory`는 함수 맨 앞에서, 카드 화면은 `OpenChoiceUI`에서 `StopAutoFire()` 옆에 `CancelSkillCharge()`를 부른다. `bIsAutoFiring`이 굳어 무한 연사가 됐던 것과 같은 종류의 문제다.

회피의 취소는 반대로 `Idle` 검사를 **통과한 뒤**에 부른다. 회피가 실제로 나가지 않는 상황에서 차징만 날리면 플레이어는 아무 이유 없이 차징을 잃는다.

### 관통은 콜리전 프로필을 바꿔야 동작한다

`AGJProjectile`의 프로필은 `BlockAllDynamic`이고 `bShouldBounce = false`다. 이 상태로 적에게 닿으면 **`UProjectileMovementComponent`가 그 자리에서 멈춘다** — `Deactivate()`를 안 불러도 구체가 통과하는 게 아니라 적 앞에 박힌다.

| | 콜리전 | 타격 경로 |
|---|---|---|
| `PierceCount == 0` | `BlockAllDynamic` | `OnComponentHit` |
| `PierceCount != 0` | 벽만 Block, 폰은 Overlap | `OnComponentBeginOverlap` (벽은 여전히 `OnComponentHit`) |

**비관통 경로에서 프로필을 되돌려 놓아야 한다.** 풀에서 재사용되므로, 안 하면 관통 스킬이 쓰고 반납한 구체를 총알이 집어갔을 때 적을 그냥 통과한다.

`PierceCount`는 **추가로 관통하는 적 수**다: `0`=1명, `1`=2명, `-1`=무한. **-1은 감소시키지 않는다** — 줄이면 -2, -3으로 내려가 "남았는지" 판정이 뒤집힌다.

`HitActors`(`TSet<AActor*>`)가 필요한 이유: 큰 구체는 한 적의 콜리전 안에 여러 프레임 머물러서 이게 없으면 **프레임마다 재타격**한다. 반대로 `Deactivate()`에서 비우지 않으면 다음 발사가 그 적을 못 때린다.

### 풀

구체 클래스별로 나눈다(`TMap<TSubclassOf<AGJProjectile>, FGJProjectilePool>`). 스킬마다 `ProjectileClass`가 다를 수 있어 하나로 못 묶는다. 미리 만들지 않고 **필요할 때 하나씩 늘린다** — 스킬을 안 쓰는 플레이에서는 구체가 하나도 안 만들어진다. 클래스당 기본 10개(무기는 30)이고, 모자라면 그 발사만 무시된다.

**`FireSkill`은 구체를 먼저 확보한 뒤에 MP를 깎는다.** 순서를 뒤집으면 풀이 비었을 때 MP만 사라진다.

### 발사 위치는 총구 소켓 하나로 모은다

`GetMuzzleComponent(OutSocket)` / `GetMuzzleLocation()`이 유일한 출처다. 무기에 `MuzzleSocket`이 있으면 거기(총알과 같은 자리), 없거나 맨손이면 캐릭터 기준 `MuzzleOffset`(전방 60 / 상방 40)으로 떨어진다. **스킬은 맨손이어도 나가야 하므로 폴백이 필수다.**

발사(`FireSkill`)와 차징 미리보기가 **반드시 같은 함수를 쓴다.** 갈라 놓으면 손을 떼는 순간 구체가 순간이동한다 — 실제로 미리보기만 소켓으로 옮겼다가 그 증상이 나왔다.

### UI — 갱신 경로가 둘로 나뉜다

| 무엇 | 경로 | 이유 |
|---|---|---|
| 슬롯 내용 (어느 칸에 무슨 스킬) | `OnSkillSlotsChanged` 델리게이트 | 가끔 바뀐다 |
| 쿨타임 비율 | 위젯 `NativeTick`에서 `GetCooldownRatio(Slot)` 폴링 | **매 프레임 바뀐다.** 델리게이트에 태우면 방송만 하다 끝난다 |

**위젯이 틱하는 것은 위 "틱을 쓰지 않는다"와 충돌하지 않는다.** 컴포넌트는 여전히 시각 비교만 하고 아무것도 안 깎는다. 물어보는 쪽이 UI일 뿐이고, 아이콘 3개짜리라 비용도 없다.

HUD 아이콘과 인벤토리 칸이 **같은 델리게이트를 듣는다.** 인벤토리에서 드래그로 자리를 바꾸면 창을 닫았을 때 HUD도 이미 바뀌어 있다.

**`GetSlotKeyLabel(Slot)`이 키 이름의 단일 출처다.** 원래 `LogSkillInfo`와 카드 교체 화면에 문자열이 따로 하드코딩돼 있었는데 UI 두 개가 붙으면서 네 곳이 될 참이었다. `IMC_GJ` 매핑을 바꾸면 이 함수만 고친다.

### `SwapSkillSlots`와 `EquipSkillInSlot`은 다른 동작이다

| 함수 | 쿨타임 | 왜 |
|---|---|---|
| `SwapSkillSlots(A, B)` (인벤토리 드래그) | **같이 옮긴다** | 안 그러면 스킬을 쓰고 자리를 바꾸는 것이 쿨타임 초기화 수단이 된다 |
| `EquipSkillInSlot(Slot, Id)` (카드로 획득) | **0으로 둔다** | 없던 스킬이 새로 들어오는 것이라 물려받을 쿨타임이 없다. 교체가 손해면 안 된다 |

### 차징 구체 (미리보기)

차징 중 총구에 구체가 나타나 발사될 크기까지 커진다.

- **메시는 `FSkillData`가 아니라 `ProjectileClass` CDO에서 읽는다.** 미리보기와 실제로 날아가는 구체가 자동으로 같아진다. 데이터를 따로 두면 언젠가 어긋나고, 고칠 때까지 아무도 모른다
- **크기는 발사와 같은 공식**(`1 + (MaxChargeMultiplier-1) × 비율`)에 **발사체 메시의 상대 스케일까지 곱한다.** 발사체는 액터 스케일 × 메시 상대 스케일로 보이는데(`BP_GJSkillProjectile`은 메시가 3배) 미리보기는 컴포넌트 하나뿐이라 그걸 직접 곱해야 크기가 맞는다
- **메시 컴포넌트는 캐릭터가 소유한다.** `UGJSkillComponent`는 `UActorComponent`라 트랜스폼이 없어 자식 메시를 못 단다. 켜고 끄고 키우는 것은 전부 스킬 컴포넌트가 하므로 **캐릭터는 여전히 스킬을 모른다**
- **총구 소켓에 어태치**한다. 조준으로 무기가 돌면 구체도 따라가고, 매 프레임 월드 좌표를 다시 찍는 것보다 싸다
- **틱은 차징 중에만 켠다.** `bCanEverTick = true` + `bStartWithTickEnabled = false`로 두고 `ShowChargeOrb`에서만 켠다 — **`bCanEverTick`이 false면 `SetComponentTickEnabled`가 아무 효과도 없다**
- **숨기는 곳은 `CancelCharge` 하나뿐이다.** 회피·인벤토리·카드 화면·사망·정상 발사가 전부 그 함수를 지나가므로 경로마다 따로 챙길 필요가 없다
- 메시를 못 찾으면 **연출만 건너뛰고** 차징과 발사는 정상 동작한다. 시각 요소가 게임플레이를 막으면 안 된다

### MP 부족은 색으로, 쿨타임은 덮개로

`HasEnoughMP(Slot)`이 **판정의 단일 출처**다. `OnSkillPressed`와 HUD 아이콘이 같이 쓴다 — 위젯이 따로 `GetCurrentMP() < MPCost`를 계산하면, 나중에 "MP 소모 감소" 같은 게 붙었을 때 **아이콘은 쓸 수 있다는데 눌러도 안 나가는** 상태가 된다. 화면이 보여주는 조건과 실제로 나가는 조건은 같은 함수여야 한다(총구 위치를 한 함수로 모은 것과 같은 이유다).

| 상태 | 표시 | 거는 위젯 |
|---|---|---|
| 쿨타임 | 덮개가 위에서 아래로 걷힘 | `CooldownBar` |
| MP 부족 | 아이콘이 파래짐 | `IconImage`의 `ColorAndOpacity` |

**둘은 독립이다.** 거는 위젯이 달라서 동시에 걸려도 서로 안 건드린다. 회색이 아니라 파랑인 이유도 이것 — 색으로 "쿨타임이 아니라 마나 문제"가 구분된다.

갱신은 **새 델리게이트 없이 기존 쿨타임 틱에 얹었다.** 아이콘은 이미 매 프레임 도니 추가 비용이 없다. 다만 `SetColorAndOpacity`는 **상태가 바뀌는 순간에만** 부른다(`LastAffordState`) — 매 프레임 부르면 값이 같아도 Slate 무효화가 걸릴 수 있다. `SetSlotData`에서 `INDEX_NONE`으로 되돌려 슬롯이 바뀌면 무조건 다시 칠한다.

> ⚠️ **틴트는 곱셈이라 채널을 깎을 수만 있다.** `UnaffordableTint`의 B를 1.0으로 두면 파랑을 "그대로 두는" 것이라 R/G만 깎여서 **파랗다기보다 어두워 보인다.** 파랑을 실제로 더하려면 **1을 넘겨야** 한다(현재 `(0.30, 0.55, 1.5)`). 아이콘이 무채색 자리표시자라 채도가 더 안 사는 것도 감안할 것.

빈 슬롯은 낼 비용이 없으므로 `HasEnoughMP`가 `true`를 준다. 어차피 아이콘이 숨겨져 있어 화면에도 안 나온다.

### 안 나갈 때는 이유를 남긴다

`OnSkillPressed`는 조용히 `return`하는 경로가 여럿이다(쿨타임, MP 부족, 빈 슬롯, 이미 차징 중, 잘못된 슬롯). 로그가 없으면 **"눌러도 아무 일도 안 일어남"으로만 보여서** 데이터를 건드린 뒤 안 나가기 시작했을 때 원인을 찾는 데 한참 걸린다. 실제로 그렇게 시간을 썼다.

지금은 쿨타임·MP 부족·빈 슬롯이 `[SKILL] 발동 안 됨: ...`을 남긴다. **`Log` 레벨에 0.5초 간격**이다 — 쿨타임 중 입력은 정상 플레이라 경고로 띄우면 시끄럽고, 누르고 있으면 매 프레임 들어온다.

**개발용 콘솔 명령**: `GJEquipSkill <스킬ID> [슬롯]`(카드 없이 장착), `GJSkillInfo`(슬롯별 스킬·쿨타임 잔량·MP·차징 상태), `GJSwapSkills <A> <B>`(두 슬롯을 쿨타임까지 맞바꿈). 카드 명령과 마찬가지로 **`AGJCharacter`의 `UFUNCTION(Exec)`**이고 몸통은 컴포넌트에 있다.

> 쿨타임이 옮겨졌는지 콘솔로 확인할 때는 **`cooldown`을 30초쯤으로 잠깐 올려놓고** 하라. 기본 3초로는 발사하고 Output Log 창으로 옮겨가 타이핑하는 데 그보다 오래 걸려서, 교환 시점엔 이미 정상 만료돼 있다. 그러면 `쿨 0.0s`가 나오는데 **버그인지 정상 만료인지 구분할 수 없다.**

---

## 6.9 룸 시스템 (Task A)

방 하나가 스스로 성립한다 — 적·아이템·상자가 **매번 다르게** 채워지고, 전멸시키면 출구가 열린다. 이 프로젝트에 **적 스폰 시스템이 아예 없던 것**을 메운 작업이다.

절차적 던전은 A(룸 하나가 성립) / B(절차적 배치) / C(스테이지 진행)로 쪼갰고, **지금 있는 것은 A뿐**이다. 설계 문서는 `Docs/superpowers/specs/2026-08-18-room-system-design.md`.

```
AGJRoomBase (abstract)        전멸 추적 · 출구 제어 · 확장 훅
│   virtual PopulateRoom()           무엇을 채울지
│   virtual HandleRoomCleared()      클리어 시 무엇을 할지
│   virtual ShouldBlockExits()       문을 막을지
│
└─ AGJCombatRoom              DT_RoomSpawn 행대로 채운다
   └─ AGJBoxRoom              파라미터로 바닥·벽·문을 생성 (그레이박스)
      └─ BP_Room_Square       크기 값만 채운 BP
```

### 방 종류마다 클래스를 만들지 않는다

시작방 / 전투방 / 보물방 / 보스방 중 **동작이 실제로 다른 것은 보스방 하나뿐이다.** 나머지는 채우고, 전멸을 세고, 문을 여는 일이 똑같고 값만 다르다. 보물방 클래스를 만들면 그 내용이 `MinEnemies=0, ChestChance=1.0`이 전부인데 **그건 데이터지 동작이 아니다.**

보스방은 클리어했을 때 문을 여는 게 아니라 **스테이지를 넘긴다.** 그건 진짜 동작 차이라 오버라이드할 값어치가 있다. 지금은 훅 셋만 열어두고 `AGJBossRoom`은 안 만들었다 — 넘길 스테이지가 아직 없다.

회복방·상점방·보급방(악마방) 요구가 들어왔을 때 이 구조가 시험대에 올랐는데, **방 클래스는 하나도 안 늘어났다.** 늘어나는 것은 아이템 클래스다(즉시 회복, 대가 지불). 방은 여전히 "무엇이 놓이냐"만 다르다.

### 모양과 역할은 다른 축이다

| 축 | 무엇이 정하나 |
|---|---|
| **모양** (정사각형 / 긴 복도 / L자) | **BP 서브클래스** 또는 `AGJBoxRoom`의 파라미터 |
| **역할** (전투 / 보물 / 시작) | **`DT_RoomSpawn` 행** |

둘을 다 BP에 넣으면 `BP_Square_Combat`, `BP_Square_Treasure`, `BP_Long_Combat`… 으로 **곱셈으로 늘어난다.** 분리하면 모양 4개 × 역할 3개가 BP 4개 + 행 3개로 끝난다.

`SetSpawnRow(FName)`가 지연 스폰용 진입점이다. Task B의 생성기가 `SpawnActorDeferred` → `SetSpawnRow` → `FinishSpawning` 순으로 부르면 `BeginPlay` 시점에 이미 역할이 정해져 있다.

### `AGJBoxRoom` — 파라미터로 만드는 그레이박스 방

`InteriorSize` / `WallHeight` / `WallThickness` / `FloorThickness` / `DoorWidth`를 `EditAnywhere`로 받아 `OnConstruction`에서 바닥·벽 5조각·출구·블로커를 생성한다. **아트가 없어도 즉시 플레이된다.**

모양을 하드코딩하지 않은 이유: 하드코딩하면 방 모양마다 C++ 클래스가 생겨 위의 축이 깨진다. 파라미터면 **새 모양이 새 클래스가 아니라 새 값**이 되고, Task B에서 방을 여러 개 찍어낼 때도 그대로 쓴다. 실제 아트가 들어간 방은 여전히 `AGJCombatRoom` 상속 BP로 만들면 되고 둘은 공존한다.

**다시 지을 때 이전 컴포넌트를 먼저 파괴해야 한다.** 안 그러면 파라미터를 고칠 때마다 벽이 겹쳐 쌓인다.

`BlueprintReadWrite`는 안 붙였다. 붙이면 BP 그래프에서 런타임에 값만 바꾸고 `RebuildGeometry`를 안 불러서 **숫자와 화면이 어긋난다.**

### 채우기 규칙

`BeginPlay` → (막기) → `PopulateRoom` → `CheckClearedAfterPopulate` 순이다.

- **개수를 점 개수로 clamp하고 점 배열을 섞는다.** clamp가 없으면 테이블이 점보다 많은 수를 요구할 때 인덱스가 넘치고, 섞지 않으면 항상 앞쪽 점만 쓰여서 배치가 매번 같아진다.
- **적이 0마리면 즉시 클리어**로 보낸다. 안 하면 보물방·시작방이 문이 안 열린 채로 굳는다.
- **막기가 채우기보다 먼저다.** 적 0마리 방은 채우기 끝에 즉시 클리어되면서 다시 열리는데, 순서가 반대면 열린 뒤에 막혀서 **영구히 갇힌다.**

### 출구

`UGJRoomExitComponent`의 자식으로 문짝 메시를 붙이고 C++은 **표시와 콜리전만** 토글한다.

> ⚠️ **표시는 자식까지 전파되지만 콜리전은 전파되지 않는다.** `SetVisibility(bPropagateToChildren=true)`만 부르면 문이 사라졌는데 못 지나가는 **보이지 않는 벽**이 남는다. 자식 프리미티브를 직접 순회해야 한다.

**이 컴포넌트는 Task B의 전제다.** 던전 생성기가 출구의 위치와 전방 방향(`GetForwardVector`)을 알아야 다음 방을 잇는다. 전방(+X)이 방 바깥을 향하도록 배치한다.

### 상자

`AGJTreasureChest`는 E로 **한 번만** 열리고 내용물 아이템 액터를 원형으로 뿌린다.

**인벤토리에 직접 넣지 않는 이유**: 인벤토리가 꽉 찼을 때 아이템이 증발한다. 바닥에 떨어뜨리면 기존 습득 흐름(`AGJItem::PickUp` → 칸이 모자라면 필드에 남음)을 그대로 타서 **새 경로가 하나도 안 생긴다.** 원형으로 흩뿌리는 것은 겹쳐 놓으면 하나만 있는 것처럼 보이기 때문이다.

### 죽음 신호

`AGJBaseCharacter::OnCharacterDied`(`OneParam`, `AGJBaseCharacter*`)를 이 작업에서 새로 만들었다. 기존 `OnDeath`는 `BlueprintImplementableEvent`라 **C++에서 구독할 수 없고**, 바인딩 가능한 것은 `OnDamaged`뿐이었다.

**BP 사망 연출(`OnDeath`) 뒤에 방송한다.** 구독자가 델리게이트 안에서 액터를 건드릴 수 있는데, 먼저 방송하면 `OnDeath`가 이미 정리된 객체 위에서 돈다.

### 구현 중 걸린 것

> ⚠️ **이미 배치된 액터는 BP에 컴포넌트를 추가해도 자동으로 안 따라온다.** 아이템·상자 스폰 포인트를 추가했는데 `아이템 0개 배치`가 나왔다. BP에는 컴포넌트가 멀쩡히 있었고, **레벨에 놓인 인스턴스만 옛 상태로 굳어 있었다.** 지우고 다시 놓으면 해결된다. 앞서 추가한 적 포인트가 멀쩡했던 것은 그 뒤에 부모 클래스를 바꾸면서 전체 리인스턴싱이 일어났기 때문이고 우연이었다.

> ⚠️ **`RootComponent`는 `TObjectPtr`다.** `Parent ? Parent : RootComponent` 같은 삼항 연산자는 원시 포인터와 타입이 갈려 `C2445`로 막힌다. `GetRootComponent()`가 `USceneComponent*`를 돌려주므로 그걸 쓴다.

**스폰 로그는 뽑은 수와 실제 수를 같이 찍는다.** `AliveEnemies.Num()`만 찍으면 "3을 뽑았다"와 "5를 뽑았는데 2마리가 스폰에 실패했다"가 구분되지 않는다. 상자도 확률에 당첨됐는데 스폰 포인트가 없으면 경고를 남긴다 — 이 경고가 위의 인스턴스 문제를 바로 짚어냈다.

---

## 7. UI / 위젯

체력바/HP·MP 바는 **C++ 베이스 클래스 + `BindWidget`** 패턴을 씀 (블루프린트 이벤트 그래프에 로직을 두지 않음 — UMG 이벤트 그래프 자동화 작업 중 문제가 있었던 이력이 있어서, 값 갱신 로직은 전부 C++ `UFUNCTION(BlueprintCallable)`로 두고 디자이너에서는 이름만 맞는 위젯을 배치하면 되게 함).

| 클래스 | 파생 WBP | BindWidget 이름 | 갱신 함수 | 위치/방식 |
|---|---|---|---|---|
| `UGJHealthBarWidget` | `WBP_EnemyHealthBar` | `HealthProgressBar` | `UpdateHealth(Current, Max)` | 적 머리 위, `UWidgetComponent` (Screen space) |
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` (strict) / `EXPBar`, `LevelText`, `SkillIcon1/2/3` (**Optional**) | `UpdateHP` / `UpdateMP` / `UpdateEXP(Current,Required,Level)` / `InitializeSkillIcons(Character)` | 좌상단, `AddToViewport()` |
| `UGJCardSelectWidget` | `WBP_CardSelect` | `CardContainer` (HorizontalBox, strict) | `ShowChoices(TArray<FGJChoiceEntry>)` → `OnChoiceSelected(int32)` | 레벨업 시 중앙, 일시정지 모달 |
| `UGJCardWidget` | `WBP_Card` | `IconImage`, `NameText`, `DescText`, `SelectButton` (전부 strict) | `Setup(Index, Entry)` → `OnCardClicked(int32)` | `CardContainer`에 런타임 생성 |
| `UGJSkillIconWidget` | `WBP_SkillIcon` | `IconImage` (strict) / `KeyText`, `CooldownBar`, `CooldownImage` (**Optional**) | `SetSlotData(Slot, Character)` + `NativeTick`에서 쿨타임·MP 폴링 | `WBP_PlayerHUD`의 `SkillRow`에 3개 |
| `UGJSkillSlotWidget` | `WBP_SkillSlot` | `IconImage` (strict) / `KeyText` (**Optional**) | `SetSlotData(Slot, Character)`, 드롭 시 `SwapSkillSlots` | 인벤토리 스킬 페이지에 3개 |

인벤토리/무기 페이지 UI(`UGJInventoryWidget`, `UGJInventorySlotWidget`, `UGJWeaponSlotWidget`)는 3.2절 참고. 게임오버 화면(`UGJGameOverWidget` → `WBP_GameOver`, `BindWidget: ReturnToHubButton` / `BindWidgetOptional: RunCountText`)은 6.5절 참고 — 이것도 동일한 C++ 베이스 + `BindWidget` 패턴이다.

`EXPBar`/`LevelText`가 strict `BindWidget`이 아니라 **`BindWidgetOptional`인 이유**: strict로 두면 C++이 먼저 들어간 순간 `WBP_PlayerHUD` 컴파일이 깨져서, 에디터에서 위젯을 배치하기 전까지 게임이 정상 동작하지 않는다. 이 프로젝트는 C++ 변경과 에디터 작업이 항상 시차를 두고 일어나므로 새로 추가하는 바인딩은 Optional이 안전하다.

**`CooldownBar`(ProgressBar)와 `CooldownImage`(머티리얼)를 둘 다 Optional로 받는 이유**: `ProgressBar`는 `SetPercent`, 머티리얼 `Image`는 스칼라 파라미터라 **호출이 아예 다르다.** 둘 다 Optional로 두고 붙어 있는 쪽만 갱신하면, WBP에서 어느 방식을 골라도 C++이 안 바뀐다. 지금은 `ProgressBar`(`BarFillType = TopToBottom` — 덮개가 위에서 걷히고 아이콘이 아래에서 드러난다)를 쓰고, 나중에 방사형 마스크 머티리얼이 생기면 **WBP에서 `CooldownBar`를 지우고 `CooldownImage`를 넣는 것만으로 교체**된다.

> ⚠️ **UMG 오버레이 슬롯의 기본 정렬은 Fill이 아니라 `HAlign_Left`/`VAlign_Top`이다.** 아이콘 위에 쿨타임 덮개를 겹칠 때 이걸 안 바꾸면 ProgressBar가 아이콘을 덮지 않고 **자기 desired size(몇 픽셀)로 좌상단에 찍혀서** "쿨타임이 안 보인다"가 된다. `IconImage` 쪽은 브러시에 64x64가 박혀 있어 Left/Top 그대로 둬도 된다. 같은 이유로 ProgressBar의 배경(`widgetStyle.backgroundImage.drawAs`)은 `NoDrawType`으로 꺼야 한다 — 기본 회색 배경이 아이콘을 항상 가린다.

`WBP_SkillSlot`(인벤토리)은 `WBP_SkillIcon`(HUD)과 달리 **쿨타임 표시가 없고**, 구조가 `SizeBox(64) → Border → Overlay → Image + Text`다. `WBP_WeaponSlot`과 같은 모양인데 이유가 있다: **빈 칸이면 아이콘을 숨기는데, 히트 영역이 아이콘뿐이면 빈 칸이 드롭 대상이 안 된다.** Border가 항상 배경을 그려서 히트 영역을 유지한다.

`WBP_PlayerHUD`의 현재 구조는 `RootCanvas > StatusRow(HorizontalBox)` 아래 왼쪽 `PortraitBox`(초상화 `PortraitImage` + `LevelText`), 오른쪽 `StatusBox`(HP/MP/EXP 바 + 그 아래 `SkillRow`, `SizeRule=Fill`)다. `SkillRow`를 넣으면서 `StatusRow`의 캔버스 슬롯 높이를 116 → 190으로 늘렸다 — 고정 크기라 64px 아이콘 줄이 그대로 잘렸다. `bAutoSize`를 켜는 방법은 쓰지 않았다(그러면 HP/MP/EXP 바가 440 폭을 채우는 대신 desired size로 쪼그라들어 기존 레이아웃이 깨진다). 초상화는 아직 아트가 없어 `T_UE_Logo_M`을 자리표시자로 물려 뒀다 — `PortraitImage`는 `bIsVariable=true`라 나중에 런타임 교체도 가능하다.

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
| `BaseAttackPower` | 10 | 무기 데미지에 배율로 적용됨 (`무기데미지 x (1 + 공격력/100)`) |
| `SkillPower` | 10 | **스킬 데미지에만** 같은 방식으로 적용됨. 평타와 나눠서 평타 특화/스킬 특화 빌드가 갈린다 (6.8절) |
| `RequiredEXP` | 100 | **이 레벨에서 다음 레벨까지 필요한 경험치**(누적 총량 아님). 레벨업 시 이 값을 빼고 초과분을 이월. **마지막 행이 곧 레벨 상한** — 행을 추가하면 코드 수정 없이 만렙이 늘어남 |
| `Defense` | 0 | 받는 데미지 경감 (체감형, 100이면 50% 경감) |
| `MoveSpeed` | 600 | `CharacterMovement.MaxWalkSpeed`에 적용. 이전에는 설정하지 않아 엔진 기본값을 쓰고 있었음 |
| `CooldownReduction` | 0 | **미사용** — 스킬 시스템 전까지 연결되지 않음 |
| `CritChance` | 0 | 치명타 확률. **0.0~1.0 범위**(0.25 = 25%). 퍼센트 정수가 아님 |
| `CritMultiplier` | 2.0 | 치명타 시 데미지 배율 |

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
| `Defense` | 0 | 받는 데미지 경감 (체감형) |
| `CritChance` | 0 | 치명타 확률 (0.0~1.0) |
| `CritMultiplier` | 2.0 | 치명타 배율 |
| `ExpReward` | 10 | 이 적을 죽인 플레이어가 얻는 경험치. 적 레벨 등에서 유도하지 않고 적마다 명시 — 유도하면 "좀 더 단단하게" 같은 조정이 성장 속도까지 같이 바꿔버림. `float`인 이유는 비교 대상인 `RequiredEXP`가 `float`이라 파이프라인을 통일하기 위함 |

### `FCardData` — `DT_CardData` (행 이름 = 카드 ID)
| 필드 | 기본값 | 설명 |
|---|---|---|
| `DisplayName` / `Description` | — | 카드에 표시되는 이름과 설명 |
| `Icon` | — | UTexture2D. 비어 있으면 아이콘 영역이 숨겨진다(빈 브러시는 흰 사각형으로 보임) |
| `EffectType` | `StatBonus` | `StatBonus` / `GrantWeapon` / `Ability`(미구현) |
| `StatEffect` | 전부 0 | `EffectType == StatBonus`일 때만 쓰임. `AddStatBonus`로 넘어간다 |
| `WeaponClass` | — | `EffectType == GrantWeapon`일 때만 쓰임 |
| `SkillId` | 없음 | `EffectType == Ability`일 때만 쓰임. `DT_SkillData`의 행 이름. **비었거나 테이블에 없으면 뽑기에서 제외된다** |
| `CardTags` | 비어 있음 | 이 카드가 속한 트리/계열. `TagWeightMultipliers`가 계층 매칭으로 가중치를 밀어준다 (6.7절) |
| `bStackable` | true | false면 한 번 고른 뒤 풀에서 영구 제외. 무기·고유 효과용 |
| `Weight` | 1.0 | 가중 랜덤의 가중치. **0 이하면 절대 안 뽑힌다**(카드를 임시로 끄는 용도로도 쓸 수 있음). 태그 배율은 이 값에 곱해지지만, 제외 판정은 항상 이 원본값으로 한다 |

### `FSkillData` — `DT_SkillData` (행 이름 = 스킬 ID)
| 필드 | 기본값 | 설명 |
|---|---|---|
| `DisplayName` / `Description` / `Icon` | — | 스킬 이름·설명·아이콘 (교체 선택 화면에 표시됨) |
| `SkillType` | `Projectile` | `Projectile` / `Persistent`(**미구현**, 발사 시 경고만) |
| `MPCost` | 10 | 떼는 순간 고정 소비 |
| `Cooldown` | 3 | 떼는 순간부터 시작 (초) |
| `BaseDamage` | 40 | 차징 배율이 곱해지기 전 값 |
| `Range` | 2000 | 이만큼 날아가면 자동 소멸 |
| `ProjectileSpeed` | 1500 | 구체 속도 |
| `ChargeTime` | 1.5 | 최대 차징까지 걸리는 시간. **0이면 차징 없음(누르는 순간 발사)** |
| `MaxChargeMultiplier` | 2.0 | 최대 차징 시 크기·데미지 배율 |
| `BaseScale` | 1.0 | 구체 기본 크기 **배율**(BP의 크기 1.0 기준). 반지름(cm)이 아닌 이유는 cm로 주면 메시 원본 크기를 알아야 비율이 나오기 때문 |
| `PierceCount` | 0 | **추가로** 관통하는 적 수. 0=1명, 1=2명, -1=무한 |
| `SkillTags` | 비어 있음 | 카드 태그와 같은 축(`Tree.Fire` 등). 지금은 표시용 |
| `ProjectileClass` | 없음 | 구체 비주얼. 비어 있으면 컴포넌트의 `DefaultProjectileClass` |

### `FRoomSpawnData` — `DT_RoomSpawn` (행 이름 = 방의 역할)

| 필드 | 타입 | 설명 |
|---|---|---|
| `EnemyPool` | `TArray<TSubclassOf<AGJEnemyCharacter>>` | 나올 수 있는 적. 스폰마다 무작위로 하나 고름 |
| `MinEnemies` / `MaxEnemies` | `int32` | 적 수 범위. **스폰 포인트 개수로 잘린다** |
| `ItemPool` | `TArray<TSubclassOf<AActor>>` | 바닥에 놓일 것. `AGJItem` BP도 무기 BP도 들어감 |
| `MinItems` / `MaxItems` | `int32` | 아이템 수 범위 |
| `ChestPool` | `TArray<TSubclassOf<AGJTreasureChest>>` | 나올 수 있는 상자 |
| `ChestChance` | `float` (0~1) | 상자가 **나오나 마나**. 개수가 아니라 확률인 이유는 보물이라서 |

**이 표가 "매번 다른 방"의 전부다.** 개수를 범위에서 뽑고, 풀에서 무작위로 골라, 스폰 포인트 중 무작위로 골라 놓는다. 같은 껍데기가 매번 다르게 나온다. 현재 값(`Combat_Basic` 행)은 **임시 테스트 값**이다.

### `FStatValues` / `FStatModifier` — 데이터 테이블 행 아님 (스탯 보너스용)

`FStatValues`는 `FCharacterStat`과 **같은 10개 필드**(`MaxHP`, `MaxMP`, `BaseAttackPower`, `SkillPower`, `RequiredEXP`, `Defense`, `MoveSpeed`, `CooldownReduction`, `CritChance`, `CritMultiplier`)를 갖되 **전부 기본값이 0**이다. `FCharacterStat`을 재사용하지 않는 이유가 이것 — 그쪽 기본값이 `MaxHP=100`, `MoveSpeed=600`, `CritMultiplier=2`라서 "보너스 없음"을 표현할 수 없다. 합칠 때 쓰는 `operator+=`는 `GJGameTypes.cpp`에 있다.

`FStatModifier`는 `FStatValues Add`(가산)와 `FStatValues Percent`(증가율) 둘을 담는다. `FTableRowBase`를 상속하지 않으므로 그 자체로는 데이터 테이블 행이 아니지만, `BlueprintType` + `EditAnywhere`로 선언되어 **다른 테이블 행의 필드로 들어갈 수 있다** — M2.6의 `FCardData`가 이걸 품는다.

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
- `FItemData.bPersistAcrossRuns`는 데이터 필드만 있고 여전히 읽히지 않음 — 런 루프(M1)는 완성됐지만 "무엇이 회차를 넘어 남는가"를 정하는 인계 규칙은 미구현 (M3)
- `RebirthCount`는 누적 도전 횟수로 설계되었으나 세이브가 없어 앱 종료 시 0으로 돌아감 (M4에서 해결 예정)
- 허브에는 런 시작 포탈 하나뿐 — 상점/영구 강화 미구현 (M6)
- `FCharacterStat.CooldownReduction`은 필드만 있고 어디에도 연결되지 않음 — 적용 대상이 될 스킬 시스템이 아직 없음
- `ESkillType::Persistent`(지속형 스킬) 미구현 — 발사 시 경고만 찍힘
- 방이 하나뿐이고 손으로 배치해야 한다 — 절차적 배치(Task B)가 아직 없다
- **"처음부터 깔림"의 대가가 Task B에서 드러난다**: 던전 전체가 한 번에 채워지면 먼 방의 적도 처음부터 살아서 플레이어를 향해 길찾기를 한다. "플레이어가 일정 거리 밖이면 AI를 꺼둔다" 정도로 싸게 막아야 한다
- 스테이지 진행과 런 클리어가 없다 (Task C) — `AGJBossRoom`이 `HandleRoomCleared`를 오버라이드할 자리만 비어 있다
- 회복 아이템과 대가 지불 아이템이 없다 (A2) — 회복방·보급방은 이것들이 생기면 **테이블 행 추가만으로** 성립한다
- 상점은 화폐 시스템이 선행한다. 바닥에 아이템을 까는 대신 **상점 NPC와 상호작용**하는 구조로 가기로 했다. 골드가 런을 넘어 남는지가 영구 강화(M6)와 얽히는 갈림길이다
- 방에 문이 **북쪽 하나뿐**이다 — `AGJBoxRoom::AddDoorway`를 여러 번 부르면 늘어나지만 Task B가 필요해질 때 하면 된다
- 문 연출이 없다 — 블로커 큐브가 그냥 사라진다. `OnRoomCleared`를 BP에서 받아 붙이면 된다
- 쿨타임 표시가 시계방향 차오름이 아니라 **위에서 아래로 걷히는 형태**다 — 방사형 마스크 머티리얼을 만들면 `WBP_SkillIcon`에서 `CooldownBar`를 지우고 `CooldownImage`를 넣는 것만으로 교체된다(**C++ 변경 없음**, 7절 참고)
- 스킬 아이콘이 자리표시자 텍스처다(`T_GridChecker_A`) — 카드 아이콘과 같은 상태
- 차징 구체에 이펙트·머티리얼 연출이 없음 — 발사될 구체의 메시를 그대로 키울 뿐이고 시전 애니메이션도 없다
- 인벤토리 스킬 칸에 마우스를 올려도 설명 툴팁이 없음 — `FSkillData.Description`이 어디에도 안 쓰인다
- Skill2(Q)·Skill3(F)에 넣을 실제 스킬이 없음 — 슬롯·입력 바인딩·교체 UI는 전부 준비됨
- `DT_SkillData`의 파이어볼 수치와 `DT_CharacterStat`의 `SkillPower` 곡선은 **임시 테스트 값**
- `FCharacterStat.CooldownReduction`은 여전히 어디에도 연결되지 않음 — **스킬 쿨타임에 적용하는 게 자연스러운 첫 후보**가 됐다
- 스테이지 클리어 시 카드 지급 트리거가 없음 — 진행 구조(M5)가 생긴 뒤. `UGJCardComponent`의 대기열 진입점(`HandleLevelUp` 몸통)을 공개 함수로 빼면 그쪽에서 부르기만 하면 된다
- 카드 리롤/스킵이 없음 — 3장이 전부 마음에 안 들어도 반드시 하나를 골라야 함. `DrawCards(Count)`는 부작용이 없게 짜여 있어서 리롤은 재호출만으로 되지만, 버튼과 횟수 관리가 없다
- 선택지 수를 늘리는 경로(`BonusCardSlots`, `ExtraCardChance`)는 멤버만 있고 아무도 쓰지 않음 — 영구 특성(M6)이 생기면 여기에 꽂는다
- 태그 배율을 올리는 주체가 없음 — `SetTagWeightMultiplier`는 콘솔에서만 호출된다. "직업 카드"나 "먹은 카드 누적"이 붙으면 둘 다 이 함수로 들어와야 한다
- 카드 희귀도가 확률(`Weight`)로만 존재하고 시각적 구분(색 테두리 등)이 없음
- `DT_CardData`의 카드 6장은 **검증용 임시 데이터** — 아이콘도 기존 텍스처를 자리표시자로 쓰고 있음
- 회복 수단이 하나도 없음 — 레벨업도 최대치 증가분만 얹고, 회복 카드나 포션이 없어서 런 내내 체력이 단방향으로 깎인다. 회복 카드를 넣으려면 `ECardEffectType`에 값 추가가 필요
- 레벨업/경험치 획득 연출(팝업, 사운드, 파티클)이 없음 — 현재는 HUD 바와 `UE_LOG`뿐
- `DT_CharacterStat`의 레벨 2~5 성장 곡선은 **임시 테스트 값** — 실제 밸런싱은 스테이지 진행(M5)이 생긴 뒤에 해야 의미가 있음
- `WBP_PlayerHUD`의 초상화(`PortraitImage`)는 `T_UE_Logo_M` 자리표시자 — 실제 캐릭터 일러스트로 교체 필요
- 모디파이어 개별 제거/시간제 버프가 없음 — `StatBonus`는 누적만 한다. 10초짜리 이동속도 버프나 무기 장착 중에만 붙는 스탯이 필요해지면 `TArray<FStatModifier>` + 핸들 방식으로 바꿔야 하며, 그때도 `RecalculateStats`만 고치면 되고 실효값을 읽는 코드는 안 바뀐다
- 적에게는 스탯 보너스가 없음 — `ApplyEnemyStat`이 테이블 값을 멤버에 직접 대입한다. 스테이지가 올라갈수록 적이 강해지는 스케일링(M5)이 필요해지면 같은 구조체를 재사용하면 된다
- 치명타가 터져도 화면에 표시되지 않음 — 치명타 여부가 공격자 쪽에만 있어서, UI를 붙이려면 커스텀 `FDamageEvent`가 필요

---

## 10. 빌드/워크플로 메모

- 에디터는 보통 라이브 코딩 켜진 채로 열려있음 → 코드 수정 후 에디터에서 **Ctrl+Alt+F11**
- **완전히 새로운 UCLASS 파일**(새 `.h`/`.cpp` 쌍)은 라이브 코딩만으로는 못 받는 경우가 있음 — 그럴 땐 에디터를 닫고 `Build.bat`으로 전체 빌드하거나, 라이브 코딩 컴파일 후 위젯 블루프린트 부모 클래스 목록에 새 클래스가 안 뜨면 에디터 재시작
- USTRUCT 레이아웃 변경(필드 추가/이름 변경)을 라이브 코딩으로 여러 번 하면, 그 구조체를 참조하는 UMG 블루프린트 그래프(`Break WeaponStat` 등)의 핀 타입이 깨질 수 있음 → 증상: "정확히 일치하는 구조체만 호환" 컴파일 에러 → **에디터 완전 재시작**(재빌드 불필요, 껐다 켜기만)으로 대부분 해결됨
- **새 `UPROPERTY`를 추가하면 라이브 코딩만으로는 부족하다 — 에디터 재시작이 필요하다.** 함수 본문 수정과 새 클래스 추가는 라이브 코딩으로 되지만, 기존 클래스/구조체에 **필드가 늘어나는 변경**은 리플렉션에 등록되지 않는다. M2.7에서 두 번 걸렸다:
  - `AGJCharacter`에 `Skill1Action` 등을 추가 → 블루프린트 CDO에는 값이 정상으로 보이는데 **런타임 인스턴스는 NULL**을 읽음 → 입력 바인딩이 통째로 안 걸림
  - `FCardData`에 `SkillId`를 추가 → MCP `set_rows`가 `Properties not found in schema: ['skillId']`로 거부, 데이터 테이블 에디터에도 칼럼이 안 뜸
  - **증상이 "값은 있는데 안 읽힌다"라 코드를 의심하게 되는 게 함정이다.** 데이터 테이블에서 읽는 값(`SkillPower` 등)은 멀쩡해서 더 헷갈린다 — CDO나 스키마에 등록돼야 하는 것만 걸린다. 새 `UPROPERTY`를 추가했으면 **에셋 작업 전에 먼저 재시작**하는 게 빠르다
- PCH 생성 중 `C1076`/`C3859` 에러는 그 순간 시스템 메모리 부족 때문(코드 문제 아님) — 메모리 여유 있는 상태에서 재시도
- UMG 위젯 트리/그래프를 MCP로 직접 조작할 때 자주 걸리는 함정은 7절 마지막 노트 참고
- **데이터 테이블은 `Data/*.csv`가 소스**다. 엑셀에서 CSV를 고치고 → 에셋 우클릭 **Reimport** → **Ctrl+S** → CSV와 `.uasset`을 **함께 커밋**한다(게임이 읽는 건 `.uasset`이라 CSV만 커밋하면 값이 안 바뀐 채로 남는다). 주의점 둘: **리임포트는 전체 교체**라 CSV에 빠진 열은 구조체 기본값으로 리셋되므로 항상 전체 열을 쓸 것, 그리고 에디터에서 직접 만든 테이블은 소스 파일 기록이 없어 **Reimport가 비활성**이다 — CSV를 콘텐츠 브라우저로 **드래그해서 덮어쓰기 임포트**를 한 번 해야 경로가 기록되면서 활성화된다(Export만으로는 연결이 생기지 않는다)
- 스탯 밸런싱은 콘솔 명령 `GJAddBonus <스탯이름> <가산> <증가율>`로 카드 없이 시험할 수 있다. `UFUNCTION(Exec)`이라 플레이어가 조종 중인 폰에서만 먹는다. **PIE에서 `~` 키는 디버그 매니저가 가로채므로**(`DebugManager.CycleToPreviousColumn`이 대신 실행됨) **에디터 Output Log 창 아래쪽 `Cmd:` 입력칸**에 치는 쪽이 확실하다. 여러 명령을 한 번에 붙여넣으면 하나로 합쳐져 첫 줄만 실행되니 한 줄씩 넣을 것
- **MCP 서버 포트는 8123**이다(`EditorPerProjectUserSettings.ini`의 `[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings] ServerPortNumber`, `.mcp.json`과 짝을 맞춰야 함). 기본값 8000은 **Incredibuild Manager 서비스가 선점**하고 있어서 언리얼 MCP 서버가 바인딩에 실패한다 — 이때 로그에는 "Starting MCP server on port 8000"만 찍히고 실패가 안 남아서, 클라이언트 쪽에서는 원인 불명의 `ECONNRESET`으로만 보인다
