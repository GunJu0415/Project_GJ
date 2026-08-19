# 룸 시스템 구현 계획 (Task A)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 방 하나가 스스로 성립한다 — 적·아이템·상자가 매번 다르게 채워지고, 전멸시키면 출구가 열린다.

**Architecture:** `AGJRoomBase`가 전멸 추적과 출구 제어를 맡고 `virtual` 훅 셋(`PopulateRoom`/`HandleRoomCleared`/`ShouldBlockExits`)을 연다. `AGJCombatRoom`이 데이터 테이블 행대로 방을 채운다. 방의 **모양은 BP 서브클래스**, **역할은 테이블 행**이라 둘이 곱셈으로 늘지 않는다.

**Tech Stack:** UE 5.8, C++ (`USceneComponent` 서브클래스, `FTableRowBase`, 다이내믹 멀티캐스트 델리게이트, `IGJInteractable`)

**설계 문서:** `Docs/superpowers/specs/2026-08-18-room-system-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다.
- **새 `UPROPERTY`나 새 `UCLASS`를 추가하면 라이브 코딩만으로는 부족하다 — 에디터 재시작이 필요하다.** 이 계획은 매 태스크가 새 클래스를 추가하므로 **컴파일 후 에셋 작업 전에 재시작**을 기본으로 넣는다. 증상이 "값은 있는데 안 읽힌다"라 코드를 의심하게 되는 게 함정이다(개발 가이드 10절).
- **로직은 C++에, 블루프린트는 얇게.** BP는 C++ 클래스를 상속받아 메시·에셋·연출만 채운다. BP 그래프에 로직을 두지 않는다.
- **서브클래스는 동작이 실제로 다를 때만 만든다.** 값만 다른 변형은 데이터 테이블 행이다.
- **인코딩**: 새 주석은 UTF-8 한글로 쓴다. 초기 파일의 깨진 옛 주석 줄은 건드리지 않는다.
- **커밋 메시지는 한국어**로 쓰고 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다. 브랜치를 나누지 않고 `main`에서 작업한다.
- 에디터가 열려 있으면 UBT 빌드가 막힌다. 컴파일은 사용자에게 **Ctrl+Alt+F11**을 요청한다.
- PIE에서 `~` 콘솔은 디버그 매니저가 가로챈다. **Output Log 창의 `Cmd:` 입력칸**을 쓰고 명령은 **한 줄씩** 입력한다.
- **`Content/GJ/Level/TestLev.umap`은 이 작업과 무관하게 이미 수정 상태다.** 커밋에 포함하지 않는다.
- **MCP(포트 8123)로 데이터 테이블과 블루프린트를 만들 수 있다.** 다만 방의 지오메트리(바닥·벽 배치)와 스폰 포인트의 시각적 위치 조정은 에디터에서 눈으로 보며 하는 게 빠르므로, 뼈대는 MCP로 만들고 **모양 다듬기는 사용자에게 요청한다.** MCP가 끊기면 전부 사용자 요청으로 돌린다.

---

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJBaseCharacter.h/.cpp` (수정) | `OnCharacterDied` 델리게이트 | 1 |
| `Source/Project_GJ/GJGameTypes.h` (수정) | `ESpawnPointType`, `FRoomSpawnData` | 2, 4 |
| `Source/Project_GJ/GJRoomSpawnPointComponent.h/.cpp` (신규) | 스폰 자리 표시 | 2 |
| `Source/Project_GJ/GJRoomBase.h/.cpp` (신규) | 전멸 추적, 출구 제어, 확장 훅 | 2, 3 |
| `Source/Project_GJ/GJCombatRoom.h/.cpp` (신규) | 테이블 행대로 방 채우기 | 2, 4 |
| `Source/Project_GJ/GJBoxRoom.h/.cpp` (신규, 계획 외 추가) | 파라미터로 바닥·벽을 생성하는 그레이박스 방 | 2 |
| `Source/Project_GJ/GJRoomExitComponent.h/.cpp` (신규) | 출구 표시와 개폐 | 3 |
| `Source/Project_GJ/GJTreasureChest.h/.cpp` (신규) | 보물 상자 | 4 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 5 |

**태스크 순서 근거**: 죽음 신호(1) → 적 스폰과 전멸 판정(2) → 출구 개폐(3) → 아이템·상자(4) → 문서(5). 2가 끝나면 로그로 "적이 랜덤하게 나오고 다 잡으면 클리어된다"가 확인되고, 3에서 그게 눈에 보이게 된다.

---

## Task 1: 사망 델리게이트

방이 전멸을 세려면 적의 죽음을 C++에서 받아야 한다. 지금 `OnDeath`는 `BlueprintImplementableEvent`라 구독할 수 없다.

**Files:**
- Modify: `Source/Project_GJ/GJBaseCharacter.h`, `Source/Project_GJ/GJBaseCharacter.cpp`

**Interfaces:**
- Produces:
  - `FOnCharacterDiedSignature` (`OneParam`, `AGJBaseCharacter*`)
  - `AGJBaseCharacter::OnCharacterDied` (`BlueprintAssignable`)

- [ ] **Step 1: 델리게이트 선언 추가**

`GJBaseCharacter.h`에서 다음 줄을 찾는다:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamagedSignature, float, DamageAmount, AActor*, DamageCauser);
```

이를 다음으로 교체한다:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDamagedSignature, float, DamageAmount, AActor*, DamageCauser);

// 이 캐릭터가 죽었다. 룸이 자기가 스폰한 적의 전멸을 세는 데 쓴다.
// OnDeath(BlueprintImplementableEvent)와 별도로 두는 이유: 그건 C++에서 구독할 수 없다.
class AGJBaseCharacter;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCharacterDiedSignature, AGJBaseCharacter*, DeadCharacter);
```

- [ ] **Step 2: 델리게이트 프로퍼티 추가**

`GJBaseCharacter.h`에서 다음 두 줄을 찾는다:

```cpp
    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnDamagedSignature OnDamaged;
```

그 **아래**에 추가한다:

```cpp

    UPROPERTY(BlueprintAssignable, Category = "Stat")
    FOnCharacterDiedSignature OnCharacterDied;
```

- [ ] **Step 3: `HandleDeath`에서 방송**

`GJBaseCharacter.cpp`의 `HandleDeath` 안에서 다음 줄을 찾는다:

```cpp
    OnDeath();
}
```

이를 다음으로 교체한다:

```cpp
    OnDeath();

    UE_LOG(LogTemp, Log, TEXT("[DEATH] %s 사망"), *GetName());

    // BP 사망 연출(OnDeath) 뒤에 방송한다. 구독자가 이 델리게이트 안에서 액터를 건드릴 수
    // 있는데, 먼저 방송하면 OnDeath가 이미 정리된 객체 위에서 돌 수 있다.
    OnCharacterDied.Broadcast(this);
}
```

- [ ] **Step 4: 컴파일 후 에디터 재시작**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일하고 **에디터를 껐다 켜줘.** `AGJBaseCharacter`에 새 `UPROPERTY`(`OnCharacterDied`)를 추가해서, 재시작하지 않으면 런타임에 델리게이트가 안 잡힌다.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 5: 사망 로그 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 **적을 하나 죽여줘.**

확인 항목:
- `[DEATH] BP_GJEnemyCharacter_C_0 사망` 같은 줄이 뜬다

**델리게이트가 실제로 도는지는 이 태스크에서 확인할 수 없다** — 구독자가 아직 없다. Task 2에서 방이 구독하면 그때 확인된다. 여기서 확인하는 것은 `HandleDeath`가 실제로 호출된다는 사실뿐이다.

- [ ] **Step 6: 커밋**

```bash
git add Source/Project_GJ/GJBaseCharacter.h Source/Project_GJ/GJBaseCharacter.cpp
git commit -F- <<'EOF'
캐릭터 사망 델리게이트 추가

룸이 자기가 스폰한 적의 전멸을 세려면 죽음을 C++에서 받아야 한다.
기존 OnDeath는 BlueprintImplementableEvent라 구독할 수 없고, 바인딩
가능한 것은 OnDamaged뿐이었다.

살아있는 적을 매초 세는 폴링 대신 델리게이트로 간다. 이 프로젝트는
OnDamaged, OnWeaponSlotsChanged, OnSkillSlotsChanged가 전부
델리게이트라 결이 맞고, 방이 여러 개가 되면 폴링은 비용도 는다.

BP 사망 연출 뒤에 방송한다. 구독자가 델리게이트 안에서 액터를 건드릴
수 있는데, 먼저 방송하면 OnDeath가 이미 정리된 객체 위에서 돈다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
```

---

## Task 2: 방 골격과 적 스폰

방이 테이블 행대로 적을 채우고 전멸을 센다. 출구는 아직 없고 **로그로 검증한다.**

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Create: `Source/Project_GJ/GJRoomSpawnPointComponent.h`, `Source/Project_GJ/GJRoomSpawnPointComponent.cpp`
- Create: `Source/Project_GJ/GJRoomBase.h`, `Source/Project_GJ/GJRoomBase.cpp`
- Create: `Source/Project_GJ/GJCombatRoom.h`, `Source/Project_GJ/GJCombatRoom.cpp`
- Create (에셋): `Content/GJ/DataTables/DT_RoomSpawn`, `Content/GJ/BluePrint/BP_Room_Square`

**Interfaces:**
- Consumes: `AGJBaseCharacter::OnCharacterDied` (Task 1)
- Produces:
  - `ESpawnPointType { Enemy, Item, Chest }`
  - `FRoomSpawnData` (`EnemyPool`, `MinEnemies`, `MaxEnemies`)
  - `UGJRoomSpawnPointComponent::PointType`
  - `AGJRoomBase::SetSpawnRow(FName)`, `OnRoomCleared`, `IsCleared()`
  - `AGJRoomBase::RegisterSpawnedEnemy(AGJEnemyCharacter*)`, `CheckClearedAfterPopulate()`
  - `AGJCombatRoom::GatherPoints(ESpawnPointType)`, `PrepareSpawnPoints(TArray&, int32&)`

- [ ] **Step 1: `GJGameTypes.h`에 열거형과 행 구조체 추가**

`GJGameTypes.h`에서 다음 줄을 찾는다:

```cpp
class AGJProjectile;
```

그 **아래**에 추가한다:

```cpp
class AGJEnemyCharacter;
```

이어서 파일 **맨 끝**(마지막 `};` 다음)에 추가한다:

```cpp

// 방 안에서 무언가가 스폰될 자리의 용도.
UENUM(BlueprintType)
enum class ESpawnPointType : uint8
{
    Enemy   UMETA(DisplayName = "적"),
    Item    UMETA(DisplayName = "아이템"),
    Chest   UMETA(DisplayName = "상자")
};

// 방 하나를 무엇으로 채울지. 행 이름 = 방의 역할(전투/보물/시작).
// 방의 모양은 BP 서브클래스가, 역할은 이 행이 정한다 - 둘을 다 BP에 넣으면
// BP_Square_Combat, BP_Square_Treasure... 로 곱셈으로 늘어난다.
USTRUCT(BlueprintType)
struct FRoomSpawnData : public FTableRowBase
{
    GENERATED_BODY()

    // 이 방에 나올 수 있는 적. 스폰할 때마다 무작위로 하나 고른다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    TArray<TSubclassOf<AGJEnemyCharacter>> EnemyPool;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 MinEnemies = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 MaxEnemies = 5;
};
```

- [ ] **Step 2: `GJRoomSpawnPointComponent.h` 생성**

새 파일 `Source/Project_GJ/GJRoomSpawnPointComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GJGameTypes.h"
#include "GJRoomSpawnPointComponent.generated.h"

// 방 안에서 무언가가 스폰될 자리. 방 BP에 원하는 만큼 꽂고 용도를 드롭다운으로 정한다.
// 용도별로 컴포넌트를 셋으로 나누지 않는 이유: 그러면 점의 용도를 바꿀 때 컴포넌트를
// 지우고 다시 만들어야 한다. 문자열 태그가 아니라 전용 타입인 이유: 오타가 컴파일 타임에 걸린다.
UCLASS(ClassGroup = (GJ), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJRoomSpawnPointComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UGJRoomSpawnPointComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room")
    ESpawnPointType PointType = ESpawnPointType::Enemy;
};
```

- [ ] **Step 3: `GJRoomSpawnPointComponent.cpp` 생성**

새 파일 `Source/Project_GJ/GJRoomSpawnPointComponent.cpp`:

```cpp
#include "GJRoomSpawnPointComponent.h"

UGJRoomSpawnPointComponent::UGJRoomSpawnPointComponent()
{
    // 자리 표시일 뿐이라 틱할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;
}
```

- [ ] **Step 4: `GJRoomBase.h` 생성**

새 파일 `Source/Project_GJ/GJRoomBase.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJRoomBase.generated.h"

class AGJBaseCharacter;
class AGJEnemyCharacter;
class AGJRoomBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomClearedSignature, AGJRoomBase*, Room);

// 방 한 칸의 공통 베이스. 무엇을 채울지는 서브클래스가 정하고, 이 클래스는
// 전멸 추적과 출구 제어만 한다.
//
// 방 종류마다 클래스를 만들지 않는다: 시작방/전투방/보물방은 채우고, 전멸을 세고,
// 문을 여는 일이 똑같고 값만 다르다. 그건 데이터지 동작이 아니다. 동작이 실제로
// 다른 것은 보스방 하나뿐이라(문을 여는 게 아니라 스테이지를 넘긴다) 훅만 열어둔다.
UCLASS(Abstract)
class PROJECT_GJ_API AGJRoomBase : public AActor
{
    GENERATED_BODY()

public:
    AGJRoomBase();

    // 지연 스폰에서 BeginPlay 전에 역할을 정한다. Task B의 던전 생성기가 쓴다:
    // SpawnActorDeferred -> SetSpawnRow -> FinishSpawning.
    UFUNCTION(BlueprintCallable, Category = "Room")
    void SetSpawnRow(FName RowName) { SpawnRowName = RowName; }

    UPROPERTY(BlueprintAssignable, Category = "Room")
    FOnRoomClearedSignature OnRoomCleared;

    UFUNCTION(BlueprintPure, Category = "Room")
    bool IsCleared() const { return bCleared; }

protected:
    virtual void BeginPlay() override;

    // 무엇을 채울지. 베이스는 아무것도 안 한다.
    virtual void PopulateRoom() {}

    // 클리어됐을 때 무엇을 할지. 보스방은 여기서 스테이지를 넘긴다.
    virtual void HandleRoomCleared();

    // 문을 막을지. 통로처럼 막을 일이 없는 방은 false로 오버라이드한다.
    virtual bool ShouldBlockExits() const { return true; }

    // 서브클래스가 적을 스폰한 뒤 이걸로 등록한다. 전멸 추적은 베이스가 한다.
    void RegisterSpawnedEnemy(AGJEnemyCharacter* Enemy);

    // 채우기가 끝난 뒤 반드시 부른다. 적이 0마리면 즉시 클리어로 보낸다.
    void CheckClearedAfterPopulate();

    UFUNCTION()
    void OnSpawnedEnemyDied(AGJBaseCharacter* DeadCharacter);

    // 행 이름 = 방의 역할. 손으로 배치할 때는 디테일 패널에서, 생성기가 놓을 때는
    // SetSpawnRow로 정해진다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    FName SpawnRowName;

    UPROPERTY()
    TArray<AGJEnemyCharacter*> AliveEnemies;

    bool bCleared = false;
};
```

- [ ] **Step 5: `GJRoomBase.cpp` 생성**

새 파일 `Source/Project_GJ/GJRoomBase.cpp`:

```cpp
#include "GJRoomBase.h"
#include "GJBaseCharacter.h"
#include "GJEnemyCharacter.h"
// CreateDefaultSubobject<USceneComponent>에 완전한 타입이 필요하다.
// Actor.h가 전방 선언만 주는 경우가 있어 명시적으로 넣는다.
#include "Components/SceneComponent.h"

AGJRoomBase::AGJRoomBase()
{
    // 방은 아무것도 매 프레임 하지 않는다.
    PrimaryActorTick.bCanEverTick = false;

    // 바닥과 벽은 BP에서 붙인다. C++은 붙일 자리만 준다.
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RoomRoot"));
}

void AGJRoomBase::BeginPlay()
{
    Super::BeginPlay();

    PopulateRoom();
    CheckClearedAfterPopulate();
}

void AGJRoomBase::RegisterSpawnedEnemy(AGJEnemyCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    Enemy->OnCharacterDied.AddDynamic(this, &AGJRoomBase::OnSpawnedEnemyDied);
    AliveEnemies.Add(Enemy);
}

void AGJRoomBase::CheckClearedAfterPopulate()
{
    // 적이 0마리인 방(보물방, 시작방)은 처음부터 열려 있어야 한다.
    // 이걸 빼먹으면 문이 안 열린 채로 굳는다.
    if (AliveEnemies.Num() == 0 && !bCleared)
    {
        HandleRoomCleared();
    }
}

void AGJRoomBase::OnSpawnedEnemyDied(AGJBaseCharacter* DeadCharacter)
{
    AliveEnemies.Remove(Cast<AGJEnemyCharacter>(DeadCharacter));

    if (AliveEnemies.Num() == 0 && !bCleared)
    {
        HandleRoomCleared();
    }
}

void AGJRoomBase::HandleRoomCleared()
{
    bCleared = true;

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s 클리어"), *GetName());

    OnRoomCleared.Broadcast(this);
}
```

- [ ] **Step 6: `GJCombatRoom.h` 생성**

새 파일 `Source/Project_GJ/GJCombatRoom.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GJRoomBase.h"
#include "GJGameTypes.h"
#include "GJCombatRoom.generated.h"

class UDataTable;
class UGJRoomSpawnPointComponent;

// 데이터 테이블 행대로 방을 채우는 방. 전투방/보물방/시작방이 전부 이 클래스이고
// 행만 다르다. 방의 모양은 이 클래스를 상속한 BP가 정한다.
UCLASS()
class PROJECT_GJ_API AGJCombatRoom : public AGJRoomBase
{
    GENERATED_BODY()

protected:
    virtual void PopulateRoom() override;

    void SpawnEnemies(const FRoomSpawnData& Row);

    // 이 방의 스폰 포인트 중 해당 용도인 것만 모은다.
    TArray<UGJRoomSpawnPointComponent*> GatherPoints(ESpawnPointType Type) const;

    // 개수를 점 개수로 자르고 점 배열을 섞는다. 스폰 전에 반드시 거친다.
    static void PrepareSpawnPoints(TArray<UGJRoomSpawnPointComponent*>& Points, int32& InOutCount);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    UDataTable* RoomSpawnTable;
};
```

- [ ] **Step 7: `GJCombatRoom.cpp` 생성**

새 파일 `Source/Project_GJ/GJCombatRoom.cpp`:

```cpp
#include "GJCombatRoom.h"
#include "GJRoomSpawnPointComponent.h"
#include "GJEnemyCharacter.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

void AGJCombatRoom::PopulateRoom()
{
    if (!RoomSpawnTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: RoomSpawnTable이 비어 있습니다."), *GetName());
        return;
    }

    const FRoomSpawnData* Row = RoomSpawnTable->FindRow<FRoomSpawnData>(SpawnRowName, TEXT("PopulateRoom"), false);
    if (!Row)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: '%s' 행을 찾지 못했습니다."),
            *GetName(), *SpawnRowName.ToString());
        return;
    }

    SpawnEnemies(*Row);
}

void AGJCombatRoom::SpawnEnemies(const FRoomSpawnData& Row)
{
    UWorld* World = GetWorld();
    if (!World || Row.EnemyPool.Num() == 0)
    {
        return;
    }

    TArray<UGJRoomSpawnPointComponent*> Points = GatherPoints(ESpawnPointType::Enemy);
    int32 Count = FMath::RandRange(Row.MinEnemies, Row.MaxEnemies);
    PrepareSpawnPoints(Points, Count);

    FActorSpawnParameters Params;
    // 스폰 자리가 살짝 겹쳐도 스폰은 되게 한다. 안 그러면 조용히 아무것도 안 나온다.
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < Count; i++)
    {
        const TSubclassOf<AGJEnemyCharacter> EnemyClass = Row.EnemyPool[FMath::RandRange(0, Row.EnemyPool.Num() - 1)];
        if (!EnemyClass)
        {
            continue;
        }

        AGJEnemyCharacter* Enemy = World->SpawnActor<AGJEnemyCharacter>(
            EnemyClass,
            Points[i]->GetComponentLocation(),
            Points[i]->GetComponentRotation(),
            Params);

        RegisterSpawnedEnemy(Enemy);
    }

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s: 적 %d마리 스폰 (행 '%s')"),
        *GetName(), AliveEnemies.Num(), *SpawnRowName.ToString());
}

TArray<UGJRoomSpawnPointComponent*> AGJCombatRoom::GatherPoints(ESpawnPointType Type) const
{
    TArray<UGJRoomSpawnPointComponent*> All;
    GetComponents<UGJRoomSpawnPointComponent>(All);

    TArray<UGJRoomSpawnPointComponent*> Result;
    for (UGJRoomSpawnPointComponent* Point : All)
    {
        if (Point && Point->PointType == Type)
        {
            Result.Add(Point);
        }
    }

    return Result;
}

void AGJCombatRoom::PrepareSpawnPoints(TArray<UGJRoomSpawnPointComponent*>& Points, int32& InOutCount)
{
    // 테이블이 점보다 많은 수를 요구할 수 있다. 자르지 않으면 인덱스가 넘친다.
    InOutCount = FMath::Clamp(InOutCount, 0, Points.Num());

    // 섞지 않으면 항상 앞쪽 점만 쓰여서 배치가 매번 같아진다.
    for (int32 i = Points.Num() - 1; i > 0; i--)
    {
        Points.Swap(i, FMath::RandRange(0, i));
    }
}
```

- [ ] **Step 8: 컴파일 후 에디터 재시작**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일하고 **에디터를 껐다 켜줘.** 새 `UCLASS` 셋(`UGJRoomSpawnPointComponent`, `AGJRoomBase`, `AGJCombatRoom`)과 새 `USTRUCT`(`FRoomSpawnData`)를 추가해서, 재시작하지 않으면 BP 부모 클래스 목록과 데이터 테이블 구조체 목록에 안 뜬다.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 9: 데이터 테이블과 방 BP 만들기 (사용자 작업)**

사용자에게 요청한다:

> **1. 데이터 테이블**
> `Content/GJ/DataTables`에 `FRoomSpawnData` 구조체로 데이터 테이블을 만들고 이름을 `DT_RoomSpawn`으로 해줘. 행을 하나 추가하고 이름을 `Combat_Basic`으로:
> - `EnemyPool`: `BP_GJEnemyCharacter` 하나 추가
> - `MinEnemies`: 3, `MaxEnemies`: 5
>
> **2. 방 BP**
> `Content/GJ/BluePrint`에 `AGJCombatRoom`을 상속한 BP를 만들고 이름을 `BP_Room_Square`로 해줘.
> - 바닥과 벽을 스태틱 메시로 대충 만든다 (정사각형이면 충분, 1500 x 1500 정도)
> - `UGJRoomSpawnPointComponent`를 **5개** 추가하고 바닥 위에 흩어 놓는다 (`PointType`은 기본값 `Enemy` 그대로)
> - 클래스 디폴트에서 `RoomSpawnTable` = `DT_RoomSpawn`, `SpawnRowName` = `Combat_Basic`
>
> **3. 배치**
> `TestLev`에 `BP_Room_Square`를 하나 놓아줘. 플레이어 시작 위치에서 걸어갈 수 있는 곳이면 된다.

- [ ] **Step 10: 스폰과 전멸 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이해줘. 방에 적이 나오면 **전부 죽여줘.** 그리고 한 번 더 플레이해줘.

확인 항목:
- `[ROOM] BP_Room_Square_C_0: 적 4마리 스폰 (행 'Combat_Basic')` 같은 줄이 뜬다
- 적이 **스폰 포인트 위에 흩어져** 있다 (한 자리에 겹쳐 있지 않다)
- 죽일 때마다 `[DEATH] ...` 로그가 뜬다
- 마지막 적을 죽이면 **`[ROOM] ... 클리어`**가 뜬다
- **두 번째 플레이에서 적 수나 위치가 다르다**

**클리어 로그가 안 뜨면 실패다.** Task 1의 델리게이트가 실제로 도는지가 여기서 처음 확인된다.

### 실행 중 추가된 것: `AGJBoxRoom`

계획에는 사용자가 바닥·벽 큐브를 손으로 놓는 것으로 돼 있었는데, 실행 중에 **파라미터로 지오메트리를 생성하는 `AGJBoxRoom : AGJCombatRoom`**을 추가했다.

**모양을 하드코딩하지 않고 파라미터로 받는 이유**: 하드코딩하면 방 모양마다 C++ 클래스가 생겨서 "모양은 BP, 역할은 데이터"라는 축이 깨진다. `InteriorSize`/`WallHeight`/`WallThickness`/`FloorThickness`/`DoorWidth`를 `EditAnywhere`로 받으면 **새 모양이 새 클래스가 아니라 새 값**이 되고, Task B에서 방을 여러 개 찍어낼 때도 그대로 쓴다.

실제 아트가 들어간 방은 여전히 `AGJCombatRoom`을 상속한 BP로 만든다. 둘은 공존한다.

`OnConstruction`에서 다시 짓기 때문에 에디터에서 값을 바꾸는 즉시 벽이 다시 그려진다. 다시 지을 때 이전 컴포넌트를 먼저 파괴해야 한다 — 안 그러면 파라미터를 고칠 때마다 벽이 겹쳐 쌓인다.

`BlueprintReadWrite`는 붙이지 않았다. 붙이면 BP 그래프에서 런타임에 크기만 바꾸고 `RebuildGeometry`를 안 불러서 숫자와 화면이 어긋난다.

`BP_Room_Square`의 부모를 `AGJCombatRoom` → `AGJBoxRoom`으로 바꿨고, **스폰 포인트 5개와 테이블 설정은 전부 유지됐다.**

- [ ] **Step 11: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJRoomSpawnPointComponent.h Source/Project_GJ/GJRoomSpawnPointComponent.cpp Source/Project_GJ/GJRoomBase.h Source/Project_GJ/GJRoomBase.cpp Source/Project_GJ/GJCombatRoom.h Source/Project_GJ/GJCombatRoom.cpp Content/GJ/DataTables/DT_RoomSpawn.uasset Content/GJ/BluePrint/BP_Room_Square.uasset
git commit -F- <<'EOF'
방 골격과 적 스폰 추가

방이 데이터 테이블 행대로 적을 채우고 전멸을 센다. 이 프로젝트에
적 스폰 시스템이 아예 없었다.

방 종류마다 클래스를 만들지 않는다. 시작방/전투방/보물방은 채우고
전멸을 세고 문을 여는 일이 똑같고 값만 다르다. 동작이 실제로 다른
것은 보스방 하나뿐이라 훅 셋만 열어두고 클래스는 나중에 만든다.

방의 모양과 역할을 다른 축에 뒀다. 모양은 BP 서브클래스, 역할은
테이블 행이다. 둘을 다 BP에 넣으면 BP_Square_Combat,
BP_Square_Treasure로 곱셈으로 늘어난다.

스폰 개수를 점 개수로 자르고 점 배열을 섞는다. 자르지 않으면 인덱스가
넘치고, 섞지 않으면 항상 앞쪽 점만 쓰여서 배치가 매번 같아진다.

적이 0마리면 즉시 클리어로 보낸다. 안 그러면 보물방이 문이 안 열린
채로 굳는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
```

---

## Task 3: 출구 개폐

클리어 전엔 막히고 클리어하면 열린다. **이 컴포넌트는 Task B의 전제이기도 하다** — 던전 생성기가 방을 이으려면 출구의 위치와 방향을 알아야 한다.

**Files:**
- Create: `Source/Project_GJ/GJRoomExitComponent.h`, `Source/Project_GJ/GJRoomExitComponent.cpp`
- Modify: `Source/Project_GJ/GJRoomBase.h`, `Source/Project_GJ/GJRoomBase.cpp`
- Modify (에셋): `Content/GJ/BluePrint/BP_Room_Square`

**Interfaces:**
- Consumes: `AGJRoomBase::ShouldBlockExits()`, `HandleRoomCleared()` (Task 2)
- Produces:
  - `UGJRoomExitComponent::SetBlocked(bool)`
  - `AGJRoomBase::SetExitsBlocked(bool)`

- [ ] **Step 1: `GJRoomExitComponent.h` 생성**

새 파일 `Source/Project_GJ/GJRoomExitComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GJRoomExitComponent.generated.h"

// 방의 출구. 문을 막는 메시와 콜리전은 이 컴포넌트의 자식으로 붙이고,
// C++은 자식 전체의 표시와 콜리전만 토글한다.
//
// Task A에서는 막고 여는 데만 쓰이지만 Task B가 이 위에 세워진다. 던전 생성기는
// 출구의 위치와 전방 방향(GetForwardVector)을 알아야 다음 방을 이어붙일 수 있다.
UCLASS(ClassGroup = (GJ), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJRoomExitComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UGJRoomExitComponent();

    // 막을 때 자식을 보이고 충돌하게, 열 때 숨기고 통과하게 한다.
    UFUNCTION(BlueprintCallable, Category = "Room")
    void SetBlocked(bool bBlocked);
};
```

- [ ] **Step 2: `GJRoomExitComponent.cpp` 생성**

새 파일 `Source/Project_GJ/GJRoomExitComponent.cpp`:

```cpp
#include "GJRoomExitComponent.h"
#include "Components/PrimitiveComponent.h"

UGJRoomExitComponent::UGJRoomExitComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGJRoomExitComponent::SetBlocked(bool bBlocked)
{
    // 표시는 자식까지 전파된다. 문짝 메시는 이 아래에 붙는다.
    SetVisibility(bBlocked, /*bPropagateToChildren=*/true);

    // 콜리전은 전파되지 않으므로 직접 순회한다. 표시만 끄면 보이지 않는 벽이 남는다.
    TArray<USceneComponent*> Children;
    GetChildrenComponents(/*bIncludeAllDescendants=*/true, Children);

    for (USceneComponent* Child : Children)
    {
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
        {
            Prim->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
}
```

- [ ] **Step 3: `GJRoomBase.h`에 출구 제어 선언 추가**

`GJRoomBase.h`에서 다음 줄을 찾는다:

```cpp
    // 채우기가 끝난 뒤 반드시 부른다. 적이 0마리면 즉시 클리어로 보낸다.
    void CheckClearedAfterPopulate();
```

그 **아래**에 추가한다:

```cpp

    // 이 방의 모든 UGJRoomExitComponent를 한꺼번에 막거나 연다.
    void SetExitsBlocked(bool bBlocked);
```

- [ ] **Step 4: `GJRoomBase.cpp`에 출구 제어 구현 추가**

`GJRoomBase.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJRoomExitComponent.h"
```

이어서 `BeginPlay`를 다음으로 교체한다:

```cpp
void AGJRoomBase::BeginPlay()
{
    Super::BeginPlay();

    // 채우기보다 먼저 막는다. 적 0마리 방은 채우기 끝에 즉시 클리어되면서 다시 열리는데,
    // 순서가 반대면 열린 뒤에 막혀서 영구히 갇힌다.
    if (ShouldBlockExits())
    {
        SetExitsBlocked(true);
    }

    PopulateRoom();
    CheckClearedAfterPopulate();
}
```

이어서 `HandleRoomCleared`를 다음으로 교체한다:

```cpp
void AGJRoomBase::HandleRoomCleared()
{
    bCleared = true;

    SetExitsBlocked(false);

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s 클리어 - 출구 개방"), *GetName());

    OnRoomCleared.Broadcast(this);
}
```

마지막으로 파일 **맨 끝**에 추가한다:

```cpp

void AGJRoomBase::SetExitsBlocked(bool bBlocked)
{
    TArray<UGJRoomExitComponent*> Exits;
    GetComponents<UGJRoomExitComponent>(Exits);

    for (UGJRoomExitComponent* Exit : Exits)
    {
        if (Exit)
        {
            Exit->SetBlocked(bBlocked);
        }
    }
}
```

- [ ] **Step 5: 컴파일 후 에디터 재시작**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일하고 **에디터를 껐다 켜줘.** 새 `UCLASS`(`UGJRoomExitComponent`)를 추가해서, 재시작하지 않으면 컴포넌트 추가 목록에 안 뜬다.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 6: 방 BP에 출구 추가 (사용자 작업)**

사용자에게 요청한다:
> `BP_Room_Square`를 열어서 벽 한 곳에 문 자리를 뚫고 출구를 만들어줘.
>
> - `UGJRoomExitComponent`를 하나 추가하고 그 문 자리에 놓는다. **전방(빨간 화살표)이 방 바깥을 향하게** 회전시켜줘 — Task B에서 이 방향으로 다음 방을 잇는다.
> - 그 컴포넌트의 **자식으로** 스태틱 메시를 하나 붙여서 문구멍을 막는다(콜리전 있는 아무 박스면 된다).

- [ ] **Step 7: 개폐 확인**

사용자에게 요청한다:
> 플레이해서 **문이 막혀 있는지** 확인하고(통과 시도), 적을 전부 죽인 뒤 **문이 사라지고 지나갈 수 있는지** 확인해줘.

확인 항목:
- 시작할 때 문이 보이고 **통과할 수 없다**
- 전멸시키면 `[ROOM] ... 클리어 - 출구 개방`이 뜬다
- 문이 **사라지고 실제로 지나갈 수 있다** (보이지 않는 벽이 남지 않는다)

**문은 사라졌는데 안 지나가지면 실패다** — 콜리전 순회가 안 먹었다는 뜻이다.

- [ ] **Step 8: 커밋**

```bash
git add Source/Project_GJ/GJRoomExitComponent.h Source/Project_GJ/GJRoomExitComponent.cpp Source/Project_GJ/GJRoomBase.h Source/Project_GJ/GJRoomBase.cpp Content/GJ/BluePrint/BP_Room_Square.uasset
git commit -F- <<'EOF'
방 출구 개폐 추가

클리어 전엔 막히고 전멸시키면 열린다. 문짝 메시는 출구 컴포넌트의
자식으로 붙이고 C++은 표시와 콜리전만 토글한다. 문 연출은 BP가 맡는
구조라 OnDeath를 비워두고 BP가 사망 연출을 맡는 것과 같다.

표시는 자식까지 전파되지만 콜리전은 전파되지 않아서 직접 순회한다.
표시만 끄면 보이지 않는 벽이 남는다.

막기를 채우기보다 먼저 한다. 적 0마리 방은 채우기 끝에 즉시
클리어되면서 다시 열리는데, 순서가 반대면 열린 뒤에 막혀서 영구히
갇힌다.

출구를 컴포넌트 타입으로 만든 것은 선행 투자가 아니라 다음 단계의
전제다. 던전 생성기는 출구의 위치와 전방 방향을 알아야 방을 잇는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
```

---

## Task 4: 아이템과 보물 상자

방 바닥에 아이템이 깔리고, 확률대로 상자가 나온다.

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Create: `Source/Project_GJ/GJTreasureChest.h`, `Source/Project_GJ/GJTreasureChest.cpp`
- Modify: `Source/Project_GJ/GJCombatRoom.h`, `Source/Project_GJ/GJCombatRoom.cpp`
- Create (에셋): `Content/GJ/BluePrint/BP_Chest_Basic`
- Modify (에셋): `Content/GJ/DataTables/DT_RoomSpawn`, `Content/GJ/BluePrint/BP_Room_Square`

**Interfaces:**
- Consumes: `AGJCombatRoom::GatherPoints`, `PrepareSpawnPoints` (Task 2)
- Produces:
  - `AGJTreasureChest` (`Contents`, `MinDrops`, `MaxDrops`, `DropRadius`, `OnChestOpened`)
  - `FRoomSpawnData::ItemPool`, `MinItems`, `MaxItems`, `ChestPool`, `ChestChance`

- [ ] **Step 1: `GJTreasureChest.h` 생성**

새 파일 `Source/Project_GJ/GJTreasureChest.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJInteractable.h"
#include "GJTreasureChest.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AGJCharacter;

// 보물 상자. E로 한 번만 열리고 내용물을 바닥에 뿌린다.
//
// 인벤토리에 직접 넣지 않는 이유: 인벤토리가 꽉 찼을 때 아이템이 증발한다.
// 바닥에 떨어뜨리면 기존 습득 흐름(AGJItem::PickUp - 칸이 모자라면 필드에 남음)을
// 그대로 타서 새 경로가 하나도 안 생긴다.
UCLASS()
class PROJECT_GJ_API AGJTreasureChest : public AActor, public IGJInteractable
{
    GENERATED_BODY()

public:
    AGJTreasureChest();

    // IGJInteractable - 실제로 범위 안일 때만 열린다 (AGJItemBase와 같은 패턴)
    virtual void Interact_Implementation(AGJCharacter* Interactor) override;

protected:
    // 여는 연출은 BP가 맡는다. 상자 메시와 애니메이션은 에디터 작업이다.
    UFUNCTION(BlueprintImplementableEvent, Category = "Chest")
    void OnChestOpened();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* ChestMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionCollision;

    // 열었을 때 바닥에 뿌릴 것. AGJItem BP도, 무기 BP도 들어간다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    TArray<TSubclassOf<AActor>> Contents;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    int32 MinDrops = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    int32 MaxDrops = 2;

    // 뿌릴 반경. 같은 자리에 겹치면 하나만 있는 것처럼 보인다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chest")
    float DropRadius = 120.f;

    bool bOpened = false;
};
```

- [ ] **Step 2: `GJTreasureChest.cpp` 생성**

새 파일 `Source/Project_GJ/GJTreasureChest.cpp`:

```cpp
#include "GJTreasureChest.h"
#include "GJCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AGJTreasureChest::AGJTreasureChest()
{
    PrimaryActorTick.bCanEverTick = false;

    ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
    RootComponent = ChestMesh;

    // AGJItemBase와 같은 구조 - 상호작용 범위는 메시 충돌과 별개로 넉넉하게 둔다.
    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(RootComponent);
    InteractionCollision->SetSphereRadius(150.f);
    InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void AGJTreasureChest::Interact_Implementation(AGJCharacter* Interactor)
{
    if (bOpened || !Interactor)
    {
        return;
    }

    // 플레이어의 상호작용 입력은 겹친 액터를 찾아 Interact만 부른다.
    // "지금 정말 범위 안인지"는 각 구현부가 스스로 판단한다.
    if (!InteractionCollision->IsOverlappingActor(Interactor))
    {
        return;
    }

    bOpened = true;

    UWorld* World = GetWorld();
    if (World && Contents.Num() > 0)
    {
        const int32 DropCount = FMath::Max(FMath::RandRange(MinDrops, MaxDrops), 0);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        for (int32 i = 0; i < DropCount; i++)
        {
            const TSubclassOf<AActor> DropClass = Contents[FMath::RandRange(0, Contents.Num() - 1)];
            if (!DropClass)
            {
                continue;
            }

            // 원형으로 흩뿌린다. 겹쳐 놓으면 하나만 있는 것처럼 보인다.
            const float Angle = 2.f * PI * i / FMath::Max(DropCount, 1);
            const FVector Offset(FMath::Cos(Angle) * DropRadius, FMath::Sin(Angle) * DropRadius, 0.f);

            World->SpawnActor<AActor>(DropClass, GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
        }

        UE_LOG(LogTemp, Log, TEXT("[CHEST] %s 열림 - %d개 드랍"), *GetName(), DropCount);
    }

    OnChestOpened();
}
```

- [ ] **Step 3: `FRoomSpawnData`에 아이템·상자 필드 추가**

`GJGameTypes.h`에서 다음 줄을 찾는다:

```cpp
class AGJEnemyCharacter;
```

이를 다음으로 교체한다:

```cpp
class AGJEnemyCharacter;
class AGJTreasureChest;
```

이어서 `FRoomSpawnData`에서 다음 두 줄을 찾는다:

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 MaxEnemies = 5;
```

그 **아래**에 추가한다:

```cpp

    // 바닥에 놓일 것. AGJItem BP도, 무기 BP도 들어간다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    TArray<TSubclassOf<AActor>> ItemPool;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 MinItems = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    int32 MaxItems = 1;

    // 나올 수 있는 상자. 풀로 둔 것은 나중에 희귀 상자를 추가할 때
    // 스키마를 안 건드리기 위해서다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    TArray<TSubclassOf<AGJTreasureChest>> ChestPool;

    // 상자가 나올 확률(0~1). 개수가 아니라 확률인 이유: 보물은 "몇 개 나오나"가 아니라
    // "나오나 마나"다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Room")
    float ChestChance = 0.f;
```

- [ ] **Step 4: `GJCombatRoom.h`에 스폰 함수 선언 추가**

`GJCombatRoom.h`에서 다음 줄을 찾는다:

```cpp
    void SpawnEnemies(const FRoomSpawnData& Row);
```

그 **아래**에 추가한다:

```cpp

    void SpawnItems(const FRoomSpawnData& Row);
    void SpawnChest(const FRoomSpawnData& Row);
```

- [ ] **Step 5: `GJCombatRoom.cpp`에 스폰 구현 추가**

`GJCombatRoom.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJTreasureChest.h"
```

이어서 `PopulateRoom` 안에서 다음 줄을 찾는다:

```cpp
    SpawnEnemies(*Row);
}
```

이를 다음으로 교체한다:

```cpp
    SpawnEnemies(*Row);
    SpawnItems(*Row);
    SpawnChest(*Row);
}
```

이어서 파일 **맨 끝**에 추가한다:

```cpp

void AGJCombatRoom::SpawnItems(const FRoomSpawnData& Row)
{
    UWorld* World = GetWorld();
    if (!World || Row.ItemPool.Num() == 0)
    {
        return;
    }

    TArray<UGJRoomSpawnPointComponent*> Points = GatherPoints(ESpawnPointType::Item);
    int32 Count = FMath::RandRange(Row.MinItems, Row.MaxItems);
    PrepareSpawnPoints(Points, Count);

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 i = 0; i < Count; i++)
    {
        const TSubclassOf<AActor> ItemClass = Row.ItemPool[FMath::RandRange(0, Row.ItemPool.Num() - 1)];
        if (!ItemClass)
        {
            continue;
        }

        World->SpawnActor<AActor>(ItemClass, Points[i]->GetComponentLocation(), Points[i]->GetComponentRotation(), Params);
    }

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s: 아이템 %d개 배치"), *GetName(), Count);
}

void AGJCombatRoom::SpawnChest(const FRoomSpawnData& Row)
{
    UWorld* World = GetWorld();
    if (!World || Row.ChestPool.Num() == 0 || FMath::FRand() >= Row.ChestChance)
    {
        return;
    }

    TArray<UGJRoomSpawnPointComponent*> Points = GatherPoints(ESpawnPointType::Chest);
    int32 Count = 1;
    PrepareSpawnPoints(Points, Count);

    if (Count == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: 상자가 나올 차례인데 Chest 스폰 포인트가 없습니다."), *GetName());
        return;
    }

    const TSubclassOf<AGJTreasureChest> ChestClass = Row.ChestPool[FMath::RandRange(0, Row.ChestPool.Num() - 1)];
    if (!ChestClass)
    {
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    World->SpawnActor<AGJTreasureChest>(ChestClass, Points[0]->GetComponentLocation(), Points[0]->GetComponentRotation(), Params);

    UE_LOG(LogTemp, Log, TEXT("[ROOM] %s: 상자 배치"), *GetName());
}
```

- [ ] **Step 6: 컴파일 후 에디터 재시작**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일하고 **에디터를 껐다 켜줘.** 새 `UCLASS`(`AGJTreasureChest`)와 `FRoomSpawnData`의 새 필드 다섯 개를 추가해서, 재시작하지 않으면 데이터 테이블에 칼럼이 안 뜬다(M2.7에서 겪은 것과 같은 증상).

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 7: 상자 BP와 테이블 값 채우기 (사용자 작업)**

사용자에게 요청한다:

> **1. 상자 BP**
> `Content/GJ/BluePrint`에 `AGJTreasureChest`를 상속한 BP를 만들고 이름을 `BP_Chest_Basic`으로 해줘.
> - `ChestMesh`에 아무 상자 같은 스태틱 메시(큐브도 됨)를 물린다
> - `Contents`에 기존 아이템 BP(`BP_Item_XXX`)를 하나 이상 넣는다. 없으면 `BP_GJWeapon_Ranged`를 넣어도 된다
>
> **2. 방 BP**
> `BP_Room_Square`에 `UGJRoomSpawnPointComponent`를 **3개 더** 추가해줘.
> - 2개는 `PointType` = `아이템`
> - 1개는 `PointType` = `상자`
>
> **3. 데이터 테이블**
> `DT_RoomSpawn`의 `Combat_Basic` 행에 값을 채워줘.
> - `ItemPool`: 아이템 BP 하나
> - `MinItems`: 1, `MaxItems`: 2
> - `ChestPool`: `BP_Chest_Basic`
> - `ChestChance`: **1.0** (검증용. 확인 끝나면 0.3 정도로 낮춘다)

- [ ] **Step 8: 아이템과 상자 확인**

사용자에게 요청한다:
> 플레이해서 **바닥의 아이템을 주워보고**, **상자를 E로 열어줘.** 열린 상자에 다시 E를 눌러보고, 그다음 한 번 더 플레이해줘.

확인 항목:
- 아이템이 아이템 스폰 포인트에 놓여 있고 **주울 수 있다**
- 상자가 상자 스폰 포인트에 있다
- E로 열면 `[CHEST] ... 열림 - 2개 드랍`이 뜨고 **주변에 아이템이 흩어진다**
- **두 번째 E는 아무 일도 안 일어난다** (로그도 안 뜬다)
- 다시 플레이하면 아이템 수나 종류가 다르다

확인이 끝나면 `ChestChance`를 **0.3**으로 낮춘다.

- [ ] **Step 9: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJTreasureChest.h Source/Project_GJ/GJTreasureChest.cpp Source/Project_GJ/GJCombatRoom.h Source/Project_GJ/GJCombatRoom.cpp Content/GJ/BluePrint/BP_Chest_Basic.uasset Content/GJ/BluePrint/BP_Room_Square.uasset Content/GJ/DataTables/DT_RoomSpawn.uasset
git commit -F- <<'EOF'
방에 아이템과 보물 상자 추가

바닥에 아이템이 깔리고 확률대로 상자가 나온다. 적과 같은 방식으로
개수를 범위에서 뽑고 점을 섞어 배치한다.

상자는 인벤토리에 직접 넣지 않고 아이템 액터를 바닥에 뿌린다. 직접
넣으면 인벤토리가 꽉 찼을 때 아이템이 증발한다. 바닥에 떨어뜨리면
기존 습득 흐름을 그대로 타서 새 경로가 안 생긴다.

원형으로 흩뿌린다. 같은 자리에 겹치면 하나만 있는 것처럼 보인다.

상자만 개수가 아니라 확률이다. 보물은 몇 개 나오나가 아니라 나오나
마나다. 풀로 둔 것은 나중에 희귀 상자를 추가할 때 스키마를 안
건드리기 위해서다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
```

---

## Task 5: 개발 가이드 갱신

**Files:**
- Modify: `Docs/DevGuide.md`
- Modify: `DevGuide.html`

- [ ] **Step 1: 새 절 추가**

`Docs/DevGuide.md`의 `## 7. UI / 위젯` **바로 앞**에 `## 6.9 룸 시스템` 절을 추가한다. 담을 내용:

- **클래스 구조**: `AGJRoomBase`(전멸 추적·출구 제어·훅 셋) → `AGJCombatRoom`(테이블 행대로 채움) → BP(모양). `AGJBossRoom`은 Task C 자리
- **방 종류마다 클래스를 만들지 않는 이유**: 시작방/전투방/보물방은 하는 일이 똑같고 값만 다르다. 동작이 실제로 다른 것은 보스방 하나뿐(문을 여는 게 아니라 스테이지를 넘긴다)
- **모양과 역할은 다른 축**: 모양=BP 서브클래스, 역할=`DT_RoomSpawn` 행. 둘 다 BP에 넣으면 곱셈으로 늘어난다
- **`OnCharacterDied`를 새로 만든 이유**: `OnDeath`가 `BlueprintImplementableEvent`라 C++에서 구독할 수 없다. BP 연출 뒤에 방송한다
- **채우기 규칙**: 개수를 점 개수로 clamp하고 점을 섞는다 — 안 하면 인덱스가 넘치거나 배치가 매번 같아진다
- **적 0마리 방은 즉시 클리어**: 안 하면 보물방이 문이 안 열린 채로 굳는다
- **막기가 채우기보다 먼저**: 순서가 반대면 적 0마리 방이 열린 뒤에 막혀서 영구히 갇힌다
- **출구 콜리전은 직접 순회**: 표시는 자식까지 전파되지만 콜리전은 안 된다. 표시만 끄면 보이지 않는 벽이 남는다
- **상자가 인벤토리에 직접 안 넣는 이유**: 꽉 찼을 때 증발한다. 바닥에 뿌리면 기존 습득 흐름을 탄다
- **`UGJRoomExitComponent`는 Task B의 전제**: 생성기가 출구 위치·방향으로 방을 잇는다
- **`SetSpawnRow`는 지연 스폰용**: `SpawnActorDeferred` → `SetSpawnRow` → `FinishSpawning`

- [ ] **Step 2: 8절에 데이터 테이블 스키마 추가**

`Docs/DevGuide.md` 8절의 `### FSkillData` 항목 **아래**에 `### FRoomSpawnData — DT_RoomSpawn (행 이름 = 방의 역할)` 항목을 추가하고 필드 8개(`EnemyPool`, `MinEnemies`, `MaxEnemies`, `ItemPool`, `MinItems`, `MaxItems`, `ChestPool`, `ChestChance`)를 기존 항목들과 같은 표 형식으로 적는다.

- [ ] **Step 3: 9절 TODO 갱신**

다음 항목을 **삭제한다**:

```markdown
- 런은 **사망으로만** 끝남 — 클리어(승리) 조건이 없음 (M5)
```

같은 목록 끝에 다음을 **추가한다**:

```markdown
- 방이 하나뿐이고 손으로 배치해야 한다 — 절차적 배치(Task B)가 아직 없다
- **"처음부터 깔림"의 대가가 Task B에서 드러난다**: 던전 전체가 한 번에 채워지면 먼 방의 적도 처음부터 살아서 플레이어를 향해 길찾기를 한다. "플레이어가 일정 거리 밖이면 AI를 꺼둔다" 정도로 싸게 막아야 한다
- 스테이지 진행과 런 클리어가 없다 (Task C) — `AGJBossRoom`이 `HandleRoomCleared`를 오버라이드할 자리만 비어 있다
- 회복 아이템과 대가 지불 아이템이 없다 (A2) — 회복방·보급방은 이것들이 생기면 테이블 행 추가만으로 성립한다
- 상점은 화폐 시스템이 선행한다. 바닥에 아이템을 까는 대신 **상점 NPC와 상호작용**하는 구조로 가기로 했다
- `DT_RoomSpawn`의 값은 **임시 테스트 값**이다
```

- [ ] **Step 4: `DevGuide.html`에 동일 내용 반영**

Step 1~3의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다.

반영 후 태그 균형을 확인한다:

Run:
```bash
python -c "
import re
html = open('DevGuide.html', encoding='utf-8').read()
bad = 0
for tag in ['table','ul','pre','div','p','li','tr','td','th','code','h2','h3','b','span']:
    o = len(re.findall(r'<'+tag+r'[ >]', html)); c = len(re.findall(r'</'+tag+r'>', html))
    if o != c:
        bad += 1
        print(f'{tag}: {o} vs {c}  MISMATCH')
print('OK' if bad==0 else f'{bad} MISMATCH')
"
```
Expected: `OK`

- [ ] **Step 5: 커밋**

```bash
git add Docs/DevGuide.md DevGuide.html
git commit -F- <<'EOF'
개발 가이드에 룸 시스템 반영

6.9절을 추가하고 방 종류마다 클래스를 만들지 않은 이유, 모양과 역할을
다른 축에 둔 이유를 적었다.

OnCharacterDied를 새로 만든 이유와 BP 연출 뒤에 방송하는 이유를
남겼다.

구현 중 걸릴 만한 것 셋을 적었다. 막기가 채우기보다 먼저여야 하는
이유, 출구 콜리전을 직접 순회해야 하는 이유, 적 0마리 방을 즉시
클리어로 보내야 하는 이유다.

9절에서 "런이 사망으로만 끝남"을 지우고 남은 갭을 적었다. 특히
처음부터 깔림의 대가가 Task B에서 드러난다는 것을 남겼다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
```
