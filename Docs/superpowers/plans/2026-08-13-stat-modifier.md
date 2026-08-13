# 스탯 보너스 레이어 (M2.5) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 카드/버프가 더한 스탯이 레벨업에 지워지지 않도록, 테이블 원본과 보너스를 분리하고 실효값을 한 곳에서 계산한다.

**Architecture:** `AGJCharacter`를 3층으로 나눈다 — `BaseStat`(테이블 원본, 레벨업만 씀), `StatBonus`(카드 누적, `AddStatBonus`만 씀), `CurrentCharacterStat`(실효값, `RecalculateStats`만 씀). `CurrentCharacterStat`은 **이미 존재하는 멤버이고 의미만 바뀌므로**, 이걸 읽는 코드는 고치지 않아도 보너스가 자동으로 실린다.

**Tech Stack:** UE 5.8, C++ (`USTRUCT(BlueprintType)`, 중첩 USTRUCT, `UFUNCTION(Exec)` 콘솔 명령, 포인터-투-멤버)

**설계 문서:** `Docs/superpowers/specs/2026-08-13-stat-modifier-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다. 각 태스크의 검증은 자동화 테스트가 아니라 구체적인 PIE 조작 시나리오로 기술한다.
- **실효값 = `(테이블값 + Add) x (1 + Percent)`.** `Percent`는 1.0이 아니라 **0에서 시작하는 증가율**이다(0.15 = +15%). 증가율은 **곱하지 않고 합산**한다 — `+15%` 두 장이면 1.30이지 1.3225가 아니다.
- **`RecalculateStats`가 실효값을 쓰는 유일한 지점이다.** 다른 어떤 함수도 `CurrentCharacterStat`이나 `MaxHP`/`Defense`/`CritChance`/`CritMultiplier`/`MaxWalkSpeed`에 직접 대입하지 않는다.
- **레벨업도 카드도 회복이 아니다.** HP/MP는 최대치 **증가분만** 현재값에 더한다. `bRestoreToFull=true`는 스폰/리스폰 경로에서만 쓴다.
- **새 `USTRUCT`은 기존 데이터 테이블에 영향이 없다.** `FStatValues`/`FStatModifier`는 아직 어떤 테이블 행에도 들어가지 않으므로 마이그레이션이나 에디터 재시작이 필요 없다. 이 구조체가 테이블에 들어가는 건 M2.6이다.
- **인코딩**: 초기 파일들의 한글 주석은 이미 U+FFFD로 손실 변환된 상태다. 파일은 사실상 UTF-8이므로 **새 주석은 UTF-8 한글로 그냥 쓰면 된다.** 깨진 옛 주석 줄은 의미를 알 수 없으므로 건드리지 말 것.
- **커밋 메시지는 한국어**로 쓴다. 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다.
- 브랜치를 나누지 않고 `main`에서 직접 작업한다(솔로 프로젝트, 사용자 요청).
- 에디터가 열려 있으면 UBT 빌드가 막힌다. 컴파일은 사용자에게 **Ctrl+Alt+F11**을 요청한다.

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJGameTypes.h` (수정) | `FStatValues`(0에서 시작하는 9개 스탯) + `FStatModifier`(Add/Percent) 선언 | 1 |
| `Source/Project_GJ/GJGameTypes.cpp` (수정) | `FStatValues::operator+=` 정의 | 1 |
| `Source/Project_GJ/GJCharacter.h` (수정) | `BaseStat`/`StatBonus` 멤버, `RecalculateStats` 선언, `GetBaseAttackPower` → `GetAttackPower` 개명 | 2 |
| `Source/Project_GJ/GJCharacter.cpp` (수정) | `UpdateCharacterStat` 축소, `RecalculateStats` 신규 | 2 |
| `Source/Project_GJ/GJWeapon_Ranged.cpp` (수정) | 개명된 getter 호출 | 2 |
| `Source/Project_GJ/GJCharacter.h/.cpp` (수정) | `AddStatBonus` + `GJAddBonus` 콘솔 명령 | 3 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 4 |

**태스크 순서 근거**: 그릇이 될 구조체(1) → 계산 경로 재편(2, 동작 변화 없음이 검증 기준) → 보너스를 넣는 입구와 검증 수단(3) → 문서(4). Task 2가 끝난 시점에는 **기능이 하나도 안 늘고 기존 동작이 그대로여야** 한다 — 이게 리팩터링이 안전했다는 증거다.

---

## Task 1: 스탯 모디파이어 구조체

이 태스크가 끝나도 **동작 변화가 전혀 없다.** 아무도 이 구조체를 아직 안 쓴다. 검증은 컴파일 통과다.

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Modify: `Source/Project_GJ/GJGameTypes.cpp`

**Interfaces:**
- Consumes: 없음
- Produces:
  - `FStatValues` — `float` 9개(`MaxHP`, `MaxMP`, `BaseAttackPower`, `RequiredEXP`, `Defense`, `MoveSpeed`, `CooldownReduction`, `CritChance`, `CritMultiplier`), 전부 기본값 `0.f`
  - `FStatValues& FStatValues::operator+=(const FStatValues&)`
  - `FStatModifier` — `FStatValues Add`, `FStatValues Percent`

- [ ] **Step 1: `GJGameTypes.h`에 두 구조체 추가**

`GJGameTypes.h`에서 `FCharacterStat`의 닫는 줄과 그 다음 섹션 주석을 찾는다(49번째 줄 부근):

```cpp
    // 치명타가 터졌을 때 데미지 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
};

// -----------------------------------------
// 무기 스탯 데이터 테이블 구조체
// -----------------------------------------
```

이를 다음으로 교체한다:

```cpp
    // 치명타가 터졌을 때 데미지 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
};

// -----------------------------------------
// 스탯 보너스 (카드/버프가 더하는 값)
// -----------------------------------------

// 전부 0에서 시작하는 스탯 값 묶음.
// FCharacterStat을 재사용하지 않는 이유: 그쪽 기본값이 MaxHP=100, MoveSpeed=600,
// CritMultiplier=2라서 "보너스 없음"을 표현할 수 없다. 보너스 구조체는 기본 생성했을 때
// 아무 효과가 없어야 한다.
// FCharacterStat의 9개 필드를 전부 미러링한다 - 일부만 지원하면 "이 스탯은 왜 카드로 못
// 올리지?"라는 비대칭이 생기고, 나중에 필드를 추가하면 USTRUCT 레이아웃이 바뀌어 그때는
// 이 구조체를 쓰는 데이터 테이블(M2.6의 DT_CardData)까지 영향을 받는다.
USTRUCT(BlueprintType)
struct FStatValues
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxMP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float RequiredEXP = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MoveSpeed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CooldownReduction = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 0.f;

    // 모디파이어를 합칠 때 쓴다. 필드별 단순 덧셈이다.
    FStatValues& operator+=(const FStatValues& Other);
};

// 카드/버프 하나가 주는 효과.
// Percent는 1.0이 아니라 0에서 시작하는 증가율이다(0.15 = +15%). 이 선택이 두 가지를
// 공짜로 만든다 - 기본 생성한 모디파이어가 무효과가 되고, 모디파이어를 합치는 게 그냥
// 필드 덧셈이 된다.
// 증가율은 곱하지 않고 합산한다: +15% 두 장이면 1.30이지 1.3225가 아니다. 합산이
// 밸런싱이 예측 가능하고, 카드를 많이 먹었을 때 지수적으로 터지지 않는다.
USTRUCT(BlueprintType)
struct FStatModifier
{
    GENERATED_BODY()

    // 가산 (+5 체력)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Add;

    // 증가율 (0.15 = +15%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Percent;
};

// -----------------------------------------
// 무기 스탯 데이터 테이블 구조체
// -----------------------------------------
```

- [ ] **Step 2: `GJGameTypes.cpp`에 `operator+=` 정의**

`GJGameTypes.cpp`는 지금 include 한 줄뿐이다. 파일 전체를 다음으로 교체한다:

```cpp
// Fill out your copyright notice in the Description page of Project Settings.


#include "GJGameTypes.h"

FStatValues& FStatValues::operator+=(const FStatValues& Other)
{
    // 필드가 늘어나면 여기에도 한 줄 추가해야 한다. 컴파일러가 안 잡아주는 지점이다.
    MaxHP             += Other.MaxHP;
    MaxMP             += Other.MaxMP;
    BaseAttackPower   += Other.BaseAttackPower;
    RequiredEXP       += Other.RequiredEXP;
    Defense           += Other.Defense;
    MoveSpeed         += Other.MoveSpeed;
    CooldownReduction += Other.CooldownReduction;
    CritChance        += Other.CritChance;
    CritMultiplier    += Other.CritMultiplier;
    return *this;
}
```

- [ ] **Step 3: 컴파일**

사용자에게 요청한다:
> 에디터에서 **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

새 `USTRUCT` 두 개를 추가했으므로 **UHT가 먼저 돈다.** `GENERATED_BODY()` 누락이나 `UPROPERTY` 문법 오류가 있으면 여기서 바로 잡힌다.

이 태스크는 아무도 이 구조체를 안 쓰므로 PIE로 확인할 것이 없다. **컴파일 통과가 검증이다.**

- [ ] **Step 4: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJGameTypes.cpp
git commit -m "$(cat <<'EOF'
스탯 모디파이어 구조체 추가

FStatValues(전부 0에서 시작하는 9개 스탯)와 FStatModifier(가산 + 증가율)를
추가했다. 아직 아무도 쓰지 않아서 동작 변화는 없다.

FCharacterStat을 재사용하지 않은 이유는 그쪽 기본값이 MaxHP=100,
MoveSpeed=600, CritMultiplier=2라서 "보너스 없음"을 표현할 수 없기
때문이다. 보너스는 기본 생성했을 때 무효과여야 한다.

같은 이유로 Percent를 1.0이 아니라 0에서 시작하는 증가율로 뒀다.
그래야 모디파이어 합치기가 그냥 필드 덧셈이 된다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: 3층 분리와 `RecalculateStats`

**이 태스크가 끝나도 게임 동작은 하나도 안 바뀌어야 한다.** 계산 경로만 재편하는 리팩터링이고, "기존과 똑같이 동작한다"가 검증 기준이다.

**Files:**
- Modify: `Source/Project_GJ/GJCharacter.h`
- Modify: `Source/Project_GJ/GJCharacter.cpp` (`UpdateCharacterStat`)
- Modify: `Source/Project_GJ/GJWeapon_Ranged.cpp` (`Fire`)

**Interfaces:**
- Consumes: `FStatValues`, `FStatModifier` (Task 1)
- Produces:
  - `AGJCharacter::BaseStat` (`FCharacterStat`, `protected`) — 테이블 원본
  - `AGJCharacter::StatBonus` (`FStatModifier`, `protected`) — Task 3의 `AddStatBonus`가 씀
  - `void AGJCharacter::RecalculateStats(bool bRestoreToFull)` (`protected`)
  - `float AGJCharacter::GetAttackPower() const` — **`GetBaseAttackPower`에서 개명됨**

- [ ] **Step 1: `GJCharacter.h`에 `BaseStat`/`StatBonus` 멤버 추가**

`GJCharacter.h`에서 다음 두 줄을 찾는다(212번째 줄 부근):

```cpp
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat CurrentCharacterStat;
```

이를 다음으로 교체한다:

```cpp
    // --- 스탯 3층 구조 ---
    // 아래 세 멤버는 각자 쓰는 주체가 하나씩만 있다. 이 규칙이 깨지면 보너스가 조용히 사라진다.

    // (1) 테이블 원본. UpdateCharacterStat만 쓴다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat BaseStat;

    // (2) 카드/버프가 누적한 보너스. AddStatBonus만 쓴다.
    // 런마다 캐릭터가 새로 스폰되면서 기본 생성되므로 초기화 코드가 따로 없다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FStatModifier StatBonus;

    // (3) 실효값 = (BaseStat + StatBonus.Add) x (1 + StatBonus.Percent).
    // RecalculateStats만 쓰고, AddEXP/UpdatePlayerHUD/GetAttackPower가 읽는다.
    // 예전에는 이 멤버가 테이블 원본이었다 - 의미가 "실효값"으로 바뀐 것이므로,
    // 여기에 직접 대입하는 코드를 새로 만들면 안 된다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    FCharacterStat CurrentCharacterStat;
```

- [ ] **Step 2: `GJCharacter.h`에 `RecalculateStats` 선언 추가**

`GJCharacter.h`에서 다음 블록을 찾는다(233번째 줄 부근):

```cpp
    // bRestoreToFull=true면 HP/MP를 가득 채운다(스폰/리스폰용, 기존 동작 그대로).
    // false면 최대치가 오른 만큼만 현재값에 더한다(레벨업용) - 레벨업이 완전 회복 수단이 되면
    // "위험할 때 잡몹 하나 잡기"가 최고의 회복법이 되어 체력 관리 긴장이 사라진다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true);
```

이를 다음으로 교체한다:

```cpp
    // bRestoreToFull=true면 HP/MP를 가득 채운다(스폰/리스폰용, 기존 동작 그대로).
    // false면 최대치가 오른 만큼만 현재값에 더한다(레벨업용) - 레벨업이 완전 회복 수단이 되면
    // "위험할 때 잡몹 하나 잡기"가 최고의 회복법이 되어 체력 관리 긴장이 사라진다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true);

    // BaseStat과 StatBonus를 합쳐 CurrentCharacterStat과 전투 스탯 멤버들을 다시 계산한다.
    // 실효값을 쓰는 유일한 지점이다 - 계산 규칙을 바꾸거나 모디파이어를 목록 기반으로
    // 갈아끼우게 되면 고칠 곳이 이 함수 하나다.
    void RecalculateStats(bool bRestoreToFull);
```

- [ ] **Step 3: `GJCharacter.h`의 getter 개명**

`GJCharacter.h`에서 다음 세 줄을 찾는다(244번째 줄 부근):

```cpp
    // 무기가 발사 시 공격력 배율을 계산할 때 읽는다 (CurrentCharacterStat이 protected라 getter가 필요함)
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetBaseAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }
```

이를 다음으로 교체한다:

```cpp
    // 무기가 발사 시 공격력 배율을 계산할 때 읽는다 (CurrentCharacterStat이 protected라 getter가 필요함)
    // 보너스가 실린 실효값이다. 예전 이름이 GetBaseAttackPower였는데, 반환값이 더 이상
    // "기본값"이 아니게 되어 이름이 거짓이 되므로 개명했다 - 그대로 뒀다면 나중에 누군가
    // "보너스 이전 값이 필요하다"며 이 함수를 써서 조용히 틀린 계산을 하게 된다.
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }
```

- [ ] **Step 4: `GJCharacter.cpp`의 `UpdateCharacterStat` 축소**

`GJCharacter.cpp`에서 `AGJCharacter::UpdateCharacterStat` 함수 전체(796번째 줄 부근, `void AGJCharacter::UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull)`부터 닫는 `}`까지)를 찾아 다음으로 교체한다:

```cpp
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
```

- [ ] **Step 5: `GJCharacter.cpp`에 `RecalculateStats` 추가**

Step 4에서 교체한 `UpdateCharacterStat`의 닫는 `}` **아래**, `bool AGJCharacter::IsMaxLevel() const` **위**에 추가한다:

```cpp

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
    const float OldMaxHP = MaxHP;
    MaxHP = S.MaxHP;
    CurrentHP = bRestoreToFull ? MaxHP : FMath::Clamp(CurrentHP + (MaxHP - OldMaxHP), 0.f, MaxHP);

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
```

- [ ] **Step 6: `GJWeapon_Ranged.cpp`의 호출부 개명**

`GJWeapon_Ranged.cpp`에서 다음 줄을 찾는다(132번째 줄 부근):

```cpp
            AttackPower = OwningCharacter->GetBaseAttackPower();
```

이를 다음으로 교체한다:

```cpp
            AttackPower = OwningCharacter->GetAttackPower();
```

- [ ] **Step 7: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

`GetBaseAttackPower`를 개명했으므로, 이 함수를 부르는 곳이 Step 6에서 고친 한 곳 말고 더 있으면 **여기서 컴파일 에러로 드러난다.** 에러가 나면 그 호출부도 `GetAttackPower`로 바꾼다. (블루프린트에서 이 노드를 쓰고 있었다면 컴파일 에러가 아니라 **블루프린트 컴파일 경고**로 나타나므로, Step 8에서 에디터 메시지 로그도 함께 확인한다.)

- [ ] **Step 8: 회귀 확인 — 동작이 하나도 안 바뀌었는가**

사용자에게 요청한다:
> `TestLev`에서 플레이해줘. **새 기능은 하나도 없고, 예전과 똑같이 동작해야** 정상이야.
>
> - 시작 시 체력/마나 바가 가득 차 있는가
> - 적을 잡아 레벨업했을 때 HUD의 레벨 숫자와 경험치 바가 예전처럼 움직이는가
> - 총을 쐈을 때 적이 예전과 같은 발수에 죽는가 (공격력 getter 개명이 제대로 걸렸는지 확인)
> - 이동 속도가 평소와 같은가
>
> 그리고 **Window → Message Log**에 블루프린트 관련 에러가 없는지도 봐줘.

Run: `grep -E "LevelUp!" Saved/Logs/Project_GJ.log | tail -6`
Expected: 체력이 깎인 상태로 레벨 1→5를 올리면 이전과 같은 `HP=` 값 흐름이 나온다(예: 40/100에서 시작하면 `60/120`, `85/145`, `115/175`, `150/210`).

**이 값들이 예전과 다르면 리팩터링이 뭔가를 바꾼 것이다.** 특히 HP 증가분 로직이 `RecalculateStats`로 옮겨가면서 순서가 틀어졌는지 확인해야 한다.

- [ ] **Step 9: 커밋**

```bash
git add Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Source/Project_GJ/GJWeapon_Ranged.cpp
git commit -m "$(cat <<'EOF'
스탯을 테이블 원본/보너스/실효값 3층으로 분리

BaseStat(테이블 원본), StatBonus(카드 누적), CurrentCharacterStat(실효값)로
나누고 RecalculateStats를 실효값의 유일한 쓰기 지점으로 만들었다.
보너스를 넣는 입구는 아직 없어서 동작 변화는 없다.

CurrentCharacterStat은 기존 멤버의 의미만 "테이블 원본"에서 "실효값"으로
바꾼 것이다. 덕분에 AddEXP, UpdatePlayerHUD, 무기 발사 등 이걸 읽는
코드를 고치지 않아도 보너스가 자동으로 실린다.

계산 시 하한을 건다. MaxHP/MaxMP가 0이면 HUD가 0으로 나누고,
RequiredEXP가 0 이하면 AddEXP의 루프 가드에 걸려 레벨업이 조용히
멈춘다. 공격력과 치명타 배율이 음수면 데미지가 음수가 되어 맞은 쪽을
회복시킨다.

GetBaseAttackPower를 GetAttackPower로 개명했다. 반환값이 보너스가 실린
실효값이 되어 이름의 "Base"가 거짓이 되기 때문이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 보너스 입구와 검증용 콘솔 명령

이 태스크가 끝나면 **보너스가 실제로 동작하고 레벨업에도 살아남는다.**

**Files:**
- Modify: `Source/Project_GJ/GJCharacter.h`
- Modify: `Source/Project_GJ/GJCharacter.cpp`

**Interfaces:**
- Consumes: `AGJCharacter::StatBonus`, `AGJCharacter::RecalculateStats` (Task 2), `FStatModifier`, `FStatValues::operator+=` (Task 1)
- Produces:
  - `void AGJCharacter::AddStatBonus(const FStatModifier& Delta)` — M2.6 카드 시스템이 호출한다
  - `void AGJCharacter::GJAddBonus(const FString& StatName, float AddValue, float PercentValue)` — 개발용 콘솔 명령

- [ ] **Step 1: `GJCharacter.h`에 공개 API 추가**

`GJCharacter.h`에서 다음 블록을 찾는다(258번째 줄 부근, `public:` 블록의 `OnLevelUp` 선언):

```cpp
    // 레벨업 시점. 아직 구독자가 없다 - 카드 선택 시스템이 여기 붙는다.
    UPROPERTY(BlueprintAssignable, Category = "Level")
    FOnLevelUpSignature OnLevelUp;
```

그 **아래**(같은 `public:` 블록 안)에 추가한다:

```cpp

    // 카드/버프가 준 스탯 보너스를 누적한다. 개별 제거는 지원하지 않는다 - 런마다 캐릭터가
    // 새로 스폰되면서 StatBonus가 기본 생성되므로 초기화가 필요 없기 때문이다. 나중에
    // 시간제 버프가 필요해지면 목록 기반으로 바꾸되, 그때도 실효값을 읽는 코드는 안 바뀐다.
    UFUNCTION(BlueprintCallable, Category = "Stat")
    void AddStatBonus(const FStatModifier& Delta);

    // 개발용 콘솔 명령. 카드 시스템 없이 보너스를 시험한다.
    // 예) GJAddBonus MaxHP 5 0              -> 최대 체력 +5
    //     GJAddBonus BaseAttackPower 0 0.15 -> 공격력 +15%
    // 카드가 생긴 뒤에도 "이 조합이면 어떻게 되나"를 카드 없이 시험할 수 있어 남겨둔다.
    UFUNCTION(Exec)
    void GJAddBonus(const FString& StatName, float AddValue, float PercentValue);
```

- [ ] **Step 2: `GJCharacter.cpp`에 `AddStatBonus`와 `GJAddBonus` 추가**

`GJCharacter.cpp`에서 `void AGJCharacter::LevelUp()` 함수의 닫는 `}` **아래**, `void AGJCharacter::ApplyConsumableEffect(...)` **위**에 추가한다:

```cpp

void AGJCharacter::AddStatBonus(const FStatModifier& Delta)
{
    StatBonus.Add += Delta.Add;
    StatBonus.Percent += Delta.Percent;

    // 카드는 회복이 아니다 - 최대치 증가분만 현재 HP/MP에 반영된다.
    // ("+5 최대 체력" 카드가 현재 체력도 +5 시키는 건 RecalculateStats가 처리한다)
    RecalculateStats(/*bRestoreToFull=*/false);
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
            TEXT("GJAddBonus: 알 수 없는 스탯 '%s'. 사용 가능: MaxHP, MaxMP, BaseAttackPower, RequiredEXP, Defense, MoveSpeed, CooldownReduction, CritChance, CritMultiplier"),
            *StatName);
        return;
    }

    AddStatBonus(Delta);

    UE_LOG(LogTemp, Log,
        TEXT("GJAddBonus: %s (가산 %.2f, 증가율 %.0f%%) -> HP=%.0f/%.0f, 공격력=%.1f, 방어력=%.1f, 치명타=%.2f/x%.2f, 이동속도=%.0f, RequiredEXP=%.0f"),
        *StatName, AddValue, PercentValue * 100.f,
        CurrentHP, MaxHP,
        CurrentCharacterStat.BaseAttackPower, Defense,
        CritChance, CritMultiplier,
        CurrentCharacterStat.MoveSpeed, CurrentCharacterStat.RequiredEXP);
}
```

- [ ] **Step 3: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 4: 보너스가 먹는지 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 **물결표(`~`) 키로 콘솔**을 연 다음 입력해줘:
>
> ```
> GJAddBonus MaxHP 5 0
> ```
>
> 체력 바가 살짝 늘어야 하고, Output Log에 `GJAddBonus: MaxHP ... HP=105/105` 가 떠야 해.
>
> 그 다음 **체력을 좀 깎고** 같은 명령을 한 번 더 쳐줘. 이번엔 최대 체력이 110이 되면서 **현재 체력도 정확히 5만 올라야** 해 (예: 80/105 → 85/110). 가득 차면 안 돼.

Run: `grep -E "GJAddBonus" Saved/Logs/Project_GJ.log | tail -5`
Expected: `HP=105/105`, 이어서 `HP=85/110` 형태.

가득 채워지면 `AddStatBonus`가 `RecalculateStats(false)`가 아니라 `true`를 넘긴 것이다.

- [ ] **Step 5: 레벨업을 넘어 살아남는지 확인 — 이 작업의 존재 이유**

사용자에게 요청한다:
> 같은 판에서 **`GJAddBonus MaxHP 5 0`을 친 뒤 적을 2마리 잡아 레벨업**해줘.
>
> 레벨 2의 테이블 최대 체력이 120이니까, 보너스 +5가 살아있으면 **125**가 나와야 해. 120이 나오면 보너스가 지워진 거야.

Run: `grep -E "LevelUp!" Saved/Logs/Project_GJ.log | tail -3`
Expected: `LevelUp! Level=2, HP=.../125` — 분모가 **125**여야 한다.

**분모가 120이면 이 작업 전체가 실패한 것이다.** `UpdateCharacterStat`이 `CurrentCharacterStat`이나 `MaxHP`에 직접 대입하는 코드가 남아있는지 확인한다.

- [ ] **Step 6: 증가율과 공격력 확인**

사용자에게 요청한다:
> 새 판에서 **적을 죽이는 데 몇 발 걸리는지 세고**, 그 다음 콘솔에 이걸 친 뒤 다시 세줘:
>
> ```
> GJAddBonus BaseAttackPower 0 0.5
> ```
>
> 공격력이 50% 오르니까 발수가 눈에 띄게 줄어야 해. Output Log의 `공격력=` 값도 1.5배가 됐는지 봐줘.

Run: `grep -E "GJAddBonus" Saved/Logs/Project_GJ.log | tail -2`
Expected: `공격력=15.0` (레벨 1의 기본 공격력 10 x 1.5).

값이 10 그대로면 `Combine`이 `Percent`를 안 쓰고 있거나, 무기가 개명 전 getter를 부르고 있다.

- [ ] **Step 7: 하한 확인**

사용자에게 요청한다:
> 콘솔에 극단적인 음수를 넣어봐. **크래시가 없어야 하고**, 게임이 계속 돌아가야 해.
>
> ```
> GJAddBonus MaxHP 0 -10
> ```
> → 최대 체력이 0이 아니라 **1**로 하한이 걸려야 해 (`HP=1/1`).
>
> ```
> GJAddBonus RequiredEXP 0 -10
> ```
> → 그 뒤에 적을 잡았을 때 **레벨업이 여전히 동작해야** 해. 조용히 멈추면 하한이 안 걸린 거야.

Run: `grep -E "GJAddBonus|LevelUp!" Saved/Logs/Project_GJ.log | tail -6`
Expected: `HP=1/1`이 찍히고 크래시 로그가 없다. `RequiredEXP` 실험 뒤에도 `LevelUp!`이 계속 나온다.

- [ ] **Step 8: 오타 경고 확인**

사용자에게 요청한다:
> 일부러 틀린 이름을 쳐줘: `GJAddBonus MaxHp2 5 0`
>
> Output Log에 **사용 가능한 스탯 목록이 담긴 경고**가 떠야 해.

Run: `grep -E "알 수 없는 스탯" Saved/Logs/Project_GJ.log | tail -2`
Expected: 경고 한 줄이 나온다.

- [ ] **Step 9: 런 초기화 확인**

사용자에게 요청한다:
> 보너스를 잔뜩 넣은 상태에서 일부러 죽어봐. 게임오버 → 허브 → 포탈로 새 런을 시작했을 때 **보너스가 전부 사라지고 레벨 1 기본 스탯(체력 100)** 이어야 해.

보너스가 남아있으면 캐릭터가 재스폰되지 않은 것이므로 M1 런 루프 쪽을 확인해야 한다.

- [ ] **Step 10: 커밋**

```bash
git add Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp
git commit -m "$(cat <<'EOF'
스탯 보너스 입구와 개발용 콘솔 명령 추가

AddStatBonus로 카드/버프가 보너스를 누적할 수 있게 했다. 보너스는
RecalculateStats를 거치므로 이후 레벨업에서도 지워지지 않는다.

카드 시스템이 아직 없어 보너스를 넣을 경로가 없으므로 GJAddBonus
콘솔 명령을 만들었다. 임시 코드가 아니라 밸런싱 내내 쓸 개발용 훅으로
남긴다. 스탯 이름이 틀리면 사용 가능한 목록을 경고로 찍는다 - 조용히
무시하면 오타를 "보너스가 안 먹네"로 오인하게 된다.

PIE 검증: 보너스가 레벨업을 넘어 유지되고(레벨 2에서 120이 아니라 125),
"+5 최대 체력"이 현재 체력도 +5 시키며, 극단적 음수에도 하한이 걸려
크래시나 레벨업 정지가 없음을 확인했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 개발 가이드 갱신

**Files:**
- Modify: `Docs/DevGuide.md`
- Modify: `DevGuide.html`

**Interfaces:**
- Consumes: Task 1~3의 모든 변경
- Produces: 없음 (문서화)

- [ ] **Step 1: 2.2절의 경고 문단을 실제 구조 설명으로 교체**

`Docs/DevGuide.md`의 2.2절 "스탯 / 레벨 / 경험치" 마지막에 있는 다음 문단(`> ⚠️`로 시작하는 줄)을 찾는다:

```markdown
> ⚠️ **카드 시스템(스탯 증가 카드)을 붙이기 전에 base+bonus 레이어가 필요하다.** 지금 `UpdateCharacterStat`은 `CurrentCharacterStat = *RowData`로 구조체를 통째로 덮어쓰고 `MaxHP`/`Defense`/`CritChance`/`MoveSpeed`를 테이블 값으로 재대입한다. 카드가 "+5 최대 체력"을 더해도 **다음 레벨업에서 지워진다.** 카드를 주는 시점이 레벨업이라 이 충돌은 반드시 발생한다. `GetBaseAttackPower()`도 `CurrentCharacterStat`을 직접 읽어 보너스가 안 실린다.
```

이를 다음으로 교체한다:

```markdown
**스탯은 3층 구조다.** 카드/버프가 준 보너스가 레벨업에 지워지지 않게 하기 위한 것이다.

| 층 | 멤버 | 쓰는 주체 | 읽는 쪽 |
|---|---|---|---|
| 테이블 원본 | `BaseStat` | `UpdateCharacterStat`만 | `RecalculateStats` |
| 보너스 누적 | `StatBonus` (`FStatModifier`) | `AddStatBonus`만 | `RecalculateStats` |
| **실효값** | `CurrentCharacterStat` | **`RecalculateStats`만** | `AddEXP`, `UpdatePlayerHUD`, `GetAttackPower` 등 전부 |

실효값 계산은 `실효값 = (테이블값 + Add) x (1 + Percent)`이며, `Percent`는 1.0이 아니라 **0에서 시작하는 증가율**이다(0.15 = +15%). 그래야 기본 생성한 `FStatModifier`가 무효과가 되고 모디파이어 합치기가 필드 덧셈이 된다. 증가율은 **곱하지 않고 합산**한다 — `+15%` 두 장이면 1.30이지 1.3225가 아니다.

`RecalculateStats`는 계산 후 **하한을 건다**: `MaxHP`/`MaxMP`/`RequiredEXP`는 최소 1, `BaseAttackPower`/`CritMultiplier`/`MoveSpeed`/`CritChance`는 최소 0. `RequiredEXP`가 0 이하가 되면 `AddEXP`의 루프 가드에 걸려 **레벨업이 조용히 멈추고**, 공격력이 -100 아래로 가면 데미지가 음수가 되어 **맞은 쪽을 회복시킨다.** `Defense`는 `ApplyDefense`가 이미 하한을 걸므로 여기선 안 건다. `CritChance`에 상한은 없다 — 1.0 초과는 빌드의 목표지 버그가 아니다.

> `RecalculateStats`가 실효값을 쓰는 **유일한 지점**이라는 게 이 구조의 전부다. `CurrentCharacterStat`이나 `MaxHP`/`Defense`/`CritChance`/`CritMultiplier`/`MaxWalkSpeed`에 다른 곳에서 직접 대입하면 그 순간 보너스가 조용히 사라진다.

**개발용 콘솔 명령**: `GJAddBonus <스탯이름> <가산> <증가율>` (예: `GJAddBonus MaxHP 5 0`, `GJAddBonus BaseAttackPower 0 0.15`). 카드 없이 보너스를 시험한다. 이름이 틀리면 사용 가능한 목록을 경고로 찍는다.

**보너스도 런마다 초기화된다.** 캐릭터가 새로 스폰되면서 `StatBonus`가 기본 생성되므로 초기화 코드가 없다. EXP와 같은 메커니즘이다.
```

- [ ] **Step 2: 8절에 새 구조체 스키마 추가**

`Docs/DevGuide.md`의 8절에서 `### FEnemyStat — DT_EnemyStat` 표가 끝난 직후(`### FItemData` 바로 위)에 추가한다:

```markdown
### `FStatValues` / `FStatModifier` — 데이터 테이블 행 아님 (스탯 보너스용)

`FStatValues`는 `FCharacterStat`과 **같은 9개 필드**(`MaxHP`, `MaxMP`, `BaseAttackPower`, `RequiredEXP`, `Defense`, `MoveSpeed`, `CooldownReduction`, `CritChance`, `CritMultiplier`)를 갖되 **전부 기본값이 0**이다. `FCharacterStat`을 재사용하지 않는 이유가 이것 — 그쪽 기본값이 `MaxHP=100`, `MoveSpeed=600`, `CritMultiplier=2`라서 "보너스 없음"을 표현할 수 없다.

`FStatModifier`는 `FStatValues Add`(가산)와 `FStatValues Percent`(증가율) 둘을 담는다. `FTableRowBase`를 상속하지 않으므로 그 자체로는 데이터 테이블 행이 아니지만, `BlueprintType` + `EditAnywhere`로 선언되어 **다른 테이블 행의 필드로 들어갈 수 있다** — M2.6의 `FCardData`가 이걸 품는다.
```

- [ ] **Step 3: 9절 TODO 목록 갱신**

`Docs/DevGuide.md`의 9절에서 다음 항목을 찾아 **삭제한다**:

```markdown
- **스탯 보너스(base+bonus) 레이어가 없음** — `UpdateCharacterStat`이 테이블 값으로 스탯을 통째로 덮어쓰기 때문에, 카드/버프가 더한 가산치는 다음 레벨업에서 지워진다. 카드 시스템(레벨업 선택지)의 **선행 조건**이며, 카드를 만들기 전에 넣어야 한다(2.2절 경고 참고)
```

같은 목록의 끝에 다음을 **추가한다**:

```markdown
- 모디파이어 개별 제거/시간제 버프가 없음 — `StatBonus`는 누적만 한다. 10초짜리 이동속도 버프나 무기 장착 중에만 붙는 스탯이 필요해지면 `TArray<FStatModifier>` + 핸들 방식으로 바꿔야 하며, 그때도 `RecalculateStats`만 고치면 되고 실효값을 읽는 코드는 안 바뀐다
- 적에게는 스탯 보너스가 없음 — `ApplyEnemyStat`이 테이블 값을 멤버에 직접 대입한다. 스테이지가 올라갈수록 적이 강해지는 스케일링(M5)이 필요해지면 같은 구조체를 재사용하면 된다
```

- [ ] **Step 4: 10절에 콘솔 명령 메모 추가**

`Docs/DevGuide.md`의 10절 마지막에 추가한다:

```markdown
- 스탯 밸런싱은 콘솔 명령 `GJAddBonus <스탯이름> <가산> <증가율>`로 카드 없이 시험할 수 있다 (PIE에서 `~` 키로 콘솔). `UFUNCTION(Exec)`이라 플레이어가 조종 중인 폰에서만 먹는다
```

- [ ] **Step 5: `DevGuide.html`에 동일 내용 반영**

`DevGuide.html`은 `Docs/DevGuide.md`와 같은 내용을 HTML로 옮긴 문서다. Step 1~4의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다.

2.2절에서는 `<p class="note">⚠️ <b>카드 시스템(스탯 증가 카드)을 붙이기 전에...` 로 시작하는 문단을 찾아 교체한다.

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
개발 가이드에 스탯 3층 구조 반영

BaseStat/StatBonus/CurrentCharacterStat의 역할과 "각 층에 쓰는 주체가
하나씩"이라는 규칙, 실효값 계산식과 증가율 합산 규칙, 하한을 거는 이유,
GJAddBonus 콘솔 명령을 문서화했다.

FStatValues가 FCharacterStat을 재사용하지 않는 이유(기본값이 0이어야 함)를
데이터 테이블 스키마 절에 남겼다.

base+bonus 레이어가 없다는 TODO를 지우고, 모디파이어 개별 제거와 적
스탯 스케일링이 없다는 새 갭을 기록했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```
