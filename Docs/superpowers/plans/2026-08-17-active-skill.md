# 액티브 스킬 시스템 (M2.7) 구현 계획

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 우클릭을 누르고 있으면 차징되고, 떼면 구체가 날아간다. 누른 시간에 비례해 구체가 커지고 데미지가 오른다.

**Architecture:** 로직 전부를 새 `UGJSkillComponent`에 넣고 `AGJCharacter` 생성자에서 붙인다. 캐릭터는 입력을 넘기기만 하고 스킬을 모른다. 구체는 기존 `AGJProjectile`에 크기·관통 파라미터를 더해 재사용한다. 차징과 쿨타임은 틱이 아니라 시각(timestamp) 비교로 계산한다.

**Tech Stack:** UE 5.8, C++ (`UActorComponent`, `FTableRowBase`, Enhanced Input, `UFUNCTION(Exec)`, `UProjectileMovementComponent`)

**설계 문서:** `Docs/superpowers/specs/2026-08-17-active-skill-design.md`

## Global Constraints

- **테스트 스위트가 없다.** 검증은 **Live Coding 컴파일 통과 + 수동 PIE 확인**이다. 각 태스크의 검증은 구체적인 PIE 조작 시나리오로 기술한다.
- **캐릭터는 스킬을 모른다.** 스킬 상태·로직은 전부 `UGJSkillComponent`에 둔다. `AGJCharacter`에 추가하는 것은 컴포넌트 생성, 입력 전달 함수, MP/스탯 접근자, 콘솔 명령뿐이다.
- **틱을 쓰지 않는다.** `PrimaryComponentTick.bCanEverTick = false`. 차징 경과와 쿨타임 잔량은 `GetWorld()->GetTimeSeconds()` 비교로 구한다.
- **`ECharacterState`에 값을 추가하지 않는다.** 차징 여부는 `UGJSkillComponent::IsCharging()`으로 묻는다.
- **콘솔 명령은 `AGJCharacter`에 단다.** 컴포넌트에 `UFUNCTION(Exec)`를 달면 콘솔이 `Command not recognized`를 낸다(M2.6에서 확인).
- **인코딩**: 새 주석은 UTF-8 한글로 그냥 쓴다. 초기 파일의 깨진 옛 주석 줄은 건드리지 않는다.
- **커밋 메시지는 한국어**로 쓰고 본문 끝에 `Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>`를 넣는다. 브랜치를 나누지 않고 `main`에서 작업한다.
- 에디터가 열려 있으면 UBT 빌드가 막힌다. 컴파일은 사용자에게 **Ctrl+Alt+F11**을 요청한다.
- **MCP(포트 8123)로 데이터 테이블·블루프린트를 만들 수 있다.** 실패하면 사용자에게 에디터 작업을 요청한다.
- PIE에서 `~` 콘솔은 디버그 매니저가 가로챈다. **Output Log 창의 `Cmd:` 입력칸**을 쓰고, 명령은 **한 줄씩** 입력한다.

## 설계 문서에서 고친 것 (구현 중 발견)

### 1. 관통은 콜리전 프로필을 바꿔야 동작한다

`AGJProjectile`의 콜리전 프로필은 `BlockAllDynamic`(`GJProjectile.cpp:18`)이고 `bShouldBounce = false`다. 이 상태로 적에게 닿으면 `UProjectileMovementComponent`가 **그 자리에서 멈춘다.** `Deactivate()`를 안 부르면 구체가 적 앞에 박혀 있을 뿐 관통하지 않는다.

그래서 발사 시점에 관통 여부로 콜리전을 갈라야 한다:

| | 콜리전 | 타격 경로 |
|---|---|---|
| `PierceCount == 0` | `BlockAllDynamic` (지금 그대로) | `OnComponentHit` |
| `PierceCount != 0` | 벽만 Block, 폰은 Overlap | `OnComponentBeginOverlap` (벽은 여전히 `OnComponentHit`) |

### 2. `PierceCount` 의미를 확정한다

설계 문서의 "0=관통 없음, N=N명까지"는 0이 1명을 뜻하게 되어 어긋난다. **`PierceCount`는 "추가로 관통하는 적 수"**로 확정한다:

| 값 | 맞히는 적 수 |
|---|---|
| `0` | 1명 (관통 없음) |
| `1` | 2명 |
| `2` | 3명 |
| `-1` | 무한 (사거리를 다 날아가야 소멸) |

---

## 파일 구조

| 파일 | 책임 | 태스크 |
|---|---|---|
| `Source/Project_GJ/GJGameTypes.h` (수정) | `SkillPower` 필드, `ESkillType`, `FSkillData`, `FCardData::SkillId` | 1, 3, 5 |
| `Source/Project_GJ/GJGameTypes.cpp` (수정) | `operator+=`에 `SkillPower` | 1 |
| `Source/Project_GJ/GJCharacter.h/.cpp` (수정) | 스탯 연쇄, MP 접근자, 컴포넌트 생성, 입력 전달, 차징 게이트, 콘솔 | 1, 3, 4 |
| `Source/Project_GJ/GJProjectile.h/.cpp` (수정) | 크기 배율, 관통, 중복 타격 방지 | 2 |
| `Source/Project_GJ/GJWeapon_Ranged.cpp` (수정) | 새 인자에 `1.0f, 0` 전달 | 2 |
| `Source/Project_GJ/GJSkillComponent.h/.cpp` (신규) | 슬롯·쿨타임·차징·MP 검사·발사·풀 | 3 |
| `Source/Project_GJ/GJCardComponent.h/.cpp` (수정) | `IsStatEffectEmpty`, `Ability` 분기, `SkillReplace` 모드 | 1, 5 |
| `Data/DT_CharacterStat.csv` (수정) | `SkillPower` 칼럼 | 1 |
| `Content/GJ/DataTables/DT_SkillData` (신규 에셋) | 스킬 정의 | 3 |
| `Content/GJ/BluePrint/BP_GJSkillProjectile` (신규 에셋) | 구체 비주얼 | 3 |
| `Docs/DevGuide.md`, `DevGuide.html` (수정) | 문서화 | 6 |

**태스크 순서 근거**: 스탯 토대(1) → 구체 확장(2, 기존 총으로 회귀 검증) → 스킬이 실제로 발사됨(3) → 차징 제약(4) → 카드 연동(5) → 문서(6). Task 2가 끝나면 **기존 총이 안 깨졌는지**가 확인되고, Task 3이 끝나면 **우클릭으로 실제 발사**된다.

---

## Task 1: `SkillPower` 스탯 추가

M2.5 3층 스탯 구조에 필드 하나를 끼워 넣는다. **컴파일러가 안 잡아주는 지점이 많아 한 곳이라도 빠지면 조용히 깨진다.**

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Modify: `Source/Project_GJ/GJGameTypes.cpp`
- Modify: `Source/Project_GJ/GJCharacter.h`, `Source/Project_GJ/GJCharacter.cpp`
- Modify: `Source/Project_GJ/GJCardComponent.cpp`
- Modify: `Data/DT_CharacterStat.csv`

**Interfaces:**
- Consumes: `FCharacterStat`, `FStatValues`, `AGJCharacter::RecalculateStats` (M2.5)
- Produces:
  - `FCharacterStat::SkillPower` (float)
  - `FStatValues::SkillPower` (float)
  - `float AGJCharacter::GetSkillPower() const`
  - `float AGJCharacter::GetCurrentMP() const`
  - `bool AGJCharacter::ConsumeMP(float Amount)`

- [ ] **Step 1: `FCharacterStat`에 `SkillPower` 추가**

`GJGameTypes.h`에서 다음 줄을 찾는다(28~29번째 줄):

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 10.0f;
```

이를 다음으로 교체한다:

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 10.0f;

    // 스킬 데미지에만 쓰이는 공격력. BaseAttackPower와 나눈 이유는 평타 특화와
    // 스킬 특화 빌드를 갈라놓기 위해서다 - 하나로 합치면 카드가 항상 양쪽을 같이 올린다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float SkillPower = 10.0f;
```

- [ ] **Step 2: `FStatValues`에 `SkillPower` 추가**

`GJGameTypes.h`의 `FStatValues` 안에서 다음 줄을 찾는다(85번째 줄 부근 — `FCharacterStat`이 아니라 **기본값이 0인 쪽**이다):

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 0.f;
```

이를 다음으로 교체한다:

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float BaseAttackPower = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stat")
    float SkillPower = 0.f;
```

- [ ] **Step 3: `operator+=`에 한 줄 추가**

`GJGameTypes.cpp`에서 다음 줄을 찾는다:

```cpp
    BaseAttackPower   += Other.BaseAttackPower;
```

그 **아래**에 추가한다:

```cpp
    SkillPower        += Other.SkillPower;
```

- [ ] **Step 4: `RecalculateStats`에 실효값 계산 추가**

`GJCharacter.cpp`에서 다음 줄을 찾는다(856번째 줄 부근):

```cpp
    S.BaseAttackPower = FMath::Max(Combine(BaseStat.BaseAttackPower, StatBonus.Add.BaseAttackPower, StatBonus.Percent.BaseAttackPower), 0.f);
```

그 **아래**에 추가한다:

```cpp
    // 음수 클램프가 필요한 이유는 BaseAttackPower와 같다 - 음수 데미지는 적을 치료한다.
    S.SkillPower = FMath::Max(Combine(BaseStat.SkillPower, StatBonus.Add.SkillPower, StatBonus.Percent.SkillPower), 0.f);
```

- [ ] **Step 5: `GJAddBonus`의 이름 매핑에 추가**

`GJCharacter.cpp`의 `GJAddBonus` 안에서 다음 줄을 찾는다(1019번째 줄 부근):

```cpp
        TryApply(TEXT("BaseAttackPower"),   &FStatValues::BaseAttackPower)   ||
```

그 **아래**에 추가한다:

```cpp
        TryApply(TEXT("SkillPower"),        &FStatValues::SkillPower)        ||
```

이어서 오타 경고의 목록 문자열을 찾는다:

```cpp
            TEXT("GJAddBonus: 알 수 없는 스탯 '%s'. 사용 가능: MaxHP, MaxMP, BaseAttackPower, RequiredEXP, Defense, MoveSpeed, CooldownReduction, CritChance, CritMultiplier"),
```

이를 다음으로 교체한다:

```cpp
            TEXT("GJAddBonus: 알 수 없는 스탯 '%s'. 사용 가능: MaxHP, MaxMP, BaseAttackPower, SkillPower, RequiredEXP, Defense, MoveSpeed, CooldownReduction, CritChance, CritMultiplier"),
```

마지막으로 성공 로그에도 값을 실어 확인이 되게 한다. 다음 두 줄을 찾는다:

```cpp
        TEXT("GJAddBonus: %s (가산 %.2f, 증가율 %.0f%%) -> HP=%.0f/%.0f, 공격력=%.1f, 방어력=%.1f, 치명타=%.2f/x%.2f, 이동속도=%.0f, RequiredEXP=%.0f"),
```

이를 다음으로 교체한다:

```cpp
        TEXT("GJAddBonus: %s (가산 %.2f, 증가율 %.0f%%) -> HP=%.0f/%.0f, 공격력=%.1f, 스킬공격력=%.1f, 방어력=%.1f, 치명타=%.2f/x%.2f, 이동속도=%.0f, RequiredEXP=%.0f"),
```

그리고 그 아래 인자 목록에서 다음 줄을 찾는다:

```cpp
        CurrentCharacterStat.BaseAttackPower, Defense,
```

이를 다음으로 교체한다:

```cpp
        CurrentCharacterStat.BaseAttackPower, CurrentCharacterStat.SkillPower, Defense,
```

- [ ] **Step 6: `IsStatEffectEmpty`에 추가 (빠뜨리면 카드가 사라진다)**

`GJCardComponent.cpp`에서 다음 줄을 찾는다:

```cpp
        return V.MaxHP == 0.f && V.MaxMP == 0.f && V.BaseAttackPower == 0.f
            && V.RequiredEXP == 0.f && V.Defense == 0.f && V.MoveSpeed == 0.f
            && V.CooldownReduction == 0.f && V.CritChance == 0.f && V.CritMultiplier == 0.f;
```

이를 다음으로 교체한다:

```cpp
        return V.MaxHP == 0.f && V.MaxMP == 0.f && V.BaseAttackPower == 0.f
            && V.SkillPower == 0.f
            && V.RequiredEXP == 0.f && V.Defense == 0.f && V.MoveSpeed == 0.f
            && V.CooldownReduction == 0.f && V.CritChance == 0.f && V.CritMultiplier == 0.f;
```

**이 한 줄을 빠뜨리면** 스킬 공격력만 올리는 카드가 "효과가 전부 0"으로 판정되어 뽑기 후보에서 조용히 제외된다.

- [ ] **Step 7: 접근자 3개 추가**

`GJCharacter.h`에서 다음 줄을 찾는다(271번째 줄):

```cpp
    float GetAttackPower() const { return CurrentCharacterStat.BaseAttackPower; }
```

그 **아래**에 추가한다:

```cpp

    // 스킬 데미지 계산에 쓰인다. 실효값(테이블 + 보너스)이다.
    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetSkillPower() const { return CurrentCharacterStat.SkillPower; }

    UFUNCTION(BlueprintPure, Category = "Character Stat")
    float GetCurrentMP() const { return CurrentMP; }

    // MP가 충분하면 차감하고 true, 부족하면 아무것도 안 하고 false.
    // HUD 갱신까지 여기서 하는 이유: 호출자마다 UpdatePlayerHUD를 기억하게 하면 언젠가 빠뜨려서
    // "MP는 줄었는데 바는 그대로"가 된다.
    UFUNCTION(BlueprintCallable, Category = "Character Stat")
    bool ConsumeMP(float Amount);
```

> `GetAttackPower()` 선언이 위 형태와 다르면, 그 선언 바로 아래에 위 세 개를 넣는다.

- [ ] **Step 8: `ConsumeMP` 구현**

`GJCharacter.cpp`에서 `void AGJCharacter::GJAddBonus(` 함수 정의를 찾아, 그 **바로 위**에 추가한다:

```cpp
bool AGJCharacter::ConsumeMP(float Amount)
{
    if (Amount <= 0.f)
    {
        return true;
    }

    if (CurrentMP < Amount)
    {
        return false;
    }

    CurrentMP = FMath::Clamp(CurrentMP - Amount, 0.f, MaxMP);
    UpdatePlayerHUD();
    return true;
}
```

- [ ] **Step 9: CSV에 칼럼 추가**

`Data/DT_CharacterStat.csv`를 통째로 다음 내용으로 교체한다:

```
Name,MaxHP,MaxMP,BaseAttackPower,SkillPower,RequiredEXP,Defense,MoveSpeed,CooldownReduction,CritChance,CritMultiplier
1,100,50,10,10,100,0,600,0,0,2
2,120,60,15,14,250,2,600,0,0.05,2
3,145,70,21,20,450,4,610,0,0.08,2
4,175,80,28,27,700,7,620,0,0.11,2
5,210,95,36,35,1000,10,630,0,0.15,2
```

성장 곡선은 `BaseAttackPower`와 비슷하되 살짝 낮게 잡았다. 실제 밸런싱은 스테이지 진행(M5)이 생긴 뒤에 해야 의미가 있으므로 지금은 "둘이 비슷하게 자란다"만 맞춘다.

- [ ] **Step 10: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. `USTRUCT` 필드를 추가했으니 UHT가 먼저 돈다.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 11: 데이터 테이블 리임포트**

사용자에게 요청한다:
> 콘텐츠 브라우저에서 `DT_CharacterStat`을 우클릭 → **Reimport**해줘. 그다음 열어서 **SkillPower 칼럼이 보이고 레벨 1이 10인지** 확인해줘.

리임포트가 회색으로 비활성이면 소스 파일 경로가 기록돼 있지 않은 것이다. 그때는 `Data/DT_CharacterStat.csv`를 콘텐츠 브라우저의 `DT_CharacterStat` 위로 **드래그해서 덮어쓰기 임포트**하면 경로가 기록된다.

- [ ] **Step 12: PIE 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 Output Log의 `Cmd:` 입력칸에 **한 줄씩** 쳐줘:
>
> ```
> GJAddBonus SkillPower 5 0
> ```
>
> 그리고 오타도 하나:
>
> ```
> GJAddBonus SkilPower 5 0
> ```

확인 항목:
- 첫 번째는 정상 적용되고 경고가 없다
- 두 번째는 "유효한 스탯 이름" 경고가 뜨고, 그 목록에 **`SkillPower`가 들어 있다**
- 레벨업해도 `SkillPower` 보너스가 살아남는다(테이블 값 + 보너스)

- [ ] **Step 13: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJGameTypes.cpp Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Source/Project_GJ/GJCardComponent.cpp Data/DT_CharacterStat.csv Content/GJ/DataTables/DT_CharacterStat.uasset
git commit -m "$(cat <<'EOF'
스킬 공격력 스탯 추가

SkillPower를 FCharacterStat과 FStatValues 양쪽에 넣었다. 평타 공격력과
나눈 이유는 평타 특화와 스킬 특화 빌드를 갈라놓기 위해서다.

필드 하나를 늘리면서 컴파일러가 안 잡아주는 여섯 군데를 같이 고쳤다.
operator+=, RecalculateStats, GJAddBonus의 이름 매핑, 그리고
IsStatEffectEmpty다. 마지막 것을 빠뜨리면 스킬 공격력만 올리는 카드가
효과 없는 카드로 판정되어 뽑기에서 조용히 사라진다.

MP를 밖에서 쓰기 위해 GetCurrentMP와 ConsumeMP를 열었다. HUD 갱신을
ConsumeMP 안에서 하는 이유는 호출자마다 기억하게 하면 언젠가 빠뜨려서
MP는 줄었는데 바는 그대로인 상태가 되기 때문이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 2: `AGJProjectile`에 크기 배율과 관통 추가

**동작 변화는 기존 총에 없어야 한다.** 이 태스크의 검증은 "총이 예전과 똑같이 나가는가"다.

**Files:**
- Modify: `Source/Project_GJ/GJProjectile.h`, `Source/Project_GJ/GJProjectile.cpp`
- Modify: `Source/Project_GJ/GJWeapon_Ranged.cpp`

**Interfaces:**
- Produces:
  - `void AGJProjectile::FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange, float InScale, int32 InPierceCount)`

- [ ] **Step 1: `GJProjectile.h` 갱신**

`GJProjectile.h`에서 다음 블록을 찾는다:

```cpp
    // 풀(Pool)에서 꺼내서 발사할 때 호출하는 함수. 데미지/속도/사거리를 매번 새로 받아서 세팅합니다.
    void FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange);
```

이를 다음으로 교체한다:

```cpp
    // 풀(Pool)에서 꺼내서 발사할 때 호출하는 함수. 데미지/속도/사거리를 매번 새로 받아서 세팅합니다.
    // InScale: 액터 전체 크기 배율. 콜리전과 메시가 같이 커진다(스킬 차징용, 총알은 1.0).
    // InPierceCount: 추가로 관통하는 적 수. 0=1명만, 1=2명, -1=무한.
    void FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange,
                         float InScale = 1.f, int32 InPierceCount = 0);
```

이어서 다음 블록을 찾는다:

```cpp
    // 타격 판정 (총알이 어딘가에 맞았을 때)
    UFUNCTION()
    void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
```

그 **아래**에 추가한다:

```cpp

    // 관통 구체는 폰을 블록하지 않고 통과하므로 Hit이 아니라 Overlap으로 들어온다.
    UFUNCTION()
    void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                        int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    // Hit과 Overlap 양쪽에서 부르는 공통 타격 처리. 데미지를 주고 관통을 소모한다.
    void HandleTouch(AActor* OtherActor);
```

마지막으로 다음 블록을 찾는다:

```cpp
private:
    bool bIsActive;
    float Damage;
```

이를 다음으로 교체한다:

```cpp
private:
    bool bIsActive;
    float Damage;

    // 남은 관통 수. -1은 무한이며 감소시키지 않는다.
    int32 RemainingPierce = 0;

    // 이번 발사에서 이미 때린 대상. 큰 구체는 한 적의 콜리전 안에 여러 프레임 머물기 때문에
    // 이게 없으면 같은 적을 프레임마다 재타격한다.
    UPROPERTY()
    TSet<AActor*> HitActors;
```

- [ ] **Step 2: 생성자에서 오버랩 이벤트 바인딩**

`GJProjectile.cpp`에서 다음 줄을 찾는다:

```cpp
    CollisionComp->OnComponentHit.AddDynamic(this, &AGJProjectile::OnHit); // 충돌 이벤트 바인딩
```

그 **아래**에 추가한다:

```cpp
    // 관통 구체용. 비관통일 때는 프로필이 BlockAllDynamic이라 오버랩이 안 생기므로 무해하다.
    CollisionComp->SetGenerateOverlapEvents(true);
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AGJProjectile::OnOverlapBegin);
```

- [ ] **Step 3: `FireInDirection` 교체**

`GJProjectile.cpp`의 `FireInDirection` 함수 전체를 다음으로 교체한다:

```cpp
void AGJProjectile::FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange,
                                    float InScale, int32 InPierceCount)
{
    bIsActive = true;
    Damage = InDamage;
    RemainingPierce = InPierceCount;
    HitActors.Reset();

    // 콜리전과 메시가 같이 커진다. 스케일 대신 SphereRadius를 직접 만지면 메시가 안 따라온다.
    SetActorScale3D(FVector(FMath::Max(InScale, 0.01f)));

    // 숨김 해제 및 충돌 켜기
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);

    // 콜리전 설정은 SetActorEnableCollision 뒤에 해야 덮어써지지 않는다.
    if (InPierceCount != 0)
    {
        // 관통이면 폰을 Block으로 두면 안 된다. bShouldBounce=false라 블로킹 히트가 나는 순간
        // ProjectileMovement가 그 자리에서 멈춰서, Deactivate를 안 해도 구체가 적 앞에 박힌다.
        // 벽(WorldStatic)은 여전히 막아야 하므로 채널별로 따로 준다.
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
        CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }
    else
    {
        // 총알은 기존 동작 그대로. 풀에서 재사용되므로 매번 되돌려 놓아야 한다 -
        // 관통 스킬이 쓰고 반납한 구체를 총알이 집어가면 벽만 막는 채로 굳는다.
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    }

    // 무기 테이블에서 가져온 속도로 직선 비행 (방향 벡터를 정규화한 뒤 속도를 곱함)
    const float Speed = (InSpeed > 0.f) ? InSpeed : ProjectileMovement->InitialSpeed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->Velocity = ShootDirection.GetSafeNormal() * Speed;
    ProjectileMovement->Activate();

    // 사거리(Range)만큼 날아가면 자동 비활성화. 등속 직선 비행이므로 (사거리 / 속도) = 도달 시간.
    GetWorldTimerManager().ClearTimer(RangeTimerHandle);
    if (InRange > 0.f && Speed > 0.f)
    {
        const float FlightTime = InRange / Speed;
        GetWorldTimerManager().SetTimer(RangeTimerHandle, this, &AGJProjectile::Deactivate, FlightTime, false);
    }
}
```

- [ ] **Step 4: `Deactivate`에 상태 초기화 추가**

`GJProjectile.cpp`의 `Deactivate` 안에서 다음 줄을 찾는다:

```cpp
    bIsActive = false;
```

그 **아래**에 추가한다:

```cpp

    // 풀로 돌아가는 객체다. 안 지우면 다음 발사가 이 적을 못 때린다.
    HitActors.Reset();
    RemainingPierce = 0;
```

- [ ] **Step 5: `OnHit` 교체 + `OnOverlapBegin` / `HandleTouch` 추가**

`GJProjectile.cpp`의 `OnHit` 함수 전체를 다음으로 교체한다:

```cpp
void AGJProjectile::HandleTouch(AActor* OtherActor)
{
    // 자기 자신, 쏜 주체, 이미 때린 대상은 건너뛴다.
    if (!bIsActive || !OtherActor || OtherActor == this || OtherActor == GetInstigator())
    {
        return;
    }

    if (HitActors.Contains(OtherActor))
    {
        return;
    }

    AGJBaseCharacter* HitCharacter = Cast<AGJBaseCharacter>(OtherActor);
    if (!HitCharacter)
    {
        return;
    }

    HitActors.Add(OtherActor);

    UGameplayStatics::ApplyDamage(
        HitCharacter,
        Damage,
        GetInstigatorController(), // Instigator가 없어도 안전하게 nullptr 반환
        this,
        UDamageType::StaticClass()
    );

    // 관통 없음 -> 첫 적에서 소멸
    if (RemainingPierce == 0)
    {
        Deactivate();
        return;
    }

    // -1은 무한이므로 줄이지 않는다. 줄이면 -2, -3으로 내려가 "남았는지" 판정이 뒤집힌다.
    if (RemainingPierce > 0)
    {
        RemainingPierce--;
        if (RemainingPierce == 0)
        {
            // 관통 횟수를 다 쓴 뒤에도 이번 적은 이미 맞혔다. 여기서 끝낸다.
            Deactivate();
        }
    }
}

void AGJProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 관통 구체는 폰을 오버랩으로 통과하므로 여기 들어오는 건 벽이다.
    // 비관통 구체(총알)는 폰도 여기로 들어온다.
    HandleTouch(OtherActor);

    // 벽에 맞았으면 HandleTouch가 아무것도 안 했으므로 여기서 끈다.
    // 이미 꺼졌으면 건너뛴다(Deactivate 중복 호출 자체는 무해하지만 타이머를 두 번 만질 이유가 없다).
    if (bIsActive)
    {
        Deactivate();
    }
}

void AGJProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                   int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 관통 구체가 적을 통과할 때만 들어온다. 벽은 Block이라 OnHit으로 간다.
    HandleTouch(OtherActor);
}
```

- [ ] **Step 6: 무기 호출부에 새 인자 전달**

`GJWeapon_Ranged.cpp`에서 다음 줄을 찾는다(142번째 줄 부근):

```cpp
        ProjectileToFire->FireInDirection(ShootDirection, OutgoingDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range);
```

이를 다음으로 교체한다:

```cpp
        // 총알은 크기 1배, 관통 없음. 기본값과 같지만 명시해서 "여기는 스킬이 아니다"를 남긴다.
        ProjectileToFire->FireInDirection(ShootDirection, OutgoingDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range,
                                          1.f, 0);
```

- [ ] **Step 7: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 8: 회귀 확인 (기존 총이 안 깨졌는가)**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 **총을 쏴서 적을 죽여봐.**

확인 항목:
- 총알이 예전과 같은 크기·속도로 나간다
- 적에게 맞으면 데미지가 들어가고 **총알이 사라진다**
- 벽에 맞아도 사라진다
- 사거리를 다 날아가면 사라진다
- 연사해도 총알이 고갈되지 않는다(풀 반납이 정상)

**이 태스크에서 새로 생기는 동작은 없다.** 하나라도 달라졌으면 관통 분기가 총알 경로에 새어 든 것이다.

- [ ] **Step 9: 커밋**

```bash
git add Source/Project_GJ/GJProjectile.h Source/Project_GJ/GJProjectile.cpp Source/Project_GJ/GJWeapon_Ranged.cpp
git commit -m "$(cat <<'EOF'
투사체에 크기 배율과 관통 추가

FireInDirection에 InScale과 InPierceCount를 더했다. 기본값이 1.0과 0이라
총알 동작은 그대로다.

관통은 콜리전 프로필을 바꿔야 동작한다. 지금 프로필은 BlockAllDynamic이고
bShouldBounce가 false라, 적에게 닿는 순간 ProjectileMovement가 그 자리에서
멈춘다. Deactivate를 안 불러도 구체가 적 앞에 박혀 있을 뿐 통과하지 않는다.
그래서 관통일 때만 폰을 Overlap으로 바꾸고 벽은 Block으로 남긴다.

풀에서 재사용되므로 비관통 경로에서 프로필을 되돌려 놓는다. 안 하면 관통
스킬이 쓰고 반납한 구체를 총알이 집어갔을 때 적을 통과해버린다.

같은 적을 두 번 때리지 않도록 HitActors를 들고 다닌다. 큰 구체는 한 적의
콜리전 안에 여러 프레임 머물기 때문에 이게 없으면 프레임마다 재타격한다.
Deactivate에서 비우지 않으면 다음 발사가 그 적을 못 때린다.

PierceCount는 추가로 관통하는 적 수다. 0이면 1명, 1이면 2명, -1은 무한이며
-1은 감소시키지 않는다 - 줄이면 -2로 내려가 판정이 뒤집힌다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 3: 스킬 컴포넌트와 차징 발사

이 태스크가 끝나면 **우클릭으로 실제로 구체가 나간다.**

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Create: `Source/Project_GJ/GJSkillComponent.h`, `Source/Project_GJ/GJSkillComponent.cpp`
- Modify: `Source/Project_GJ/GJCharacter.h`, `Source/Project_GJ/GJCharacter.cpp`
- Create (에셋): `Content/GJ/DataTables/DT_SkillData`, `Content/GJ/BluePrint/BP_GJSkillProjectile`

**Interfaces:**
- Consumes: `AGJCharacter::GetSkillPower/GetCurrentMP/ConsumeMP` (Task 1), `AGJProjectile::FireInDirection(..., InScale, InPierceCount)` (Task 2)
- Produces:
  - `enum class ESkillType : uint8 { Projectile, Persistent }`
  - `FSkillData : public FTableRowBase`
  - `UGJSkillComponent` — `OnSkillPressed(int32)`, `OnSkillReleased(int32)`, `EquipSkill(FName)`, `EquipSkillInSlot(int32, FName)`, `IsCharging()`, `CancelCharge()`, `GetSkillInSlot(int32)`, `LogSkillInfo()`
  - `AGJCharacter::SkillComponent` (protected 멤버) + `UFUNCTION(Exec) GJEquipSkill/GJSkillInfo`

- [ ] **Step 1: `GJGameTypes.h`에 스킬 타입 추가**

`GJGameTypes.h`에서 다음 블록을 찾는다(카드 섹션의 시작):

```cpp
// -----------------------------------------
// 카드 (레벨업 선택지)
// -----------------------------------------
```

그 **위**에 추가한다:

```cpp
// -----------------------------------------
// 스킬 (액티브 스킬)
// -----------------------------------------

UENUM(BlueprintType)
enum class ESkillType : uint8
{
    // 구체를 발사한다
    Projectile UMETA(DisplayName = "발사형"),
    // 미구현 - 장판/오라 같은 지속 효과. 고르면 경고만 찍힌다
    Persistent UMETA(DisplayName = "지속형 (미구현)")
};

// 스킬 하나의 정의. 행 이름이 곧 스킬 ID다.
USTRUCT(BlueprintType)
struct FSkillData : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    UTexture2D* Icon = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    ESkillType SkillType = ESkillType::Projectile;

    // 떼는 순간 고정 소비. 차징률에 비례시키지 않는 이유는 약한 구체 연타가 최적해가 되는
    // 균형을 잡으려면 쿨타임까지 같이 조정해야 해서 변수가 둘로 늘기 때문이다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float MPCost = 10.f;

    // 떼는 순간부터 시작 (초)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Cooldown = 3.f;

    // 차징 배율이 곱해지기 전 기본 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float BaseDamage = 40.f;

    // 이만큼 날아가면 자동 소멸
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float Range = 2000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float ProjectileSpeed = 1500.f;

    // 최대 차징까지 걸리는 시간. 0이면 차징이 없고 누르는 순간 발사된다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float ChargeTime = 1.5f;

    // 최대 차징 시 크기와 데미지에 함께 곱해지는 배율
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float MaxChargeMultiplier = 2.f;

    // 구체 기본 크기 배율 (BP에서 만든 크기 1.0 기준).
    // 반지름(cm)이 아닌 이유: cm로 주면 콜리전은 맞춰도 메시는 원본 크기를 알아야 비율이 나온다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    float BaseScale = 1.f;

    // 추가로 관통하는 적 수. 0=1명, 1=2명, -1=무한.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    int32 PierceCount = 0;

    // 카드 태그와 같은 축(Tree.Fire 등). 지금은 표시용이고, 나중에 스킬을 얻으면 같은 트리
    // 카드를 밀어주는 연결점이 된다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    FGameplayTagContainer SkillTags;

    // 구체 비주얼. 비어 있으면 컴포넌트의 DefaultProjectileClass를 쓴다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Skill")
    TSubclassOf<AGJProjectile> ProjectileClass;
};

```

이어서 `GJGameTypes.h` 상단의 전방 선언에서 다음 줄을 찾는다:

```cpp
class AGJWeaponBase;
```

그 **아래**에 추가한다:

```cpp
class AGJProjectile;
```

- [ ] **Step 2: `GJSkillComponent.h` 생성**

새 파일 `Source/Project_GJ/GJSkillComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GJGameTypes.h"
#include "GJSkillComponent.generated.h"

class AGJCharacter;
class AGJProjectile;

// 스킬 슬롯 수. 우클릭 / Q / R 세 개다.
#define GJ_SKILL_SLOT_COUNT 3

// UPROPERTY TMap은 값으로 TArray를 직접 담지 못해서 한 겹 감싼다.
USTRUCT()
struct FGJProjectilePool
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<AGJProjectile*> Projectiles;
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGJSkillComponent();

    // 입력 전달. 캐릭터가 IA_Skill1/2/3의 Started/Completed에서 부른다.
    void OnSkillPressed(int32 SlotIndex);
    void OnSkillReleased(int32 SlotIndex);

    // 빈 슬롯에 장착한다. 슬롯이 다 찼으면 false를 돌려주고 아무것도 안 한다 -
    // 호출자(카드 컴포넌트)가 그때 교체 선택지를 띄운다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    bool EquipSkill(FName SkillId);

    // 지정한 슬롯을 덮어쓴다. 교체 선택 결과를 적용하는 경로다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void EquipSkillInSlot(int32 SlotIndex, FName SkillId);

    // 차징 중이면 true. 캐릭터의 입력 핸들러가 이걸 보고 다른 동작을 막는다.
    UFUNCTION(BlueprintPure, Category = "Skill")
    bool IsCharging() const { return ChargingSlot != INDEX_NONE; }

    // 차징을 버린다. MP도 쿨타임도 소모하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void CancelCharge();

    // 슬롯에 장착된 스킬 ID. 비었으면 NAME_None.
    UFUNCTION(BlueprintPure, Category = "Skill")
    FName GetSkillInSlot(int32 SlotIndex) const;

    // 테이블에서 스킬 정의를 찾는다. 없으면 nullptr.
    const FSkillData* FindSkill(FName SkillId) const;

    // 슬롯 3개의 상태를 로그로 출력한다. AGJCharacter의 GJSkillInfo가 부른다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void LogSkillInfo() const;

protected:
    virtual void BeginPlay() override;

    // 스킬 정의 테이블 (DT_SkillData). 비어 있으면 스킬 시스템 전체가 조용히 꺼진다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    UDataTable* SkillTable;

    // ProjectileClass가 비어 있는 스킬이 쓸 기본 구체 (BP_GJSkillProjectile)
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    TSubclassOf<AGJProjectile> DefaultProjectileClass;

    // 캐릭터 기준 발사 위치 오프셋 (X=전방, Y=우측, Z=상방).
    // 무기의 MuzzleSocket을 안 쓰는 이유: 스킬은 맨손이어도 나가야 한다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    FVector MuzzleOffset = FVector(60.f, 0.f, 40.f);

    // 구체 클래스마다 만드는 풀의 최대 크기. 스킬은 쿨타임이 있어 동시에 떠 있는 수가
    // 무기(30)보다 훨씬 적다. 모자라면 그 발사만 무시된다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    int32 PoolSizePerClass = 10;

    // 슬롯별 장착 스킬 ID. 생성자에서 GJ_SKILL_SLOT_COUNT개로 채운다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    TArray<FName> EquippedSkills;

    // 슬롯별 쿨타임이 끝나는 월드 시각. 지금 시각보다 작으면 사용 가능.
    TArray<float> CooldownEndTime;

    // 차징을 시작한 월드 시각. ChargingSlot이 INDEX_NONE이면 의미 없다.
    float ChargeStartTime = 0.f;

    // 지금 차징 중인 슬롯. INDEX_NONE이면 차징 안 함.
    int32 ChargingSlot = INDEX_NONE;

    // 구체 클래스별 풀. 스킬마다 비주얼이 다를 수 있어 하나로 못 묶는다.
    UPROPERTY()
    TMap<TSubclassOf<AGJProjectile>, FGJProjectilePool> ProjectilePools;

    AGJCharacter* GetOwnerCharacter() const;

    // 비활성 구체를 꺼내온다. 없으면 풀 크기 한도 내에서 새로 스폰한다.
    AGJProjectile* GetPooledProjectile(TSubclassOf<AGJProjectile> ProjClass);

    // 실제 발사. ChargeRatio는 0~1이다.
    void FireSkill(int32 SlotIndex, const FSkillData& Skill, float ChargeRatio);
};
```

- [ ] **Step 3: `GJSkillComponent.cpp` 생성**

새 파일 `Source/Project_GJ/GJSkillComponent.cpp`:

```cpp
#include "GJSkillComponent.h"
#include "GJCharacter.h"
#include "GJProjectile.h"
#include "GJCombatStatics.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"

UGJSkillComponent::UGJSkillComponent()
{
    // 차징과 쿨타임을 시각 비교로 계산하므로 매 프레임 할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;

    EquippedSkills.Init(NAME_None, GJ_SKILL_SLOT_COUNT);
    CooldownEndTime.Init(0.f, GJ_SKILL_SLOT_COUNT);
}

void UGJSkillComponent::BeginPlay()
{
    Super::BeginPlay();

    if (!GetOwnerCharacter())
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: 소유자가 AGJCharacter가 아닙니다. 스킬이 동작하지 않습니다."));
    }

    // 테이블이 없으면 모든 스킬이 조용히 안 나간다. 시작할 때 한 번만 알려준다 -
    // 발사 시점에 찍으면 우클릭할 때마다 스팸된다.
    if (!SkillTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: SkillTable이 비어 있어 스킬을 쓸 수 없습니다. BP_GJCharacter의 SkillComponent를 확인하세요."));
    }
}

AGJCharacter* UGJSkillComponent::GetOwnerCharacter() const
{
    return Cast<AGJCharacter>(GetOwner());
}

const FSkillData* UGJSkillComponent::FindSkill(FName SkillId) const
{
    if (!SkillTable || SkillId.IsNone())
    {
        return nullptr;
    }

    return SkillTable->FindRow<FSkillData>(SkillId, TEXT("FindSkill"), false);
}

FName UGJSkillComponent::GetSkillInSlot(int32 SlotIndex) const
{
    return EquippedSkills.IsValidIndex(SlotIndex) ? EquippedSkills[SlotIndex] : NAME_None;
}

bool UGJSkillComponent::EquipSkill(FName SkillId)
{
    if (!FindSkill(SkillId))
    {
        UE_LOG(LogTemp, Warning, TEXT("EquipSkill: '%s'가 DT_SkillData에 없습니다."), *SkillId.ToString());
        return true;  // 슬롯 문제가 아니므로 교체 화면을 띄울 이유가 없다
    }

    for (int32 i = 0; i < EquippedSkills.Num(); i++)
    {
        if (EquippedSkills[i].IsNone())
        {
            EquipSkillInSlot(i, SkillId);
            return true;
        }
    }

    // 빈 슬롯이 없다. 호출자가 무엇을 버릴지 물어야 한다.
    return false;
}

void UGJSkillComponent::EquipSkillInSlot(int32 SlotIndex, FName SkillId)
{
    if (!EquippedSkills.IsValidIndex(SlotIndex))
    {
        return;
    }

    // 교체 대상 슬롯이 차징 중이었다면 버린다. 안 그러면 뗐을 때 없어진 스킬이 나간다.
    if (ChargingSlot == SlotIndex)
    {
        CancelCharge();
    }

    EquippedSkills[SlotIndex] = SkillId;

    // 새 스킬을 쿨타임 없이 바로 쓰게 한다. 교체는 손해가 아니어야 한다.
    CooldownEndTime[SlotIndex] = 0.f;

    UE_LOG(LogTemp, Log, TEXT("EquipSkillInSlot: 슬롯 %d <- %s"), SlotIndex, *SkillId.ToString());
}

void UGJSkillComponent::CancelCharge()
{
    ChargingSlot = INDEX_NONE;
    ChargeStartTime = 0.f;
}

void UGJSkillComponent::OnSkillPressed(int32 SlotIndex)
{
    if (!EquippedSkills.IsValidIndex(SlotIndex))
    {
        return;
    }

    // 이미 다른 슬롯을 차징 중이면 무시한다. 두 개를 동시에 차징하면
    // 뗄 때 어느 쪽인지 판정이 갈리고, 그 조작을 설명할 방법도 없다.
    if (ChargingSlot != INDEX_NONE)
    {
        return;
    }

    const FSkillData* Skill = FindSkill(EquippedSkills[SlotIndex]);
    if (!Skill)
    {
        // 빈 슬롯이다. 로그를 찍으면 클릭할 때마다 스팸된다.
        return;
    }

    UWorld* World = GetWorld();
    AGJCharacter* Character = GetOwnerCharacter();
    if (!World || !Character)
    {
        return;
    }

    if (World->GetTimeSeconds() < CooldownEndTime[SlotIndex])
    {
        return;
    }

    if (Character->GetCurrentMP() < Skill->MPCost)
    {
        return;
    }

    // 차징이 없는 스킬은 누르는 순간 나간다. 뗄 때까지 기다리면 차징도 없는데
    // 손을 떼야 발사되는 이상한 감각이 된다.
    if (Skill->ChargeTime <= 0.f)
    {
        FireSkill(SlotIndex, *Skill, 0.f);
        return;
    }

    ChargingSlot = SlotIndex;
    ChargeStartTime = World->GetTimeSeconds();
}

void UGJSkillComponent::OnSkillReleased(int32 SlotIndex)
{
    if (ChargingSlot != SlotIndex)
    {
        return;
    }

    const FSkillData* Skill = FindSkill(EquippedSkills[SlotIndex]);
    UWorld* World = GetWorld();

    // 어떤 경로로 끝나든 차징 상태는 반드시 푼다.
    const float StartTime = ChargeStartTime;
    CancelCharge();

    if (!Skill || !World)
    {
        return;
    }

    const float Elapsed = World->GetTimeSeconds() - StartTime;
    const float Ratio = FMath::Clamp(Elapsed / Skill->ChargeTime, 0.f, 1.f);

    FireSkill(SlotIndex, *Skill, Ratio);
}

AGJProjectile* UGJSkillComponent::GetPooledProjectile(TSubclassOf<AGJProjectile> ProjClass)
{
    if (!ProjClass)
    {
        ProjClass = DefaultProjectileClass;
    }
    if (!ProjClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillComponent: DefaultProjectileClass가 비어 있어 구체를 만들 수 없습니다."));
        return nullptr;
    }

    AActor* OwnerActor = GetOwner();
    UWorld* World = GetWorld();
    if (!OwnerActor || !World)
    {
        return nullptr;
    }

    FGJProjectilePool& Pool = ProjectilePools.FindOrAdd(ProjClass);

    for (AGJProjectile* Existing : Pool.Projectiles)
    {
        if (Existing && !Existing->IsActive())
        {
            return Existing;
        }
    }

    if (Pool.Projectiles.Num() >= PoolSizePerClass)
    {
        return nullptr;
    }

    // 미리 다 만들지 않고 필요할 때 하나씩 늘린다. 스킬을 안 쓰는 플레이에서는
    // 구체가 하나도 안 만들어진다.
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = OwnerActor;
    SpawnParams.Instigator = Cast<APawn>(OwnerActor);
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AGJProjectile* NewProjectile = World->SpawnActor<AGJProjectile>(
        ProjClass, OwnerActor->GetActorLocation(), FRotator::ZeroRotator, SpawnParams);

    if (NewProjectile)
    {
        Pool.Projectiles.Add(NewProjectile);
    }

    return NewProjectile;
}

void UGJSkillComponent::FireSkill(int32 SlotIndex, const FSkillData& Skill, float ChargeRatio)
{
    AGJCharacter* Character = GetOwnerCharacter();
    UWorld* World = GetWorld();
    if (!Character || !World)
    {
        return;
    }

    if (Skill.SkillType != ESkillType::Projectile)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("FireSkill: 지속형 스킬 '%s'는 아직 미구현입니다."), *EquippedSkills[SlotIndex].ToString());
        return;
    }

    // 구체를 먼저 확보한다. MP를 먼저 깎으면 풀이 비었을 때 MP만 사라진다.
    AGJProjectile* Projectile = GetPooledProjectile(Skill.ProjectileClass);
    if (!Projectile)
    {
        return;
    }

    // 차징 중에 MP가 빠졌을 수 있으므로 여기서 다시 확인한다.
    if (!Character->ConsumeMP(Skill.MPCost))
    {
        return;
    }

    const float Multiplier = 1.f + (Skill.MaxChargeMultiplier - 1.f) * ChargeRatio;

    const FVector Forward = Character->GetActorForwardVector();
    const FVector SpawnLocation = Character->GetActorLocation()
        + Forward * MuzzleOffset.X
        + Character->GetActorRightVector() * MuzzleOffset.Y
        + FVector::UpVector * MuzzleOffset.Z;

    Projectile->SetActorLocationAndRotation(SpawnLocation, Forward.Rotation());

    // 풀 구체는 스폰 시점의 인스티게이터로 굳으므로 발사할 때마다 갱신한다.
    // 안 하면 적 처치 경험치를 줄 대상을 못 찾고 자기 피격 방지도 안 먹는다.
    Projectile->SetInstigator(Character);

    bool bWasCritical = false;
    const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
        Skill.BaseDamage * Multiplier,
        Character->GetSkillPower(),
        Character->CritChance,
        Character->CritMultiplier,
        bWasCritical);

    Projectile->FireInDirection(
        Forward, OutgoingDamage, Skill.ProjectileSpeed, Skill.Range,
        Skill.BaseScale * Multiplier, Skill.PierceCount);

    CooldownEndTime[SlotIndex] = World->GetTimeSeconds() + Skill.Cooldown;

    UE_LOG(LogTemp, Log,
        TEXT("FireSkill: 슬롯 %d '%s' 차징 %.0f%% -> 배율 x%.2f, 데미지 %.1f%s, 크기 x%.2f, MP -%.0f, 쿨 %.1fs"),
        SlotIndex, *EquippedSkills[SlotIndex].ToString(), ChargeRatio * 100.f, Multiplier,
        OutgoingDamage, bWasCritical ? TEXT(" (치명타)") : TEXT(""),
        Skill.BaseScale * Multiplier, Skill.MPCost, Skill.Cooldown);
}

void UGJSkillComponent::LogSkillInfo() const
{
    AGJCharacter* Character = GetOwnerCharacter();
    UWorld* World = GetWorld();
    if (!Character || !World)
    {
        return;
    }

    const TCHAR* SlotKeys[GJ_SKILL_SLOT_COUNT] = { TEXT("우클릭"), TEXT("Q"), TEXT("R") };
    const float Now = World->GetTimeSeconds();

    UE_LOG(LogTemp, Log, TEXT("=== 스킬 상태 (MP %.0f, 스킬공격력 %.1f) ==="),
        Character->GetCurrentMP(), Character->GetSkillPower());

    for (int32 i = 0; i < EquippedSkills.Num(); i++)
    {
        const float Remaining = FMath::Max(CooldownEndTime[i] - Now, 0.f);
        UE_LOG(LogTemp, Log, TEXT("  슬롯 %d (%s): %s / 쿨 %.1fs%s"),
            i, SlotKeys[i],
            EquippedSkills[i].IsNone() ? TEXT("(비어 있음)") : *EquippedSkills[i].ToString(),
            Remaining,
            (ChargingSlot == i) ? TEXT(" / 차징 중") : TEXT(""));
    }
}
```

- [ ] **Step 4: `GJCharacter.h`에 컴포넌트·입력·콘솔 추가**

`GJCharacter.h` 상단의 전방 선언에서 다음 줄을 찾는다:

```cpp
class UGJCardComponent;
```

그 **아래**에 추가한다:

```cpp
class UGJSkillComponent;
```

이어서 다음 세 줄을 찾는다:

```cpp
    // 레벨업 카드 선택 (OnLevelUp을 구독해서 알아서 동작함 - 캐릭터는 카드를 모른다)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Card")
    UGJCardComponent* CardComponent;
```

그 **아래**에 추가한다:

```cpp

    // 액티브 스킬 (슬롯/쿨타임/차징 전부 여기 - 캐릭터는 입력만 넘긴다)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skill")
    UGJSkillComponent* SkillComponent;

    // 스킬 입력 액션. BP_GJCharacter 디테일 패널에서 IA_Skill1/2/3을 할당해야 동작한다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill1Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill2Action;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
    UInputAction* Skill3Action;

    // 슬롯 번호를 넘기기만 하는 얇은 래퍼. BindAction이 인자 있는 함수를 못 받아서 필요하다.
    void Skill1Pressed();
    void Skill1Released();
    void Skill2Pressed();
    void Skill2Released();
    void Skill3Pressed();
    void Skill3Released();
```

이어서 `public:` 블록의 `GJShowCards` 선언 **아래**에 추가한다:

```cpp

    // 예) GJEquipSkill Skill_Fireball     -> 첫 빈 슬롯에 장착
    //     GJEquipSkill Skill_Fireball 1   -> 슬롯 1(Q)에 강제 장착
    UFUNCTION(Exec)
    void GJEquipSkill(const FString& SkillId, int32 SlotIndex = -1);

    // 슬롯별 장착 스킬, 쿨타임 잔량, MP, 차징 상태를 로그로 출력
    UFUNCTION(Exec)
    void GJSkillInfo();
```

- [ ] **Step 5: `GJCharacter.cpp`에 컴포넌트 생성과 입력 바인딩 추가**

`GJCharacter.cpp` 상단 include 블록에서 다음 줄을 찾는다:

```cpp
#include "GJCardComponent.h"
```

그 **아래**에 추가한다:

```cpp
#include "GJSkillComponent.h"
```

이어서 다음 줄을 찾는다:

```cpp
    CardComponent = CreateDefaultSubobject<UGJCardComponent>(TEXT("CardComponent"));
```

그 **아래**에 추가한다:

```cpp
    SkillComponent = CreateDefaultSubobject<UGJSkillComponent>(TEXT("SkillComponent"));
```

이어서 `SetupPlayerInputComponent` 안에서 다음 블록을 찾는다:

```cpp
            EnhancedInput->BindAction(WeaponSlot2Action, ETriggerEvent::Started, this, &AGJCharacter::SwapToWeaponSlot2);
```

그 아래(같은 `if (EnhancedInput)` 블록 안, 닫는 중괄호 **직전**)에 추가한다:

```cpp

        // 스킬은 누름과 뗌을 둘 다 받아야 차징이 성립한다.
        // Canceled도 Completed와 같이 묶는다 - 입력이 취소로 끝나면 Completed가 안 와서
        // 차징이 눌린 채 굳는다(공격 입력에서 겪은 문제와 같은 종류다).
        if (Skill1Action)
        {
            EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Started, this, &AGJCharacter::Skill1Pressed);
            EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Completed, this, &AGJCharacter::Skill1Released);
            EnhancedInput->BindAction(Skill1Action, ETriggerEvent::Canceled, this, &AGJCharacter::Skill1Released);
        }
        if (Skill2Action)
        {
            EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Started, this, &AGJCharacter::Skill2Pressed);
            EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Completed, this, &AGJCharacter::Skill2Released);
            EnhancedInput->BindAction(Skill2Action, ETriggerEvent::Canceled, this, &AGJCharacter::Skill2Released);
        }
        if (Skill3Action)
        {
            EnhancedInput->BindAction(Skill3Action, ETriggerEvent::Started, this, &AGJCharacter::Skill3Pressed);
            EnhancedInput->BindAction(Skill3Action, ETriggerEvent::Completed, this, &AGJCharacter::Skill3Released);
            EnhancedInput->BindAction(Skill3Action, ETriggerEvent::Canceled, this, &AGJCharacter::Skill3Released);
        }
```

- [ ] **Step 6: `GJCharacter.cpp`에 래퍼와 콘솔 명령 구현 추가**

`GJCharacter.cpp`에서 `void AGJCharacter::GJShowCards()` 함수의 닫는 `}`를 찾아, 그 **아래**에 추가한다:

```cpp

void AGJCharacter::Skill1Pressed()  { if (SkillComponent) SkillComponent->OnSkillPressed(0); }
void AGJCharacter::Skill1Released() { if (SkillComponent) SkillComponent->OnSkillReleased(0); }
void AGJCharacter::Skill2Pressed()  { if (SkillComponent) SkillComponent->OnSkillPressed(1); }
void AGJCharacter::Skill2Released() { if (SkillComponent) SkillComponent->OnSkillReleased(1); }
void AGJCharacter::Skill3Pressed()  { if (SkillComponent) SkillComponent->OnSkillPressed(2); }
void AGJCharacter::Skill3Released() { if (SkillComponent) SkillComponent->OnSkillReleased(2); }

void AGJCharacter::GJEquipSkill(const FString& SkillId, int32 SlotIndex)
{
    if (!SkillComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJEquipSkill: SkillComponent가 없습니다."));
        return;
    }

    const FName Id(*SkillId);

    if (SlotIndex >= 0)
    {
        SkillComponent->EquipSkillInSlot(SlotIndex, Id);
        return;
    }

    if (!SkillComponent->EquipSkill(Id))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("GJEquipSkill: 빈 슬롯이 없습니다. 슬롯 번호를 지정하세요 (예: GJEquipSkill %s 0)."), *SkillId);
    }
}

void AGJCharacter::GJSkillInfo()
{
    if (!SkillComponent)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJSkillInfo: SkillComponent가 없습니다."));
        return;
    }

    SkillComponent->LogSkillInfo();
}
```

- [ ] **Step 7: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘. 새 `UCLASS`(`UGJSkillComponent`)와 새 `USTRUCT`/`UENUM`이라 라이브 코딩만으로는 안 잡힐 수 있어. 컴파일 후 `BP_GJCharacter`를 열었을 때 컴포넌트 목록에 **SkillComponent**가 안 보이면 **에디터를 재시작**해줘.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 8: `BP_GJSkillProjectile` 생성**

MCP로 `/Game/GJ/BluePrint/BP_GJProjectile`을 복제해 `/Game/GJ/BluePrint/BP_GJSkillProjectile`을 만든다(`AssetTools.duplicate`). 총알 BP를 베끼면 메시·머티리얼이 이미 붙어 있어 바로 보인다.

그다음 `ObjectTools.set_properties`로 CDO의 `MeshComp` 스케일을 키워 총알과 구분되게 한다(구체가 총알과 같은 크기면 차징 확인이 안 된다):

```
instance: /Game/GJ/BluePrint/BP_GJSkillProjectile.Default__BP_GJSkillProjectile_C:MeshComp
values:   {"RelativeScale3D":{"x":3.0,"y":3.0,"z":3.0}}
```

MCP로 안 되면 사용자에게 요청한다:
> `Content/GJ/BluePrint`의 `BP_GJProjectile`을 복제해서 `BP_GJSkillProjectile`로 이름 짓고, 안의 `MeshComp` 스케일을 3배로 키워줘.

- [ ] **Step 9: `DT_SkillData` 생성**

MCP `DataTableTools.create`로 행 구조체 `SkillData`, 경로 `/Game/GJ/DataTables/DT_SkillData`를 만들고 아래 1행을 넣는다.

| 행 이름 | DisplayName | Description | SkillType | MPCost | Cooldown | BaseDamage | Range | ProjectileSpeed | ChargeTime | MaxChargeMultiplier | BaseScale | PierceCount | ProjectileClass |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `Skill_Fireball` | 파이어볼 | 꾹 누르면 더 크고 강해진다 | Projectile | 10 | 3 | 40 | 2000 | 1500 | 1.5 | 2.0 | 1.0 | 0 | `/Game/GJ/BluePrint/BP_GJSkillProjectile.BP_GJSkillProjectile_C` |

**MCP 주의**(M2.6에서 확인): `set_rows`에 오브젝트 참조를 넘길 때 `{"refPath": ...}` 객체 형태는 **조용히 무시된다.** 평문 소프트 경로 문자열(`"/Game/.../X.X_C"`)로 넘겨야 들어간다. 넣은 뒤 `get_rows`로 `projectileClass`가 `None`이 아닌지 반드시 확인한다.

`Icon`은 비워둔다 — 스킬 아이콘 UI가 아직 없어서 쓰이지 않는다.

- [ ] **Step 10: `BP_GJCharacter`에 에셋 4개 연결**

MCP `ObjectTools.set_properties`로 CDO에 설정한다:

```
instance: /Game/GJ/BluePrint/BP_GJCharacter.Default__BP_GJCharacter_C
values:   {"Skill1Action":"/Game/GJ/Input/IA_Skill1.IA_Skill1",
           "Skill2Action":"/Game/GJ/Input/IA_Skill2.IA_Skill2",
           "Skill3Action":"/Game/GJ/Input/IA_Skill3.IA_Skill3"}
```

```
instance: /Game/GJ/BluePrint/BP_GJCharacter.Default__BP_GJCharacter_C:SkillComponent
values:   {"SkillTable":"/Game/GJ/DataTables/DT_SkillData.DT_SkillData",
           "DefaultProjectileClass":"/Game/GJ/BluePrint/BP_GJSkillProjectile.BP_GJSkillProjectile_C"}
```

`get_properties`로 네 값이 전부 `None`이 아닌지 확인한 뒤 `AssetTools.save_assets`로 저장한다.

- [ ] **Step 11: 차징 발사 확인**

사용자에게 요청한다:
> `TestLev`에서 플레이하고 Output Log의 `Cmd:` 입력칸에 쳐줘:
>
> ```
> GJEquipSkill Skill_Fireball
> ```
>
> 그다음 **우클릭을 짧게 톡** 한 번, **1.5초 이상 꾹 눌렀다 떼기** 한 번 해줘.

Run: `grep -E "FireSkill" Saved/Logs/Project_GJ.log | tail -5`

확인 항목:
- 짧게: `차징 0% -> 배율 x1.00, 크기 x1.00` 근처
- 길게: `차징 100% -> 배율 x2.00, 크기 x2.00`, 데미지도 2배
- **눈으로 봐도 구체 크기가 다르다**
- MP가 매번 10씩 줄어든다
- 쿨타임 3초 안에 다시 누르면 아무 일도 안 일어난다
- 적에게 맞으면 데미지가 들어가고 구체가 사라진다

이어서 관통을 확인한다:
> `DT_SkillData`에서 `Skill_Fireball`의 **PierceCount를 1로** 바꾸고 저장한 뒤, 적 3마리를 일렬로 세우고 쏴줘.

확인 항목:
- **2마리가 맞고 구체가 사라진다** (`PierceCount=1` = 추가 1명 = 총 2명)
- 같은 적이 한 발에 두 번 맞지 않는다
- 벽에 맞으면 그 자리에서 사라진다
- 두 번째 발사가 첫 번째 발사의 관통 상태에 오염되지 않는다

확인이 끝나면 `PierceCount`를 **0으로 되돌린다**.

- [ ] **Step 12: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJSkillComponent.h Source/Project_GJ/GJSkillComponent.cpp Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Content/GJ/DataTables/DT_SkillData.uasset Content/GJ/BluePrint/BP_GJSkillProjectile.uasset Content/GJ/BluePrint/BP_GJCharacter.uasset
git commit -m "$(cat <<'EOF'
스킬 컴포넌트와 차징 발사 추가

UGJSkillComponent를 만들고 캐릭터 생성자에서 붙였다. 우클릭을 누르고
있으면 차징되고 떼면 구체가 나간다. 아직 카드와 연결되지 않아 콘솔
명령으로 장착한다.

틱을 쓰지 않는다. 차징 경과와 쿨타임 잔량을 시각 비교로 구한다. 부수
효과로 일시정지된 동안에는 차징이 차오르지 않는다 - 카드 화면을 띄운 채
차징이 몰래 최대까지 가는 일이 없다.

차징이 없는 스킬(ChargeTime=0)은 누르는 순간 나간다. 뗄 때까지 기다리면
차징도 없는데 손을 떼야 발사되는 이상한 감각이 된다.

구체를 먼저 확보한 뒤에 MP를 깎는다. 순서를 뒤집으면 풀이 비었을 때
MP만 사라진다.

풀은 구체 클래스별로 나눠 필요할 때 하나씩 늘린다. 스킬마다 비주얼이
다를 수 있어 하나로 못 묶고, 미리 다 만들면 스킬을 안 쓰는 플레이에서도
구체가 스폰된다.

입력은 Started/Completed에 더해 Canceled도 묶었다. 입력이 취소로 끝나면
Completed가 안 와서 차징이 눌린 채 굳는다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: 차징 중 입력 제약

차징 중에는 이동과 회피만 된다. 회피가 차징을 끊는 유일한 수단이다.

**Files:**
- Modify: `Source/Project_GJ/GJCharacter.cpp`

**Interfaces:**
- Consumes: `UGJSkillComponent::IsCharging()`, `UGJSkillComponent::CancelCharge()` (Task 3)

- [ ] **Step 1: 평타 막기**

`GJCharacter.cpp`의 `AttackInputPressed` 안에서 다음 줄을 찾는다(413번째 줄 부근):

```cpp
    if (!StateComponent) return;
```

그 **위**에 추가한다:

```cpp
    // 차징 중에는 평타가 안 나간다. 차징을 끊으려면 회피를 써야 한다.
    if (SkillComponent && SkillComponent->IsCharging()) return;

```

- [ ] **Step 2: 재장전 막기**

`GJCharacter.cpp`의 `ReloadInputPressed` 안에서 다음 줄을 찾는다:

```cpp
    if (!EquippedWeapon || !StateComponent) return;
```

그 **위**에 추가한다:

```cpp
    if (SkillComponent && SkillComponent->IsCharging()) return;

```

- [ ] **Step 3: 무기 스왑 막기**

`GJCharacter.cpp`의 `SwapToWeaponSlot` 안에서 다음 줄을 찾는다(1223번째 줄 부근):

```cpp
    if (!WeaponSlots.IsValidIndex(SlotIndex) || !WeaponSlots[SlotIndex])
```

그 **위**에 추가한다:

```cpp
    if (SkillComponent && SkillComponent->IsCharging()) return;

```

- [ ] **Step 4: 인벤토리 막기**

`GJCharacter.cpp`의 `ToggleInventory` 함수 본문 **맨 첫 줄**에 추가한다:

```cpp
    // 차징 중에는 인벤토리가 안 열린다. 덕분에 "모달을 여는 순간 차징이 눌린 채 굳는" 경로가
    // 플레이어 조작에서는 사라진다 - 남는 건 레벨업 카드 화면뿐이다.
    if (SkillComponent && SkillComponent->IsCharging()) return;

```

- [ ] **Step 5: 회피로 차징 취소**

`GJCharacter.cpp`의 `PerformDodge` 안에서 다음 줄을 찾는다(623번째 줄 부근):

```cpp
    if (!StateComponent || StateComponent->GetState() != ECharacterState::Idle) return;
```

그 **아래**에 추가한다:

```cpp

    // 회피가 차징을 끊는 유일한 수단이다. MP도 쿨타임도 소모하지 않는다.
    // 위의 Idle 검사를 통과한 뒤에 부르는 이유: 회피가 실제로 나가지 않는 상황에서
    // 차징만 날려버리면 플레이어는 아무 이유 없이 차징을 잃는다.
    if (SkillComponent)
    {
        SkillComponent->CancelCharge();
    }
```

- [ ] **Step 6: 사망 시 차징 취소**

`GJCharacter.cpp`의 `AGJCharacter::HandleDeath()` 안에서 다음 세 줄을 찾는다(128~130번째 줄). **`bIsAutoFiring = false;`는 `AttackInputReleased`에도 있으므로 반드시 이 주석까지 포함해서 찾는다:**

```cpp
    // 죽는 순간 연사 중이었다면 즉시 멈춤 (그렇지 않으면 Tick에서 TryAutoFire가 계속 호출됨 -
    // Dead 상태 체크로 어차피 막히긴 하지만, 아예 호출 자체를 멈추는 게 더 깔끔함)
    bIsAutoFiring = false;
```

그 **아래**에 추가한다:

```cpp

    // 차징도 같이 버린다. 죽은 뒤 마우스를 떼면 구체가 나가면 안 된다.
    if (SkillComponent)
    {
        SkillComponent->CancelCharge();
    }
```

- [ ] **Step 7: 카드 화면 뜰 때 차징 취소**

`GJCharacter.h`에서 다음 줄을 찾는다:

```cpp
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopAutoFire() { bIsAutoFiring = false; }
```

이를 다음으로 교체한다:

```cpp
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StopAutoFire() { bIsAutoFiring = false; }

    // 모달 UI를 열 때 연사와 함께 차징도 버린다. 입력 모드가 UI로 바뀌면 마우스 "뗌"이
    // 캐릭터에 안 들어와서 차징이 눌린 채 굳고, UI를 닫는 순간 최대 차징으로 발사된다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CancelSkillCharge();
```

`GJCharacter.cpp`의 `Skill1Pressed` 정의 **위**에 추가한다:

```cpp
void AGJCharacter::CancelSkillCharge()
{
    if (SkillComponent)
    {
        SkillComponent->CancelCharge();
    }
}

```

`GJCardComponent.cpp`의 `OpenChoiceUI` 안에서 다음 줄을 찾는다:

```cpp
    Character->StopAutoFire();
```

그 **아래**에 추가한다:

```cpp
    Character->CancelSkillCharge();
```

- [ ] **Step 8: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 9: 제약 확인**

사용자에게 요청한다:
> 플레이하고 `GJEquipSkill Skill_Fireball`로 장착한 뒤, **우클릭을 꾹 누른 채로** 다음을 하나씩 해봐:
>
> 좌클릭 / R / 1 / 2 / Tab / 이동(WASD) / Shift(회피)

확인 항목:
- 좌클릭 — 총이 안 나간다
- R — 재장전이 안 된다
- 1, 2 — 무기가 안 바뀐다
- Tab — 인벤토리가 안 열린다
- WASD — **정상적으로 움직인다**
- Shift(회피) — 회피가 나가고, 그 뒤 우클릭을 떼도 **구체가 안 나가고 MP도 안 준다**

- [ ] **Step 10: 카드 화면 확인**

사용자에게 요청한다:
> **우클릭을 꾹 누른 채로** 적을 죽여서 레벨업시켜줘(경험치가 부족하면 `GJAddBonus RequiredEXP -90 0`을 먼저 쳐서 쉽게 만들어). 카드 화면이 뜬 뒤 마우스를 떼고 카드를 골라줘.

확인 항목:
- 카드 화면이 정상적으로 뜬다
- 카드를 고른 뒤 **구체가 저절로 나가지 않는다**
- MP가 줄지 않았다
- 게임 재개 후 우클릭이 정상 동작한다

- [ ] **Step 11: 커밋**

```bash
git add Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCharacter.cpp Source/Project_GJ/GJCardComponent.cpp
git commit -m "$(cat <<'EOF'
차징 중 입력 제약 추가

차징 중에는 이동과 회피만 된다. 평타, 재장전, 무기 스왑, 인벤토리가
전부 막히고 회피가 차징을 끊는 유일한 수단이다. 강한 한 방을 포기하고
회피를 소모하는 판단이 생긴다.

ECharacterState에 값을 추가하지 않고 SkillComponent::IsCharging()을
묻는 방식으로 했다. 그 enum은 값이 하나뿐인데 차징은 이동과 동시에
성립해서, 넣으면 차징이 끝났을 때 무엇으로 되돌릴지 알 수 없다.

회피의 취소는 Idle 검사를 통과한 뒤에 부른다. 회피가 실제로 나가지 않는
상황에서 차징만 날리면 플레이어는 아무 이유 없이 차징을 잃는다.

인벤토리를 막은 덕분에 "모달을 여는 순간 차징이 굳는" 경로가 플레이어
조작에서는 사라졌다. 남는 건 저절로 뜨는 레벨업 카드 화면뿐이라
거기에만 CancelSkillCharge를 걸었다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: 카드로 스킬 획득과 슬롯 교체

**Files:**
- Modify: `Source/Project_GJ/GJGameTypes.h`
- Modify: `Source/Project_GJ/GJCardComponent.h`, `Source/Project_GJ/GJCardComponent.cpp`
- Modify (에셋): `Content/GJ/DataTables/DT_CardData`

**Interfaces:**
- Consumes: `UGJSkillComponent::EquipSkill/EquipSkillInSlot/GetSkillInSlot/FindSkill` (Task 3), `FGJChoiceEntry`, `UGJCardComponent::OpenChoiceUI` (M2.6)
- Produces: `FCardData::SkillId`

- [ ] **Step 1: `FCardData`에 `SkillId` 추가**

`GJGameTypes.h`의 `FCardData` 안에서 다음 두 줄을 찾는다:

```cpp
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    TSubclassOf<AGJWeaponBase> WeaponClass;
```

그 **아래**에 추가한다:

```cpp

    // EffectType == Ability일 때만 쓰인다. DT_SkillData의 행 이름.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Card")
    FName SkillId;
```

- [ ] **Step 2: `EGJChoiceMode`에 `SkillReplace` 추가**

`GJCardComponent.h`에서 다음 블록을 찾는다:

```cpp
enum class EGJChoiceMode : uint8
{
    None,
    Card,
    WeaponReplace
};
```

이를 다음으로 교체한다:

```cpp
enum class EGJChoiceMode : uint8
{
    None,
    Card,           // 인덱스 = 뽑힌 카드 목록의 위치
    WeaponReplace,  // 인덱스 = 버릴 무기 슬롯 번호
    SkillReplace    // 인덱스 = 버릴 스킬 슬롯 번호
};
```

- [ ] **Step 3: `GJCardComponent.h`에 대기 스킬 멤버 추가**

`GJCardComponent.h`에서 다음 두 줄을 찾는다:

```cpp
    UPROPERTY()
    AGJWeaponBase* PendingWeapon;
```

그 **아래**에 추가한다:

```cpp

    // SkillReplace 모드일 때 장착 대기 중인 스킬 ID
    FName PendingSkillId;
```

- [ ] **Step 4: `AGJCharacter`에 스킬 컴포넌트 접근자 추가**

다음 두 스텝이 이 접근자를 쓰므로 먼저 만든다.

`GJCharacter.h`의 `public:` 블록에서 `GJSkillInfo` 선언 **아래**에 추가한다:

```cpp

    // 카드 컴포넌트가 능력 카드를 적용할 때 쓴다.
    UFUNCTION(BlueprintPure, Category = "Skill")
    UGJSkillComponent* GetSkillComponent() const { return SkillComponent; }
```

- [ ] **Step 5: 뽑기 필터에 `Ability` 조건 추가**

`GJCardComponent.cpp` 상단 include 블록에 추가한다:

```cpp
#include "GJSkillComponent.h"
```

이어서 `DrawCards` 안에서 다음 블록을 찾는다:

```cpp
        if (Row->EffectType == ECardEffectType::StatBonus && IsStatEffectEmpty(Row->StatEffect))
        {
            continue;
        }
        // Ability는 거르지 않는다. 테이블에 있으면 UI에는 보이고, 고르면 적용 단계에서
        // 경고가 찍힌다(M2.7 작업 시 바로 확인할 수 있게).
```

이를 다음으로 교체한다:

```cpp
        if (Row->EffectType == ECardEffectType::StatBonus && IsStatEffectEmpty(Row->StatEffect))
        {
            continue;
        }
        // 스킬 ID가 비었거나 테이블에 없는 능력 카드는 골라도 아무 일이 없다.
        // WeaponClass가 빈 무기 카드를 거르는 것과 같은 이유다.
        // 슬롯이 꽉 찼다는 이유로는 거르지 않는다 - 그 경우 카드를 고른 뒤 무엇을 버릴지 정한다.
        if (Row->EffectType == ECardEffectType::Ability)
        {
            const AGJCharacter* OwnerChar = GetOwnerCharacter();
            const UGJSkillComponent* Skills = OwnerChar ? OwnerChar->GetSkillComponent() : nullptr;
            if (!Skills || !Skills->FindSkill(Row->SkillId))
            {
                continue;
            }
        }
```

- [ ] **Step 6: `ApplyCard`의 `Ability` 분기 교체**

`GJCardComponent.cpp`의 `ApplyCard` 안에서 다음 블록을 찾는다:

```cpp
    case ECardEffectType::Ability:
    {
        // 조용히 무시하면 데이터 테이블에 능력 카드를 넣어두고 "왜 안 먹지?"로 헤매게 된다.
        UE_LOG(LogTemp, Warning,
            TEXT("ApplyCard: 능력 카드 '%s'는 아직 미구현입니다 (스킬 시스템 M2.7 필요)."),
            *CardId.ToString());
        break;
    }
```

이를 다음으로 교체한다:

```cpp
    case ECardEffectType::Ability:
    {
        UGJSkillComponent* Skills = Character->GetSkillComponent();
        if (!Skills)
        {
            UE_LOG(LogTemp, Warning, TEXT("ApplyCard: SkillComponent가 없어 '%s'를 적용할 수 없습니다."), *CardId.ToString());
            break;
        }

        // 빈 슬롯이 있으면 여기서 끝난다.
        if (Skills->EquipSkill(Row->SkillId))
        {
            break;
        }

        // 슬롯이 꽉 찼다. 어느 스킬을 버릴지 묻는다. 무기 교체와 같은 위젯을 쓴다 -
        // 위젯이 FGJChoiceEntry를 받아 인덱스만 돌려주기 때문에 전용 화면이 필요 없다.
        const TCHAR* SlotKeys[3] = { TEXT("우클릭"), TEXT("Q"), TEXT("R") };

        TArray<FGJChoiceEntry> Entries;
        for (int32 SlotIndex = 0; SlotIndex < 3; SlotIndex++)
        {
            const FName EquippedId = Skills->GetSkillInSlot(SlotIndex);
            const FSkillData* Equipped = Skills->FindSkill(EquippedId);

            FGJChoiceEntry Entry;
            Entry.DisplayName = Equipped ? Equipped->DisplayName : FText::FromName(EquippedId);
            // 슬롯 선택이 곧 키 선택이다. 번호만 보여주면 어느 손가락이 바뀌는지 알 수 없다.
            Entry.Description = FText::Format(
                NSLOCTEXT("GJ", "ReplaceSkillSlot", "{0} 자리를 버리고 교체한다"),
                FText::FromString(SlotKeys[SlotIndex]));
            Entry.Icon = Equipped ? Equipped->Icon : nullptr;
            Entries.Add(Entry);
        }

        // 화면을 못 띄우면 카드 3장이 뜬 채 모드만 바뀌어, 카드를 누르는 순간 그 인덱스가
        // 스킬 슬롯으로 해석된다. 그 상태에 빠지느니 슬롯 0을 덮어쓰는 편이 낫다.
        if (!OpenChoiceUI(Entries))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("ApplyCard: 스킬 교체 화면을 띄우지 못해 슬롯 0을 덮어씁니다 (%s)."), *CardId.ToString());
            Skills->EquipSkillInSlot(0, Row->SkillId);
            break;
        }

        PendingSkillId = Row->SkillId;
        CurrentMode = EGJChoiceMode::SkillReplace;

        // 스택 불가 카드는 여기서 기록한다. 교체는 두 단계라 아래의 공통 기록 지점
        // (return true 직전)을 지나가지 않는다.
        if (!Row->bStackable)
        {
            TakenCards.Add(CardId);
        }

        return false;  // 아직 안 끝났다 - 대기열을 줄이면 안 된다
    }
```

- [ ] **Step 7: `HandleChoiceSelected`에 `SkillReplace` 처리 추가**

`GJCardComponent.cpp`의 `HandleChoiceSelected` 안에서 다음 블록을 찾는다:

```cpp
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
```

그 **아래**에 추가한다:

```cpp

    if (CurrentMode == EGJChoiceMode::SkillReplace)
    {
        if (UGJSkillComponent* Skills = Character->GetSkillComponent())
        {
            Skills->EquipSkillInSlot(ChoiceIndex, PendingSkillId);
        }
        PendingSkillId = NAME_None;

        PendingChoices--;
        ShowNextChoice();
    }
```

- [ ] **Step 8: 컴파일**

사용자에게 요청한다:
> **Ctrl+Alt+F11**로 컴파일해줘.

Run: `grep -cE "error C" Saved/Logs/Project_GJ.log`
Expected: `0`

- [ ] **Step 9: `DT_CardData`의 `Card_Fireball`에 `SkillId` 지정**

MCP `DataTableTools.set_rows`로 설정한다:

```
data_table: /Game/GJ/DataTables/DT_CardData.DT_CardData
values:     {"Card_Fireball":{"displayName":"파이어볼","description":"우클릭으로 발사. 꾹 누르면 강해진다","skillId":"Skill_Fireball"}}
```

`get_rows`로 `skillId`가 `Skill_Fireball`인지 확인한 뒤 `save_assets`로 저장한다. 이름과 설명에서 "(미구현)" 표기를 지우는 것도 함께 한다.

- [ ] **Step 10: 카드 획득 확인**

사용자에게 요청한다:
> 플레이하고 레벨업해서 **파이어볼 카드가 뜰 때까지** 반복해줘(`Card_Fireball`은 가중치 0.2라 잘 안 나온다. `GJSetTagWeight Tree.Fire 5`를 먼저 치면 훨씬 자주 나온다). 카드를 고른 뒤 `GJSkillInfo`를 쳐줘.

확인 항목:
- **"미구현입니다" 경고가 더 이상 안 뜬다**
- `GJSkillInfo`에 `슬롯 0 (우클릭): Skill_Fireball`이 보인다
- 우클릭으로 실제 발사된다

- [ ] **Step 11: 슬롯 교체 확인**

사용자에게 요청한다:
> Output Log의 `Cmd:` 입력칸에 **한 줄씩** 쳐서 슬롯 3개를 채워줘:
>
> ```
> GJEquipSkill Skill_Fireball 0
> ```
> ```
> GJEquipSkill Skill_Fireball 1
> ```
> ```
> GJEquipSkill Skill_Fireball 2
> ```
>
> 그다음 레벨업해서 파이어볼 카드를 다시 골라줘.

확인 항목:
- 카드를 고르면 **선택지 3개가 뜨고**, 설명이 `우클릭 자리를 버리고 교체` / `Q 자리를...` / `R 자리를...`다
- 고른 슬롯이 정확히 교체된다(`GJSkillInfo`로 확인)
- **레벨이 3번 올랐다면 카드 화면이 총 3번 뜬다** — 교체 화면은 그중 하나에 딸린 2단계지 별도의 한 번이 아니다
- 교체 뒤 `Card_Fireball`이 다시 뽑히지 않는다(`bStackable=false`가 교체 경로에서도 기록됐는가)

- [ ] **Step 12: 커밋**

```bash
git add Source/Project_GJ/GJGameTypes.h Source/Project_GJ/GJCharacter.h Source/Project_GJ/GJCardComponent.h Source/Project_GJ/GJCardComponent.cpp Content/GJ/DataTables/DT_CardData.uasset
git commit -m "$(cat <<'EOF'
능력 카드로 스킬 획득과 슬롯 교체 추가

FCardData에 SkillId를 넣고, Ability 카드가 경고를 찍는 대신 실제로
스킬을 장착하게 했다.

슬롯이 꽉 차면 어느 스킬을 버릴지 묻는다. 무기 교체와 같은 위젯을
쓴다 - 위젯이 FGJChoiceEntry를 받아 인덱스만 돌려주기 때문에 전용
화면이 필요 없고 EGJChoiceMode에 값 하나만 더하면 된다.

선택지 설명에 슬롯 번호가 아니라 키를 보여준다. 슬롯 0/1/2가
우클릭/Q/R이라 슬롯 선택이 곧 키 선택인데, 번호만 보여주면 어느
손가락이 바뀌는지 알 수 없다.

무기 교체에서 겪은 두 가지를 그대로 적용했다. 이 단계에서 대기열을
줄이면 연속 레벨업 중에 카드 한 장이 증발하고, 스택 불가 카드를 분기
안에서 기록하지 않으면 같은 카드가 계속 다시 뜬다.

SkillId가 비었거나 테이블에 없는 능력 카드는 뽑기에서 제외한다.
슬롯이 꽉 찼다는 이유로는 거르지 않는다 - 그건 고른 뒤 정할 몫이다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: 개발 가이드 갱신

**Files:**
- Modify: `Docs/DevGuide.md`
- Modify: `DevGuide.html`

- [ ] **Step 1: 새 절 추가**

`Docs/DevGuide.md`의 `## 7. UI / 위젯` 절 **바로 위**에 새 절 `## 6.8 액티브 스킬 시스템`을 추가한다. 다음 내용을 담는다:

- 구조: `AGJCharacter`(입력 전달만) → `UGJSkillComponent`(슬롯 3개/쿨타임/차징/풀) → `AGJProjectile`(공용)
- 슬롯 0/1/2 = 우클릭/Q/R
- 차징 공식: `배율 = 1 + (MaxChargeMultiplier - 1) x clamp(경과/ChargeTime, 0, 1)`, 크기와 데미지에 함께 곱해짐
- `ChargeTime <= 0`이면 누르는 순간 발사
- MP·쿨타임은 떼는 순간 고정값
- 데미지는 `SkillPower`(평타의 `BaseAttackPower`와 별개), 치명타는 공유
- **틱을 쓰지 않고 시각 비교** → 일시정지 중 차징이 안 차오름
- **`ECharacterState`에 `Charging`을 넣지 않은 이유**: 단일 상태 enum인데 차징은 이동과 동시에 성립. 끝났을 때 무엇으로 되돌릴지 알 수 없고, `Rolling`/`Dodge` 중복도 같은 이유로 생긴 것으로 보임
- 차징 중 제약 표 (이동 O / 회피 O·취소 / 평타·재장전·스왑·인벤토리 X)
- **관통은 콜리전 프로필을 바꿔야 동작**: `BlockAllDynamic` + `bShouldBounce=false`면 적에게 닿는 순간 멈춤. 관통일 때만 폰을 Overlap, 벽은 Block. 풀 재사용이라 비관통 경로에서 프로필을 되돌려야 함
- `PierceCount` 의미: 0=1명, 1=2명, -1=무한(감소 안 함)
- `HitActors`가 필요한 이유(큰 구체의 프레임당 재타격)와 `Deactivate`에서 비워야 하는 이유
- 풀은 구체 클래스별, 지연 스폰
- 콘솔: `GJEquipSkill <ID> [슬롯]`, `GJSkillInfo`

- [ ] **Step 2: 6.7절(카드)에 스킬 교체 반영**

`Docs/DevGuide.md` 6.7절의 효과 적용 표에서 `Ability` 행을 다음으로 교체한다:

```
| 효과 적용 | `StatBonus` → `AddStatBonus`, `GrantWeapon` → 스폰 후 `PickUpWeapon`(슬롯이 차 있으면 교체 선택), `Ability` → `EquipSkill`(슬롯이 차 있으면 교체 선택) |
```

그리고 `EGJChoiceMode` 설명에 `SkillReplace`를 더한다.

- [ ] **Step 3: 8절 스키마에 `FSkillData` 추가**

`Docs/DevGuide.md` 8절의 `### FCardData` 절 **바로 아래**에 `### FSkillData — DT_SkillData (행 이름 = 스킬 ID)` 표를 추가한다. Task 3 Step 1의 필드 전부와 기본값을 옮긴다.

`### FCharacterStat` 표에는 `SkillPower` 행을 추가하고, `FCardData` 표에는 `SkillId` 행을 추가한다.

- [ ] **Step 4: 9절 TODO 갱신**

다음 두 항목을 **삭제한다**:

```markdown
- 액티브 스킬 개념이 없음 — 파이어볼 같은 능력 카드를 붙이려면 스킬 슬롯/쿨다운/MP 소모/입력 바인딩이 전부 새로 필요하다. 발사체(`AGJProjectile` 풀)는 재사용 가능
- 능력 카드(`ECardEffectType::Ability`)는 골라도 경고만 찍힘 — 위 항목의 스킬 시스템이 통째로 없음 (M2.7)
```

같은 목록 끝에 다음을 **추가한다**:

```markdown
- **`IA_Skill3`과 `IA_Reload`가 둘 다 R키에 매핑돼 있음** — Skill3에 실제 스킬을 넣으면 R을 누를 때 재장전과 스킬이 같이 나간다. `IMC_GJ`에서 정리 필요
- 스킬 쿨타임·슬롯 HUD가 없음 — 어느 키에 무슨 스킬이 있는지 `GJSkillInfo` 콘솔로만 확인 가능. 슬롯 교체가 가능해진 만큼 실제로 불편함
- `ESkillType::Persistent`(지속형 스킬) 미구현 — 고르면 경고만 찍힘
- Skill2(Q)·Skill3(R)에 넣을 실제 스킬이 없음 — 슬롯과 입력 바인딩은 준비됨
- 시전 애니메이션·차징 이펙트·발사 이펙트가 없음 — 구체가 그냥 나타난다
- `DT_SkillData`의 파이어볼 수치와 `DT_CharacterStat`의 `SkillPower` 곡선은 **임시 테스트 값**
- `FCharacterStat.CooldownReduction`은 여전히 어디에도 연결되지 않음 — 스킬 쿨타임에 적용하는 게 자연스러운 첫 후보
```

- [ ] **Step 5: `DevGuide.html`에 동일 내용 반영**

`DevGuide.html`에 Step 1~4의 변경을 동일하게 반영한다. 기존 문서의 CSS 클래스(`.note`, `.meta`, `.path`)와 표 구조를 그대로 따른다. 새 절은 `<h2 id="skills">6.8 액티브 스킬 시스템</h2>`로 7절 앞에 넣는다.

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
        print(f'{tag}: {o} vs {c}  <-- MISMATCH')
print('OK' if bad==0 else f'{bad} MISMATCH')
"
```
Expected: `OK`

- [ ] **Step 6: 커밋**

```bash
git add Docs/DevGuide.md DevGuide.html
git commit -m "$(cat <<'EOF'
개발 가이드에 액티브 스킬 시스템 반영

6.8절을 새로 넣었다. 캐릭터가 입력만 넘기고 스킬을 모르는 구조, 차징
공식, 틱 대신 시각 비교를 쓰는 이유와 그 부수 효과를 남겼다.

ECharacterState에 Charging을 넣지 않은 이유를 적었다. 단일 상태 enum인데
차징은 이동과 동시에 성립해서, 끝났을 때 무엇으로 되돌릴지 알 수 없다.

관통이 콜리전 프로필을 바꿔야 동작한다는 것도 적었다. BlockAllDynamic에
bShouldBounce가 false면 적에게 닿는 순간 투사체가 멈춘다. 풀에서
재사용되므로 비관통 경로에서 프로필을 되돌려야 한다는 것까지 함께.

9절에서 "액티브 스킬 개념이 없음"을 지우고, 이번에 드러난 갭을 적었다.
특히 IA_Skill3과 IA_Reload가 둘 다 R키에 매핑돼 있다.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>
EOF
)"
```
