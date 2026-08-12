# EXP와 레벨업 (M2 인런 성장) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 적을 죽이면 경험치를 얻고, 경험치가 차면 레벨이 올라 `DT_CharacterStat`의 다음 행 스탯을 갖는다.

**Architecture:** 경험치 누적과 레벨업 판정은 `AGJCharacter`에 모은다. 죽인 주체는 `AGJBaseCharacter::TakeDamage`가 사망 확정 시점에 `LastDamageInstigator`(약참조)로 기억해 두고, `AGJEnemyCharacter::HandleDeath()`가 그것을 읽어 경험치를 지급한다. 레벨 상한은 코드 상수가 아니라 `DT_CharacterStat`의 마지막 행이다.

**Tech Stack:** UE 5.8, C++ (`FTableRowBase` USTRUCT, `TWeakObjectPtr`, `DECLARE_DYNAMIC_MULTICAST_DELEGATE`, UMG `BindWidgetOptional`)

**설계 문서:** `Docs/superpowers/specs/2026-08-12-exp-levelup-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다(설계 문서 7절). 각 태스크의 검증은 자동화 테스트가 아니라 구체적인 PIE 조작 시나리오로 기술한다.
- **`USTRUCT` 레이아웃 변경은 실제 비용이 있다.** `FEnemyStat`에 필드를 추가하면 데이터 테이블의 기존 행에 새 칸이 생기고, 이를 참조하는 UMG 블루프린트 핀이 깨질 수 있다. 새 칸이 데이터 테이블에 안 보이면 **에디터 완전 재시작**이 필요하다(라이브 코딩은 USTRUCT 레이아웃을 기존 에셋에 전파하지 못한 이력이 있다).
- **경험치 파이프라인은 전부 `float`이다.** `FEnemyStat::ExpReward`, `AGJCharacter::CurrentEXP`, `AddEXP(float)` 모두 `float`. 비교 대상인 `FCharacterStat::RequiredEXP`가 `float`이기 때문이다. 어디서도 `int32`로 바꾸지 말 것.
- **`RequiredEXP`는 누적이 아니다.** "이 레벨에서 다음 레벨까지 필요한 양"이다. 레벨업할 때 이 값을 빼고 초과분을 이월한다.
- **레벨업은 회복이 아니다.** HP/MP는 최대치 **증가분만** 현재값에 더한다. 풀 회복시키면 안 된다.
- **인코딩**: 초기 파일들의 한글 주석은 이미 U+FFFD로 손실 변환된 상태다. 파일은 사실상 UTF-8이므로 **새 주석은 UTF-8 한글로 그냥 쓰면 된다.** 깨진 옛 주석 줄은 의미를 알 수 없으므로 건드리지 말 것.
- **커밋 메시지는 한국어**로 쓴다. 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다.
- 브랜치를 나누지 않고 `main`에서 직접 작업한다(솔로 프로젝트, 사용자 요청).
- `.uasset`/`.umap`은 바이너리라 직접 편집할 수 없다. 데이터 테이블 값 입력과 위젯 배치는 **사용자가 에디터에서** 해야 한다. MCP `set_rows`를 쓸 경우 **행 전체를 명시**한다 — 지정하지 않은 필드가 구조체 기본값으로 리셋된 이력이 있다.

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJCharacter.h` (수정) | `CurrentEXP`/`AddEXP`/`LevelUp`/`IsMaxLevel`/`OnLevelUp` 선언, `UpdateCharacterStat` 시그니처 변경 | 1 |
| `Source/Project_GJ/GJCharacter.cpp` (수정) | 경험치 누적·레벨업 루프, HP/MP 증가분 반영 | 1 |
| `Source/Project_GJ/GJGameTypes.h` (수정) | `FEnemyStat::ExpReward` | 2 |
| `Source/Project_GJ/GJBaseCharacter.h/.cpp` (수정) | `LastDamageInstigator` 저장 | 2 |
| `Source/Project_GJ/GJEnemyCharacter.h/.cpp` (수정) | `ExpReward` 멤버 + 사망 시 킬러에게 지급 | 2 |
| `Source/Project_GJ/GJWeapon_Ranged.cpp` (수정) | 발사 시 총알 인스티게이터 갱신 (경험치 귀속의 선행 조건) | 2 |
| `Source/Project_GJ/GJPlayerHUDWidget.h/.cpp` (수정) | `EXPBar`/`LevelText` + `UpdateEXP` | 3 |
| `Source/Project_GJ/GJCharacter.cpp` (수정) | `UpdatePlayerHUD`에서 `UpdateEXP` 호출 | 3 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 4 |

**태스크 순서 근거**: 경험치를 받을 그릇(1) → 경험치를 주는 쪽(2) → 눈에 보이게(3) → 문서(4). Task 1만으로는 경험치 소스가 없어 PIE 확인이 불가능하므로 컴파일이 검증이고, Task 2가 끝나면 **로그로** 레벨업이 확인되며, Task 3이 끝나면 화면으로 보인다.

---

## Task 1: 플레이어 경험치/레벨업 코어

이 태스크가 끝나도 **화면에 보이는 변화는 없다.** 경험치를 주는 쪽이 아직 없기 때문이다. 검증은 컴파일 통과다.

**Files:**
- Modify: `Source/Project_GJ/GJCharacter.h`
- Modify: `Source/Project_GJ/GJCharacter.cpp` (`UpdateCharacterStat`)

**Interfaces:**
- Consumes: `AGJCharacter::CurrentCharacterStat` (`FCharacterStat`, 기존 `protected` 멤버), `AGJBaseCharacter::MaxHP`/`CurrentHP`
- Produces:
  - `void AGJCharacter::AddEXP(float Amount)` — Task 2의 적이 호출한다
  - `bool AGJCharacter::IsMaxLevel() const`
  - `float AGJCharacter::CurrentEXP` (`protected`) — Task 3의 `UpdatePlayerHUD`가 읽는다
  - `FOnLevelUpSignature AGJCharacter::OnLevelUp` (`BlueprintAssignable`, `int32 NewLevel`)
  - `void AGJCharacter::UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true)` — **시그니처 변경**

- [ ] **Step 1: `GJCharacter.h`에 레벨업 델리게이트 선언 추가**

`GJCharacter.h`에서 다음 줄을 찾는다(40번째 줄 부근):

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActiveWeaponAmmoChangedSignature, int32, CurrentAmmo, int32, MaxAmmo);
```

그 **아래**에 추가한다:

```cpp

// 레벨이 오를 때마다 브로드캐스트됨. 지금은 구독자가 없지만, 레벨업 시 카드 3장이 떠서 하나를
// 고르는 선택 시스템이 붙을 자리다 - 그때 이 델리게이트 하나만 구독하면 된다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUpSignature, int32, NewLevel);
```

- [ ] **Step 2: `GJCharacter.h`의 `UpdateCharacterStat` 시그니처 변경**

`GJCharacter.h`에서 다음 두 줄을 찾는다(222번째 줄 부근):

```cpp
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel);
```

이를 다음으로 교체한다:

```cpp
    // bRestoreToFull=true면 HP/MP를 가득 채운다(스폰/리스폰용, 기존 동작 그대로).
    // false면 최대치가 오른 만큼만 현재값에 더한다(레벨업용) - 레벨업이 완전 회복 수단이 되면
    // "위험할 때 잡몹 하나 잡기"가 최고의 회복법이 되어 체력 관리 긴장이 사라진다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    void UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true);
```

기본 인자가 `true`이므로 **기존 호출부(`GJCharacter.cpp`의 `BeginPlay`)는 고칠 필요가 없다.**

- [ ] **Step 3: `GJCharacter.h`에 경험치 멤버와 `LevelUp` 선언 추가**

같은 `protected:` 블록에서 다음 줄을 찾는다(219번째 줄 부근):

```cpp
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character Stat")
    int32 CurrentLevel;
```

그 **아래**에 추가한다:

```cpp

    // 현재 레벨에서 쌓은 경험치. 누적 총량이 아니라 "이번 레벨의 진행도"다 -
    // 레벨업할 때 CurrentCharacterStat.RequiredEXP만큼 빼고 초과분을 이월한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Level")
    float CurrentEXP = 0.f;

    // 다음 레벨의 스탯을 적용하고 OnLevelUp을 쏜다. AddEXP 내부에서만 호출된다.
    void LevelUp();
```

- [ ] **Step 4: `GJCharacter.h`에 공개 API 추가**

`GJCharacter.h`에서 다음 줄을 찾는다(231번째 줄 부근, `public:` 블록):

```cpp
    // 무기가 발사 시 공격력 배율을 계산할 때 읽는다 (CurrentCharacterStat이 protected라 getter가 필요함)
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetBaseAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }
```

그 **아래**(같은 `public:` 블록 안)에 추가한다:

```cpp

    // 경험치를 더한다. 적 처치가 주 경로지만 퀘스트/상자 등 다른 소스가 생겨도 이 입구를 쓴다.
    // 한 번의 호출로 여러 레벨이 오를 수 있다(초과분은 다음 레벨로 이월됨).
    UFUNCTION(BlueprintCallable, Category = "Level")
    void AddEXP(float Amount);

    // DT_CharacterStat에 다음 레벨 행이 없으면 만렙이다. 상한을 코드 상수로 두지 않으므로
    // 테이블에 행을 추가하는 것만으로 만렙이 늘어난다.
    UFUNCTION(BlueprintPure, Category = "Level")
    bool IsMaxLevel() const;

    // 레벨업 시점. 아직 구독자가 없다 - 카드 선택 시스템이 여기 붙는다.
    UPROPERTY(BlueprintAssignable, Category = "Level")
    FOnLevelUpSignature OnLevelUp;
```

- [ ] **Step 5: `GJCharacter.cpp`의 `UpdateCharacterStat` 교체**

`GJCharacter.cpp`에서 `AGJCharacter::UpdateCharacterStat` 함수 전체(791번째 줄 부근, `void AGJCharacter::UpdateCharacterStat(int32 NewLevel)`부터 닫는 `}`까지)를 찾아 다음으로 교체한다:

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
            CurrentCharacterStat = *RowData;

            // 레벨업 경로(bRestoreToFull=false)에서는 최대치가 오른 만큼만 현재값에 더한다.
            // 체력 30/100에서 최대 체력이 120이 되면 50/120이 된다 - 성장의 이득은 주되
            // 위험한 상태는 그대로 유지된다.
            const float OldMaxHP = MaxHP;
            MaxHP = CurrentCharacterStat.MaxHP;
            CurrentHP = bRestoreToFull ? MaxHP : FMath::Clamp(CurrentHP + (MaxHP - OldMaxHP), 0.f, MaxHP);

            const float OldMaxMP = MaxMP;
            MaxMP = CurrentCharacterStat.MaxMP;
            CurrentMP = bRestoreToFull ? MaxMP : FMath::Clamp(CurrentMP + (MaxMP - OldMaxMP), 0.f, MaxMP);

            // 전투 스탯 - TakeDamage(방어력)와 무기 발사(치명타)가 읽는다.
            // 이쪽은 "현재값"이라는 개념이 없어서 두 경로 모두 그냥 새 값으로 덮어쓴다.
            Defense = CurrentCharacterStat.Defense;
            CritChance = CurrentCharacterStat.CritChance;
            CritMultiplier = CurrentCharacterStat.CritMultiplier;

            // 플레이어 이동 속도는 지금까지 어디서도 설정하지 않아 엔진 기본값을 쓰고 있었다.
            // 이제 데이터 테이블 값으로 명시적으로 설정한다.
            GetCharacterMovement()->MaxWalkSpeed = CurrentCharacterStat.MoveSpeed;
        }
    }

    UpdatePlayerHUD();
}
```

- [ ] **Step 6: `GJCharacter.cpp`에 `IsMaxLevel`/`AddEXP`/`LevelUp` 추가**

Step 5에서 교체한 `UpdateCharacterStat`의 닫는 `}` **아래**, `void AGJCharacter::ApplyConsumableEffect(...)` **위**에 추가한다:

```cpp

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
```

- [ ] **Step 7: 컴파일**

사용자에게 요청한다:
> 에디터에서 **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

이 태스크는 경험치를 주는 쪽이 없어서 PIE로 확인할 것이 없다. **컴파일 통과가 검증이다.**

- [ ] **Step 8: 커밋**

```bash
git add Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp
git commit -m "$(cat <<'EOF'
플레이어 경험치/레벨업 코어 추가

AddEXP/LevelUp/IsMaxLevel과 CurrentEXP를 추가했다. 아직 경험치를 주는
쪽이 없어서 동작 변화는 없다.

RequiredEXP를 "다음 레벨까지 필요한 양"으로 해석해 레벨업 시 빼고
초과분을 이월하며, 루프 구조라 한 번에 여러 레벨이 올라도 정확하다.
레벨 상한은 코드 상수가 아니라 DT_CharacterStat의 마지막 행이라
테이블에 행을 추가하면 코드 수정 없이 만렙이 늘어난다.

UpdateCharacterStat에 bRestoreToFull 인자를 추가해 레벨업에서는
HP/MP 최대치 증가분만 반영되게 했다. 기본값이 true라 기존 호출부는
동작이 바뀌지 않는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: 적 처치 시 경험치 지급

이 태스크가 끝나면 **경험치와 레벨업이 실제로 동작한다.** 다만 화면 표시는 Task 3이라, 확인은 Output Log로 한다.

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h` (`FEnemyStat`)
- Modify: `Source/Project_GJ/GJBaseCharacter.h`
- Modify: `Source/Project_GJ/GJBaseCharacter.cpp` (`TakeDamage`)
- Modify: `Source/Project_GJ/GJEnemyCharacter.h`
- Modify: `Source/Project_GJ/GJEnemyCharacter.cpp` (`ApplyEnemyStat`, `HandleDeath`)
- Modify: `Source/Project_GJ/GJWeapon_Ranged.cpp` (`Fire`) — 아래 Step 7의 선행 버그 수정

**Interfaces:**
- Consumes: `AGJCharacter::AddEXP(float)` (Task 1)
- Produces:
  - `FEnemyStat::ExpReward` (`float`, 기본값 10)
  - `AGJBaseCharacter::LastDamageInstigator` (`TWeakObjectPtr<AController>`, `protected`)
  - `AGJEnemyCharacter::ExpReward` (`float`, `protected`)

- [ ] **Step 1: `FEnemyStat`에 `ExpReward` 추가**

`GJGameTypes.h`의 `FEnemyStat`에서 다음 세 줄을 찾는다(155번째 줄 부근, 구조체의 마지막 필드):

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;
};
```

이를 다음으로 교체한다:

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float CritMultiplier = 2.f;

    // 이 적을 죽였을 때 플레이어가 얻는 경험치. 적 레벨 같은 다른 값에서 유도하지 않고
    // 적마다 명시한다 - 유도하면 "좀 더 단단하게" 같은 밸런스 조정이 성장 속도까지 같이
    // 바꿔버리고, "체력만 많은 샌드백"이나 "약한데 빠른 견제형" 같은 적을 표현할 수 없다.
    // float인 이유: 비교 대상인 FCharacterStat::RequiredEXP가 float이라 파이프라인을 통일한다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float ExpReward = 10.f;
};
```

- [ ] **Step 2: `AGJBaseCharacter`에 킬러 기억용 멤버 추가**

`GJBaseCharacter.h`에서 다음 줄을 찾는다(76번째 줄 부근):

```cpp
protected:
    // 추후에 피격 이펙트나 사망 연출을 붙일 수 있도록 블루프린트에 노출해 둔 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Stat")
    void OnDeath();
```

이를 다음으로 교체한다:

```cpp
protected:
    // 마지막으로 이 캐릭터에게 데미지를 준 컨트롤러. HandleDeath()에는 가해자 정보가 전혀
    // 없어서(인자도 없고 멤버로도 안 남음), TakeDamage에서 사망이 확정되는 순간 기억해 둔다.
    // 적 처치 경험치를 "실제로 죽인 플레이어"에게 주기 위해 필요하다.
    // 약참조인 이유: 적은 죽고 DestroyDelay(기본 2초) 뒤에 파괴되므로, 그 사이에 가해자
    // 컨트롤러가 먼저 사라져도 댕글링 포인터가 되지 않아야 한다.
    UPROPERTY()
    TWeakObjectPtr<AController> LastDamageInstigator;

    // 추후에 피격 이펙트나 사망 연출을 붙일 수 있도록 블루프린트에 노출해 둔 이벤트
    UFUNCTION(BlueprintImplementableEvent, Category = "Stat")
    void OnDeath();
```

- [ ] **Step 3: `TakeDamage`에서 가해자 기억**

`GJBaseCharacter.cpp`의 `AGJBaseCharacter::TakeDamage`에서 다음 블록을 찾는다(66번째 줄 부근):

```cpp
    if (CurrentHP <= 0.f)
    {
        HandleDeath();
    }
```

이를 다음으로 교체한다:

```cpp
    if (CurrentHP <= 0.f)
    {
        // HandleDeath()는 가해자를 인자로 받지 않으므로, 사망이 확정된 이 시점에 기억해 둔다.
        // 적 쪽 HandleDeath 오버라이드가 이 값을 읽어 경험치를 지급한다.
        LastDamageInstigator = EventInstigator;
        HandleDeath();
    }
```

- [ ] **Step 4: `AGJEnemyCharacter`에 `ExpReward` 멤버 추가**

`GJEnemyCharacter.h`에서 다음 블록을 찾는다(70번째 줄 부근):

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AttackWindup = 0.3f;
```

그 **아래**에 추가한다:

```cpp

    // 이 적을 죽인 플레이어에게 주는 경험치
    // (EnemyDataHandle을 할당하면 FEnemyStat.ExpReward 값으로 덮어써짐)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float ExpReward = 10.f;
```

- [ ] **Step 5: `ApplyEnemyStat`에서 `ExpReward` 채우기**

`GJEnemyCharacter.cpp`의 `AGJEnemyCharacter::ApplyEnemyStat`에서 다음 블록을 찾는다(81번째 줄 부근, 함수의 마지막 부분):

```cpp
    // 전투 스탯 - TakeDamage(방어력)와 ApplyAttackDamage(치명타)가 읽는다
    Defense = RowData->Defense;
    CritChance = RowData->CritChance;
    CritMultiplier = RowData->CritMultiplier;
}
```

이를 다음으로 교체한다:

```cpp
    // 전투 스탯 - TakeDamage(방어력)와 ApplyAttackDamage(치명타)가 읽는다
    Defense = RowData->Defense;
    CritChance = RowData->CritChance;
    CritMultiplier = RowData->CritMultiplier;

    // HandleDeath에서 다시 데이터 테이블을 조회하지 않도록 여기서 멤버에 복사해 둔다
    // (다른 스탯들과 동일한 패턴)
    ExpReward = RowData->ExpReward;
}
```

- [ ] **Step 6: `HandleDeath`에서 킬러에게 경험치 지급**

`GJEnemyCharacter.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJCharacter.h"
```

`AGJEnemyCharacter::HandleDeath()`에서 다음 두 줄을 찾는다(161번째 줄 부근, 함수 시작 부분):

```cpp
    Super::HandleDeath();

    // 죽는 순간 대기 중이던 공격 판정이 있다면 취소
```

이를 다음으로 교체한다:

```cpp
    Super::HandleDeath();

    // 죽인 주체에게 경험치를 준다. TakeDamage가 사망 직전에 기억해 둔 가해자 컨트롤러를 쓴다.
    // 캐스팅이 실패하는 경우(적이 적을 죽임, 환경 사망, 컨트롤러가 이미 파괴됨)에는 아무에게도
    // 주지 않는다 - 여기서는 "받을 사람이 없다"가 정답이지 오류가 아니다.
    if (AController* KillerController = LastDamageInstigator.Get())
    {
        if (AGJCharacter* KillerCharacter = Cast<AGJCharacter>(KillerController->GetPawn()))
        {
            KillerCharacter->AddEXP(ExpReward);
        }
    }

    // 죽는 순간 대기 중이던 공격 판정이 있다면 취소
```

**적이 적을 죽이거나 환경 사망하는 경우**는 위의 두 겹 `if`(약참조 유효성 + 캐스팅)가 그대로 걸러낸다. 현재 게임에는 아군 오사나 낙사가 없어 PIE로 재현할 수단이 없으므로 별도 검증 단계를 두지 않는다 — 코드 구조상 널 안전이 보장된다.

- [ ] **Step 7: 선행 버그 수정 — 주운 무기의 총알에 인스티게이터가 없다**

**이걸 안 고치면 주워 쓴 무기로 죽인 적은 경험치를 주지 않는다.**

`AGJWeapon_Ranged::CreateProjectilePool()`은 `BeginPlay`에서 총알을 미리 스폰하면서 그 시점의 `GetInstigator()`를 총알에 박아둔다. 그런데 **필드에 놓여 있다가 주운 무기**는 `BeginPlay` 시점에 주인이 없다 — `AGJWeaponBase::OnPickedUp`의 `SetInstigator`는 **무기 액터에만** 걸리고, 이미 스폰된 총알들의 인스티게이터는 영원히 `nullptr`로 남는다.

그 결과 `AGJProjectile::OnHit`이 `GetInstigatorController()`로 넘기는 값이 `nullptr`이 되고, Step 3에서 저장하는 `LastDamageInstigator`도 `nullptr`이 되어 경험치를 줄 대상을 찾지 못한다. (같은 이유로 `OtherActor != GetInstigator()` 자기 피격 방지도 그 무기에서는 동작하지 않는다.) `EquipWeapon()`으로 스폰되는 시작 무기만 우연히 정상 동작하고 있었다.

`GJWeapon_Ranged.cpp`의 `AGJWeapon_Ranged::Fire()`에서 다음 줄을 찾는다(116번째 줄 부근):

```cpp
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());
```

이를 다음으로 교체한다:

```cpp
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());

        // 총알 풀은 무기의 BeginPlay에서 만들어지는데, 필드에 놓여 있다가 주운 무기는 그 시점에
        // 주인이 없어서 풀 전체가 인스티게이터 nullptr로 굳어버린다(OnPickedUp의 SetInstigator는
        // 무기 액터에만 걸린다). 그러면 OnHit이 넘기는 가해자 컨트롤러가 null이 되어 적 처치
        // 경험치를 줄 대상을 찾지 못하고, 자기 피격 방지도 동작하지 않는다.
        // 발사 시점에 현재 주인으로 갱신하면 스왑/픽업/드랍 어느 경로든 항상 맞는다.
        ProjectileToFire->SetInstigator(GetInstigator());
```

- [ ] **Step 8: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. `FEnemyStat`에 필드를 추가했으니, 다음 단계에서 `DT_EnemyStat`에 `ExpReward` 칸이 안 보이면 **에디터를 완전히 재시작**해야 해(재빌드는 필요 없고 껐다 켜기만).

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 9: 데이터 테이블 채우기 — 여기가 안 되면 레벨업을 볼 수 없다**

사용자에게 요청한다:
> 두 테이블을 채워줘.
>
> **(A) `DT_EnemyStat`** — 각 행에 새로 생긴 `ExpReward` 칸에 **50**을 넣어줘 (테스트하기 쉽게 크게 잡은 값이야).
>
> **(B) `DT_CharacterStat`** — 지금 실질적으로 **레벨 1 행만** 의미가 있어. 다음 행이 없으면 코드가 바로 만렙으로 판단해서 **레벨업 자체를 테스트할 수 없어.** 행 이름 `2`, `3`, `4`, `5`를 추가하고 이렇게 채워줘:
>
> | 행 | MaxHP | MaxMP | BaseAttackPower | RequiredEXP | Defense | MoveSpeed | CooldownReduction | CritChance | CritMultiplier |
> |---|---|---|---|---|---|---|---|---|---|
> | 1 | 100 | 50 | 10 | 100 | 0 | 600 | 0 | 0 | 2.0 |
> | 2 | 120 | 60 | 15 | 250 | 2 | 600 | 0 | 0.05 | 2.0 |
> | 3 | 145 | 70 | 21 | 450 | 4 | 610 | 0 | 0.08 | 2.0 |
> | 4 | 175 | 80 | 28 | 700 | 7 | 620 | 0 | 0.11 | 2.0 |
> | 5 | 210 | 95 | 36 | 1000 | 10 | 630 | 0 | 0.15 | 2.0 |
>
> `CritChance`는 **0.0~1.0 범위**야 — 0.05가 5%다. 100을 넣으면 안 돼.
> 1행 값은 지금 있는 값 그대로니까 안 건드려도 되고, 다르면 위 표대로 맞춰줘.

MCP가 연결되어 있으면 `editor_toolset.toolsets.data_table.DataTableTools`로 시도해도 된다. 단, **행 전체 필드를 명시**해야 한다 — 지정하지 않은 필드가 구조체 기본값으로 리셋된 이력이 있다.

- [ ] **Step 10: 경험치·레벨업 PIE 확인 (Output Log)**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 적을 잡아봐. 아직 UI가 없으니 **Window → Output Log**를 열어두고 봐줘.
>
> - 적을 **2마리** 잡으면(50 + 50 = 100 = 레벨 1의 `RequiredEXP`) 로그에 `LevelUp! Level=2 ...`가 떠야 해.
> - 로그의 `HP=...` 부분을 봐줘. **체력을 좀 깎인 상태에서** 레벨업했다면 `MaxHP`는 120이 됐는데 `CurrentHP`는 120이 아니라 **원래 체력 + 20**이어야 해 (풀 회복이 아님).

Run: `grep -E "LevelUp!" Saved/Logs/Project_GJ.log | tail -10`
Expected: `LevelUp! Level=2, HP=...` 형태의 줄이 나온다.

확인 항목:
- 레벨업이 로그에 찍힌다
- `CurrentHP`가 `MaxHP`와 같지 않다(체력이 깎인 상태로 레벨업한 경우) — 같다면 `bRestoreToFull`이 잘못 전달된 것이다

이어서 **필드에서 주운 무기**로도 확인한다(Step 7의 수정이 실제로 걸렸는지 보는 단계다):
> 맵에 놓인 무기를 **E로 주워서** 그 무기로 적을 잡아봐. 시작 무기와 **똑같이** 경험치가 들어와야 해.

주운 무기로만 경험치가 안 들어오면 Step 7의 `SetInstigator`가 빠진 것이다.

- [ ] **Step 11: 다중 레벨업·이월 확인**

사용자에게 요청한다:
> `DT_EnemyStat`의 `ExpReward`를 **400**으로 임시로 올리고 적 **한 마리**만 잡아봐.

Run: `grep -E "LevelUp!" Saved/Logs/Project_GJ.log | tail -10`
Expected: 한 번의 킬로 `Level=2`와 `Level=3` 로그가 **연속으로** 찍힌다 (400 → 레벨1의 100 차감 후 300 남음 → 레벨2의 250 차감 후 50 남음 → 레벨3에서 멈춤).

이월이 안 되면 `CurrentEXP -= ...` 대신 `CurrentEXP = 0`을 한 것이다.

- [ ] **Step 12: 만렙 안전성 확인**

사용자에게 요청한다:
> `ExpReward`를 **5000**으로 올리고 적을 3~4마리 잡아봐. 레벨 5(마지막 행)까지 올라간 뒤로는 **더 이상 레벨업 로그가 안 떠야 하고, 크래시도 없어야** 해. Output Log에 `FindRow` 관련 경고도 쌓이면 안 돼.
>
> 확인했으면 `ExpReward`를 **50**으로 되돌려줘.

Run: `grep -E "LevelUp!|Failed to find row|FindRow" Saved/Logs/Project_GJ.log | tail -20`
Expected: `Level=5`까지만 나오고, `FindRow`/`Failed to find row` 경고가 없다.

경고가 쌓이면 `IsMaxLevel`의 `bWarnIfRowMissing`에 `false`를 안 넘긴 것이다.

- [ ] **Step 13: 런 초기화 확인**

사용자에게 요청한다:
> 레벨을 2 이상으로 올린 뒤 일부러 죽어봐. 게임오버 → 허브 → 포탈로 새 런을 시작했을 때 **레벨이 1로 돌아가 있어야** 해. Output Log를 지우고 새 런에서 적을 2마리 잡았을 때 `LevelUp! Level=2`가 다시 떠야 정상이야.

레벨이 유지되면 캐릭터가 재스폰되지 않은 것이므로 M1 런 루프 쪽을 확인해야 한다.

- [ ] **Step 14: 테스트 값 원복 확인**

사용자에게 요청한다:
> 테스트하느라 바꿨던 `DT_EnemyStat`의 `ExpReward`가 **50**으로 돌아와 있는지 확인해줘.

극단적인 테스트 값이 커밋에 섞이면 나중에 밸런스가 이상하다고 착각하게 된다.

- [ ] **Step 15: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJBaseCharacter.h Source/Project_GJ/GJBaseCharacter.cpp Source/Project_GJ/GJEnemyCharacter.h Source/Project_GJ/GJEnemyCharacter.cpp Source/Project_GJ/GJWeapon_Ranged.cpp Content/GJ/DataTables/DT_EnemyStat.uasset Content/GJ/DataTables/DT_CharacterStat.uasset
git commit -m "$(cat <<'EOF'
적 처치 시 경험치 지급

FEnemyStat에 ExpReward를 추가하고, 적이 죽으면 실제로 죽인 플레이어에게
경험치를 준다. 이제 경험치와 레벨업이 동작한다(UI는 아직 없음).

HandleDeath에는 가해자 정보가 전혀 없어서, TakeDamage에서 사망이
확정되는 시점에 LastDamageInstigator로 기억해 두는 방식으로 해결했다.
적은 죽고 2초 뒤에 파괴되므로 그 사이 컨트롤러가 먼저 사라질 수 있어
약참조로 잡았다. 캐스팅이 실패하면(적끼리 죽임, 환경 사망) 조용히 넘어간다.

함께 발견한 선행 버그도 고쳤다. 총알 풀은 무기 BeginPlay에서 만들어지는데
필드에 놓여 있다 주운 무기는 그 시점에 주인이 없어서 풀 전체가 인스티게이터
nullptr로 굳어 있었다(OnPickedUp의 SetInstigator는 무기 액터에만 걸린다).
그러면 가해자 컨트롤러가 null이라 경험치를 줄 대상을 못 찾고 자기 피격
방지도 동작하지 않는다. 발사 시점에 현재 주인으로 갱신하도록 했다.

DT_CharacterStat에 레벨 2~5 성장 곡선을 추가했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 경험치 UI

**Files:**
- Modify: `Source/Project_GJ/GJPlayerHUDWidget.h`
- Modify: `Source/Project_GJ/GJPlayerHUDWidget.cpp`
- Modify: `Source/Project_GJ/GJCharacter.cpp` (`UpdatePlayerHUD`)

**Interfaces:**
- Consumes: `AGJCharacter::CurrentEXP`, `AGJCharacter::CurrentCharacterStat.RequiredEXP`, `AGJCharacter::CurrentLevel` (Task 1)
- Produces: `void UGJPlayerHUDWidget::UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level)`

- [ ] **Step 1: `GJPlayerHUDWidget.h` 교체**

파일 전체를 다음으로 교체한다:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;

UCLASS()
class PROJECT_GJ_API UGJPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateHP(float CurrentHP, float MaxHP);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateMP(float CurrentMP, float MaxMP);

    // RequiredEXP는 "이번 레벨의 목표치"다(누적 총량이 아님). 경험치 바는 이번 레벨의 진행도만 그린다.
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level);

protected:
    // WBP 디자이너에서 이 이름들과 똑같은 Progress Bar 위젯을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HPBar;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* MPBar;

    // HP/MP와 달리 BindWidgetOptional이다. strict BindWidget으로 두면 C++이 먼저 들어간 순간
    // WBP_PlayerHUD 컴파일이 깨져서, 에디터에서 위젯을 배치할 때까지 게임이 정상 동작하지 않는다.
    // 이 프로젝트는 C++ 변경과 에디터 작업이 항상 시차를 두고 일어나므로 Optional이 맞다.
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* EXPBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* LevelText;
};
```

- [ ] **Step 2: `GJPlayerHUDWidget.cpp`에 `UpdateEXP` 추가**

파일 상단 include 블록을 다음으로 교체한다:

```cpp
#include "GJPlayerHUDWidget.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
```

그리고 파일 **맨 끝**(`UpdateMP` 함수의 닫는 `}` 아래)에 추가한다:

```cpp

void UGJPlayerHUDWidget::UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level)
{
    if (EXPBar)
    {
        // RequiredEXP가 0이면(데이터 미입력 등) 나누기 0이 되므로 가득 찬 것으로 표시한다
        const float Percent = (RequiredEXP > 0.f)
            ? FMath::Clamp(CurrentEXP / RequiredEXP, 0.f, 1.f)
            : 1.f;
        EXPBar->SetPercent(Percent);
    }

    if (LevelText)
    {
        LevelText->SetText(FText::AsNumber(Level));
    }
}
```

- [ ] **Step 3: `UpdatePlayerHUD`에서 `UpdateEXP` 호출**

`GJCharacter.cpp`의 `AGJCharacter::UpdatePlayerHUD`에서 다음 두 줄을 찾는다(842번째 줄 부근):

```cpp
    PlayerHUDWidgetInstance->UpdateHP(CurrentHP, MaxHP);
    PlayerHUDWidgetInstance->UpdateMP(CurrentMP, MaxMP);
```

이를 다음으로 교체한다:

```cpp
    PlayerHUDWidgetInstance->UpdateHP(CurrentHP, MaxHP);
    PlayerHUDWidgetInstance->UpdateMP(CurrentMP, MaxMP);
    PlayerHUDWidgetInstance->UpdateEXP(CurrentEXP, CurrentCharacterStat.RequiredEXP, CurrentLevel);
```

새 갱신 경로를 만들지 않는다 — 레벨업(`UpdateCharacterStat`), 경험치 획득(`AddEXP`), 피격, 소비 아이템 사용이 이미 전부 이 함수를 거친다.

- [ ] **Step 4: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -E "error C|error :" Saved/Logs/Project_GJ.log | tail -20`
Expected: `error C`가 없다.

- [ ] **Step 5: `WBP_PlayerHUD`에 위젯 배치**

사용자에게 요청한다:
> `WBP_PlayerHUD`를 열어서 두 개를 추가해줘. **이름이 정확히 일치해야** 자동 바인딩돼.
>
> - **Progress Bar** 하나, 이름 `EXPBar` — HP/MP 바 아래쪽에 가로로 길게
> - **Text Block** 하나, 이름 `LevelText` — 경험치 바 옆이나 위에
>
> 둘 다 **Is Variable 체크박스가 켜져 있어야** 해(기본으로 켜져 있음). 색은 HP/MP와 구분되게 아무거나 골라줘.
>
> 배치 전에도 컴파일은 깨지지 않아 — `BindWidgetOptional`이라 위젯이 없으면 그냥 갱신을 건너뛴다.

- [ ] **Step 6: UI PIE 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 확인해줘.
>
> - 시작하면 레벨 숫자가 **1**, 경험치 바가 **비어 있어야** 해
> - 적을 한 마리 잡으면 경험치 바가 **절반**까지 차야 해 (`ExpReward` 50 / `RequiredEXP` 100)
> - 두 마리째 잡으면 레벨 숫자가 **2**로 바뀌고 경험치 바가 **다시 비워져야** 해 (레벨 2의 목표치는 250이라 이월된 0에서 시작)
> - 레벨 5(만렙)까지 올린 뒤에는 적을 더 잡아도 경험치 바가 **가득 찬 채로 고정**되어야 해

확인 항목: 위 네 가지가 모두 맞아야 한다. 바가 전혀 안 움직이면 위젯 이름이 `EXPBar`와 다른 것이다(대소문자 포함).

- [ ] **Step 7: 커밋**

```bash
git add Source/Project_GJ/GJPlayerHUDWidget.h Source/Project_GJ/GJPlayerHUDWidget.cpp Source/Project_GJ/GJCharacter.cpp Content/GJ/UI/WBP_PlayerHUD.uasset
git commit -m "$(cat <<'EOF'
HUD에 경험치 바와 레벨 표시 추가

UGJPlayerHUDWidget에 EXPBar/LevelText를 추가하고 UpdatePlayerHUD에서
함께 갱신한다. 경험치 바는 누적이 아니라 이번 레벨의 진행도를 그린다.

HP/MP와 달리 BindWidgetOptional로 선언했다. strict BindWidget이면
C++이 먼저 들어간 시점에 WBP_PlayerHUD 컴파일이 깨져서, 에디터에서
위젯을 배치하기 전까지 게임이 동작하지 않기 때문이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

> ⚠️ `Content/GJ/UI/WBP_PlayerHUD.uasset` 경로가 실제와 다르면 `git status`로 확인해서 맞는 경로를 `git add` 한다.

---

## Task 4: 개발 가이드 갱신

**Files:**
- Modify: `Docs/DevGuide.md`
- Modify: `DevGuide.html`

**Interfaces:**
- Consumes: Task 1~3의 모든 변경
- Produces: 없음 (문서화)

- [ ] **Step 1: 2.2절 "스탯 / 레벨" 항목 교체**

`Docs/DevGuide.md`의 132~133번째 줄 부근에서 다음을 찾는다:

```markdown
**스탯 / 레벨**
- `UpdateCharacterStat(NewLevel)` — `DT_CharacterStat`에서 레벨(행 이름) 조회 → `MaxHP/CurrentHP`, `MaxMP/CurrentMP` 갱신 → `UpdatePlayerHUD()` 호출
```

이를 다음으로 교체한다:

```markdown
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
```

- [ ] **Step 2: 5절 데미지 파이프라인에 경험치 지급 흐름 추가**

`Docs/DevGuide.md`의 5절에서 다음 줄을 찾는다(304번째 줄 부근):

```
  → CurrentHP <= 0 이면 HandleDeath()
```

이를 다음으로 교체한다:

```
  → CurrentHP <= 0 이면
       LastDamageInstigator = EventInstigator   ← 가해자를 기억(경험치 지급용)
       HandleDeath()
```

그리고 317번째 줄 부근의 "**치명타 여부는 대상에게 전달되지 않는다.**"로 시작하는 문단 **아래**에 추가한다:

```markdown
**적 처치 경험치는 `TakeDamage`가 기억한 가해자로 지급된다.** `HandleDeath()`에는 가해자 정보가 전혀 없어서(인자도 없고 멤버로도 안 남음), `AGJBaseCharacter::TakeDamage`가 사망이 확정되는 시점에 `LastDamageInstigator`(`TWeakObjectPtr<AController>`)로 기억해 둔다. `AGJEnemyCharacter::HandleDeath()`가 이걸 읽어 `GetPawn()` → `AGJCharacter` 캐스팅에 성공하면 `AddEXP(ExpReward)`를 호출한다. 약참조인 이유는 적이 죽고 `DestroyDelay`(기본 2초) 뒤에 파괴되므로 그 사이 컨트롤러가 먼저 사라질 수 있기 때문이다. 캐스팅이 실패하면(적끼리 죽임, 환경 사망) 아무에게도 주지 않는다 — 여기서는 "받을 사람이 없다"가 정답이다.
```

- [ ] **Step 3: 4.2절에 총알 인스티게이터 갱신 기록**

`Docs/DevGuide.md`의 4.2절(`AGJWeapon_Ranged`) 끝에 추가한다:

```markdown
> `Fire()`는 풀에서 꺼낸 총알에 **매 발사마다 `SetInstigator(GetInstigator())`를 다시 건다.** 총알 풀은 무기의 `BeginPlay`에서 만들어지는데, 필드에 놓여 있다가 주운 무기는 그 시점에 주인이 없어서 풀 전체가 인스티게이터 `nullptr`로 굳어버린다(`OnPickedUp`의 `SetInstigator`는 **무기 액터에만** 걸린다). 그 상태로는 `AGJProjectile::OnHit`이 넘기는 가해자 컨트롤러가 null이라 **적 처치 경험치가 아무에게도 안 가고**, `OtherActor != GetInstigator()` 자기 피격 방지도 동작하지 않는다. `EquipWeapon()`으로 스폰되는 시작 무기만 우연히 정상이었던 문제다.
```

- [ ] **Step 4: 7절 UI 표에 EXP 바 반영**

`Docs/DevGuide.md`의 7절 표에서 다음 줄을 찾는다(385번째 줄 부근):

```markdown
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` | `UpdateHP(Current,Max)` / `UpdateMP(Current,Max)` | 좌상단, `AddToViewport()` |
```

이를 다음으로 교체한다:

```markdown
| `UGJPlayerHUDWidget` | `WBP_PlayerHUD` | `HPBar`, `MPBar` (strict) / `EXPBar`, `LevelText` (**Optional**) | `UpdateHP(Current,Max)` / `UpdateMP(Current,Max)` / `UpdateEXP(Current,Required,Level)` | 좌상단, `AddToViewport()` |
```

그리고 같은 절의 387번째 줄(인벤토리/게임오버 위젯을 설명하는 문단) **아래**에 추가한다:

```markdown
`EXPBar`/`LevelText`가 strict `BindWidget`이 아니라 **`BindWidgetOptional`인 이유**: strict로 두면 C++이 먼저 들어간 순간 `WBP_PlayerHUD` 컴파일이 깨져서, 에디터에서 위젯을 배치하기 전까지 게임이 정상 동작하지 않는다. 이 프로젝트는 C++ 변경과 에디터 작업이 항상 시차를 두고 일어나므로 새로 추가하는 바인딩은 Optional이 안전하다.
```

- [ ] **Step 5: 8절 데이터 테이블 스키마 갱신**

`Docs/DevGuide.md`의 `FCharacterStat` 표에서 다음 줄을 찾는다(406번째 줄):

```markdown
| `RequiredEXP` | 100 | |
```

이를 다음으로 교체한다:

```markdown
| `RequiredEXP` | 100 | **이 레벨에서 다음 레벨까지 필요한 경험치**(누적 총량 아님). 레벨업 시 이 값을 빼고 초과분을 이월. **마지막 행이 곧 레벨 상한** — 행을 추가하면 코드 수정 없이 만렙이 늘어남 |
```

`FEnemyStat` 표의 마지막 줄(440번째 줄 부근) **아래**에 추가한다:

```markdown
| `ExpReward` | 10 | 이 적을 죽인 플레이어가 얻는 경험치. 적 레벨 등에서 유도하지 않고 적마다 명시 — 유도하면 "좀 더 단단하게" 같은 조정이 성장 속도까지 같이 바꿔버림. `float`인 이유는 비교 대상인 `RequiredEXP`가 `float`이라 파이프라인을 통일하기 위함 |
```

- [ ] **Step 6: 9절 TODO 목록 갱신**

`Docs/DevGuide.md`의 473번째 줄에서 다음을 찾아 **삭제한다**:

```markdown
- `RequiredEXP`는 데이터 테이블에만 있고 코드에서 한 번도 안 읽힘 — EXP/레벨업 자체가 없어서 `UpdateCharacterStat`은 BeginPlay에서 레벨 1로 한 번만 호출됨 (M2)
```

같은 목록의 끝에 다음을 **추가한다**:

```markdown
- 레벨업 시 선택지(카드 3장)가 없음 — `AGJCharacter::OnLevelUp` 델리게이트만 준비돼 있고 구독자가 없음. 스테이지 클리어 쪽 트리거는 진행 구조(M5)가 생긴 뒤에 별도로 필요
- 레벨업/경험치 획득 연출(팝업, 사운드, 파티클)이 없음 — 현재는 HUD 바와 `UE_LOG`뿐
- `DT_CharacterStat`의 레벨 2~5 성장 곡선은 **임시 테스트 값** — 실제 밸런싱은 스테이지 진행(M5)이 생긴 뒤에 해야 의미가 있음
```

- [ ] **Step 7: `DevGuide.html`에 동일 내용 반영**

`DevGuide.html`은 `Docs/DevGuide.md`와 같은 내용을 HTML로 옮긴 문서다. Step 1~6의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다.

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

- [ ] **Step 8: 커밋**

```bash
git add Docs/DevGuide.md DevGuide.html
git commit -m "$(cat <<'EOF'
개발 가이드에 EXP/레벨업 반영

경험치 누적과 레벨업 API, 킬러 판별 방식(LastDamageInstigator),
RequiredEXP의 비누적 해석과 테이블 마지막 행 = 레벨 상한 규칙,
레벨업이 풀 회복이 아닌 이유, EXP UI가 BindWidgetOptional인 이유,
주운 무기의 총알 인스티게이터 갱신 이유를 문서화했다.

RequiredEXP 미사용 TODO를 지우고, 카드 선택 시스템과 레벨업 연출이
없다는 새 갭을 기록했다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```
