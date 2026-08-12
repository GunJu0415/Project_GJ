# EXP와 레벨업 (M2 인런 성장) — 설계 문서

> 작성일: 2026-08-12
> 대상: Project_GJ (UE 5.8, C++ 우선 + 얇은 블루프린트 레이어)
> 상태: 승인됨, 구현 계획 작성 대기
> 선행: M1 런 루프(`2026-08-08-run-loop-design.md`), 전투 스탯과 데미지 공식(`2026-08-08-combat-stats-design.md`) 완료

---

## 1. 배경과 문제

로그라이크의 한 회차(런)는 **"들어갈 때보다 강해져서 나온다"**가 성립해야 성립한다. 지금은 그 축이 없다.

- `AGJCharacter::CurrentLevel`은 `BeginPlay`에서 1로 고정되고 **평생 바뀌지 않는다.** 레벨을 올리는 코드가 존재하지 않는다.
- `FCharacterStat::RequiredEXP`는 선언되어 있지만 **코드에서 한 번도 읽히지 않는다.** 경험치라는 개념 자체가 없다.
- `AGJEnemyCharacter::HandleDeath()`는 적을 사라지게만 할 뿐, **누가 죽였는지도 모르고 보상도 주지 않는다.**
- `DT_CharacterStat`은 레벨을 행 이름으로 쓰는 구조라 성장 테이블의 **틀은 이미 있는데, 그 틀을 올라갈 방법이 없다.**

직전 작업에서 방어력·이동속도·치명타를 추가해 **"레벨이 오르면 무엇이 강해지는가"**를 이미 정의해 두었다. 이 작업은 그 스탯 테이블을 **실제로 타고 올라가는 수단**을 만든다.

**목표: 적을 죽이면 경험치를 얻고, 경험치가 차면 레벨이 올라 `DT_CharacterStat`의 다음 행 스탯을 갖는다.**

---

## 2. 범위

### 포함

- `FEnemyStat`에 `ExpReward` 추가 — 적마다 주는 경험치를 데이터로 지정
- 적이 죽었을 때 **실제로 죽인 플레이어**에게 경험치 지급
- `AGJCharacter`에 경험치 누적·레벨업 처리 (`CurrentEXP` / `AddEXP` / `LevelUp`)
- 한 번의 킬로 여러 레벨이 오르는 경우와 초과 경험치 이월
- 레벨 상한 = `DT_CharacterStat`의 마지막 행
- 레벨업 시 HP/MP는 **증가분만** 회복 (풀 회복 아님)
- HUD에 경험치 바 + 레벨 숫자
- 카드 선택 시스템이 붙을 자리(`OnLevelUp` 델리게이트)만 미리 확보

### 제외

| 항목 | 사유 |
|---|---|
| 카드 3장 선택 시스템 | 유저가 명시적으로 뒤로 미룸("일단 이거는 나중에 잡고"). 이번엔 훅만 만든다 |
| 레벨/경험치의 회차 간 유지 | **의도적으로 제외.** 6절 참고 |
| 영구 특성(메타 프로그레션) | M6. 별도 재화 + 별도 저장소로 만든다. 이 작업의 EXP와 섞지 않는다 |
| 스테이지 클리어 보상 | M5에서 진행 구조가 생긴 뒤 |
| 경험치 획득 연출(팝업, 사운드) | 범위 밖. `OnLevelUp`으로 나중에 붙일 수 있다 |
| 근접 무기로 죽였을 때의 경험치 | M7에서 근접 히트 판정이 생기면 **자동으로 동작한다**(4.2절) |

---

## 3. 경험치 획득

### 3.1 적마다 하드코딩한다

경험치 보상은 `FEnemyStat`에 값으로 박는다. 레벨에서 자동으로 유도하지 않는다.

| 필드 | 타입 | 기본값 | 비고 |
|---|---|---|---|
| `ExpReward` | `float` | 10 | 이 적을 죽였을 때 주는 경험치 |

`int32`가 아니라 `float`인 이유: 비교 대상인 `FCharacterStat::RequiredEXP`가 이미 `float`이다. 경험치 값을 정수로 두면 누적·비교·이월 과정에서 계속 형 변환이 섞이고, 나중에 "경험치 획득량 +15%" 같은 배율이 붙을 때 반올림 규칙을 따로 정해야 한다. 파이프라인 전체를 `float`으로 통일한다 — `ExpReward`, `CurrentEXP`, `AddEXP`의 인자가 모두 `float`이다. 표시할 때만 정수로 반올림한다.

**유도 방식을 쓰지 않은 이유.** `MaxHP`, `AttackDamage`, `Defense`가 이미 전부 행마다 명시된 값이다. 경험치만 다른 스탯에서 계산해 내면 **밸런스 축이 서로 묶인다** — 어떤 적을 좀 더 단단하게 만들려고 HP를 올린 순간 경험치까지 따라 올라가서, 의도하지 않은 성장 속도 변화가 생긴다. 특히 "체력만 많고 안 위험한 샌드백"이나 "약한데 빠른 견제형" 같은 적은 유도 공식으로 표현할 수 없다.

행 하나에 숫자 하나 더 넣는 비용으로 이 결합을 없앨 수 있으므로, 명시가 맞다.

### 3.2 킬러를 어떻게 아는가

`AGJEnemyCharacter::HandleDeath()`에는 **가해자 정보가 전혀 없다.** 죽는 순간 자기 자신만 안다.

죽인 주체를 알아내는 지점은 `AGJBaseCharacter::TakeDamage()`다. 여기에는 `EventInstigator`(가해자 컨트롤러)가 이미 들어온다. 그래서 **HP가 0이 되어 `HandleDeath()`를 부르기 직전에 가해자를 기억해 둔다.**

```
AGJBaseCharacter
  UPROPERTY()
  TWeakObjectPtr<AController> LastDamageInstigator;
```

`TWeakObjectPtr`인 이유: 컨트롤러가 먼저 파괴되어도 **댕글링 포인터가 되지 않는다.** 적이 죽고 2초 뒤 `DestroySelf`까지 살아 있는 구조라, 그 사이에 대상이 사라질 여지가 실제로 있다.

**대안으로 고려했던 것.** `HandleDeath()`에 가해자 파라미터를 추가하는 방법도 있었으나, 이미 여러 곳에서 오버라이드·호출되는 가상 함수라 시그니처를 바꾸면 파급이 크다. 멤버 저장이 기존 코드를 가장 적게 건드린다.

### 3.3 흐름

```
플레이어가 적을 죽임
  → AGJBaseCharacter::TakeDamage()
       LastDamageInstigator = EventInstigator   ← HP 0 확인 직후, HandleDeath() 직전
       → HandleDeath()
  → AGJEnemyCharacter::HandleDeath()  [오버라이드]
       LastDamageInstigator → GetPawn() → AGJCharacter로 캐스팅
       성공하면 → Character->AddEXP(ExpReward)
```

캐스팅이 실패하면(적이 적을 죽임, 낙사 등 환경 사망, 컨트롤러가 이미 사라짐) **아무에게도 경험치가 가지 않는다.** 조용히 넘어가는 것이 맞다 — 이 경우 "받을 사람이 없다"가 정답이지 오류가 아니다.

### 3.4 ExpReward를 적 인스턴스에 복사해 둔다

`ApplyEnemyStat()`이 이미 `MaxHP`/`Defense` 등을 데이터 테이블에서 멤버로 복사하고 있다. `ExpReward`도 같은 방식으로 `AGJEnemyCharacter`의 멤버에 복사한다. `HandleDeath()`에서 다시 데이터 테이블을 조회하지 않기 위함이며, 기존 스탯들과 동일한 패턴이라 새 규칙이 생기지 않는다.

---

## 4. 레벨업

### 4.1 RequiredEXP의 의미 — 누적이 아니다

`FCharacterStat::RequiredEXP`는 **"이 레벨에서 다음 레벨로 가는 데 필요한 양"**으로 해석한다. 누적 총량이 아니다.

| 행(레벨) | RequiredEXP | 뜻 |
|---:|---:|---|
| 1 | 100 | 레벨 1 → 2 에 100 필요 |
| 2 | 250 | 레벨 2 → 3 에 250 필요 |
| 3 | 500 | 레벨 3 → 4 에 500 필요 |

이 해석을 택하면 `CurrentEXP`는 **레벨업할 때마다 0 부근으로 리셋**되며, 경험치 바가 "이번 레벨의 진행도"를 그대로 나타낸다. 누적 해석은 UI에서 매번 이전 레벨 누적치를 빼야 하고 테이블 값이 계속 커지므로 채택하지 않는다.

### 4.2 AddEXP

```
void AGJCharacter::AddEXP(float Amount)

  Amount <= 0 이면 return
  이미 만렙이면 return                       ← 만렙에서는 경험치를 쌓지 않는다

  CurrentEXP += Amount

  while (CurrentEXP >= CurrentCharacterStat.RequiredEXP)
  {
      if (IsMaxLevel())                      ← 이번 루프에서 만렙에 도달했으면
      {
          CurrentEXP = 0
          break
      }
      CurrentEXP -= CurrentCharacterStat.RequiredEXP   ← 초과분 이월
      LevelUp()                                        ← CurrentCharacterStat이 여기서 갱신됨
  }

  UpdatePlayerHUD()
```

루프 구조라 **보스를 잡아 2~3레벨이 한 번에 오르는 경우**도 그대로 처리된다. 각 반복에서 그 시점 레벨의 `RequiredEXP`를 빼므로 이월량도 정확하다.

`RequiredEXP`가 0 이하인 행이 있으면 무한 루프가 되므로, 루프 조건에 `RequiredEXP > 0`을 함께 둔다. 데이터 실수가 에디터를 멈추게 두면 안 된다.

`AddEXP`는 `public` + `BlueprintCallable`로 둔다. 적 처치 외에 퀘스트·상자 등 다른 경험치 소스가 생겨도 같은 입구를 쓴다.

### 4.3 레벨 상한 = 테이블의 마지막 행

```
bool AGJCharacter::IsMaxLevel() const
    → CharacterStatTable에서 (CurrentLevel + 1) 행을 찾는다
    → 없으면 만렙
```

이때 `FindRow`의 세 번째 인자 `bWarnIfRowMissing`에 **`false`를 넘긴다.** 기본값이 `true`라 그대로 두면 만렙에 도달한 뒤 적을 죽일 때마다 "행을 찾을 수 없음" 경고가 로그를 채운다. 여기서는 행이 없는 것이 **정상 동작(만렙)**이지 오류가 아니다.

**상한 상수를 코드에 두지 않는다.** `DT_CharacterStat`에 행 6, 7을 추가하면 코드 수정 없이 만렙이 늘어나고, 행이 하나뿐이면 즉시 만렙으로 취급되어 존재하지 않는 행을 찾다 실패하는 일이 없다. 데이터가 진실의 원천이라는 이 프로젝트의 기존 방향과 같다.

### 4.4 LevelUp — HP/MP는 증가분만

```
void AGJCharacter::LevelUp()
    UpdateCharacterStat(CurrentLevel + 1, /*bRestoreToFull=*/false)
    OnLevelUp.Broadcast(CurrentLevel)
```

문제는 현재 `UpdateCharacterStat`이 **무조건 `CurrentHP = MaxHP`로 풀 회복**을 한다는 점이다. 이걸 레벨업에 그대로 쓰면 **레벨업 = 완전 회복**이 되어, 위기 상황에서 잡몹 하나 잡는 것이 최고의 회복 수단이 된다. 로그라이크에서 체력 관리 긴장이 통째로 사라진다.

그래서 파라미터를 하나 추가한다:

```
void UpdateCharacterStat(int32 NewLevel, bool bRestoreToFull = true);
```

| 값 | 호출 지점 | 동작 |
|---|---|---|
| `true` (기본) | `BeginPlay`, 리스폰 | **지금과 완전히 동일.** 기존 호출부를 고칠 필요가 없다 |
| `false` | `LevelUp()` | 최대치 증가분만 현재값에 더한다 |

```
const float OldMaxHP = MaxHP;
MaxHP = CurrentCharacterStat.MaxHP;
if (bRestoreToFull)  CurrentHP = MaxHP;
else                 CurrentHP = FMath::Clamp(CurrentHP + (MaxHP - OldMaxHP), 0.f, MaxHP);
// MP도 동일
```

체력 30/100에서 최대 체력이 120으로 오르면 50/120이 된다. 성장의 이득은 주되 **위험한 상태는 유지된다.** 방어력·치명타·이동속도는 두 경로 모두 새 값으로 덮어쓴다(현재값이라는 개념이 없는 스탯이므로).

기본 인자를 `true`로 둔 덕분에 이 변경은 **기존 동작에 대해 무해하다.**

### 4.5 카드 시스템 훅

```
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelUp, int32, NewLevel);

UPROPERTY(BlueprintAssignable, Category = "Level")
FOnLevelUp OnLevelUp;
```

지금은 아무도 구독하지 않는다. 나중에 카드 3장 선택 UI가 **여기 한 곳에만** 붙으면 되도록 자리를 잡아 두는 것이다. 스테이지 클리어 쪽 트리거는 진행 구조가 생기는 M5에서 별도로 만든다.

이 프로젝트에서 이미 `OnDamaged`/`OnAmmoChanged`/`OnStateChanged`가 같은 패턴을 쓰고 있어 새로운 관례가 아니다.

---

## 5. UI

`UGJPlayerHUDWidget`에 두 개를 추가한다. **둘 다 `BindWidgetOptional`이다.**

| 필드 | 타입 | 바인딩 |
|---|---|---|
| `EXPBar` | `UProgressBar` | `BindWidgetOptional` |
| `LevelText` | `UTextBlock` | `BindWidgetOptional` |

```
void UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level);
```

**`BindWidget`(strict)이 아닌 이유.** 기존 `HPBar`/`MPBar`는 strict라 위젯이 없으면 WBP 컴파일이 실패한다. EXP를 strict로 선언하면 C++ 컴파일 직후 `WBP_PlayerHUD`가 **깨진 상태**가 되어, 유저가 에디터에서 위젯을 배치할 때까지 게임이 정상 동작하지 않는다. Optional이면 C++만 먼저 들어가도 아무 문제가 없고, 에디터에서 같은 이름의 위젯을 추가하는 순간 자동으로 살아난다. 이 프로젝트는 C++ 변경과 에디터 작업이 항상 시차를 두고 일어나므로 Optional이 맞다.

갱신은 **이미 있는 `AGJCharacter::UpdatePlayerHUD()`** 안에 `UpdateEXP(...)` 호출 한 줄을 더하는 것으로 끝난다. 새 갱신 경로를 만들지 않는다 — 레벨업, 경험치 획득, 피격, 소비 아이템 사용이 모두 이 함수를 이미 거친다.

`RequiredEXP`가 0이면(만렙 등) 나누기 0이 되므로, 위젯 쪽에서 0 체크 후 바를 가득 찬 상태로 표시한다.

---

## 6. 경험치는 런마다 초기화된다

**의도된 동작이다.**

플레이어가 죽으면 M1 런 루프가 허브 레벨을 로드하고, 새 런을 시작하면 전투 레벨을 다시 로드한다. 레벨이 리로드되면 캐릭터가 새로 스폰되고 `BeginPlay`에서 `CurrentLevel = 1` → `UpdateCharacterStat(1)`을 타므로, **경험치·레벨 초기화를 위한 코드를 따로 쓸 필요가 없다.**

회차를 넘어 남는 성장은 이 시스템이 아니라 **M6 영구 특성**이 담당한다. 별도의 재화로 찍는 영구 특성 트리이며, 저장 대상도 다르다. 그래서 이번 작업에서 `CurrentEXP`/`CurrentLevel`은 **어떤 세이브 경로에도 넣지 않는다.** 두 축을 섞으면 나중에 "이번 런의 성장"과 "영구 성장"을 분리하기 어려워진다.

```
런 내부 성장 (이 문서)        — 죽으면 사라진다. EXP/레벨/카드
영구 성장 (M6)                — 계속 쌓인다. 별도 재화로 찍는 특성 트리
```

---

## 7. 검증 방법

자동화된 테스트 스위트가 없으므로 **컴파일 통과 + 수동 PIE 확인**이다.

1. Live Coding 컴파일 통과
2. `DT_EnemyStat`에 `ExpReward`를 넣고, 적을 죽였을 때 **경험치 바가 그만큼 차는지**
3. 경험치를 다 채웠을 때 **레벨 숫자가 오르고 경험치 바가 리셋**되는지
4. 레벨업 직후 **최대 체력이 오르고, 현재 체력이 풀이 아니라 증가분만 회복**되는지 (체력을 절반쯤 깎은 상태로 레벨업)
5. `ExpReward`를 `RequiredEXP`보다 크게(예: 500 vs 100) 설정해 **한 번의 킬로 여러 레벨**이 오르고 **초과분이 이월**되는지
6. 마지막 행 레벨에 도달한 뒤 계속 적을 죽여도 **레벨이 더 오르지 않고 크래시도 없는지**
7. 죽어서 허브로 갔다가 새 런을 시작하면 **레벨 1, 경험치 0으로 초기화**되는지
8. 적이 다른 적을 죽이거나 낙사시켜도 **크래시 없이 조용히 넘어가는지**

---

## 8. 필요한 에디터 작업

| 작업 | 비고 |
|---|---|
| `DT_EnemyStat` 각 행에 `ExpReward` 값 입력 | `FEnemyStat`에 필드가 늘어나므로 기존 행에 새 칸이 생긴다 |
| `DT_CharacterStat`에 레벨 2~5 행 채우기 | **현재 실질적으로 레벨 1 행만 의미가 있다.** 다음 행이 없으면 즉시 만렙이라 레벨업을 확인할 수 없다 |
| `WBP_PlayerHUD`에 `EXPBar`(Progress Bar) / `LevelText`(Text Block) 배치 | 이름이 정확히 일치해야 자동 바인딩된다 |
| (필요 시) 에디터 재시작 | `USTRUCT` 레이아웃 변경 후 데이터 테이블에 새 칸이 안 보이는 이력이 있다 |

> ⚠️ 데이터 테이블 행을 MCP로 수정할 때는 **행 전체를 명시**해야 한다. 지정하지 않은 필드가 구조체 기본값으로 리셋된 이력이 있다.

---

## 9. 후속 작업과의 관계

```
전투 스탯 + 데미지 공식 (완료)   — 레벨이 오르면 강해질 대상
 └─ EXP / 레벨업  ← 이 문서       — 그 스탯 테이블을 타고 올라가는 수단
      ├─ 카드 3장 선택            — OnLevelUp에 붙는다 (연기됨)
      └─ M5 스테이지 진행         — 스테이지 클리어도 성장 지점이 된다

M6 영구 특성                      — 별개의 축. 이 문서의 EXP와 섞지 않는다
M7 근접 무기 히트 판정            — TakeDamage를 거치므로 경험치 지급이 자동으로 동작한다
```
