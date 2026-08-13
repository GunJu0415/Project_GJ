# 스탯 보너스 레이어 (M2.5) 설계

## 1. 배경 / 문제

카드 선택 시스템(M2.6)에서 "최대 체력 +5", "공격력 +15%" 같은 스탯 카드를 만들려는데, **지금 구조에서는 그 값이 다음 레벨업에 지워진다.**

`AGJCharacter::UpdateCharacterStat`은 데이터 테이블 행을 통째로 복사한 뒤 스탯 멤버를 재대입한다:

```cpp
CurrentCharacterStat = *RowData;          // 구조체 통째로 덮어쓰기
MaxHP       = CurrentCharacterStat.MaxHP; // 테이블 값으로 재대입
Defense     = CurrentCharacterStat.Defense;
CritChance  = CurrentCharacterStat.CritChance;
GetCharacterMovement()->MaxWalkSpeed = CurrentCharacterStat.MoveSpeed;
```

카드가 `MaxHP += 5`를 해도 다음 `UpdateCharacterStat` 호출에서 사라진다. 그리고 **카드를 주는 시점이 곧 레벨업**이므로 이 충돌은 "언젠가"가 아니라 매번 일어난다. 레벨 2에서 +5를 받고 레벨 3이 되는 순간 없어진다.

`GetBaseAttackPower()`도 같은 문제다. `CurrentCharacterStat.BaseAttackPower`를 직접 읽어 무기에 넘기므로, 보너스를 어디에 더해도 발사 데미지에 안 실린다.

**이 문제는 지금 고치는 것이 압도적으로 싸다.** 카드를 20종 만든 뒤에 고치면 카드 정의와 적용 코드를 전부 다시 손봐야 한다.

## 2. 범위

**포함**
- 가산(`+5`)과 증가율(`+15%`)을 함께 담는 모디파이어 구조체
- 테이블 원본 / 보너스 누적 / 실효값의 3층 분리
- 실효값을 계산하는 단일 진입점 `RecalculateStats()`
- 카드가 부를 공개 API `AddStatBonus()`
- 잘못된 카드 값이 게임을 망가뜨리지 않게 하는 하한 처리
- 카드 시스템이 없는 상태에서 검증할 콘솔 명령

**제외**
- 카드 데이터/UI/선택 로직 — M2.6
- 모디파이어 개별 제거, 시간제 버프 — 지금은 누적만. 필요해지면 목록 기반으로 바꾸되, 그때도 읽는 쪽 코드는 안 바뀐다
- 적 스탯 보너스 — 카드는 플레이어 전용. 적 스테이지 스케일링(M5)은 다른 축이며 같은 구조체를 재사용하면 된다
- 곱연산 다단 구조(증가율과 최종 배율 분리) — 카드가 한 장도 없는 지금은 과설계

## 3. 데이터 구조

### 3.1 `FStatValues` — 0에서 시작하는 값 묶음

```cpp
USTRUCT(BlueprintType)
struct FStatValues
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float MaxHP = 0.f;
    // ... MaxMP, BaseAttackPower, RequiredEXP, Defense, MoveSpeed,
    //     CooldownReduction, CritChance, CritMultiplier (전부 0.f)

    FStatValues& operator+=(const FStatValues& Other);  // 필드별 덧셈
};
```

**`FCharacterStat`을 재사용하지 않는 이유**: 그쪽 기본값이 `MaxHP=100`, `MoveSpeed=600`, `CritMultiplier=2`라서 "보너스 없음"을 표현할 수 없다. 보너스 구조체는 기본 생성했을 때 아무 효과가 없어야 한다.

`FCharacterStat`의 9개 필드를 **전부** 미러링한다. 일부만 지원하면 "이 스탯은 왜 카드로 못 올리지?"라는 비대칭이 생기고, 나중에 필드를 추가하면 `USTRUCT` 레이아웃이 바뀌어 에디터 재시작이 필요해진다.

### 3.2 `FStatModifier` — 가산 + 증가율

```cpp
USTRUCT(BlueprintType)
struct FStatModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Add;      // 가산   (+5 체력)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modifier")
    FStatValues Percent;  // 증가율 (0.15 = +15%)
};
```

**`Percent`는 1.0이 아니라 0에서 시작하는 증가율이다.** 이 선택이 두 가지를 공짜로 만든다:

- 기본 생성한 `FStatModifier`가 아무 효과 없는 값이 된다 (1.0에서 시작하면 "곱연산 없음"이 0이 아니라 1이라 덧셈으로 합칠 수 없다)
- 모디파이어를 합치는 게 그냥 필드 덧셈이 된다. `+15%` 카드 두 장이면 `0.15 + 0.15 = 0.30`

증가율은 **곱하지 않고 합산한다**(1.15 × 1.15가 아니라 1 + 0.30). 합산이 밸런싱이 예측 가능하고, 카드를 많이 먹었을 때 지수적으로 터지지 않는다.

**두 구조체 모두 `BlueprintType` + `EditAnywhere`로 선언한다.** M2.6에서 `FCardData`가 `FStatModifier`를 필드로 품어 데이터 테이블에서 직접 편집하게 되므로, 처음부터 테이블에 넣을 수 있는 형태여야 한다.

### 3.3 `AGJCharacter`의 3층

```cpp
FCharacterStat BaseStat;             // 테이블 원본  — UpdateCharacterStat만 씀
FStatModifier  StatBonus;            // 카드 누적    — AddStatBonus만 씀
FCharacterStat CurrentCharacterStat; // 실효값       — RecalculateStats만 씀
```

`CurrentCharacterStat`은 **이미 존재하는 멤버이며, 의미만 "테이블 원본"에서 "실효값"으로 바뀐다.** 이게 이 설계의 핵심이다 — 이걸 읽는 코드(`AddEXP`, `GetBaseAttackPower`, `UpdatePlayerHUD`)를 한 줄도 고치지 않고 보너스가 자동으로 실린다.

## 4. 계산 규칙

```
실효값 = (테이블값 + Add) x (1 + Percent)
```

계산 직후 하한을 건다. 카드가 만들 수 있는 함정을 막기 위한 것이다:

| 스탯 | 하한 | 안 걸었을 때 |
|---|---|---|
| `MaxHP`, `MaxMP` | 1 | HUD의 `Current/Max`가 0으로 나누고, 최대 체력 0이면 즉사 |
| `RequiredEXP` | 1 | `AddEXP`의 `RequiredEXP > 0` 루프 가드에 걸려 **레벨업이 조용히 멈춘다**. 크래시가 아니라 아무 일도 안 일어나서 원인 추적이 어렵다 |
| `BaseAttackPower` | 0 | 데미지 공식이 `무기데미지 x (1 + 공격력/100)`이라 공격력이 -100 아래로 가면 **데미지가 음수**가 되고, `TakeDamage`의 `CurrentHP -= 음수`가 **적을 회복시킨다** |
| `CritMultiplier` | 0 | 같은 이유로 치명타가 터질 때만 회복시키는 현상 |
| `MoveSpeed`, `CritChance` | 0 | 음수 속도/확률 |

`CooldownReduction`은 아직 어디서도 읽지 않으므로 하한을 두지 않는다. 스킬 시스템(M2.7)이 생겨 이 값을 쓰기 시작할 때 그쪽에서 범위를 정한다.

`Defense`는 하한을 걸지 않는다 — `UGJCombatStatics::ApplyDefense`가 이미 `FMath::Max(Defense, 0)`을 한다. 같은 방어를 두 곳에 두면 나중에 한쪽만 고치게 된다.

`CritChance`에 **상한은 두지 않는다.** 1.0을 넘으면 항상 치명타인데, 그건 빌드가 도달하려는 목표지 버그가 아니다.

`CritMultiplier`는 테이블 기본값이 2.0이므로 증가율 `+0.15`는 2.3이 된다. 가산으로 `+0.5`를 주면 2.5다. 둘 다 의도된 동작이다.

## 5. 호출 경로

```
UpdateCharacterStat(NewLevel, bRestoreToFull)   // 스폰 / 레벨업
    BaseStat = 테이블 행
    RecalculateStats(bRestoreToFull)

AddStatBonus(const FStatModifier& Delta)         // 카드 (M2.6이 호출)
    StatBonus.Add     += Delta.Add;
    StatBonus.Percent += Delta.Percent;
    RecalculateStats(false)                      // 카드는 회복이 아니다

RecalculateStats(bRestoreToFull)                 // 유일한 쓰기 지점
    CurrentCharacterStat = combine(BaseStat, StatBonus) + 하한
    OldMaxHP 기억 → MaxHP 갱신
      → CurrentHP = bRestoreToFull ? MaxHP : Clamp(CurrentHP + (MaxHP - OldMaxHP), 0, MaxHP)
    MP도 동일
    Defense / CritChance / CritMultiplier → AGJBaseCharacter 멤버에 push
      (TakeDamage와 GJWeapon_Ranged::Fire가 이 멤버들을 직접 읽기 때문)
    MaxWalkSpeed 적용
    UpdatePlayerHUD()
```

**HP 증가분 로직이 `UpdateCharacterStat`에서 `RecalculateStats`로 옮겨간다.** 그 결과 "최대 체력 +5" 카드가 현재 체력도 +5 시키는 동작이 따로 코드를 짜지 않아도 나온다 — 레벨업의 "증가분만 반영" 규칙과 카드의 동작이 같은 한 줄에서 나오기 때문이다.

`RecalculateStats`가 실효값을 쓰는 **유일한 지점**이라는 게 중요하다. 나중에 증가율 합산을 곱연산으로 바꾸든, 모디파이어를 목록 기반으로 갈아끼우든, 고칠 곳이 이 함수 하나다.

## 6. 기존 코드에 미치는 영향

**읽는 쪽은 사실상 변경 없음.** `AddEXP`, `UpdatePlayerHUD`, `AGJBaseCharacter::TakeDamage`, `ApplyConsumableEffect`, 재장전의 MP 계산 전부 그대로 둔다.

**딱 하나 예외**: `GetBaseAttackPower()`를 `GetAttackPower()`로 이름만 바꾼다. 몸통은 그대로(`CurrentCharacterStat.BaseAttackPower` 반환)지만, 그 값이 이제 보너스가 실린 실효값이라 이름의 "Base"가 **거짓이 된다.** 나중에 누군가 "보너스 이전 값이 필요하다"며 이 함수를 쓰면 조용히 틀린 계산을 하게 된다. 호출부는 `AGJWeapon_Ranged::Fire` 한 곳뿐이라 비용이 없다.

**적은 안 건드린다.** `AGJEnemyCharacter::ApplyEnemyStat`은 지금처럼 테이블 값을 멤버에 직접 대입한다. 적에게 보너스를 줄 일이 생기면(M5 스테이지 스케일링) 그때 같은 구조체를 재사용한다.

**런마다 초기화는 공짜다.** 캐릭터가 런마다 새로 스폰되므로 `StatBonus`는 기본 생성되어 전부 0이다. EXP와 같은 메커니즘이라 초기화 코드가 필요 없다. 회차를 넘어 남는 성장은 M6 영구 특성이 담당하며, `StatBonus`는 어떤 세이브 경로에도 들어가지 않는다.

## 7. 검증

테스트 스위트가 없으므로 **컴파일 통과 + 수동 PIE**다. 카드 시스템이 아직 없어 `AddStatBonus`를 부를 경로가 없으므로 콘솔 명령을 만든다:

```cpp
UFUNCTION(Exec)
void GJAddBonus(FString StatName, float AddValue, float PercentValue);
```

PIE 콘솔에서 `GJAddBonus MaxHP 5 0`처럼 쓴다. 스탯 이름을 문자열로 받아 해당 필드에 값을 넣는다. **임시 코드가 아니라 밸런싱 내내 쓸 개발용 훅으로 남긴다** — 카드가 생긴 뒤에도 "이 조합이면 어떻게 되나"를 카드 없이 시험할 수 있다.

이름이 일치하지 않으면 **`UE_LOG` 경고로 유효한 스탯 이름 목록을 찍는다.** 조용히 무시하면 오타를 쳤을 때 "보너스가 안 먹네"로 오인해서 없는 버그를 쫓게 된다. 이름 비교는 대소문자를 구분하지 않는다.

확인 항목:

1. **회귀** — 카드 없이 레벨 1→5까지 올렸을 때 오늘 확인한 값과 동일한가 (`40/100 → 150/210`)
2. `GJAddBonus MaxHP 5 0` → 최대 체력과 현재 체력이 **둘 다** +5
3. **그 상태로 레벨업 → +5가 유지되는가** ← 이 작업이 존재하는 이유
4. `GJAddBonus BaseAttackPower 0 0.15` → 적을 죽이는 데 필요한 발수가 줄어드는가
5. `GJAddBonus MaxHP 0 -10` (극단적 음수) → 최대 체력이 1로 하한 걸리고 크래시 없음
6. `GJAddBonus RequiredEXP 0 -10` → 하한이 걸려 **레벨업이 계속 동작**하는가
7. 죽고 새 런 시작 → 보너스가 전부 사라지고 레벨 1 기본 스탯인가

## 8. 에디터 작업

**없다.** 데이터 테이블 스키마와 위젯이 그대로다. 순수 C++ 작업이라 컴파일만 하면 된다.

`FStatValues`/`FStatModifier`는 새 `USTRUCT`이므로 기존 데이터 테이블 레이아웃에 영향이 없다. 이 구조체가 테이블에 들어가는 건 M2.6에서 `FCardData`가 생길 때다.

## 9. 후속 관계

- **M2.6 카드 선택 시스템** — 이 작업의 직접적인 소비자. `FCardData`가 `FStatModifier`를 필드로 품고, 카드를 고르면 `AddStatBonus`를 호출한다. 스탯 카드가 이것 없이는 성립하지 않으므로 **선행 조건**이다
- **M5 스테이지 진행** — 적 스탯 스케일링에 같은 구조체를 재사용할 수 있다
- **M6 영구 특성** — 영구 재화로 찍는 특성도 결국 스탯 보너스다. 다만 저장소가 다르므로(런 초기화되면 안 됨) `StatBonus`와 별도의 누적기를 두고 `RecalculateStats`에서 함께 합산하는 형태가 된다. 이 설계는 그 확장을 막지 않는다 — 합산 지점이 한 곳이기 때문이다
