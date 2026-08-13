# 카드 선택 시스템 (M2.6) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 레벨업하면 카드 3장이 뜨고, 하나를 고르면 그 효과가 이번 런 동안 적용된다.

**Architecture:** 로직 전부를 새 `UGJCardComponent`에 넣고 `AGJCharacter` 생성자에서 붙인다. 캐릭터는 `OnLevelUp`을 쏘기만 하고 카드를 모른다. 선택 UI는 "카드"가 아니라 "선택지 목록"을 표시하고 **인덱스만** 돌려주므로, 카드 선택과 무기 교체가 같은 위젯을 재사용한다.

**Tech Stack:** UE 5.8, C++ (`UActorComponent`, `FTableRowBase`, `UFUNCTION(Exec)`, UMG `BindWidget`, `UGameplayStatics::SetGamePaused`)

**설계 문서:** `Docs/superpowers/specs/2026-08-13-card-selection-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다. 각 태스크의 검증은 자동화 테스트가 아니라 구체적인 PIE 조작 시나리오로 기술한다.
- **캐릭터는 카드를 모른다.** 카드 관련 상태·로직은 전부 `UGJCardComponent`에 둔다. `AGJCharacter`에 추가하는 것은 컴포넌트 생성 한 줄과 무기 교체 API 하나뿐이다.
- **위젯은 `FCardData`를 모른다.** `FGJChoiceEntry`(이름/설명/아이콘) 목록을 받고 선택 **인덱스**를 돌려준다. 인덱스의 의미는 컴포넌트가 해석한다.
- **일시정지·입력 모드는 인벤토리 모달 패턴을 그대로 따른다**(`GJCharacter.cpp`의 `ToggleInventory`, 165~203번째 줄): 열 때 `SetGamePaused(true)` + `FInputModeUIOnly` + `SetWidgetToFocus` + `bIsAutoFiring=false`, 닫을 때 `SetGamePaused(false)` + `FInputModeGameOnly` + `SetConsumeCaptureMouseDown(false)`. 셋 다 없으면 각각 무한 연사, 포커스 이탈, 첫 클릭 씹힘이 생겼던 이력이 있다.
- **소프트락 금지.** 위젯 생성이 실패하면 일시정지를 걸지 않고 경고 후 대기열을 비운다. 화면은 안 뜨는데 게임만 멈추는 상태가 가장 추적하기 어렵다.
- **새 `UCLASS`는 에디터 재시작이 필요할 수 있다.** 라이브 코딩만으로는 위젯 블루프린트의 부모 클래스 목록에 새 클래스가 안 뜨는 경우가 있다(개발 가이드 10절).
- **인코딩**: 새 주석은 UTF-8 한글로 그냥 쓴다. 초기 파일의 깨진 옛 주석 줄은 건드리지 않는다.
- **커밋 메시지는 한국어**로 쓰고 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다. 브랜치를 나누지 않고 `main`에서 작업한다.
- 에디터가 열려 있으면 UBT 빌드가 막힌다. 컴파일은 사용자에게 **Ctrl+Alt+F11**을 요청한다.
- **MCP(포트 8123)로 데이터 테이블과 위젯을 만들 수 있다.** 실패하면 사용자에게 에디터 작업을 요청한다.

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJGameTypes.h` (수정) | `ECardEffectType`, `FCardData`, `FGJChoiceEntry` | 1 |
| `Source/Project_GJ/GJCharacter.h/.cpp` (수정) | `ReplaceWeaponInSlot` 공개 API, 컴포넌트 생성 | 1, 2 |
| `Source/Project_GJ/GJWeaponBase.h` (수정) | `GetWeaponRowName` getter | 1 |
| `Source/Project_GJ/GJCardComponent.h/.cpp` (신규) | 카드 풀·뽑기·대기열·효과 적용·UI 제어 | 2, 4 |
| `Source/Project_GJ/GJCardWidget.h/.cpp` (신규) | 선택지 하나(아이콘/이름/설명/버튼) | 3 |
| `Source/Project_GJ/GJCardSelectWidget.h/.cpp` (신규) | 선택지 N개를 늘어놓고 인덱스를 돌려줌 | 3 |
| `Content/GJ/DataTables/DT_CardData` (신규 에셋) | 카드 정의 | 2 |
| `Content/GJ/UI/WBP_Card`, `WBP_CardSelect` (신규 에셋) | 위젯 | 3 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 5 |

**태스크 순서 근거**: 데이터 그릇(1) → 뽑기 로직과 콘솔 검증(2) → 화면(3) → 흐름 연결과 전체 검증(4) → 문서(5). Task 2가 끝나면 **UI 없이도 콘솔 로그로 뽑기가 검증**되고, Task 3이 끝나면 화면이 뜨며, Task 4가 끝나면 실제로 플레이된다.

---

## Task 1: 데이터 구조와 캐릭터/무기 API

동작 변화 없음. 검증은 컴파일 통과다.

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Modify: `Source/Project_GJ/GJCharacter.h`, `Source/Project_GJ/GJCharacter.cpp`
- Modify: `Source/Project_GJ/GJWeaponBase.h`

**Interfaces:**
- Consumes: `FStatModifier` (M2.5)
- Produces:
  - `enum class ECardEffectType : uint8 { StatBonus, GrantWeapon, Ability }`
  - `FCardData : public FTableRowBase` — `DisplayName`, `Description`, `Icon`, `EffectType`, `StatEffect`, `WeaponClass`, `bStackable`, `Weight`
  - `FGJChoiceEntry` — `DisplayName`, `Description`, `Icon`
  - `bool AGJCharacter::ReplaceWeaponInSlot(int32 SlotIndex, AGJWeaponBase* NewWeapon)` (`public`)
  - `FName AGJWeaponBase::GetWeaponRowName() const` (`public`)

- [ ] **Step 1: `GJGameTypes.h`에 전방 선언 추가**

`GJGameTypes.h` 상단에서 다음 줄을 찾는다(5번째 줄):

```cpp
#include "GJGameTypes.generated.h" // 이름 맞춰주기
```

이를 다음으로 교체한다:

```cpp
#include "GJGameTypes.generated.h" // 이름 맞춰주기

// FCardData가 TSubclassOf로만 참조하므로 전방 선언으로 충분하다.
// 여기서 GJWeaponBase.h를 include하면 GJWeaponBase.h가 GJGameTypes.h를 다시 include해서
// 순환이 된다.
class AGJWeaponBase;
```

- [ ] **Step 2: `GJGameTypes.h`에 카드 타입 추가**

`GJGameTypes.h`에서 `FStatModifier` 구조체의 닫는 줄과 그 다음 섹션 주석을 찾는다:

```cpp
    // 증가율 (0.15 = +15%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Percent;
};

// -----------------------------------------
// 무기 스탯 데이터 테이블 구조체
// -----------------------------------------
```

이를 다음으로 교체한다:

```cpp
    // 증가율 (0.15 = +15%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Percent;
};

// -----------------------------------------
// 카드 (레벨업 선택지)
// -----------------------------------------

UENUM(BlueprintType)
enum class ECardEffectType : uint8
{
    // StatEffect를 AddStatBonus로 넘긴다
    StatBonus   UMETA(DisplayName = "스탯 보너스"),
    // WeaponClass를 스폰해서 지급한다
    GrantWeapon UMETA(DisplayName = "무기 획득"),
    // 미구현 - 스킬 시스템(M2.7)이 생기기 전까지는 골라도 경고만 찍힌다
    Ability     UMETA(DisplayName = "능력 획득 (미구현)")
};

// 카드 한 장의 정의. 행 이름이 곧 카드 ID다.
USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    UTexture2D* Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    ECardEffectType EffectType = ECardEffectType::StatBonus;

    // EffectType == StatBonus일 때만 쓰인다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FStatModifier StatEffect;

    // EffectType == GrantWeapon일 때만 쓰인다
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    TSubclassOf<AGJWeaponBase> WeaponClass;

    // false면 한 번 고른 뒤 풀에서 영구 제외된다(무기나 고유 효과용).
    // true면 여러 번 등장할 수 있어 "같은 카드를 쌓아 빌드를 밀어붙이는" 플레이가 가능하다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    bool bStackable = true;

    // 가중 랜덤의 가중치. 0 이하면 절대 안 뽑힌다(카드를 임시로 끄는 용도로도 쓸 수 있다).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    float Weight = 1.f;
};

// 선택지 위젯에 넘기는 표시용 데이터.
// 위젯이 FCardData를 직접 알면 무기 교체 화면을 따로 만들어야 하므로, 표시에 필요한
// 것만 담은 이 구조체로 한 겹 끊는다.
USTRUCT(BlueprintType)
struct FGJChoiceEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    FText Description;

    UPROPERTY(BlueprintReadOnly, Category = "Choice")
    UTexture2D* Icon = nullptr;
};

// -----------------------------------------
// 무기 스탯 데이터 테이블 구조체
// -----------------------------------------
```

- [ ] **Step 3: `GJWeaponBase.h`에 행 이름 getter 추가**

`GJWeaponBase.h`에서 다음 줄을 찾는다(67번째 줄 부근):

```cpp
    FORCEINLINE FWeaponStat GetWeaponStat() const { return WeaponStat; }
```

그 **아래**에 추가한다:

```cpp

    // 무기 교체 UI에 표시할 이름. FWeaponStat에는 이름 필드가 없고 데이터 테이블의
    // 행 이름이 곧 무기 ID라서 그걸 그대로 쓴다.
    UFUNCTION(BlueprintPure, Category = "Weapon Stat")
    FName GetWeaponRowName() const { return WeaponDataHandle.RowName; }
```

- [ ] **Step 4: `GJCharacter.h`에 `ReplaceWeaponInSlot` 선언 추가**

`GJCharacter.h`에서 다음 두 줄을 찾는다(353번째 줄 부근):

```cpp
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool PickUpWeapon(AGJWeaponBase* NewWeapon);
```

그 **아래**에 추가한다:

```cpp

    // 지정한 슬롯의 무기를 버리고 그 자리에 새 무기를 넣는다.
    // 카드로 무기를 받을 때 "어느 무기를 버릴지" 고른 결과를 적용하는 경로다.
    // DropWeapon을 그냥 public으로 여는 대신 이 함수를 두는 이유: "버리고 넣는" 두 동작의
    // 순서가 맞아야 플레이어가 고른 슬롯에 정확히 들어가는데, 그 순서를 호출자마다
    // 기억하게 하면 언젠가 틀린다.
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool ReplaceWeaponInSlot(int32 SlotIndex, AGJWeaponBase* NewWeapon);
```

- [ ] **Step 5: `GJCharacter.cpp`에 `ReplaceWeaponInSlot` 구현 추가**

`GJCharacter.cpp`에서 `bool AGJCharacter::PickUpWeapon(AGJWeaponBase* NewWeapon)` 함수의 닫는 `}`를 찾아, 그 **아래**에 추가한다:

```cpp

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
```

- [ ] **Step 6: 컴파일**

사용자에게 요청한다:
> 에디터에서 **Ctrl+Alt+F11**로 컴파일해줘. 새 `USTRUCT`/`UENUM`을 추가했으니 UHT가 먼저 돈다.

Run: `grep -E "error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

이 태스크는 아무도 새 타입을 안 쓰므로 PIE로 확인할 것이 없다. **컴파일 통과가 검증이다.**

- [ ] **Step 7: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJWeaponBase.h Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp
git commit -m "$(cat <<'EOF'
카드 데이터 구조와 무기 교체 API 추가

FCardData(행 이름 = 카드 ID)와 효과 종류 enum, 그리고 선택지 위젯에
넘길 표시용 구조체 FGJChoiceEntry를 추가했다. 아직 아무도 쓰지 않아서
동작 변화는 없다.

무기 교체를 위해 AGJCharacter::ReplaceWeaponInSlot과
AGJWeaponBase::GetWeaponRowName을 열었다. DropWeapon을 그냥 public으로
열지 않은 이유는 "버리고 넣는" 순서가 맞아야 플레이어가 고른 슬롯에
들어가기 때문이다 - 순서를 한 곳에 가둔다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: 카드 컴포넌트와 뽑기 로직

이 태스크가 끝나면 **UI 없이 콘솔 로그로 뽑기가 검증된다.**

**Files:**
- Create: `Source/Project_GJ/GJCardComponent.h`, `Source/Project_GJ/GJCardComponent.cpp`
- Modify: `Source/Project_GJ/GJCharacter.h`, `Source/Project_GJ/GJCharacter.cpp` (컴포넌트 생성)
- Create (에셋): `Content/GJ/DataTables/DT_CardData`

**Interfaces:**
- Consumes: `FCardData`, `ECardEffectType` (Task 1), `AGJCharacter::GetWeaponInSlot`
- Produces:
  - `UGJCardComponent` — `CardTable`, `NumCardsToDraw`, `BonusCardSlots`, `ExtraCardChance`, `TakenCards`
  - `int32 UGJCardComponent::GetDrawCount() const`
  - `TArray<FName> UGJCardComponent::DrawCards(int32 Count) const`
  - `void UGJCardComponent::GJDrawCards()` (`UFUNCTION(Exec)`)
  - `AGJCharacter::CardComponent` (`protected` 멤버) + `GetCardComponent()`

- [ ] **Step 1: `GJCardComponent.h` 생성**

새 파일 `Source/Project_GJ/GJCardComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GJGameTypes.h"
#include "GJCardComponent.generated.h"

class AGJCharacter;

// 컴포넌트가 지금 무엇을 묻고 있는지. 선택지 위젯은 인덱스만 돌려주므로,
// 그 인덱스가 "뽑힌 카드 목록의 위치"인지 "버릴 무기 슬롯 번호"인지는 이 상태로 판단한다.
// UENUM이 아닌 이유: 리플렉션에 노출할 필요가 없다(블루프린트도 UI도 이 값을 안 본다).
enum class EGJChoiceMode : uint8
{
    None,
    Card,
    WeaponReplace
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJCardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGJCardComponent();

    // 개발용 콘솔 명령. UI 없이 뽑기 결과만 로그로 확인한다.
    // 카드가 생긴 뒤에도 "지금 풀에서 뭐가 나올 수 있나"를 보는 용도로 남긴다.
    UFUNCTION(Exec)
    void GJDrawCards();

protected:
    virtual void BeginPlay() override;

    // 카드 정의 테이블 (DT_CardData). 비어 있으면 카드 시스템 전체가 조용히 꺼진다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    UDataTable* CardTable;

    // 기본 선택지 장수. 코드에 3을 박아두면 "2장짜리 선택" 같은 조정에 컴파일이 필요해진다.
    // 이 값은 원본이라 런타임에 덮어쓰지 않는다 - 보너스는 아래 두 멤버로 얹는다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    int32 NumCardsToDraw = 3;

    // 영구 특성으로 늘어난 선택지 수. 지금은 아무도 안 바꾸지만, 메타 프로그레션(M6)이
    // 붙을 자리를 미리 뚫어둔다. 여기 대신 NumCardsToDraw를 직접 덮으면 원본값을 잃는다.
    UPROPERTY(BlueprintReadWrite, Category = "Card")
    int32 BonusCardSlots = 0;

    // 이 확률로 선택지가 한 장 더 뜬다 (0.2 = 20%). 판정은 뽑기 함수 밖에서 한다 -
    // DrawCards가 "몇 장 뽑을지"까지 정하면 리롤할 때마다 장수가 흔들린다.
    UPROPERTY(BlueprintReadWrite, Category = "Card")
    float ExtraCardChance = 0.f;

    // 이미 고른 bStackable=false 카드. 스택 가능한 카드는 기록할 이유가 없다.
    // 런마다 컴포넌트가 새로 만들어지므로 초기화 코드가 필요 없다.
    TSet<FName> TakenCards;

    // 이번에 몇 장 뽑을지 정한다. 확률 판정이 들어있어 호출할 때마다 결과가 다를 수 있으므로,
    // 한 번의 선택 화면에는 한 번만 부른다(리롤은 장수를 다시 굴리지 않는다).
    int32 GetDrawCount() const;

    // 가중 랜덤 비복원 추출로 최대 Count장을 뽑는다.
    // 후보가 부족하면 있는 만큼만, 하나도 없으면 빈 배열을 돌려준다.
    // 부작용이 없다(TakenCards를 건드리지 않는다). 리롤이 이 함수 재호출만으로 되는 이유다.
    TArray<FName> DrawCards(int32 Count) const;

    // 소유자를 AGJCharacter로 캐스팅해서 돌려준다. 다른 액터에 잘못 붙였으면 nullptr.
    AGJCharacter* GetOwnerCharacter() const;
};
```

- [ ] **Step 2: `GJCardComponent.cpp` 생성**

새 파일 `Source/Project_GJ/GJCardComponent.cpp`:

```cpp
#include "GJCardComponent.h"
#include "GJCharacter.h"
#include "GJWeaponBase.h"
#include "Engine/DataTable.h"

UGJCardComponent::UGJCardComponent()
{
    // 카드 로직은 전부 이벤트 구동(레벨업, 버튼 클릭)이라 매 프레임 할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;
}

void UGJCardComponent::BeginPlay()
{
    Super::BeginPlay();
}

AGJCharacter* UGJCardComponent::GetOwnerCharacter() const
{
    return Cast<AGJCharacter>(GetOwner());
}

// StatBonus 카드인데 효과가 전부 0이면 골라도 아무 일이 없어 플레이어가 손해를 본다.
// 데이터 미입력을 뽑기 단계에서 걸러내기 위한 판정이다.
static bool IsStatEffectEmpty(const FStatModifier& Modifier)
{
    auto AllZero = [](const FStatValues& V)
    {
        return V.MaxHP == 0.f && V.MaxMP == 0.f && V.BaseAttackPower == 0.f
            && V.RequiredEXP == 0.f && V.Defense == 0.f && V.MoveSpeed == 0.f
            && V.CooldownReduction == 0.f && V.CritChance == 0.f && V.CritMultiplier == 0.f;
    };
    return AllZero(Modifier.Add) && AllZero(Modifier.Percent);
}

int32 UGJCardComponent::GetDrawCount() const
{
    int32 Count = NumCardsToDraw + BonusCardSlots;

    if (ExtraCardChance > 0.f && FMath::FRand() < ExtraCardChance)
    {
        Count++;
    }

    // 0장이 되면 선택 화면이 빈 채로 떠서 진행이 막힌다. 데이터를 어떻게 넣든 최소 1장은 보장한다.
    return FMath::Max(Count, 1);
}

TArray<FName> UGJCardComponent::DrawCards(int32 Count) const
{
    TArray<FName> Result;

    if (!CardTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: CardTable이 비어있어 카드를 뽑을 수 없습니다."));
        return Result;
    }

    // 1. 후보 수집
    TArray<FName> Candidates;
    TArray<float> Weights;

    for (const FName& RowName : CardTable->GetRowNames())
    {
        const FCardData* Row = CardTable->FindRow<FCardData>(RowName, TEXT("DrawCards"), false);
        if (!Row)
        {
            continue;
        }

        // 가중치 0 이하 = 임시로 꺼둔 카드
        if (Row->Weight <= 0.f)
        {
            continue;
        }

        // 스택 불가인데 이미 먹은 카드
        if (!Row->bStackable && TakenCards.Contains(RowName))
        {
            continue;
        }

        // 데이터가 비어 있는 행은 뽑아도 의미가 없다.
        // 무기 슬롯이 꽉 찼다는 이유로는 거르지 않는다 - 그 경우 카드를 고른 뒤
        // 어느 무기를 버릴지 플레이어가 정한다.
        if (Row->EffectType == ECardEffectType::GrantWeapon && !Row->WeaponClass)
        {
            continue;
        }
        if (Row->EffectType == ECardEffectType::StatBonus && IsStatEffectEmpty(Row->StatEffect))
        {
            continue;
        }
        // Ability는 거르지 않는다. 테이블에 있으면 UI에는 보이고, 고르면 적용 단계에서
        // 경고가 찍힌다(M2.7 작업 시 바로 확인할 수 있게).

        Candidates.Add(RowName);
        Weights.Add(Row->Weight);
    }

    // 2. 가중 랜덤 비복원 추출
    const int32 DrawCount = FMath::Min(Count, Candidates.Num());
    for (int32 Draw = 0; Draw < DrawCount; Draw++)
    {
        // 매 반복마다 총합을 다시 구한다. 뽑힌 카드를 목록에서 빼고도 총합을 그대로 쓰면
        // 이미 사라진 가중치가 구간에 남아, 난수가 그 구간에 떨어졌을 때 엉뚱한 카드가
        // 뽑히거나 마지막 카드로 몰린다.
        float TotalWeight = 0.f;
        for (float W : Weights)
        {
            TotalWeight += W;
        }
        if (TotalWeight <= 0.f)
        {
            break;
        }

        const float Roll = FMath::FRandRange(0.f, TotalWeight);
        float Accum = 0.f;
        int32 PickedIndex = Candidates.Num() - 1;  // 부동소수 오차로 루프를 못 빠져나갈 때의 안전값
        for (int32 i = 0; i < Candidates.Num(); i++)
        {
            Accum += Weights[i];
            if (Roll < Accum)
            {
                PickedIndex = i;
                break;
            }
        }

        Result.Add(Candidates[PickedIndex]);
        Candidates.RemoveAt(PickedIndex);
        Weights.RemoveAt(PickedIndex);
    }

    return Result;
}

void UGJCardComponent::GJDrawCards()
{
    const TArray<FName> Drawn = DrawCards(GetDrawCount());

    if (Drawn.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJDrawCards: 뽑을 수 있는 카드가 없습니다 (테이블 비었거나 전부 제외됨)."));
        return;
    }

    FString Joined;
    for (const FName& Id : Drawn)
    {
        if (!Joined.IsEmpty())
        {
            Joined += TEXT(", ");
        }
        Joined += Id.ToString();
    }

    UE_LOG(LogTemp, Log, TEXT("GJDrawCards: %d장 -> %s (이미 먹은 고유카드 %d개)"),
        Drawn.Num(), *Joined, TakenCards.Num());
}
```

- [ ] **Step 3: `GJCharacter.h`에 컴포넌트 멤버 추가**

`GJCharacter.h` 상단의 전방 선언에서 다음 줄을 찾는다(23번째 줄 부근):

```cpp
class UGJInventoryComponent;
```

그 **아래**에 추가한다:

```cpp
class UGJCardComponent;
```

이어서 다음 세 줄을 찾는다(398번째 줄 부근):

```cpp
    // 인벤토리 데이터/로직 (버튼 등은 이 컴포넌트에 직접 연결해서 쓰면 됨)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
    UGJInventoryComponent* InventoryComponent;
```

그 **아래**에 추가한다:

```cpp

    // 레벨업 카드 선택 (OnLevelUp을 구독해서 알아서 동작함 - 캐릭터는 카드를 모른다)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card")
    UGJCardComponent* CardComponent;
```

- [ ] **Step 4: `GJCharacter.cpp`에서 컴포넌트 생성**

`GJCharacter.cpp`에서 다음 줄을 찾는다(45번째 줄 부근):

```cpp
    InventoryComponent = CreateDefaultSubobject<UGJInventoryComponent>(TEXT("InventoryComponent"));
```

그 **아래**에 추가한다:

```cpp
    CardComponent = CreateDefaultSubobject<UGJCardComponent>(TEXT("CardComponent"));
```

그리고 `GJCharacter.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJCardComponent.h"
```

- [ ] **Step 5: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. 완전히 새로운 `UCLASS`(`UGJCardComponent`)라 라이브 코딩만으로는 안 잡힐 수 있어. 컴파일 후 `BP_GJCharacter`를 열었을 때 컴포넌트 목록에 **CardComponent**가 안 보이면 **에디터를 재시작**해줘.

Run: `grep -E "error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 6: `DT_CardData` 생성**

MCP로 데이터 테이블을 만든다. `editor_toolset.toolsets.data_table.DataTableTools`의 도구 목록을 `describe_toolset`으로 확인한 뒤, 행 구조체 `CardData`로 `/Game/GJ/DataTables/DT_CardData`를 생성하고 아래 6행을 넣는다.

아이콘은 아직 전용 아트가 없으므로 **프로젝트에 있는 텍스처를 자리표시자로** 쓴다.

| 행 이름 | DisplayName | Description | Icon | EffectType | 효과 | bStackable | Weight |
|---|---|---|---|---|---|---|---|
| `Card_HP5` | 튼튼함 | 최대 체력 +5 | `/Game/Characters/Mannequins/Textures/Shared/T_UE_Logo_M` | StatBonus | `Add.MaxHP = 5` | true | 1.0 |
| `Card_Atk15` | 예리함 | 공격력 +15% | `/Game/Weapons/Rifle/Textures/T_Rifle_D` | StatBonus | `Percent.BaseAttackPower = 0.15` | true | 1.0 |
| `Card_Speed` | 날렵함 | 이동 속도 +50 | `/Game/Cursor/T_Arrow` | StatBonus | `Add.MoveSpeed = 50` | true | 1.0 |
| `Card_Crit` | 급소 노리기 | 치명타 확률 +5% | `/Game/Weapons/Generic/Textures/T_Weapon_D` | StatBonus | `Add.CritChance = 0.05` | true | 0.5 |
| `Card_Rifle` | 소총 획득 | 소총을 얻는다 | `/Game/Weapons/Rifle/Textures/T_Rifle_D` | GrantWeapon | `WeaponClass = /Game/GJ/BluePrint/BP_GJWeapon_Ranged.BP_GJWeapon_Ranged_C` | **false** | 0.3 |
| `Card_Fireball` | 파이어볼 (미구현) | 아직 동작하지 않는다 | `/Game/Effects/Textures/General/squares` | Ability | — | **false** | 0.2 |

이 6장이 검증에 필요한 경로를 전부 덮는다: 가산/증가율 스탯, 낮은 가중치, 스택 불가, 무기 지급, 미구현 스텁.

MCP로 안 되면 사용자에게 요청한다:
> `Content/GJ/DataTables`에 **Data Table**을 만들고 행 구조체로 `CardData`를 골라서 `DT_CardData`로 저장한 뒤, 위 표대로 6행을 넣어줘.

- [ ] **Step 7: `BP_GJCharacter`의 컴포넌트에 테이블 지정**

사용자에게 요청한다:
> `BP_GJCharacter`를 열고 컴포넌트 목록에서 **CardComponent**를 선택한 다음, 디테일 패널의 **Card → Card Table**에 방금 만든 `DT_CardData`를 지정하고 컴파일·저장해줘.

MCP가 붙어 있으면 `ObjectTools.set_properties`로 블루프린트 CDO의 `CardComponent.CardTable`을 설정해도 된다.

- [ ] **Step 8: 뽑기 확인 (콘솔)**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 **Output Log 창 아래 `Cmd:` 입력칸**에 쳐줘 (PIE의 `~` 키는 디버그 매니저가 가로챈다):
>
> ```
> GJDrawCards
> ```
>
> 여러 번 쳐줘. 매번 **3장이 서로 다른 카드로** 나와야 하고, 가중치가 낮은 `Card_Rifle`(0.3)과 `Card_Fireball`(0.2)은 `Card_HP5`(1.0)보다 **눈에 띄게 덜** 나와야 해.

Run: `grep -E "GJDrawCards" Saved/Logs/Project_GJ.log | tail -10`
Expected: `GJDrawCards: 3장 -> Card_XXX, Card_YYY, Card_ZZZ (이미 먹은 고유카드 0개)` 형태가 반복된다.

확인 항목:
- 한 줄 안에 **같은 카드가 두 번 나오지 않는다** (비복원 추출)
- 매번 조합이 달라진다 (가중 랜덤이 실제로 굴러간다)
- 경고 없이 동작한다

`Exec` 함수는 액터가 소유한 컴포넌트까지 탐색해서 처리한다. 만약 `GJDrawCards`가 "Command not recognized"로 안 먹으면, 같은 이름의 `UFUNCTION(Exec)`를 `AGJCharacter`에 만들어 `CardComponent->GJDrawCards()`를 호출하도록 우회한다.

- [ ] **Step 9: 커밋**

```bash
git add Source/Project_GJ/GJCardComponent.h Source/Project_GJ/GJCardComponent.cpp Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Content/GJ/DataTables/DT_CardData.uasset Content/GJ/BluePrint/BP_GJCharacter.uasset
git commit -m "$(cat <<'EOF'
카드 컴포넌트와 가중 랜덤 뽑기 추가

UGJCardComponent를 만들고 캐릭터 생성자에서 붙였다. 아직 레벨업과
연결되지 않아 콘솔 명령으로만 동작한다.

뽑기는 가중 랜덤 비복원 추출이다. 한 장 뽑을 때마다 총합을 다시 계산하는데,
그러지 않으면 이미 빠진 카드의 가중치가 구간에 남아 뽑기가 한쪽으로 몰린다.

후보에서 거르는 것: 가중치 0 이하, 이미 먹은 스택 불가 카드, 무기 클래스가
비어있는 무기 카드, 효과가 전부 0인 스탯 카드. 무기 슬롯이 꽉 찼다는
이유로는 거르지 않는다 - 그건 카드를 고른 뒤 플레이어가 정할 몫이다.

DT_CardData에 검증용 6장을 넣었다. 가산/증가율 스탯, 낮은 가중치,
스택 불가, 무기 지급, 미구현 스텁 경로를 전부 덮는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 선택지 위젯

이 태스크가 끝나면 **화면에 선택지가 뜬다.** 아직 레벨업과는 무관하고, 콘솔 명령으로 띄워서 확인한다.

**Files:**
- Create: `Source/Project_GJ/GJCardWidget.h`, `Source/Project_GJ/GJCardWidget.cpp`
- Create: `Source/Project_GJ/GJCardSelectWidget.h`, `Source/Project_GJ/GJCardSelectWidget.cpp`
- Modify: `Source/Project_GJ/GJCardComponent.h/.cpp` (위젯 표시 + 디버그 명령)
- Create (에셋): `Content/GJ/UI/WBP_Card`, `Content/GJ/UI/WBP_CardSelect`

**Interfaces:**
- Consumes: `FGJChoiceEntry` (Task 1), `UGJCardComponent::DrawCards` (Task 2)
- Produces:
  - `void UGJCardWidget::Setup(int32 InChoiceIndex, const FGJChoiceEntry& Entry)`
  - `UGJCardWidget::OnCardClicked` — `(int32 ChoiceIndex)`
  - `void UGJCardSelectWidget::ShowChoices(const TArray<FGJChoiceEntry>& Choices)`
  - `UGJCardSelectWidget::OnChoiceSelected` — `(int32 ChoiceIndex)`

- [ ] **Step 1: `GJCardWidget.h` 생성**

새 파일 `Source/Project_GJ/GJCardWidget.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJGameTypes.h"
#include "GJCardWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;

// 이 선택지가 목록에서 몇 번째인지를 돌려준다. 카드 ID가 아니라 인덱스인 이유는
// 같은 위젯을 무기 교체(인덱스 = 버릴 슬롯 번호)에도 쓰기 때문이다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardWidgetClickedSignature, int32, ChoiceIndex);

UCLASS()
class PROJECT_GJ_API UGJCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 표시 내용을 채우고 자기 인덱스를 기억한다.
    void Setup(int32 InChoiceIndex, const FGJChoiceEntry& Entry);

    UPROPERTY(BlueprintAssignable, Category = "Card")
    FOnCardWidgetClickedSignature OnCardClicked;

protected:
    virtual void NativeConstruct() override;

    // WBP_Card를 이 태스크에서 새로 만들므로 strict BindWidget을 쓴다.
    // (기존 WBP에 바인딩을 추가할 때만 BindWidgetOptional이 필요하다 - 그때는 에디터 작업
    //  전까지 WBP 컴파일이 깨지기 때문. 여기선 위젯과 WBP가 같이 만들어진다.)
    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* NameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DescText;

    UPROPERTY(meta = (BindWidget))
    UButton* SelectButton;

    int32 ChoiceIndex = INDEX_NONE;

    UFUNCTION()
    void HandleButtonClicked();
};
```

- [ ] **Step 2: `GJCardWidget.cpp` 생성**

새 파일 `Source/Project_GJ/GJCardWidget.cpp`:

```cpp
#include "GJCardWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UGJCardWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (SelectButton)
    {
        // 위젯이 재사용되는 경우를 대비해 중복 바인딩을 막는다.
        SelectButton->OnClicked.RemoveDynamic(this, &UGJCardWidget::HandleButtonClicked);
        SelectButton->OnClicked.AddDynamic(this, &UGJCardWidget::HandleButtonClicked);
    }
}

void UGJCardWidget::Setup(int32 InChoiceIndex, const FGJChoiceEntry& Entry)
{
    ChoiceIndex = InChoiceIndex;

    if (NameText)
    {
        NameText->SetText(Entry.DisplayName);
    }
    if (DescText)
    {
        DescText->SetText(Entry.Description);
    }
    if (IconImage)
    {
        if (Entry.Icon)
        {
            IconImage->SetBrushFromTexture(Entry.Icon);
            IconImage->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            // 아이콘이 없는 카드도 있을 수 있다. 빈 브러시를 그리면 흰 사각형이 남으므로 숨긴다.
            IconImage->SetVisibility(ESlateVisibility::Hidden);
        }
    }
}

void UGJCardWidget::HandleButtonClicked()
{
    OnCardClicked.Broadcast(ChoiceIndex);
}
```

- [ ] **Step 3: `GJCardSelectWidget.h` 생성**

새 파일 `Source/Project_GJ/GJCardSelectWidget.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJGameTypes.h"
#include "GJCardSelectWidget.generated.h"

class UHorizontalBox;
class UGJCardWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChoiceSelectedSignature, int32, ChoiceIndex);

UCLASS()
class PROJECT_GJ_API UGJCardSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 선택지 목록을 받아 카드 위젯을 그만큼 만들어 늘어놓는다.
    // 카드 선택(최대 3개)과 무기 교체(2개) 양쪽이 이 함수를 쓴다.
    void ShowChoices(const TArray<FGJChoiceEntry>& Choices);

    UPROPERTY(BlueprintAssignable, Category = "Card")
    FOnChoiceSelectedSignature OnChoiceSelected;

protected:
    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* CardContainer;

    // WBP_Card를 지정한다. 인벤토리의 SlotWidgetClass와 같은 패턴이다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    TSubclassOf<UGJCardWidget> CardWidgetClass;

    UFUNCTION()
    void HandleCardClicked(int32 ChoiceIndex);
};
```

- [ ] **Step 4: `GJCardSelectWidget.cpp` 생성**

새 파일 `Source/Project_GJ/GJCardSelectWidget.cpp`:

```cpp
#include "GJCardSelectWidget.h"
#include "GJCardWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

void UGJCardSelectWidget::ShowChoices(const TArray<FGJChoiceEntry>& Choices)
{
    if (!CardContainer || !CardWidgetClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJCardSelectWidget: CardContainer 또는 CardWidgetClass가 비어있습니다."));
        return;
    }

    // 같은 위젯 인스턴스를 재사용하므로 이전 선택지를 먼저 지운다.
    // 안 지우면 카드가 계속 옆으로 쌓인다(연속 레벨업에서 바로 드러난다).
    CardContainer->ClearChildren();

    for (int32 i = 0; i < Choices.Num(); i++)
    {
        UGJCardWidget* CardWidget = CreateWidget<UGJCardWidget>(this, CardWidgetClass);
        if (!CardWidget)
        {
            continue;
        }

        CardWidget->Setup(i, Choices[i]);
        CardWidget->OnCardClicked.AddDynamic(this, &UGJCardSelectWidget::HandleCardClicked);

        UHorizontalBoxSlot* BoxSlot = CardContainer->AddChildToHorizontalBox(CardWidget);
        if (BoxSlot)
        {
            BoxSlot->SetPadding(FMargin(12.f, 0.f));
        }
    }
}

void UGJCardSelectWidget::HandleCardClicked(int32 ChoiceIndex)
{
    OnChoiceSelected.Broadcast(ChoiceIndex);
}
```

- [ ] **Step 5: `GJCardComponent.h`에 위젯 표시 API 추가**

`GJCardComponent.h`의 `class AGJCharacter;` 아래에 추가한다:

```cpp
class UGJCardSelectWidget;
```

그리고 `protected:` 블록의 `int32 GetDrawCount() const;` **위**에 추가한다:

```cpp
    // 선택지 화면 클래스 (WBP_CardSelect). 비어 있으면 카드 선택을 건너뛴다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    TSubclassOf<UGJCardSelectWidget> CardSelectWidgetClass;

    UPROPERTY()
    UGJCardSelectWidget* CardSelectWidgetInstance;

    // 선택지를 화면에 띄운다. 위젯 생성이 실패하면 false를 돌려준다 -
    // 호출자는 이때 일시정지를 걸면 안 된다(화면 없이 게임만 멈추는 소프트락이 된다).
    bool OpenChoiceUI(const TArray<FGJChoiceEntry>& Entries);

    // 뽑힌 카드 ID 목록을 표시용 구조체로 바꾼다.
    TArray<FGJChoiceEntry> BuildCardEntries(const TArray<FName>& CardIds) const;
```

`public:` 블록의 `GJDrawCards` 아래에 추가한다:

```cpp
    // 개발용. 레벨업 없이 카드 화면만 띄워본다(일시정지는 걸지 않는다 - Task 4에서 붙는다).
    // Exec를 여기 달면 콘솔이 못 찾는다(Task 2에서 확인됨). AGJCharacter에 창구를 만든다.
    UFUNCTION(BlueprintCallable, Category = "Card")
    void GJShowCards();
```

이어서 `AGJCharacter`에 콘솔 창구를 만든다. `GJCharacter.h`의 `GJSetTagWeight` 선언 아래에 추가한다:

```cpp
    // 예) GJShowCards -> 레벨업 없이 카드 화면만 띄운다
    UFUNCTION(Exec)
    void GJShowCards();
```

`GJCharacter.cpp`의 `AGJCharacter::GJSetTagWeight` 구현 아래에 추가한다:

```cpp
void AGJCharacter::GJShowCards()
{
    if (!CardComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: CardComponent가 없습니다."));
        return;
    }

    CardComponent->GJShowCards();
}
```

- [ ] **Step 6: `GJCardComponent.cpp`에 위젯 표시 구현 추가**

`GJCardComponent.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJCardSelectWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
```

파일 **맨 끝**에 추가한다:

```cpp

TArray<FGJChoiceEntry> UGJCardComponent::BuildCardEntries(const TArray<FName>& CardIds) const
{
    TArray<FGJChoiceEntry> Entries;
    if (!CardTable)
    {
        return Entries;
    }

    for (const FName& Id : CardIds)
    {
        const FCardData* Row = CardTable->FindRow<FCardData>(Id, TEXT("BuildCardEntries"), false);
        if (!Row)
        {
            continue;
        }

        FGJChoiceEntry Entry;
        Entry.DisplayName = Row->DisplayName;
        Entry.Description = Row->Description;
        Entry.Icon = Row->Icon;
        Entries.Add(Entry);
    }

    return Entries;
}

bool UGJCardComponent::OpenChoiceUI(const TArray<FGJChoiceEntry>& Entries)
{
    AGJCharacter* Character = GetOwnerCharacter();
    if (!Character)
    {
        return false;
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC)
    {
        return false;
    }

    if (!CardSelectWidgetInstance)
    {
        if (!CardSelectWidgetClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: CardSelectWidgetClass가 비어있어 카드 선택을 건너뜁니다."));
            return false;
        }
        CardSelectWidgetInstance = CreateWidget<UGJCardSelectWidget>(PC, CardSelectWidgetClass);
        if (!CardSelectWidgetInstance)
        {
            UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: 카드 선택 위젯 생성에 실패했습니다."));
            return false;
        }
    }

    CardSelectWidgetInstance->ShowChoices(Entries);

    if (!CardSelectWidgetInstance->IsInViewport())
    {
        CardSelectWidgetInstance->AddToViewport();
    }

    return true;
}

void UGJCardComponent::GJShowCards()
{
    const TArray<FName> Drawn = DrawCards(GetDrawCount());
    if (Drawn.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: 뽑을 수 있는 카드가 없습니다."));
        return;
    }

    if (!OpenChoiceUI(BuildCardEntries(Drawn)))
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: 화면을 띄우지 못했습니다."));
    }
}
```

- [ ] **Step 7: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. 새 `UCLASS`가 두 개(`UGJCardWidget`, `UGJCardSelectWidget`)라, 다음 단계에서 위젯 블루프린트의 부모 클래스 목록에 안 뜨면 **에디터를 재시작**해야 해.

Run: `grep -E "error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 8: `WBP_Card` 생성**

MCP `UMGToolSet`으로 `/Game/GJ/UI`에 부모 클래스 `UGJCardWidget`으로 `WBP_Card`를 만들고 아래 구조를 배치한다. **이름이 정확히 일치해야** strict `BindWidget`이 걸린다.

```
RootCanvas (CanvasPanel)
  CardRoot (VerticalBox)        크기 220 x 320
    IconImage   (Image)         200 x 200
    NameText    (TextBlock)     폰트 18, 가운데 정렬
    DescText    (TextBlock)     폰트 12, 자동 줄바꿈
    SelectButton (Button)       텍스트 "선택"
```

`IconImage`, `NameText`, `DescText`, `SelectButton` 네 개는 **Is Variable이 켜져 있어야** 한다.

만든 뒤 `CompileWidgetBlueprint`로 컴파일하고 `save_assets`로 저장한다. **컴파일이 실패하면 바인딩 이름이 틀린 것이다** — 에러 메시지에 어떤 이름이 없는지 나온다.

- [ ] **Step 9: `WBP_CardSelect` 생성**

MCP로 `/Game/GJ/UI`에 부모 클래스 `UGJCardSelectWidget`으로 `WBP_CardSelect`를 만든다.

```
RootCanvas (CanvasPanel)
  CardContainer (HorizontalBox)   화면 중앙 앵커, 가운데 정렬
```

생성 후 `ObjectTools.set_properties`로 CDO의 `CardWidgetClass`에 `/Game/GJ/UI/WBP_Card.WBP_Card_C`를 지정한다(**`_C` 접미사가 붙은 생성 클래스 경로**여야 한다 — 개발 가이드 7절).

컴파일·저장한다.

- [ ] **Step 10: `BP_GJCharacter`의 컴포넌트에 위젯 클래스 지정**

사용자에게 요청한다:
> `BP_GJCharacter` → **CardComponent** → 디테일 패널 **Card → Card Select Widget Class**에 `WBP_CardSelect`를 지정하고 컴파일·저장해줘.

- [ ] **Step 11: 화면 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 Output Log의 `Cmd:` 입력칸에 쳐줘:
>
> ```
> GJShowCards
> ```
>
> 화면에 **카드 3장이 가로로** 뜨고, 각 카드에 아이콘/이름/설명/선택 버튼이 보여야 해. 아직 버튼을 눌러도 아무 일도 안 일어나는 게 정상이야(효과 적용은 다음 태스크).
>
> 한 번 더 치면 **카드가 옆으로 쌓이지 않고 새로 3장으로 교체**돼야 해.

확인 항목:
- 카드 3장이 표시된다
- 아이콘/이름/설명이 데이터 테이블 값과 맞는다
- 두 번째 호출에서 카드가 누적되지 않는다(`ClearChildren`이 걸렸는지)

- [ ] **Step 12: 커밋**

```bash
git add Source/Project_GJ/GJCardWidget.h Source/Project_GJ/GJCardWidget.cpp Source/Project_GJ/GJCardSelectWidget.h Source/Project_GJ/GJCardSelectWidget.cpp Source/Project_GJ/GJCardComponent.h Source/Project_GJ/GJCardComponent.cpp Content/GJ/UI/WBP_Card.uasset Content/GJ/UI/WBP_CardSelect.uasset Content/GJ/BluePrint/BP_GJCharacter.uasset
git commit -m "$(cat <<'EOF'
카드 선택지 위젯 추가

UGJCardWidget(선택지 하나)과 UGJCardSelectWidget(N개를 늘어놓고 인덱스를
돌려줌)을 추가했다. 아직 레벨업과 연결되지 않아 콘솔 명령으로만 뜬다.

위젯은 FCardData를 모르고 FGJChoiceEntry(이름/설명/아이콘)만 받는다.
선택 결과도 카드 ID가 아니라 목록에서의 인덱스로 돌려준다. 덕분에 무기
교체 화면을 따로 만들 필요가 없다 - 같은 위젯에 무기 2개를 넣으면 된다.

ShowChoices는 매번 ClearChildren을 한다. 안 하면 연속 레벨업에서 카드가
옆으로 계속 쌓인다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 레벨업 연결, 대기열, 효과 적용

이 태스크가 끝나면 **실제로 플레이된다.**

**Files:**
- Modify: `Source/Project_GJ/GJCardComponent.h`, `Source/Project_GJ/GJCardComponent.cpp`

**Interfaces:**
- Consumes: `AGJCharacter::OnLevelUp`, `AddStatBonus`, `PickUpWeapon`, `GetWeaponInSlot`, `ReplaceWeaponInSlot`, `AGJWeaponBase::GetWeaponStat`, `GetWeaponRowName`
- Produces: 없음 (외부에서 부르는 새 API 없음)

- [ ] **Step 1: `GJCardComponent.h`에 흐름 제어 멤버 추가**

`GJCardComponent.h`의 `protected:` 블록에서 다음 줄을 찾는다:

```cpp
    // 이미 고른 bStackable=false 카드. 스택 가능한 카드는 기록할 이유가 없다.
    // 런마다 컴포넌트가 새로 만들어지므로 초기화 코드가 필요 없다.
    TSet<FName> TakenCards;
```

그 **아래**에 추가한다:

```cpp

    // 아직 처리하지 않은 카드 선택 횟수.
    // 킬 한 번에 레벨이 2->5로 오르면 OnLevelUp이 4번 연달아 들어오는데, 그때 화면을
    // 한 번만 띄우면 플레이어가 보상 3번을 잃는다. 카운터로 쌓아두고 하나씩 소모한다.
    int32 PendingChoices = 0;

    // 지금 무엇을 묻는 중인가. 위젯은 인덱스만 돌려주므로 그 해석에 필요하다.
    EGJChoiceMode CurrentMode = EGJChoiceMode::None;

    // Card 모드일 때 인덱스 -> 카드 ID 매핑
    TArray<FName> CurrentCardIds;

    // WeaponReplace 모드일 때 지급 대기 중인 무기 (이미 월드에 스폰된 상태)
    UPROPERTY()
    AGJWeaponBase* PendingWeapon;

    UFUNCTION()
    void HandleLevelUp(int32 NewLevel);

    UFUNCTION()
    void HandleChoiceSelected(int32 ChoiceIndex);

    // 대기열에 남은 게 있으면 다음 선택지를 띄우고, 없으면 UI를 닫고 게임을 재개한다.
    void ShowNextChoice();

    // 위젯을 내리고 일시정지/입력 모드를 원복한다.
    void CloseChoiceUI();

    // 카드 효과를 적용한다. 무기 교체 선택이 필요해서 아직 끝나지 않았으면 false를
    // 돌려준다 - 그 경우 호출자는 대기열을 줄이면 안 된다.
    bool ApplyCard(FName CardId);
```

- [ ] **Step 2: `GJCardComponent.cpp`의 `BeginPlay`에서 구독**

`GJCardComponent.cpp`에서 다음 블록을 찾는다:

```cpp
void UGJCardComponent::BeginPlay()
{
    Super::BeginPlay();
}
```

이를 다음으로 교체한다:

```cpp
void UGJCardComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AGJCharacter* Character = GetOwnerCharacter())
    {
        Character->OnLevelUp.AddDynamic(this, &UGJCardComponent::HandleLevelUp);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: 소유자가 AGJCharacter가 아닙니다. 카드 시스템이 동작하지 않습니다."));
    }
}
```

- [ ] **Step 3: `GJCardComponent.cpp`에 흐름 제어 구현 추가**

`GJCardComponent.cpp` 상단 include 블록에 추가한다:

```cpp
#include "Kismet/GameplayStatics.h"
```

파일 **맨 끝**에 추가한다:

```cpp

void UGJCardComponent::HandleLevelUp(int32 NewLevel)
{
    PendingChoices++;

    // 이미 화면이 떠 있으면 카운터만 올리고 끝낸다. AddEXP의 while 루프가 OnLevelUp을
    // 연달아 쏘는 동안 여기로 여러 번 들어오는데, 각각이 화면을 띄우려 하면 안 된다.
    if (CurrentMode == EGJChoiceMode::None)
    {
        ShowNextChoice();
    }
}

void UGJCardComponent::ShowNextChoice()
{
    // 후보가 0장이면 그 한 번을 소모하고 다음으로 넘어간다.
    // 재귀가 아니라 루프인 이유: 풀이 완전히 비었을 때 대기열 길이만큼 스택이 쌓인다.
    while (PendingChoices > 0)
    {
        // 장수는 선택 화면 한 번당 한 번만 굴린다. 리롤이 붙어도 여기가 아니라
        // CurrentCardIds.Num()을 다시 쓰게 되므로 장수가 흔들리지 않는다.
        CurrentCardIds = DrawCards(GetDrawCount());
        if (CurrentCardIds.Num() > 0)
        {
            break;
        }
        PendingChoices--;
    }

    if (PendingChoices <= 0)
    {
        CloseChoiceUI();
        return;
    }

    CurrentMode = EGJChoiceMode::Card;

    if (!OpenChoiceUI(BuildCardEntries(CurrentCardIds)))
    {
        // 화면을 못 띄웠는데 일시정지를 걸면 아무것도 안 보이는 채로 게임이 멈춘다.
        // 대기열을 비우고 정상 상태로 돌아간다.
        UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: 선택 화면을 띄우지 못해 카드 선택을 건너뜁니다."));
        PendingChoices = 0;
        CurrentMode = EGJChoiceMode::None;
        return;
    }
}

void UGJCardComponent::CloseChoiceUI()
{
    CurrentMode = EGJChoiceMode::None;
    CurrentCardIds.Reset();

    if (CardSelectWidgetInstance && CardSelectWidgetInstance->IsInViewport())
    {
        CardSelectWidgetInstance->RemoveFromParent();
    }

    AGJCharacter* Character = GetOwnerCharacter();
    if (!Character)
    {
        return;
    }

    UGameplayStatics::SetGamePaused(Character, false);

    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        // FInputModeGameOnly 기본값(bConsumeCaptureMouseDown=true)은 마우스 캡처를 되찾는
        // 그 클릭을 캡처용으로만 쓰고 게임 입력으로는 안 넘긴다 - 그래서 UI를 닫은 직후
        // 첫 클릭이 씹혔던 이력이 있다(인벤토리에서 겪은 문제).
        FInputModeGameOnly InputMode;
        InputMode.SetConsumeCaptureMouseDown(false);
        PC->SetInputMode(InputMode);
    }
}

bool UGJCardComponent::ApplyCard(FName CardId)
{
    AGJCharacter* Character = GetOwnerCharacter();
    if (!Character || !CardTable)
    {
        return true;
    }

    const FCardData* Row = CardTable->FindRow<FCardData>(CardId, TEXT("ApplyCard"), false);
    if (!Row)
    {
        return true;
    }

    switch (Row->EffectType)
    {
    case ECardEffectType::StatBonus:
    {
        Character->AddStatBonus(Row->StatEffect);
        break;
    }

    case ECardEffectType::GrantWeapon:
    {
        if (!Row->WeaponClass)
        {
            break;
        }

        FActorSpawnParameters SpawnParams;
        SpawnParams.Owner = Character;
        SpawnParams.Instigator = Character;
        // 캐릭터 위치에 겹쳐서 스폰하므로 충돌 때문에 스폰이 취소되면 안 된다.
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        AGJWeaponBase* NewWeapon = GetWorld()->SpawnActor<AGJWeaponBase>(
            Row->WeaponClass, Character->GetActorLocation(), Character->GetActorRotation(), SpawnParams);

        if (!NewWeapon)
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyCard: 무기 스폰에 실패했습니다 (%s)."), *CardId.ToString());
            break;
        }

        const bool bSlotsFull = (Character->GetWeaponInSlot(0) != nullptr)
                             && (Character->GetWeaponInSlot(1) != nullptr);

        if (!bSlotsFull)
        {
            Character->PickUpWeapon(NewWeapon);
            break;
        }

        // 슬롯이 꽉 찼다. 어느 무기를 버릴지 플레이어에게 묻는다.
        // 여기서 대기열을 줄이면 안 된다 - 이 레벨업의 처리는 슬롯을 고른 뒤에야 끝난다.
        PendingWeapon = NewWeapon;
        CurrentMode = EGJChoiceMode::WeaponReplace;

        TArray<FGJChoiceEntry> Entries;
        for (int32 SlotIndex = 0; SlotIndex < 2; SlotIndex++)
        {
            AGJWeaponBase* Equipped = Character->GetWeaponInSlot(SlotIndex);
            FGJChoiceEntry Entry;
            Entry.DisplayName = FText::FromName(Equipped ? Equipped->GetWeaponRowName() : NAME_None);
            Entry.Description = FText::Format(
                NSLOCTEXT("GJ", "ReplaceSlot", "{0}번 무기를 버리고 교체한다"), FText::AsNumber(SlotIndex + 1));
            Entry.Icon = Equipped ? Equipped->GetWeaponStat().WeaponIcon : nullptr;
            Entries.Add(Entry);
        }

        OpenChoiceUI(Entries);
        return false;  // 아직 안 끝났다
    }

    case ECardEffectType::Ability:
    {
        // 조용히 무시하면 데이터 테이블에 능력 카드를 넣어두고 "왜 안 먹지?"로 헤매게 된다.
        UE_LOG(LogTemp, Warning,
            TEXT("ApplyCard: 능력 카드 '%s'는 아직 미구현입니다 (스킬 시스템 M2.7 필요)."),
            *CardId.ToString());
        break;
    }
    }

    if (!Row->bStackable)
    {
        TakenCards.Add(CardId);
    }

    return true;
}

void UGJCardComponent::HandleChoiceSelected(int32 ChoiceIndex)
{
    AGJCharacter* Character = GetOwnerCharacter();
    if (!Character)
    {
        return;
    }

    if (CurrentMode == EGJChoiceMode::Card)
    {
        if (!CurrentCardIds.IsValidIndex(ChoiceIndex))
        {
            return;
        }

        const FName CardId = CurrentCardIds[ChoiceIndex];
        if (!ApplyCard(CardId))
        {
            // 무기 교체 선택으로 넘어갔다. 대기열은 그대로 두고 화면만 바뀐 상태다.
            return;
        }

        PendingChoices--;
        ShowNextChoice();
        return;
    }

    if (CurrentMode == EGJChoiceMode::WeaponReplace)
    {
        if (PendingWeapon)
        {
            Character->ReplaceWeaponInSlot(ChoiceIndex, PendingWeapon);
            PendingWeapon = nullptr;
        }

        PendingChoices--;
        ShowNextChoice();
    }
}
```

- [ ] **Step 4: `AGJCharacter`에 `StopAutoFire` 추가**

`bIsAutoFiring`은 `AGJCharacter`의 `protected` 멤버라 컴포넌트가 직접 못 끈다. 다음 스텝에서 쓰므로 먼저 만든다.

`GJCharacter.h`에서 `ReplaceWeaponInSlot` 선언 **아래**에 추가한다:

```cpp

    // 연사 상태를 강제로 해제한다. 모달 UI를 열 때 필요하다 - 입력 모드가 UI로 바뀌면
    // 마우스 "뗌" 이벤트가 캐릭터에 안 들어와서 bIsAutoFiring이 켜진 채로 굳고,
    // UI를 닫고 Tick이 다시 돌 때 무한 연사가 된다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopAutoFire() { bIsAutoFiring = false; }
```

- [ ] **Step 5: `OpenChoiceUI`에 일시정지와 입력 모드 추가**

`GJCardComponent.cpp`의 `OpenChoiceUI`에서 다음 블록을 찾는다:

```cpp
    CardSelectWidgetInstance->ShowChoices(Entries);

    if (!CardSelectWidgetInstance->IsInViewport())
    {
        CardSelectWidgetInstance->AddToViewport();
    }

    return true;
}
```

이를 다음으로 교체한다:

```cpp
    // 선택 결과를 받을 구독은 한 번만 건다(위젯 인스턴스를 재사용하므로).
    CardSelectWidgetInstance->OnChoiceSelected.RemoveDynamic(this, &UGJCardComponent::HandleChoiceSelected);
    CardSelectWidgetInstance->OnChoiceSelected.AddDynamic(this, &UGJCardComponent::HandleChoiceSelected);

    CardSelectWidgetInstance->ShowChoices(Entries);

    if (!CardSelectWidgetInstance->IsInViewport())
    {
        CardSelectWidgetInstance->AddToViewport();
    }

    // 일시정지는 액터 Tick 스케줄을 건너뛰게 하는 것뿐이라 사실상 공짜다.
    // UI/Slate는 월드 Tick과 별개라 멈춘 동안에도 계속 반응한다.
    UGameplayStatics::SetGamePaused(Character, true);

    // 입력 모드가 UI로 바뀌는 과정에서 마우스 버튼 "뗌" 이벤트가 캐릭터에 안 들어가는
    // 경우가 있어, 연사 중이었다면 강제로 꺼준다. 안 그러면 카드를 고르고 게임이
    // 재개됐을 때 무한 연사가 된다(인벤토리에서 겪은 문제).
    Character->StopAutoFire();

    // UIOnly로 완전히 UI에만 입력을 묶는다. GameAndUI로 두면 카드 바깥 클릭이 게임
    // 뷰포트로 흘러가 키보드 포커스를 가져가버린다.
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(CardSelectWidgetInstance->TakeWidget());
    PC->SetInputMode(InputMode);

    return true;
}
```

- [ ] **Step 6: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 7: 기본 흐름 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 적을 2마리 잡아 레벨업해줘.
>
> - 카드 3장이 뜨고 **게임이 멈춰야** 한다 (적이 안 움직임)
> - 마우스로 카드 하나를 고르면 **화면이 닫히고 게임이 재개**돼야 한다
> - 스탯 카드를 골랐다면 HUD 체력 바나 이동 속도로 효과가 확인돼야 한다
> - 카드를 고른 직후 **클릭이 바로 먹어야** 한다(첫 클릭 씹힘 없음)
> - **저절로 총이 나가면 안 된다**(무한 연사 없음)

- [ ] **Step 8: 연속 레벨업 확인**

사용자에게 요청한다:
> Output Log의 `Cmd:` 입력칸에 **한 줄씩** 쳐줘:
>
> ```
> GJAddBonus RequiredEXP -90 0
> ```
>
> 레벨 1의 필요 경험치가 100 → 10이 되니까 적을 하나만 잡아도 **여러 레벨이 한 번에** 오른다. 그러면 카드 화면이 **연달아 여러 번** 떠야 해. 한 장 고를 때마다 다음 3장이 나오고, 다 소진되면 게임이 재개된다.

Run: `grep -E "LevelUp!" Saved/Logs/Project_GJ.log | tail -6`
Expected: 레벨업 로그 개수와 카드 화면이 뜬 횟수가 일치한다.

**카드 화면이 한 번만 뜨고 끝나면** 대기열이 동작하지 않는 것이다.

- [ ] **Step 9: 무기 교체 확인**

사용자에게 요청한다:
> 무기 슬롯 두 개를 다 채운 상태(필드 무기를 하나 주워서 1·2번이 다 참)에서 레벨업해서 **`소총 획득` 카드**를 골라줘.
>
> - 선택지가 **2개짜리 화면으로 바뀌어야** 한다 (지금 든 무기 두 개)
> - 하나를 고르면 **그 무기가 바닥에 떨어지고** 새 소총이 그 자리에 들어가야 한다
> - **1번과 2번 양쪽 다** 시험해줘 — 어느 쪽을 골라도 새 무기가 정확히 그 자리에 들어가야 한다
>
> 이어서 **슬롯이 하나만 찬 상태**에서 소총 카드를 골라봐. 이때는 교체 화면 없이 빈 슬롯에 바로 들어가야 한다.

마지막으로 **연속 레벨업 중에 무기 교체가 끼는 경우**를 본다. 사용자에게 요청한다:
> `GJAddBonus RequiredEXP -90 0`으로 연속 레벨업을 만든 상태에서, 뜬 카드 중 **소총 획득**을 골라 무기 교체까지 하고 나서 **남은 카드 선택이 계속 이어지는지** 봐줘.
>
> 레벨이 3번 올랐다면 카드 화면이 총 3번 떠야 한다 — 무기 교체 화면은 그 3번 중 하나에 딸린 2단계지 별도의 한 번이 아니다. **교체를 하고 나서 남은 횟수가 하나 줄어 있으면** 대기열을 잘못된 시점에 줄인 것이다.

- [ ] **Step 10: 나머지 경로 확인**

사용자에게 요청한다:
> 세 가지만 더 봐줘.
>
> 1. **`파이어볼 (미구현)` 카드**를 골라봐 → 화면은 정상적으로 닫히고 Output Log에 "능력 카드 ... 미구현" 경고가 떠야 해 (크래시나 멈춤 없이)
> 2. **`소총 획득` 카드를 한 번 고른 뒤** 계속 레벨업해봐 → `bStackable=false`라 **다시는 안 나와야** 해
> 3. **죽고 새 런**을 시작한 뒤 레벨업 → 제외됐던 소총 카드가 **다시 나와야** 해 (컴포넌트가 새로 만들어지므로)

Run: `grep -E "미구현|GJDrawCards" Saved/Logs/Project_GJ.log | tail -5`
Expected: 능력 카드 경고가 찍히고 크래시 로그가 없다.

이어서 **뽑을 카드가 하나도 없는 경우**를 확인한다. 스택 가능한 카드가 있는 한 풀은 안 마르므로, 일부러 만들어야 한다. 사용자에게 요청한다:
> `DT_CardData`에서 **모든 행의 `Weight`를 0으로** 바꾸고(Reimport 또는 직접 편집) 레벨업해봐.
>
> **카드 화면이 아예 안 뜨고 게임이 멈추지도 않은 채** 그냥 넘어가야 해. Output Log에 "뽑을 수 있는 카드가 없습니다" 경고만 뜬다. 확인했으면 `Weight`를 원래대로 되돌려줘.

여기서 게임이 멈추면 `ShowNextChoice`의 루프가 대기열을 소모하지 못하는 것이고, 화면이 빈 채로 뜨면 후보 0장 체크가 빠진 것이다.

- [ ] **Step 11: 소프트락 방지 확인**

사용자에게 요청한다:
> `BP_GJCharacter` → CardComponent → **Card Select Widget Class를 비우고** 컴파일·저장한 뒤 레벨업해봐.
>
> **게임이 멈추면 안 되고**, Output Log에 "CardSelectWidgetClass가 비어있어..." 경고만 떠야 해. 확인했으면 `WBP_CardSelect`를 다시 지정해줘.

이 경로가 막혀 있지 않으면, 나중에 에디터 설정이 빠진 채로 레벨업했을 때 화면은 안 뜨는데 게임만 멈춰서 원인을 찾기 어렵다.

- [ ] **Step 12: 커밋**

```bash
git add Source/Project_GJ/GJCardComponent.h Source/Project_GJ/GJCardComponent.cpp Source/Project_GJ/GJCharacter.h
git commit -m "$(cat <<'EOF'
레벨업에 카드 선택 연결

OnLevelUp을 구독해 카드 3장을 띄우고, 고른 카드의 효과를 적용한다.
이제 실제로 플레이된다.

연속 레벨업을 대기열로 처리한다. 킬 한 번에 레벨이 2->5로 오르면
OnLevelUp이 4번 연달아 들어오는데, 카운터로 쌓아두고 한 장씩 소모한다.
화면이 이미 떠 있으면 카운터만 올린다.

무기 슬롯이 꽉 찬 상태에서 무기 카드를 고르면 어느 무기를 버릴지 묻는다.
같은 선택지 위젯에 무기 2개를 넣어 재사용하므로 교체용 위젯이 없다.
이때 대기열을 줄이지 않는 게 중요하다 - 먼저 줄이면 연속 레벨업 중에
카드 한 장이 통째로 증발한다.

일시정지와 입력 모드는 인벤토리 모달의 검증된 패턴을 그대로 썼다.
StopAutoFire, SetWidgetToFocus, 닫을 때 ConsumeCaptureMouseDown false까지
셋 다 - 없으면 각각 무한 연사, 포커스 이탈, 첫 클릭 씹힘이 생긴다.

위젯을 못 띄우면 일시정지를 걸지 않고 대기열을 비운다. 화면은 안 뜨는데
게임만 멈추는 상태가 가장 추적하기 어렵다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: 개발 가이드 갱신

**Files:**
- Modify: `Docs/DevGuide.md`
- Modify: `DevGuide.html`

**Interfaces:**
- Consumes: Task 1~4의 모든 변경
- Produces: 없음 (문서화)

- [ ] **Step 1: 새 절 "카드 선택 시스템" 추가**

`Docs/DevGuide.md`의 `## 7. UI / 위젯` 절 **바로 위**에 새 절을 추가한다:

```markdown
## 6.7 카드 선택 시스템 (레벨업 선택지)

레벨이 오르면 카드 3장이 뜨고 하나를 고른다. 로직 전부가 `UGJCardComponent`(`GJCardComponent.h/.cpp`)에 있고, `AGJCharacter`는 생성자에서 컴포넌트를 붙이고 `OnLevelUp`을 쏘기만 한다 — **캐릭터는 카드를 모른다.**

| 요소 | 설명 |
|---|---|
| `DT_CardData` (`FCardData`) | 행 이름 = 카드 ID. 이름/설명/아이콘 + `EffectType` + 페이로드(`StatEffect` 또는 `WeaponClass`) + `bStackable` + `Weight` |
| 뽑기 | 가중 랜덤 **비복원** 추출. 한 장 뽑을 때마다 **총합을 다시 계산**한다 — 안 그러면 빠진 카드의 가중치가 구간에 남아 뽑기가 한쪽으로 몰린다 |
| 제외 조건 | `Weight <= 0`, 이미 먹은 `bStackable=false` 카드, `WeaponClass`가 빈 무기 카드, 효과가 전부 0인 스탯 카드. **무기 슬롯이 꽉 찼다는 이유로는 안 거른다** |
| 대기열 | `PendingChoices` 카운터. 킬 한 번에 레벨 2→5가 실제로 일어나므로(`AddEXP`의 `while` 루프) 화면을 한 번만 띄우면 보상 3번을 잃는다 |
| 효과 적용 | `StatBonus` → `AddStatBonus`, `GrantWeapon` → 스폰 후 `PickUpWeapon`(슬롯이 차 있으면 교체 선택), `Ability` → **미구현, 경고만**(M2.7) |

**위젯은 카드를 모른다.** `UGJCardSelectWidget`은 `FGJChoiceEntry`(이름/설명/아이콘) 목록을 받아 늘어놓고 **선택 인덱스**만 돌려준다. 인덱스의 의미는 컴포넌트가 `EGJChoiceMode`로 해석한다 — `Card`면 뽑힌 카드 목록의 위치, `WeaponReplace`면 버릴 슬롯 번호. **덕분에 무기 교체용 위젯이 따로 없다.**

**무기 슬롯이 꽉 찬 상태에서 무기 카드를 고르면** 같은 위젯에 지금 든 무기 2개를 넣어 "어느 걸 버릴지" 묻고, `AGJCharacter::ReplaceWeaponInSlot`으로 적용한다. 버린 무기는 `DropWeapon`을 거쳐 바닥에 떨어지므로 다시 주울 수 있다. **이 단계에서는 대기열을 줄이지 않는다** — 먼저 줄이면 연속 레벨업 중에 카드 한 장이 통째로 증발한다.

> **소프트락 방지**: 위젯 클래스가 비어 있거나 생성에 실패하면 **일시정지를 걸지 않고** 경고 후 대기열을 비운다. 화면은 안 뜨는데 게임만 멈추는 상태가 가장 추적하기 어렵다.

일시정지·입력 모드는 인벤토리 모달과 같은 패턴이다(2.2절 `ToggleInventory` 참고): 열 때 `SetGamePaused(true)` + `FInputModeUIOnly` + `SetWidgetToFocus` + `StopAutoFire()`, 닫을 때 `SetGamePaused(false)` + `FInputModeGameOnly` + `SetConsumeCaptureMouseDown(false)`. 인벤토리와 달리 **닫기 키가 없다** — 반드시 한 장 골라야 넘어간다.

**개발용 콘솔 명령**: `GJDrawCards`(뽑기 결과를 로그로만), `GJShowCards`(레벨업 없이 화면만 띄움), `GJSetTagWeight <태그> <배율>`(트리 밀어주기 시험). 셋 다 **`AGJCharacter`의 `UFUNCTION(Exec)`**이고 몸통은 `UGJCardComponent`에 있다 — 컴포넌트에 직접 `Exec`를 달면 콘솔이 `Command not recognized`를 낸다(Task 2에서 확인).

**카드도 런마다 초기화된다.** 컴포넌트가 캐릭터와 함께 새로 만들어지므로 `TakenCards`가 비워진다. EXP·스탯 보너스와 같은 메커니즘이다.
```

- [ ] **Step 2: 7절 UI 표에 카드 위젯 추가**

`Docs/DevGuide.md` 7절의 위젯 표에서 다음 줄을 찾는다:

```markdown
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` (strict) / `EXPBar`, `LevelText` (**Optional**) | `UpdateHP(Current,Max)` / `UpdateMP(Current,Max)` / `UpdateEXP(Current,Required,Level)` | 좌상단, `AddToViewport()` |
```

그 **아래**에 추가한다:

```markdown
| `UGJCardSelectWidget` | `WBP_CardSelect` | `CardContainer` (HorizontalBox) | `ShowChoices(TArray<FGJChoiceEntry>)` → `OnChoiceSelected(int32)` | 레벨업 시 중앙, 일시정지 모달 |
| `UGJCardWidget` | `WBP_Card` | `IconImage`, `NameText`, `DescText`, `SelectButton` | `Setup(Index, Entry)` → `OnCardClicked(int32)` | `CardContainer`에 런타임 생성 |
```

- [ ] **Step 3: 8절에 `FCardData` 스키마 추가**

`Docs/DevGuide.md` 8절에서 `### FStatValues / FStatModifier` 절 **바로 위**에 추가한다:

```markdown
### `FCardData` — `DT_CardData` (행 이름 = 카드 ID)
| 필드 | 기본값 | 설명 |
|---|---|---|
| `DisplayName` / `Description` | — | 카드에 표시되는 이름과 설명 |
| `Icon` | — | UTexture2D. 비어 있으면 아이콘 영역이 숨겨진다 |
| `EffectType` | `StatBonus` | `StatBonus` / `GrantWeapon` / `Ability`(미구현) |
| `StatEffect` | 전부 0 | `EffectType == StatBonus`일 때만 쓰임. `AddStatBonus`로 넘어간다 |
| `WeaponClass` | — | `EffectType == GrantWeapon`일 때만 쓰임 |
| `bStackable` | true | false면 한 번 고른 뒤 풀에서 영구 제외. 무기·고유 효과용 |
| `Weight` | 1.0 | 가중 랜덤의 가중치. **0 이하면 절대 안 뽑힌다**(카드를 임시로 끄는 용도로도 쓸 수 있음) |
```

- [ ] **Step 4: 9절 TODO 갱신**

`Docs/DevGuide.md` 9절에서 다음 항목을 찾아 **삭제한다**:

```markdown
- 레벨업 시 선택지(카드 3장)가 없음 — `AGJCharacter::OnLevelUp` 델리게이트만 준비돼 있고 구독자가 없음. 스테이지 클리어 쪽 트리거는 진행 구조(M5)가 생긴 뒤에 별도로 필요
```

같은 목록 끝에 다음을 **추가한다**:

```markdown
- 능력 카드(`ECardEffectType::Ability`)는 골라도 경고만 찍힘 — 액티브 스킬 시스템(스킬 슬롯/쿨다운/MP 소모/입력 바인딩)이 통째로 없음 (M2.7)
- 스테이지 클리어 시 카드 지급 트리거가 없음 — 진행 구조(M5)가 생긴 뒤. `UGJCardComponent`의 대기열 진입점을 공개 함수로 빼면 그쪽에서 부르기만 하면 된다
- 카드 리롤/스킵이 없음 — 3장이 전부 마음에 안 들어도 반드시 하나를 골라야 함
- 카드 희귀도가 확률(`Weight`)로만 존재하고 시각적 구분(색 테두리 등)이 없음
- `DT_CardData`의 카드 6장은 **검증용 임시 데이터** — 아이콘도 기존 텍스처를 자리표시자로 쓰고 있음
```

- [ ] **Step 5: `DevGuide.html`에 동일 내용 반영**

`DevGuide.html`에 Step 1~4의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다. 새 절은 `<h2>6.7 카드 선택 시스템 (레벨업 선택지)</h2>`로 7절 앞에 넣는다.

반영 후 태그 균형을 확인한다:

Run:
```bash
python -c "
import re
html = open('DevGuide.html', encoding='utf-8').read()
bad = 0
for tag in ['table','ul','pre','div','p','li','tr','td','th','code']:
    o = len(re.findall(r'<'+tag+r'[ >]', html)); c = len(re.findall(r'</'+tag+r'>', html))
    if o != c: bad += 1
    print(f'{tag}: {o} vs {c}', '' if o==c else '<-- MISMATCH')
print('OK' if bad==0 else f'{bad} MISMATCH')
"
```
Expected: 모든 태그의 열림/닫힘 개수가 일치하고 마지막 줄이 `OK`다.

- [ ] **Step 6: 커밋**

```bash
git add Docs/DevGuide.md DevGuide.html
git commit -m "$(cat <<'EOF'
개발 가이드에 카드 선택 시스템 반영

컴포넌트 경계(캐릭터는 카드를 모른다), 가중 랜덤 비복원 추출에서 매번
총합을 다시 계산해야 하는 이유, 대기열이 필요한 이유(킬 한 번에 레벨
2->5), 위젯이 인덱스만 돌려주는 설계와 그 덕에 무기 교체 화면이 따로
없다는 점을 문서화했다.

FCardData 스키마와 카드 위젯 두 개를 각각 데이터 테이블 절과 UI 표에
추가했다. 개발용 콘솔 명령(GJDrawCards, GJShowCards)도 남겼다.

레벨업 선택지가 없다는 TODO를 지우고, 능력 카드 미구현/스테이지 클리어
트리거 부재/리롤 없음/카드 데이터가 임시값이라는 새 갭을 기록했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```
