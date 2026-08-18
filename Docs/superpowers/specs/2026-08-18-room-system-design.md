# 룸 시스템 설계 (Task A: 룸 하나가 성립)

**작성일:** 2026-08-18

## 목표

방 하나가 스스로 성립하게 만든다. 방에 들어가면 적·아이템·보물상자가 **매번 다르게** 채워져 있고, 적을 전멸시키면 출구가 열린다.

## 왜 이것부터인가

절차적 던전은 서로 다른 시스템 넷이다 — 룸 규격, 배치 알고리즘, 적 스폰, 클리어/진행. 한 번에 열면 어디가 틀렸는지 못 찾는다.

| | 내용 | 상태 |
|---|---|---|
| **A. 룸 하나가 성립** | 룸 규격, 스폰, 전멸 판정, 출구 개방 | **이 문서** |
| **B. 절차적 배치** | 룸 여러 개를 랜덤 방향으로 연결, 겹침 방지 | 이후 |
| **C. 스테이지 진행** | 보스방 클리어 → 다음 스테이지, 런 클리어 | 이후 |

**지금 이 프로젝트에 적 스폰 시스템이 아예 없다.** 적은 레벨에 손으로 배치돼 있고, 런은 사망으로만 끝나며 클리어 조건이 없다. A가 그 공백을 메운다. A만 끝나도 카드·스킬이 향할 목표가 생긴다.

B는 A 없이는 검증할 수 없다 — 방을 아무리 잘 이어붙여도 안이 비어 있으면 맞게 만들었는지 알 수가 없다.

## 제약

**나는 방의 형태를 만들 수 없다.** 레벨 지오메트리와 액터 배치는 전부 에디터 작업이다. 내가 맡는 것은 "무엇을 어디에 스폰할지 정하는 로직"이고, 방 껍데기·상자 메시·문 연출은 사용자가 만든다.

`RuntimeGeneration=Dynamic`이 이미 켜져 있어(`Config/DefaultEngine.ini`) 런타임에 스폰된 지오메트리에도 내비메시가 따라온다. B에서 방을 런타임에 배치해도 적 길찾기가 동작한다.

## 설계 원칙: 모양과 역할은 다른 축이다

| 축 | 무엇이 정하나 |
|---|---|
| **모양** (정사각형 / 긴 복도 / L자) | **BP 서브클래스** — 사용자가 에디터에서 만든다 |
| **역할** (전투 / 보물 / 시작) | **`DT_RoomSpawn` 행** — 배치할 때 정해진다 |

둘을 다 BP에 넣으면 `BP_Square_Combat`, `BP_Square_Treasure`, `BP_Long_Combat`… 으로 **곱셈으로 늘어난다.** 분리하면 모양 4개 × 역할 3개가 **BP 4개 + 테이블 행 3개**로 끝난다.

## 클래스 구조

```
AGJRoomBase (abstract, C++)
│   스폰 포인트 수집 · 방 채우기 · 전멸 추적 · 출구 제어
│
│   virtual void PopulateRoom()           ← 무엇을 채울지
│   virtual void HandleRoomCleared()      ← 클리어 시 무엇을 할지
│   virtual bool ShouldBlockExits() const ← 문을 막을지
│
├─ AGJCombatRoom (C++)
│     DT_RoomSpawn 행대로 적·아이템·상자를 채운다.
│     전투방 · 보물방 · 시작방이 전부 이 클래스 + 다른 행으로 성립한다.
│     └─ BP_Room_XXX  ← 사용자가 만드는 방 껍데기 (모양)
│
└─ AGJBossRoom (C++)  ← Task C. 지금 만들지 않는다
      HandleRoomCleared를 오버라이드해 스테이지 진행을 알린다.
```

### 왜 방 종류마다 클래스를 만들지 않는가

시작방 / 전투방 / 보물방 / 보스방 중 **동작이 실제로 다른 것은 보스방 하나뿐이다.** 나머지 셋은 채우고, 전멸을 세고, 문을 여는 일이 똑같고 값만 다르다. 보물방 클래스를 만들면 그 내용은 `MinEnemies=0, ChestChance=1.0`이 전부인데, 그건 데이터지 동작이 아니다.

보스방은 클리어했을 때 **문을 여는 게 아니라 스테이지를 넘긴다.** 이건 진짜 동작 차이라 오버라이드할 값어치가 있다.

지금은 훅 세 개만 열어두고 `AGJBossRoom`은 만들지 않는다. 넘길 스테이지가 아직 없다. Task C에서 파일 두 개를 붙이면 되고 베이스는 안 건드린다.

## 신규 클래스

### `AGJBaseCharacter::OnCharacterDied` (기존 클래스에 델리게이트 추가)

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDiedSignature, AGJBaseCharacter*, DeadCharacter);

UPROPERTY(BlueprintAssignable, Category = "Combat")
FOnCharacterDiedSignature OnCharacterDied;
```

`HandleDeath()` 끝에서 방송한다.

**왜 필요한가:** 지금 `OnDeath`는 `BlueprintImplementableEvent`라 C++에서 구독할 수 없고, 바인딩 가능한 것은 `OnDamaged`뿐이다. 방이 전멸을 세려면 죽음을 C++에서 받아야 한다.

**왜 폴링이 아닌가:** 이 프로젝트는 `OnDamaged`·`OnWeaponSlotsChanged`·`OnSkillSlotsChanged`가 전부 델리게이트다. 살아있는 적을 매초 세는 것은 결이 다르고, 방이 여러 개가 되면 비용도 는다.

플레이어도 이 델리게이트를 방송하지만 방은 자기가 스폰한 적만 구독하므로 문제없다.

### `UGJRoomSpawnPointComponent : USceneComponent`

```cpp
UENUM(BlueprintType)
enum class ESpawnPointType : uint8
{
    Enemy,
    Item,
    Chest
};

UPROPERTY(EditAnywhere, Category = "Room")
ESpawnPointType PointType = ESpawnPointType::Enemy;
```

방 BP 안에 원하는 만큼 꽂는다. 스폰은 이 컴포넌트의 월드 트랜스폼에서 일어난다.

**왜 컴포넌트 하나에 열거형인가:** 셋으로 나누면 방 BP에서 점의 용도를 바꿀 때 컴포넌트를 지우고 다시 만들어야 한다. 열거형이면 드롭다운 한 번이다. 문자열 태그와 달리 전용 타입이라 오타가 컴파일 타임에 걸리는 이점은 그대로다.

### `UGJRoomExitComponent : USceneComponent`

출구 표시. 문을 막는 메시·콜리전은 **이 컴포넌트의 자식으로** 붙인다. C++은 자식 전체의 표시와 콜리전만 토글한다.

**Task A에서는 막고 여는 데만 쓰이지만, Task B가 이 컴포넌트 위에 세워진다.** 던전 생성기는 방의 출구 위치와 전방 방향을 알아야 다음 방을 이어붙일 수 있다. 지금 타입으로 만들어 두는 것은 선행 투자가 아니라 B의 전제다.

### `AGJRoomBase : AActor` (abstract)

```cpp
public:
    // 지연 스폰에서 BeginPlay 전에 역할을 정한다 (Task B가 쓴다).
    void SetSpawnRow(FName RowName);

    UPROPERTY(BlueprintAssignable, Category = "Room")
    FOnRoomClearedSignature OnRoomCleared;   // DECLARE_..._OneParam(AGJRoomBase*)

    UFUNCTION(BlueprintPure, Category = "Room")
    bool IsCleared() const { return bCleared; }

protected:
    virtual void BeginPlay() override;

    virtual void PopulateRoom();
    virtual void HandleRoomCleared();
    virtual bool ShouldBlockExits() const;

    UFUNCTION()
    void OnSpawnedEnemyDied(AGJBaseCharacter* DeadCharacter);

    void SetExitsBlocked(bool bBlocked);

    UPROPERTY(EditAnywhere, Category = "Room")
    UDataTable* RoomSpawnTable;

    UPROPERTY(EditAnywhere, Category = "Room")
    FName SpawnRowName;

    UPROPERTY()
    TArray<AGJEnemyCharacter*> AliveEnemies;

    bool bCleared = false;
```

`BeginPlay()`가 `PopulateRoom()`을 부른다. Task B의 지연 스폰(`SpawnActorDeferred` → `SetSpawnRow` → `FinishSpawning`)에서는 `BeginPlay` 시점에 이미 역할이 정해져 있다.

### `AGJTreasureChest : AActor, IGJInteractable`

```cpp
UPROPERTY(EditAnywhere, Category = "Chest")
TArray<TSubclassOf<AActor>> Contents;    // 뿌릴 아이템 액터 (AGJItem BP, 무기 BP 등)

UPROPERTY(EditAnywhere, Category = "Chest")
int32 MinDrops = 1;

UPROPERTY(EditAnywhere, Category = "Chest")
int32 MaxDrops = 2;

UPROPERTY(EditAnywhere, Category = "Chest")
float DropRadius = 120.f;

UFUNCTION(BlueprintImplementableEvent, Category = "Chest")
void OnChestOpened();
```

E로 열면 **내용물 아이템 액터를 주변에 뿌린다.** 한 번만 열린다(`bOpened`).

**왜 인벤토리에 직접 넣지 않는가:** 인벤토리가 꽉 찼을 때 아이템이 증발한다. 필드에 떨어뜨리면 기존 습득 흐름(`AGJItem::PickUp` → 칸이 모자라면 필드에 남음)을 그대로 타서 **새 경로가 하나도 안 생긴다.**

`AGJItemBase`와 같은 패턴으로 `InteractionCollision`(Sphere)을 두고 `Interact_Implementation`에서 범위를 확인한다.

## 데이터 테이블: `DT_RoomSpawn` (`FRoomSpawnData : FTableRowBase`)

| 필드 | 타입 | 설명 |
|---|---|---|
| `EnemyPool` | `TArray<TSubclassOf<AGJEnemyCharacter>>` | 이 방에 나올 수 있는 적 |
| `MinEnemies` / `MaxEnemies` | `int32` | 적 수 범위 |
| `ItemPool` | `TArray<TSubclassOf<AActor>>` | 바닥에 놓일 것. `AGJItem` BP도, 무기 BP도 들어간다 |
| `MinItems` / `MaxItems` | `int32` | 아이템 수 범위 |
| `ChestPool` | `TArray<TSubclassOf<AGJTreasureChest>>` | 나올 수 있는 상자 |
| `ChestChance` | `float` (0~1) | 상자가 나올 확률 |

**이 표가 "매번 다른 방"의 전부다.** 개수를 범위에서 뽑고, 풀에서 무작위로 골라, 스폰 포인트 중 무작위로 골라 놓는다. 같은 껍데기가 매번 다르게 나온다.

**상자만 개수가 아니라 확률인 이유:** 보물은 "몇 개 나오나"가 아니라 "나오나 마나"다. 풀로 둔 것은 나중에 희귀 상자를 추가할 때 스키마를 안 건드리기 위해서고, 지금 비용은 0이다.

## 채우기 알고리즘

```
BeginPlay():
  0. ShouldBlockExits()가 true면 SetExitsBlocked(true) - 채우기 전에 먼저 막는다
  1. PopulateRoom()

PopulateRoom():
  1. GetComponents<UGJRoomSpawnPointComponent>()로 점을 모아 타입별로 나눈다
  2. RoomSpawnTable에서 SpawnRowName 행을 찾는다
     - 없으면 경고를 남기고 빈 방으로 둔다 (아래 "적 0마리" 규칙으로 즉시 클리어)
  3. 적:
       Count = RandRange(Min, Max)를 Enemy 점 개수로 clamp
       점 배열을 셔플해 앞에서 Count개만 쓴다 (한 점에 겹쳐 스폰되지 않게)
       각 점에 EnemyPool에서 무작위 클래스를 스폰
       각 적의 OnCharacterDied를 구독하고 AliveEnemies에 넣는다
  4. 아이템: Item 점에 같은 방식
  5. 상자: FRand() < ChestChance 이면 Chest 점 하나에 ChestPool에서 무작위 스폰
  6. AliveEnemies가 비어 있으면 즉시 HandleRoomCleared()
```

`ShouldBlockExits()` 기본 구현은 `true`다. **막기를 채우기보다 먼저 하는 이유:** 적 0마리 방은 채우기 끝에 즉시 클리어되면서 `SetExitsBlocked(false)`로 다시 열린다. 순서가 반대면 열린 뒤에 막혀서 영구히 갇힌다. 통로처럼 아예 막을 일이 없는 방은 이 훅을 `false`로 오버라이드한다.

**점 개수로 clamp하고 셔플하는 이유:** 테이블이 점보다 많은 수를 요구할 수 있다. clamp가 없으면 인덱스가 넘치고, 셔플이 없으면 같은 점에 여러 마리가 겹쳐 스폰된다.

## 클리어 판정

`OnSpawnedEnemyDied` → `AliveEnemies`에서 제거 → 비었고 아직 클리어가 아니면 `HandleRoomCleared()`.

`HandleRoomCleared()` 기본 구현: `bCleared = true` → `SetExitsBlocked(false)` → `OnRoomCleared.Broadcast(this)`.

**적 0마리 방은 즉시 클리어다.** `MinEnemies=0`이거나 풀이 비면 문이 안 열린 채로 굳는다. 채우기가 끝난 시점에 적이 없으면 바로 클리어로 보낸다. 상점·보물방이 이 경로로 자연스럽게 성립한다.

문짝 메시와 열리는 연출은 `OnRoomCleared`를 BP에서 받아 붙인다. C++은 표시와 콜리전만 토글한다 — `OnDeath`를 비워두고 BP가 사망 연출을 맡는 것과 같은 구조다.

## Task A에서 하지 않는 것

- 방을 여러 개 잇기, 겹침 방지, 시작·보스방 보장 → **B**
- 스테이지 진행, 런 클리어, `AGJBossRoom` → **C**
- 방 활성화 트리거. **방은 `BeginPlay`에 채워진다** (사용자 결정: "처음부터 깔려 있게")

> ⚠️ **B에서 반드시 다시 꺼낼 것:** "처음부터 깔림"은 던전 전체가 한 번에 채워진다는 뜻이고, **먼 방의 적도 처음부터 살아서 플레이어를 향해 길찾기를 한다.** A에서는 방이 하나라 드러나지 않는다. B 설계 때 "플레이어가 일정 거리 밖이면 AI를 꺼둔다" 정도로 싸게 막는다. 지금 미리 만들지 않는다.

## 사용자가 만들 것

| 에셋 | 내용 |
|---|---|
| `BP_Room_XXX` **1개** | `AGJCombatRoom` 상속. 바닥·벽, `UGJRoomSpawnPointComponent` 몇 개(Enemy 4~5, Item 2, Chest 1), `UGJRoomExitComponent` 1~2개와 그 자식으로 붙인 블로커 메시 |
| `BP_Chest_XXX` **1개** | `AGJTreasureChest` 상속. 메시만 |
| `DT_RoomSpawn` | 행 1~2개 (전투방, 보물방) |

A 검증에는 방 1개면 충분하다. 나머지 모양은 B에서 늘린다.

## 검증

테스트 스위트가 없다. **Live Coding 컴파일 통과 + 수동 PIE 확인**이다.

`TestLev`에 `BP_Room_XXX`를 하나 놓고 플레이:

1. 방에 적이 **랜덤한 수·조합으로** 스폰된다
2. 바닥에 아이템이 놓여 있고 주울 수 있다
3. 상자가 확률대로 나오고, E로 열면 아이템이 뿌려지며, 두 번은 안 열린다
4. 출구 블로커가 처음엔 막혀 있다
5. 적을 전멸시키면 **블로커가 사라지고** `OnRoomCleared` 로그가 뜬다
6. 다시 플레이하면 **구성이 달라진다**
7. `MinEnemies=0`인 행을 쓰면 **시작부터 열려 있다**

## 미결 사항

없음. Task B(절차적 배치)와 Task C(스테이지 진행)는 각자 별도의 스펙과 계획으로 다룬다.
