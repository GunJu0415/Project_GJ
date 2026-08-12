# 전투 스탯과 데미지 공식 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 방어력·이동속도·쿨타임감소·치명타 스탯을 추가하고, 이들을 엮는 데미지 공식을 한 곳에 정의해 기존 데미지 경로에 연결한다.

**Architecture:** 공격 계산(공격력 배율 + 치명타)은 공격자가, 방어 경감은 `AGJBaseCharacter::TakeDamage`가 담당한다. 방어력이 `TakeDamage` 한 곳에 있으므로 앞으로 어떤 데미지 소스가 추가되어도 경감이 자동 적용된다. 공식 자체는 `UGJCombatStatics` 한 파일에 모은다.

**Tech Stack:** UE 5.8, C++ (`UBlueprintFunctionLibrary`, `FTableRowBase` USTRUCT, 엔진 표준 `TakeDamage` 파이프라인)

**설계 문서:** `Docs/superpowers/specs/2026-08-08-combat-stats-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다(설계 문서 5절). 각 태스크의 검증은 자동화 테스트가 아니라 구체적인 PIE 조작 시나리오로 기술한다.
- **`USTRUCT` 레이아웃 변경은 실제 비용이 있다.** `FCharacterStat`/`FEnemyStat`에 필드를 추가하면 이를 참조하는 UMG 블루프린트 그래프의 핀이 깨질 수 있다(과거 `Break WeaponStat`이 이 문제를 겪음). 증상은 "정확히 일치하는 구조체만 호환" 컴파일 에러이며, **에디터 완전 재시작**으로 대부분 해결된다. 또한 데이터 테이블의 기존 행에 새 칸이 생기므로 값을 채워야 한다.
- **인코딩**: 초기 파일들의 한글 주석은 이미 U+FFFD로 손실 변환된 상태다. 파일은 사실상 UTF-8이므로 **새 주석은 UTF-8 한글로 그냥 쓰면 된다.** 다만 깨진 옛 주석 줄은 의미를 알 수 없으므로 건드리지 말 것.
- **커밋 메시지는 한국어**로 쓴다. 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다.
- 새 C++ 파일은 `Source/Project_GJ/` 루트에 만든다.
- `CritChance`의 범위는 **0.0~1.0**이다(0.25 = 25%). 퍼센트 정수가 아니다.
- 브랜치를 나누지 않고 `main`에서 직접 작업한다(솔로 프로젝트, 사용자 요청).

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJCombatStatics.h/.cpp` (신규) | 데미지 공식 단일 소스 | 1 |
| `Source/Project_GJ/GJGameTypes.h` (수정) | `FCharacterStat`/`FEnemyStat`에 스탯 필드 추가 | 2 |
| `Source/Project_GJ/GJBaseCharacter.h` (수정) | 공용 런타임 스탯(`Defense`/`CritChance`/`CritMultiplier`) | 2 |
| `Source/Project_GJ/GJBaseCharacter.cpp` (수정) | `TakeDamage`에 방어 경감 적용 | 2 |
| `Source/Project_GJ/GJCharacter.h/.cpp` (수정) | 플레이어 스탯 적용 + 공격력 getter | 3 |
| `Source/Project_GJ/GJEnemyCharacter.cpp` (수정) | 적 스탯 적용 | 3 |
| `Source/Project_GJ/GJWeapon_Ranged.cpp` (수정) | 발사 시 공격 계산 연결 | 4 |
| `Source/Project_GJ/GJEnemyCharacter.cpp` (수정) | 적 공격 시 공격 계산 연결 | 4 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 5 |

**태스크 순서 근거**: 공식(1) → 스탯 그릇과 방어 적용(2) → 스탯 채우기(3) → 공격 계산 연결(4) → 문서(5). 2번이 끝나면 방어력이 이미 동작하고, 4번이 끝나면 공격 측이 완성된다.

---

## Task 1: 데미지 공식 라이브러리

**Files:**
- Create: `Source/Project_GJ/GJCombatStatics.h`
- Create: `Source/Project_GJ/GJCombatStatics.cpp`

**Interfaces:**
- Consumes: 없음 (독립 유틸리티)
- Produces:
  - `static float UGJCombatStatics::CalculateOutgoingDamage(float BaseDamage, float AttackPower, float CritChance, float CritMultiplier, bool& bOutWasCritical)`
  - `static float UGJCombatStatics::ApplyDefense(float IncomingDamage, float Defense)`

- [ ] **Step 1: `GJCombatStatics.h` 생성**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GJCombatStatics.generated.h"

// 데미지 공식의 단일 소스. 공격 계산은 공격자가, 방어 경감은 맞는 쪽(TakeDamage)이 호출하지만
// 공식 자체는 이 파일 한 곳에만 존재한다 - 밸런스를 조정할 때 여기만 보면 된다.
//
// 전체 공식:
//   공격데미지 = 무기데미지 x (1 + 공격력/100) x 치명타배율
//   최종데미지 = 공격데미지 x 100/(100 + 방어력)
UCLASS()
class PROJECT_GJ_API UGJCombatStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 공격 측 계산 - 공격력 배율을 곱하고 치명타를 굴린다.
    // CritChance는 0.0~1.0 범위다(0.25 = 25%). bOutWasCritical로 치명타 여부를 돌려준다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static float CalculateOutgoingDamage(
        float BaseDamage,
        float AttackPower,
        float CritChance,
        float CritMultiplier,
        bool& bOutWasCritical);

    // 방어 측 경감 - 체감형이라 방어력을 아무리 올려도 100% 무효화에 도달하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static float ApplyDefense(float IncomingDamage, float Defense);
};
```

- [ ] **Step 2: `GJCombatStatics.cpp` 생성**

```cpp
#include "GJCombatStatics.h"

float UGJCombatStatics::CalculateOutgoingDamage(
    float BaseDamage,
    float AttackPower,
    float CritChance,
    float CritMultiplier,
    bool& bOutWasCritical)
{
    // 공격력은 배율로 들어간다 - 센 무기일수록 성장이 더 크게 돌아와서 무기 선택에 의미가 생긴다
    const float PowerScaledDamage = BaseDamage * (1.f + AttackPower / 100.f);

    // FMath::FRand()는 0.0~1.0을 돌려주므로 CritChance와 직접 비교하면 된다
    bOutWasCritical = (CritChance > 0.f) && (FMath::FRand() < CritChance);

    const float FinalMultiplier = bOutWasCritical ? CritMultiplier : 1.f;

    return PowerScaledDamage * FinalMultiplier;
}

float UGJCombatStatics::ApplyDefense(float IncomingDamage, float Defense)
{
    if (IncomingDamage <= 0.f)
    {
        return 0.f;
    }

    // 음수 방어력이 들어오면 데미지가 증폭되어버리므로 0으로 막는다
    const float SafeDefense = FMath::Max(Defense, 0.f);

    // 체감형 경감: 방어력 100마다 "체력이 1배씩 더 있는" 효과.
    // 분모가 항상 100 이상이라 0으로 나눌 일이 없다.
    const float Mitigated = IncomingDamage * (100.f / (100.f + SafeDefense));

    // 방어력이 극단적으로 높을 때 데미지가 0에 수렴해 사실상 무적이 되는 것을 막는다.
    // 다만 하한을 그냥 1로 두면 0.5짜리 약한 공격이 오히려 1로 늘어나는 역전이 생기므로,
    // 들어온 데미지 자체가 1보다 작으면 그 값을 하한으로 쓴다. 방어력은 어떤 경우에도 데미지를 늘리지 않는다.
    const float MinimumDamage = FMath::Min(1.f, IncomingDamage);

    return FMath::Max(Mitigated, MinimumDamage);
}
```

- [ ] **Step 3: 컴파일**

사용자에게 요청한다:
> 에디터에서 **Ctrl+Alt+F11**로 컴파일해줘. `UGJCombatStatics`는 새 UCLASS 파일이라 인식이 안 되면 에디터 재시작이 필요할 수 있어.

Run: `grep -E "LogLiveCoding|error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

이 태스크는 아직 아무 곳에서도 호출되지 않으므로 PIE 확인은 없다. 컴파일 통과가 검증이다.

- [ ] **Step 4: 커밋**

```bash
git add Source/Project_GJ/GJCombatStatics.h Source/Project_GJ/GJCombatStatics.cpp
git commit -m "$(cat <<'EOF'
데미지 공식 라이브러리 추가

공격 계산(공격력 배율 + 치명타)과 방어 경감(체감형)을 UGJCombatStatics
한 곳에 모았다. 아직 호출부는 연결하지 않았다.

방어력 경감은 100/(100+방어력) 형태라 무적에 도달하지 않으며,
최소 데미지 하한이 원래 데미지를 넘지 않도록 해서 약한 공격이
방어력 때문에 오히려 세지는 역전을 막았다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: 스탯 필드 추가와 방어력 적용

이 태스크가 끝나면 **방어력이 실제로 동작한다.** 공격 측(공격력·치명타)은 Task 4에서 연결된다.

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h` (`FCharacterStat`, `FEnemyStat`)
- Modify: `Source/Project_GJ/GJBaseCharacter.h`
- Modify: `Source/Project_GJ/GJBaseCharacter.cpp` (`TakeDamage`)

**Interfaces:**
- Consumes: `UGJCombatStatics::ApplyDefense(float, float)` (Task 1)
- Produces:
  - `AGJBaseCharacter::Defense` (`float`, `protected`, `BlueprintReadOnly`)
  - `AGJBaseCharacter::CritChance` (`float`, `protected`, `BlueprintReadOnly`)
  - `AGJBaseCharacter::CritMultiplier` (`float`, `protected`, `BlueprintReadOnly`)
  - `FCharacterStat`의 새 필드: `Defense`, `MoveSpeed`, `CooldownReduction`, `CritChance`, `CritMultiplier`
  - `FEnemyStat`의 새 필드: `Defense`, `CritChance`, `CritMultiplier`

- [ ] **Step 1: `FCharacterStat`에 필드 추가**

`GJGameTypes.h`의 `FCharacterStat`에서 `RequiredEXP` 선언 **아래**, 닫는 `};` **위**에 삽입한다:

```cpp

    // 받는 데미지를 경감시킨다. 체감형이라 아무리 올려도 무적이 되지 않음 (100이면 50% 경감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    // 캐릭터 이동 속도. 지금까지 플레이어는 이 값을 어디서도 설정하지 않아 엔진 기본값(600)을
    // 그대로 썼는데, 기본값을 600으로 맞춰 두었으므로 기존 플레이 감각은 바뀌지 않는다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MoveSpeed = 600.f;

    // 스킬 쿨타임 감소. 적용 대상이 될 스킬 시스템이 아직 없어서 지금은 어디에도 연결되지 않는다.
    // 나중에 스탯을 또 추가하면 데이터 테이블 마이그레이션을 두 번 해야 하므로 미리 자리를 잡아둔 것.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CooldownReduction = 0.f;

    // 치명타 확률. 0.0~1.0 범위다 (0.25 = 25%). 퍼센트 정수가 아님에 주의.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    // 치명타가 터졌을 때 데미지 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
```

- [ ] **Step 2: `FEnemyStat`에 필드 추가**

`GJGameTypes.h`의 `FEnemyStat`에서 `AttackWindup` 선언 **아래**, 닫는 `};` **위**에 삽입한다:

```cpp

    // 받는 데미지를 경감시킨다. 적마다 단단함을 다르게 줄 수 있다 (100이면 50% 경감)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float Defense = 0.f;

    // 치명타 확률. 0.0~1.0 범위다 (0.25 = 25%)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
```

적에게는 `MoveSpeed`가 이미 있고, 쿨타임 감소는 의미가 없어 추가하지 않는다.

- [ ] **Step 3: `AGJBaseCharacter.h`에 공용 런타임 스탯 추가**

`CurrentHP` 선언 **아래**, `IsDead()` **위**에 삽입한다:

```cpp

    // 받는 데미지 경감에 쓰인다. TakeDamage가 읽으므로 플레이어/적 모두 여기에 둔다.
    // 실제 값은 각자의 데이터 테이블에서 채운다 (플레이어는 UpdateCharacterStat, 적은 ApplyEnemyStat).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float Defense = 0.f;

    // 치명타 확률 (0.0~1.0). 공격할 때 굴린다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float CritChance = 0.f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stat")
    float CritMultiplier = 2.f;
```

`Defense`/`CritChance`/`CritMultiplier`는 `public` 영역(`MaxHP`가 있는 곳)에 두어 무기와 적 공격 코드가 읽을 수 있게 한다.

- [ ] **Step 4: `TakeDamage`에 방어 경감 적용**

`GJBaseCharacter.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJCombatStatics.h"
```

`AGJBaseCharacter::TakeDamage`에서 다음 줄을 찾는다:

```cpp
    CurrentHP = FMath::Clamp(CurrentHP - ActualDamage, 0.f, MaxHP);
    OnDamaged.Broadcast(ActualDamage, DamageCauser);
```

이를 다음으로 교체한다:

```cpp
    // 방어력은 "맞는 쪽의 성질"이므로 여기 한 곳에서만 적용한다.
    // 이렇게 두면 앞으로 어떤 데미지 소스가 추가되어도(장판, 도트, 폭발) 경감이 자동으로 걸린다.
    const float MitigatedDamage = UGJCombatStatics::ApplyDefense(ActualDamage, Defense);

    CurrentHP = FMath::Clamp(CurrentHP - MitigatedDamage, 0.f, MaxHP);
    OnDamaged.Broadcast(MitigatedDamage, DamageCauser);
```

`OnDamaged`에도 경감 후 값을 넘기는 이유: HP 바 갱신이나 데미지 표시가 실제로 깎인 양과 일치해야 한다.

함수 끝의 `return ActualDamage;`는 그대로 둔다 — 엔진 규약상 반환값은 "이 액터가 처리한 데미지"이며, 이후 호출자가 이 값을 쓰는 곳이 현재 없다.

- [ ] **Step 5: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. `USTRUCT`에 필드를 추가했으니, 만약 "정확히 일치하는 구조체만 호환" 같은 에러가 나면 **에디터를 완전히 재시작**해야 해(재빌드는 필요 없고 껐다 켜기만).

Run: `grep -E "LogLiveCoding|error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 6: 데이터 테이블에 새 칸 값 채우기**

MCP가 연결되어 있으면 `editor_toolset.toolsets.data_table.DataTableTools`로 시도한다. 안 되면 사용자에게 요청한다:

> `DT_CharacterStat`을 열어서 각 레벨 행의 새 칸을 채워줘:
> - `Defense`: 0
> - `MoveSpeed`: 600 (지금 쓰던 값 그대로)
> - `CooldownReduction`: 0
> - `CritChance`: 0
> - `CritMultiplier`: 2.0
>
> `DT_EnemyStat`도 각 행에:
> - `Defense`: 0
> - `CritChance`: 0
> - `CritMultiplier`: 2.0
>
> 일단 전부 기본값으로 채워두면 돼 — 다음 단계에서 값을 바꿔가며 테스트할 거야.

- [ ] **Step 7: 방어력이 실제로 동작하는지 PIE 확인**

사용자에게 요청한다:
> `DT_CharacterStat`에서 플레이어 레벨 1행의 `Defense`를 **100**으로 바꾸고 저장한 뒤, `TestLev`에서 적한테 맞아봐. 그 다음 `Defense`를 **0**으로 되돌리고 다시 맞아봐.

확인 항목: 방어력 100일 때 HP 바가 깎이는 폭이 방어력 0일 때의 **절반**이어야 한다(적 공격력 10 → 방어력 100에서 5).

이 단계에서 차이가 없으면 `Defense`가 실제로 채워지지 않은 것이다 — Task 3에서 채우므로, **지금 시점에는 아직 차이가 없는 것이 정상이다.** 이 확인은 Task 3 Step 4로 미룬다.

- [ ] **Step 8: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJBaseCharacter.h Source/Project_GJ/GJBaseCharacter.cpp Content/GJ/DataTables/DT_CharacterStat.uasset Content/GJ/DataTables/DT_EnemyStat.uasset
git commit -m "$(cat <<'EOF'
전투 스탯 필드 추가 및 방어력 경감 적용

FCharacterStat에 방어력/이동속도/쿨감/치명타를, FEnemyStat에 방어력/치명타를
추가하고, AGJBaseCharacter에 공용 런타임 스탯을 두어 TakeDamage가
플레이어와 적을 동일하게 처리하도록 했다.

방어 경감은 TakeDamage 한 곳에만 두어 앞으로 추가될 데미지 소스에도
자동으로 적용되게 했다. OnDamaged에도 경감 후 값을 넘겨 HP 바 표시와
실제 차감량이 일치한다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 데이터 테이블 값을 캐릭터에 반영

**Files:**
- Modify: `Source/Project_GJ/GJCharacter.h` (공격력 getter 추가)
- Modify: `Source/Project_GJ/GJCharacter.cpp` (`UpdateCharacterStat`)
- Modify: `Source/Project_GJ/GJEnemyCharacter.cpp` (`ApplyEnemyStat`)

**Interfaces:**
- Consumes: Task 2의 `FCharacterStat`/`FEnemyStat` 새 필드, `AGJBaseCharacter::Defense`/`CritChance`/`CritMultiplier`
- Produces: `float AGJCharacter::GetBaseAttackPower() const` — Task 4의 무기가 호출한다

- [ ] **Step 1: `AGJCharacter`에 공격력 getter 추가**

`CurrentCharacterStat`은 `protected`라 무기 쪽에서 직접 못 읽는다. `GJCharacter.h`의 `ApplyConsumableEffect` 선언 **아래**(같은 `public:` 블록)에 추가한다:

```cpp
    // 무기가 발사 시 공격력 배율을 계산할 때 읽는다 (CurrentCharacterStat이 protected라 getter가 필요함)
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetBaseAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }
```

- [ ] **Step 2: `UpdateCharacterStat`에서 새 스탯 채우기**

`GJCharacter.cpp`의 `AGJCharacter::UpdateCharacterStat`에서 다음 블록을 찾는다:

```cpp
            MaxMP = CurrentCharacterStat.MaxMP;
            CurrentMP = MaxMP;
```

그 **아래**에 추가한다:

```cpp

            // 전투 스탯 - TakeDamage(방어력)와 무기 발사(치명타)가 읽는다
            Defense = CurrentCharacterStat.Defense;
            CritChance = CurrentCharacterStat.CritChance;
            CritMultiplier = CurrentCharacterStat.CritMultiplier;

            // 플레이어 이동 속도는 지금까지 어디서도 설정하지 않아 엔진 기본값을 쓰고 있었다.
            // 이제 데이터 테이블 값으로 명시적으로 설정한다.
            GetCharacterMovement()->MaxWalkSpeed = CurrentCharacterStat.MoveSpeed;
```

`GJCharacter.cpp`는 이미 `#include "GameFramework/CharacterMovementComponent.h"`를 갖고 있으므로 include 추가는 필요 없다.

- [ ] **Step 3: `ApplyEnemyStat`에서 새 스탯 채우기**

`GJEnemyCharacter.cpp`의 `AGJEnemyCharacter::ApplyEnemyStat`에서 다음 줄을 찾는다:

```cpp
    GetCharacterMovement()->MaxWalkSpeed = RowData->MoveSpeed;
```

그 **아래**에 추가한다:

```cpp

    // 전투 스탯 - TakeDamage(방어력)와 ApplyAttackDamage(치명타)가 읽는다
    Defense = RowData->Defense;
    CritChance = RowData->CritChance;
    CritMultiplier = RowData->CritMultiplier;
```

- [ ] **Step 4: 컴파일 후 방어력·이동속도 PIE 확인**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일하고, 아래 두 가지를 확인해줘.
>
> **(A) 방어력**: `DT_CharacterStat` 레벨 1행의 `Defense`를 **100**으로 바꾸고 `TestLev`에서 적한테 맞아봐. 그 다음 **0**으로 되돌리고 다시 맞아봐. 방어력 100일 때 HP가 깎이는 폭이 **절반**이어야 해.
>
> **(B) 이동속도**: `MoveSpeed`를 **300**으로 바꾸고 플레이해봐. 눈에 띄게 느려져야 해. 확인했으면 **600**으로 되돌려줘.

두 확인이 모두 통과해야 다음으로 넘어간다. 방어력에 차이가 없으면 `Defense`가 채워지지 않은 것이고, 이동 속도가 안 변하면 `MaxWalkSpeed` 설정이 안 걸린 것이다.

- [ ] **Step 5: 적 방어력 확인**

사용자에게 요청한다:
> `DT_EnemyStat`에서 적의 `Defense`를 **100**으로 올리고 적을 쏴봐. 적이 죽는 데 필요한 탄이 **대략 2배**로 늘어나야 해. 확인했으면 **0**으로 되돌려줘.

- [ ] **Step 6: 커밋**

```bash
git add Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Source/Project_GJ/GJEnemyCharacter.cpp Content/GJ/DataTables/DT_CharacterStat.uasset Content/GJ/DataTables/DT_EnemyStat.uasset
git commit -m "$(cat <<'EOF'
데이터 테이블 전투 스탯을 캐릭터에 반영

UpdateCharacterStat과 ApplyEnemyStat에서 방어력/치명타를 채우고,
플레이어 이동 속도를 데이터 테이블 값으로 설정한다. 지금까지 플레이어는
MaxWalkSpeed를 어디서도 설정하지 않아 엔진 기본값을 쓰고 있었다.

무기가 공격력 배율을 계산할 수 있도록 GetBaseAttackPower getter를 추가했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 공격 계산 연결 (공격력 배율 + 치명타)

이 태스크가 끝나면 **공식이 완성된다.**

**Files:**
- Modify: `Source/Project_GJ/GJWeapon_Ranged.cpp` (`Fire`)
- Modify: `Source/Project_GJ/GJEnemyCharacter.cpp` (`ApplyAttackDamage`)

**Interfaces:**
- Consumes:
  - `UGJCombatStatics::CalculateOutgoingDamage(float, float, float, float, bool&)` (Task 1)
  - `AGJCharacter::GetBaseAttackPower()` (Task 3)
  - `AGJBaseCharacter::CritChance` / `CritMultiplier` (Task 2)
- Produces: 없음 (마지막 연결 지점)

- [ ] **Step 1: `GJWeapon_Ranged.cpp`에 발사 시 공격 계산 연결**

include 블록에 추가한다:

```cpp
#include "GJCombatStatics.h"
#include "GJCharacter.h"
```

`AGJWeapon_Ranged::Fire()`에서 다음 블록을 찾는다:

```cpp
    if (ProjectileToFire)
    {
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());
        // 데이터 테이블에서 가져온 데미지/속도/사거리 그대로 발사합니다.
        ProjectileToFire->FireInDirection(ShootDirection, WeaponStat.BaseDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range);
```

이를 다음으로 교체한다:

```cpp
    if (ProjectileToFire)
    {
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());

        // 무기를 든 캐릭터의 공격력/치명타를 반영해 실제 발사 데미지를 계산한다.
        // OnPickedUp에서 SetInstigator를 하므로 플레이어가 든 무기는 항상 여기서 캐릭터를 찾을 수 있다.
        float AttackPower = 0.f;
        float CritChance = 0.f;
        float CritMultiplier = 1.f;
        if (AGJCharacter* OwningCharacter = Cast<AGJCharacter>(GetInstigator()))
        {
            AttackPower = OwningCharacter->GetBaseAttackPower();
            CritChance = OwningCharacter->CritChance;
            CritMultiplier = OwningCharacter->CritMultiplier;
        }

        // 캐스팅이 실패하면 위 기본값 그대로라 무기 기본 데미지만 나간다(배율 1배, 치명타 없음)
        bool bWasCritical = false;
        const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
            WeaponStat.BaseDamage, AttackPower, CritChance, CritMultiplier, bWasCritical);

        ProjectileToFire->FireInDirection(ShootDirection, OutgoingDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range);
```

- [ ] **Step 2: `GJEnemyCharacter.cpp`에 적 공격 계산 연결**

include 블록에 추가한다:

```cpp
#include "GJCombatStatics.h"
```

`AGJEnemyCharacter::ApplyAttackDamage()`에서 다음 줄을 찾는다:

```cpp
    UGameplayStatics::ApplyDamage(TargetPlayer, AttackDamage, GetController(), this, UDamageType::StaticClass());
```

이를 다음으로 교체한다:

```cpp
    // 적은 공격력 배율을 쓰지 않는다 - AttackDamage가 이미 최종 공격력이라 AttackPower에 0을 넘긴다.
    // 치명타는 적도 굴린다.
    bool bWasCritical = false;
    const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
        AttackDamage, 0.f, CritChance, CritMultiplier, bWasCritical);

    UGameplayStatics::ApplyDamage(TargetPlayer, OutgoingDamage, GetController(), this, UDamageType::StaticClass());
```

- [ ] **Step 3: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "LogLiveCoding|error C" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 4: 공격력 배율 PIE 확인**

사용자에게 요청한다:
> `DT_CharacterStat` 레벨 1행의 `BaseAttackPower`를 **0**으로 놓고 적을 몇 대 쏴서 죽는 데 몇 발 걸리는지 세어봐. 그 다음 **100**으로 올리고 다시 세어봐.

확인 항목: 공격력 100이면 데미지가 **2배**(`× (1 + 100/100)`)이므로 필요한 탄수가 대략 **절반**이어야 한다.

- [ ] **Step 5: 치명타 PIE 확인**

사용자에게 요청한다:
> `DT_CharacterStat`의 `CritChance`를 **1.0**(100%)으로, `CritMultiplier`를 **2.0**으로 놓고 적을 쏴봐. 그 다음 `CritChance`를 **0**으로 되돌리고 비교해줘.

확인 항목: 치명타 100%일 때 적이 죽는 데 필요한 탄수가 **절반**이어야 한다.

> ⚠️ `CritChance`는 0.0~1.0 범위다. **100을 넣으면 안 된다** — 100%를 원하면 `1.0`이다.

- [ ] **Step 6: 최소 데미지 보장 확인**

사용자에게 요청한다:
> `DT_EnemyStat`의 적 `Defense`를 **10000**으로 극단적으로 올리고 적을 계속 쏴봐. 아주 느리더라도 **적이 결국 죽어야** 해(데미지가 0이 되어 영원히 안 죽으면 안 됨). 확인했으면 **0**으로 되돌려줘.

- [ ] **Step 7: 데이터 테이블 값 원복 확인**

사용자에게 요청한다:
> 테스트하느라 바꿨던 값들을 전부 원래대로 돌려놨는지 확인해줘 — `DT_CharacterStat`의 `BaseAttackPower`/`CritChance`/`Defense`/`MoveSpeed`, `DT_EnemyStat`의 `Defense`.

극단적인 테스트 값이 커밋에 섞여 들어가면 나중에 밸런스가 이상하다고 착각하게 된다.

- [ ] **Step 8: 커밋**

```bash
git add Source/Project_GJ/GJWeapon_Ranged.cpp Source/Project_GJ/GJEnemyCharacter.cpp Content/GJ/DataTables/DT_CharacterStat.uasset Content/GJ/DataTables/DT_EnemyStat.uasset
git commit -m "$(cat <<'EOF'
공격 계산 연결로 데미지 공식 완성

무기 발사와 적 근접 공격이 UGJCombatStatics::CalculateOutgoingDamage를
거치도록 해서 공격력 배율과 치명타가 실제로 적용된다.

무기는 GetInstigator로 든 캐릭터를 찾아 스탯을 읽고, 캐스팅이 실패하면
무기 기본 데미지만 나가도록 안전하게 처리했다. 적은 AttackDamage가 이미
최종 공격력이라 공격력 배율을 쓰지 않는다.

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

- [ ] **Step 1: `Docs/DevGuide.md`의 데미지 파이프라인 절 갱신**

5절 "데미지 파이프라인"의 흐름 다이어그램을 다음으로 교체한다:

```
[공격 측] 무기 발사 / 적 근접 공격
  → UGJCombatStatics::CalculateOutgoingDamage(무기데미지, 공격력, 치명타확률, 치명타배율)
      = 무기데미지 x (1 + 공격력/100) x 치명타배율
  → UGameplayStatics::ApplyDamage(대상, 계산된 데미지)

[방어 측] AGJBaseCharacter::TakeDamage() [override]
  → UGJCombatStatics::ApplyDefense(받은 데미지, 내 Defense)
      = 데미지 x 100/(100 + 방어력)     ← 방어력은 여기 한 곳에서만
  → CurrentHP 차감
  → OnDamaged.Broadcast(경감 후 데미지)   ← UI가 여길 구독
  → CurrentHP <= 0 이면 HandleDeath()
```

그리고 같은 절에 다음 설명을 추가한다:

```markdown
**공식이 사는 곳**: `UGJCombatStatics`(`GJCombatStatics.h/.cpp`, `UBlueprintFunctionLibrary`). 공격 계산과 방어 경감이 서로 다른 지점에서 호출되지만 **공식 자체는 이 파일 하나에만** 있다 — 밸런스 조정 시 여기만 보면 된다.

**방어력은 체감형**이다(`100/(100+방어력)`). 방어력 100마다 "체력이 1배씩 더 있는" 효과이고, 아무리 올려도 100% 무효화에 도달하지 않는다.

**최소 데미지 하한**: 방어력이 극단적으로 높아도 데미지가 0에 수렴해 사실상 무적이 되지 않도록 하한을 둔다. 단 하한값은 `min(1.0, 들어온 데미지)`라, 원래 1보다 약한 공격이 방어력을 거치며 오히려 세지는 역전은 생기지 않는다.

**적은 공격력 배율을 쓰지 않는다** — `EnemyStat.AttackDamage`가 이미 최종 공격력이라 `AttackPower`에 0을 넘긴다. 치명타는 적도 굴린다.

**치명타 여부는 대상에게 전달되지 않는다.** `CalculateOutgoingDamage`가 `bOutWasCritical`을 돌려주지만 공격자 쪽에서만 알 수 있다. 치명타 데미지 폰트 같은 UI를 붙이려면 커스텀 `FDamageEvent`가 필요하다.
```

- [ ] **Step 2: 데이터 테이블 스키마 표 갱신**

8절 `FCharacterStat` 표에 다음 행을 추가한다:

| 필드 | 기본값 | 설명 |
|---|---|---|
| `Defense` | 0 | 받는 데미지 경감 (체감형, 100이면 50% 경감) |
| `MoveSpeed` | 600 | `CharacterMovement.MaxWalkSpeed`에 적용. 이전에는 설정하지 않아 엔진 기본값을 쓰고 있었음 |
| `CooldownReduction` | 0 | **미사용** — 스킬 시스템 전까지 연결되지 않음 |
| `CritChance` | 0 | 치명타 확률. **0.0~1.0 범위**(0.25 = 25%) |
| `CritMultiplier` | 2.0 | 치명타 시 데미지 배율 |

`FEnemyStat` 표에 다음 행을 추가한다:

| 필드 | 기본값 | 설명 |
|---|---|---|
| `Defense` | 0 | 받는 데미지 경감 |
| `CritChance` | 0 | 치명타 확률 (0.0~1.0) |
| `CritMultiplier` | 2.0 | 치명타 배율 |

- [ ] **Step 3: 클래스 계층 목록에 `UGJCombatStatics` 추가**

1절 "클래스 계층 한눈에 보기"의 데이터 섹션 **위**에 추가한다:

```
UGJCombatStatics (UBlueprintFunctionLibrary) — 데미지 공식 단일 소스
```

- [ ] **Step 4: `BaseAttackPower` 설명 채우기와 TODO 목록 갱신**

8절 `FCharacterStat` 표의 `BaseAttackPower` 행은 현재 설명 칸이 비어 있다(`| BaseAttackPower | 10 | |`). 이제 실제로 쓰이므로 설명을 채운다:

```
| `BaseAttackPower` | 10 | 무기 데미지에 배율로 적용됨 (`무기데미지 x (1 + 공격력/100)`) |
```

9절 TODO 목록에서 삭제할 항목은 없다. 다음 항목을 **추가**한다:

```markdown
- `FCharacterStat.CooldownReduction`은 필드만 있고 어디에도 연결되지 않음 — 적용 대상이 될 스킬 시스템이 아직 없음
- 치명타가 터져도 화면에 표시되지 않음 — 치명타 여부가 공격자 쪽에만 있어서, UI를 붙이려면 커스텀 `FDamageEvent`가 필요
```

- [ ] **Step 5: `DevGuide.html`에 동일 내용 반영**

`DevGuide.html`은 `Docs/DevGuide.md`와 같은 내용을 HTML로 옮긴 문서다. Step 1~4의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다.

반영 후 태그 균형을 확인한다:

Run:
```bash
python -c "
import re
html = open('DevGuide.html', encoding='utf-8').read()
for tag in ['table','ul','pre','div','p','li','tr','td','th','code']:
    o = len(re.findall(r'<'+tag+r'[ >]', html)); c = len(re.findall(r'</'+tag+r'>', html))
    print(f'{tag}: {o} vs {c}', '' if o==c else '<-- MISMATCH')
"
```
Expected: 모든 태그의 열림/닫힘 개수가 일치한다.

- [ ] **Step 6: 커밋**

```bash
git add Docs/DevGuide.md DevGuide.html
git commit -m "$(cat <<'EOF'
개발 가이드에 전투 스탯과 데미지 공식 반영

UGJCombatStatics 기반의 새 데미지 파이프라인, 체감형 방어력 공식,
최소 데미지 하한 규칙, 데이터 테이블 스키마 변경을 문서화.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```
