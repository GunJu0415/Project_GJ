# 액티브 스킬 시스템 (M2.7) 설계

**작성일**: 2026-08-17
**범위**: Skill1(우클릭) 차징 발사체 하나. 스킬 슬롯·쿨타임·MP 소모·카드 연동까지.

## 목표

우클릭을 누르고 있으면 차징되고, 떼면 구체가 날아간다. 누른 시간에 비례해 구체가 커지고 데미지가 오른다. 스킬은 레벨업 카드로 획득한다.

## 범위

**이번에 만드는 것**
- `UGJSkillComponent` — 슬롯 3개, 쿨타임, 차징, MP 검사, 발사
- `DT_SkillData` (`FSkillData`) — 스킬 정의 테이블
- `AGJProjectile` 확장 — 크기 배율, 관통
- `FCharacterStat.SkillPower` — 일반 공격력과 분리된 스킬 공격력
- `ECardEffectType::Ability` 카드가 실제로 스킬을 장착시킴
- 차징 중 입력 제약

**이번에 안 만드는 것**
- Skill2(Q)·Skill3(R) 실제 스킬 — 슬롯과 입력만 준비하고 내용은 비워둔다
- 지속형 스킬 — `ESkillType::Persistent`는 스텁이고 고르면 경고만 찍는다
- 스킬 쿨타임/슬롯 HUD — 콘솔로만 확인한다

## 결정 사항과 근거

### 카드 → 스킬 단방향

`FCardData`에 `SkillId`를 두고, `FSkillData`에는 카드 ID를 두지 않는다.

한 스킬을 주는 카드가 여러 개일 수 있다(등급별 파이어볼, 스킬+스탯 복합 카드). 양방향 링크를 두면 그 순간 역방향이 "여러 개 중 하나"만 가리키게 되어 조용히 거짓이 된다.

### 차징 모델 — 항상 발사, 비율에 비례

```
비율   = clamp(경과시간 / ChargeTime, 0, 1)
배율   = 1 + (MaxChargeMultiplier - 1) x 비율
구체크기 = BaseScale x 배율
데미지  = CalculateOutgoingDamage(BaseDamage x 배율, SkillPower, CritChance, CritMultiplier)
```

언제 떼든 발사된다. 짧게 톡 치면 작고 약한 구체, 꾹 누르면 크고 센 구체. **"최소 차징 미만이면 불발" 규칙을 두지 않는 이유는 입력이 씹힌 느낌을 없애기 위해서다** — 우클릭은 뗀 이상 항상 무언가를 한다. (차징을 무효화하는 유일한 경로는 아래의 회피이며, 그건 플레이어가 의도적으로 고른 취소다.)

MP와 쿨타임은 **떼는 순간 고정값**으로 소비된다. 차징률에 비례시키면 약한 구체 연타가 최적해가 될 수 있는데, 그 균형을 잡으려면 쿨타임까지 같이 조정해야 해서 변수가 둘로 늘어난다.

`ChargeTime <= 0`인 스킬은 **누르는 순간 발사**한다(배율 1.0 고정). 뗄 때까지 기다리면 차징도 없는데 손을 떼야 나가는 이상한 감각이 된다.

### 데미지는 `SkillPower`로 — 일반 공격력과 분리

무기와 같은 `CalculateOutgoingDamage`를 쓰되 공격력 자리에 `BaseAttackPower`가 아닌 **`SkillPower`**를 넣는다. 평타 특화와 스킬 특화 빌드가 갈린다.

치명타(`CritChance`/`CritMultiplier`)는 **공유한다.** 스킬 전용 치명타 스탯까지 나누면 스탯이 두 벌이 되는데, 그걸 정당화할 만한 설계 요구가 아직 없다.

### 차징 중에는 이동과 회피만

| 동작 | 차징 중 |
|---|---|
| 이동 | O (**속도 감소 없음**) |
| 회피(구르기) | O → 차징 취소, MP 소모 없음 |
| 좌클릭 평타 | X (입력 무시) |
| 재장전 | X (입력 무시) |
| 무기 스왑 1/2 | X (입력 무시) |
| 인벤토리 | X (입력 무시) |

**차징을 끊는 유일한 수단이 회피다.** 강한 한 방을 포기하고 회피를 소모하는 판단이 생긴다.

이동 속도를 깎지 않는 이유: 회피 말고 아무것도 못 하는 것만으로 충분한 대가다. 속도까지 깎으면 짧게 톡 치는 약한 발사조차 위험 부담이 생겨 우클릭을 아예 안 쓰게 된다.

### `ECharacterState`에 `Charging`을 추가하지 않는다

`ECharacterState`는 값을 하나만 갖는 단일 상태인데, 차징은 이동·대기와 **동시에 성립**한다. 여기에 넣으면:

1. 차징이 끝났을 때 무엇으로 되돌릴지 알 수 없다. `Idle`로 돌리면 차징 중 시작한 회피가 아직 안 끝났을 때 `Dodge`를 덮어써서 회피가 상태를 잃는다.
2. 이 enum은 이미 `Rolling`/`Dodge`, `Attacking`/`Attack` 중복으로 지저분하다(CLAUDE.md에 기록됨). 동시에 성립하는 것을 단일 상태에 밀어 넣다 생긴 흔적으로 보이며, 같은 실수를 반복할 이유가 없다.

대신 **`UGJSkillComponent::IsCharging()`**을 노출하고, 입력 핸들러가 이를 보고 조기 반환한다.

### `AGJProjectile`을 상속하지 않고 파라미터를 추가한다

스킬 구체와 총알의 차이는 **크기 가변 + 관통** 둘뿐이다. 서브클래스를 만들면 풀 관리·충돌·비활성화 로직이 두 벌이 되는데 그 셋은 완전히 동일하다.

비주얼 구분은 `FSkillData.ProjectileClass`로 다른 BP를 물려서 한다.

### 틱을 쓰지 않는다

차징 경과와 쿨타임 잔량을 매 프레임 깎지 않고 **시각(timestamp) 비교**로 계산한다. `ChargeStartTime`, `CooldownEndTime`만 들고 있으면 된다.

부수 효과: `GetWorld()->GetTimeSeconds()`는 일시정지된 월드에서 흐르지 않으므로 **카드 화면이 떠 있는 동안 차징이 몰래 차오르지 않는다.**

## 데이터 모델

### `FSkillData` — `DT_SkillData` (행 이름 = 스킬 ID)

| 필드 | 타입 | 기본값 | 설명 |
|---|---|---|---|
| `DisplayName` | FText | — | 스킬 이름 (UI·로그) |
| `Description` | FText | — | 설명 |
| `Icon` | UTexture2D* | nullptr | 스킬 아이콘 |
| `SkillType` | `ESkillType` | `Projectile` | `Projectile` / `Persistent`(미구현) |
| `MPCost` | float | 10 | 떼는 순간 고정 소비 |
| `Cooldown` | float | 3 | 떼는 순간부터 시작 (초) |
| `BaseDamage` | float | 40 | 차징 배율이 곱해지기 전 값 |
| `Range` | float | 2000 | 이만큼 날아가면 자동 소멸 |
| `ProjectileSpeed` | float | 1500 | 구체 속도 |
| `ChargeTime` | float | 1.5 | 최대 차징까지 걸리는 시간. **0이면 차징 없음** |
| `MaxChargeMultiplier` | float | 2.0 | 최대 차징 시 크기·데미지 배율 |
| `BaseScale` | float | 1.0 | 구체 기본 크기 배율 (BP의 크기 1.0 기준) |
| `PierceCount` | int32 | 0 | 0=관통 없음, N=N명까지, **-1=무한** |
| `SkillTags` | FGameplayTagContainer | 비어 있음 | 카드 태그와 같은 축(`Tree.Fire` 등) |
| `ProjectileClass` | TSubclassOf\<AGJProjectile\> | nullptr | 비어 있으면 컴포넌트의 기본 클래스 |

`BaseRadius`(cm)가 아니라 `BaseScale`(배율)인 이유: 반지름을 cm로 주면 콜리전은 맞출 수 있어도 메시는 원본 크기를 알아야 비율을 계산할 수 있다. BP에서 "크기 1.0 기준"으로 만들어두고 `SetActorScale3D`를 걸면 콜리전과 메시가 항상 함께 커진다.

### `FCharacterStat` / `FStatValues` — `SkillPower` 추가

`FStatValues`는 `FCharacterStat`과 같은 필드 집합을 **전부 0 기본값**으로 갖는 구조라, 양쪽에 같이 추가해야 카드로 스킬 공격력을 올릴 수 있다. 9개 → 10개 필드가 된다.

**연쇄로 고쳐야 하는 곳** (하나라도 빠지면 조용히 깨진다):

| 위치 | 안 고치면 |
|---|---|
| `FCharacterStat` | 테이블에서 스킬 공격력을 못 준다 |
| `FStatValues` | 카드로 스킬 공격력을 못 올린다 |
| `FStatValues::operator+=` | 보너스가 합산되지 않는다 |
| `AGJCharacter::RecalculateStats` | 실효값이 갱신되지 않는다 (음수 클램프 포함) |
| `AGJCharacter::GJAddBonus` | 콘솔로 시험할 수 없다 |
| `UGJCardComponent::IsStatEffectEmpty` | **스킬 공격력만 올리는 카드가 빈 카드로 걸러져 뽑기에서 사라진다** |
| `Data/DT_CharacterStat.csv` | 리임포트 시 칼럼 누락 오류 |

### `FCardData` — `SkillId` 추가

| 필드 | 타입 | 설명 |
|---|---|---|
| `SkillId` | FName | `EffectType == Ability`일 때만 쓰임. `DT_SkillData`의 행 이름 |

`SkillId`가 비었거나 테이블에 없는 `Ability` 카드는 **뽑기 후보에서 제외**한다. `WeaponClass`가 빈 `GrantWeapon` 카드를 거르는 것과 같은 이유다 — 골라도 아무 일이 없어 플레이어가 손해를 본다.

## 아키텍처

```
AGJCharacter
  └─ UGJSkillComponent  (생성자에서 부착, CardComponent 옆)
       ├─ SkillTable            DT_SkillData
       ├─ EquippedSkills[3]     슬롯별 스킬 ID (FName)
       ├─ CooldownEndTime[3]    슬롯별 쿨타임 종료 시각
       ├─ ChargeStartTime       차징 시작 시각 (-1 = 차징 안 함)
       ├─ ChargingSlot          지금 차징 중인 슬롯 (INDEX_NONE = 없음)
       ├─ MuzzleOffset          발사 위치 오프셋 (EditDefaultsOnly)
       ├─ DefaultProjectileClass
       └─ ProjectilePool        AGJProjectile 풀 (무기 풀과 별개, 기본 크기 10)
```

풀 크기가 무기(30)보다 작은 이유: 스킬은 쿨타임이 있어 동시에 떠 있을 수 있는 구체 수가 훨씬 적다. 부족하면 그 발사만 무시되므로(무기 풀과 동일한 동작) 안전한 실패다.

**스킬마다 `ProjectileClass`가 다르면 풀 하나로는 안 된다.** 클래스별로 풀을 나눠 `TMap<UClass*, TArray<AGJProjectile*>>`로 들고, 처음 쓰는 클래스를 만나면 그때 만든다. 스킬 하나뿐인 지금은 항목이 하나지만, 두 번째 스킬에 다른 비주얼을 주는 순간 필요해지고 그때는 풀 구조를 통째로 바꿔야 한다.

| 누가 | 무엇을 |
|---|---|
| `AGJCharacter` | `IA_Skill1/2/3`의 Started/Completed를 컴포넌트로 넘기기만 한다. **스킬을 모른다** |
| `UGJSkillComponent` | 슬롯·쿨타임·차징·MP 검사·발사. `DT_SkillData`를 읽는다 |
| `AGJProjectile` | `FireInDirection`에 크기 배율과 관통 횟수가 추가된다 |
| `UGJCardComponent` | `Ability` 카드에서 `EquipSkill(SkillId)`를 부르고, 슬롯이 다 찼으면 교체 선택지를 띄운다 |

발사 위치는 무기의 `MuzzleSocket`이 아니라 **컴포넌트의 `MuzzleOffset`**(캐릭터 기준 전방·상방)이다. 스킬은 맨손이어도 나가야 한다. `EditDefaultsOnly`라 BP에서 컴파일 없이 조절된다.

발사 방향은 `Character->GetActorForwardVector()`다. 마우스 조준이 이미 캐릭터를 커서 쪽으로 돌려놓기 때문에 별도 계산이 필요 없다(무기와 동일).

## 흐름

```
Skill 입력 눌림 (Started, 슬롯 N)
  ├ 슬롯이 비었나            → 무시 (로그 없음, 매 클릭 스팸됨)
  ├ 쿨타임 중인가            → 무시
  ├ MP 부족한가              → 무시
  ├ 이미 다른 슬롯 차징 중인가 → 무시
  ├ ChargeTime <= 0          → 즉시 발사 (배율 1.0)
  └ 그 외                    → ChargeStartTime 기록

Skill 입력 뗌 (Completed, 슬롯 N)
  ├ 그 슬롯 차징 중이 아니면 → 무시
  ├ 비율 = clamp(경과 / ChargeTime, 0, 1)
  ├ 배율 = 1 + (MaxChargeMultiplier - 1) x 비율
  ├ MP 재확인 (차징 중 재장전으로 빠졌을 수 있다) → 부족하면 취소
  └ 발사 → MP 차감 → 쿨타임 시작 → 차징 해제
```

## 스킬 슬롯 교체 (무기 교체 화면 재사용)

슬롯이 다 차 있어도 스킬 카드는 **정상적으로 뽑히고 정상적으로 획득된다.** 고르면 "어느 스킬을 버릴지" 2단계로 묻는다 — 무기와 완전히 같은 흐름이다.

`UGJCardSelectWidget`은 `FGJChoiceEntry` 목록을 받아 **인덱스만** 돌려주므로 새 위젯이 필요 없다. `EGJChoiceMode`에 값 하나만 더한다:

```cpp
enum class EGJChoiceMode : uint8
{
    None,
    Card,           // 인덱스 = 뽑힌 카드 목록의 위치
    WeaponReplace,  // 인덱스 = 버릴 무기 슬롯 번호
    SkillReplace    // 인덱스 = 버릴 스킬 슬롯 번호  <- 추가
};
```

```
Ability 카드 선택
  ├ 빈 슬롯이 있으면 → 첫 빈 슬롯에 장착하고 끝 (return true)
  └ 다 찼으면
       ├ PendingSkillId 보관, CurrentMode = SkillReplace
       ├ 슬롯 3개의 DisplayName/Description/Icon을 DT_SkillData에서 읽어 선택지로
       └ return false  (아직 안 끝났다 - 대기열을 줄이면 안 된다)

SkillReplace 선택
  └ EquippedSkills[인덱스] = PendingSkillId → 대기열 감소 → 다음 선택지
```

**무기 교체에서 배운 두 가지를 그대로 적용한다:**

1. **이 단계에서 대기열(`PendingChoices`)을 줄이지 않는다.** 먼저 줄이면 연속 레벨업 중에 카드 한 장이 통째로 증발한다.
2. **스택 불가 카드는 이 분기 안에서 따로 `TakenCards`에 기록한다.** 공통 기록 지점은 `ApplyCard` 맨 아래인데 교체 분기는 `return false`로 그 전에 빠져나간다. 안 하면 같은 스킬 카드가 계속 다시 뜬다.
3. **선택지 화면 생성이 실패하면** 슬롯 선택을 포기하고 **슬롯 0을 덮어쓴다.** 카드 3장이 뜬 채 모드만 바뀌어 엉뚱한 인덱스가 슬롯으로 해석되는 상태보다 낫다.

**슬롯 선택은 곧 키 선택이다.** 슬롯 0/1/2가 각각 우클릭/Q/R이므로, 플레이어는 "어느 스킬을 버릴지"와 동시에 "새 스킬을 어느 키에 놓을지"를 정하는 셈이다. 선택지 설명에 그 키를 같이 보여준다(예: `우클릭 - 파이어볼을 버리고 교체`).

빈 슬롯이 있을 때 묻지 않고 첫 빈 슬롯에 넣는 것도 무기와 같다. 매번 물으면 클릭이 늘기만 한다.

## 관통

`AGJProjectile::OnHit`에서 남은 관통 횟수가 있으면 `Deactivate()` 대신 카운트만 줄이고 계속 날아간다.

**`PierceCount = -1`(무한)일 때는 카운트를 줄이지 않는다.** 줄이면 -2, -3으로 내려가다 "남았는지" 판정을 어떻게 쓰느냐에 따라 무한이 아니게 되거나 영원히 안 사라진다. 판정은 `남은 횟수 != 0`이고, -1은 감소 대상에서 제외한다. 이 경우 구체는 `Range`를 다 날아가야 사라진다.

**같은 적을 다시 때리지 않도록 `TSet<AActor*> HitActors`를 들고 다닌다.** 없으면 큰 구체가 한 적의 콜리전 안에 머무는 동안 프레임마다 재타격한다.

`Deactivate()`에서 `HitActors.Reset()`과 관통 횟수 복원을 반드시 한다. **풀로 돌아가는 객체라 안 지우면 다음 발사가 그 적을 못 때린다.**

## 엣지 케이스

| 상황 | 처리 |
|---|---|
| 레벨업 카드 화면이 뜸 | `CancelCharge()` — `StopAutoFire()` 옆에 나란히. 안 하면 차징이 눌린 채 굳는다 |
| 차징 중 사망 | `CancelCharge()`, MP 소모 없음 |
| 차징 중 회피 | `CancelCharge()`, MP 소모 없음 |
| 차징 중 MP 소진(재장전은 막히지만 다른 경로 대비) | 뗄 때 재확인해서 취소 |
| 스킬 미장착 우클릭 | 조용히 무시 |
| `SkillTable`이 비어 있음 | 경고 1회, 스킬 시스템 전체가 조용히 꺼짐 |
| `SkillId`가 테이블에 없음 | 뽑기에서 제외됨. 그래도 들어오면 `EquipSkill`이 경고 후 장착하지 않음 |
| 슬롯이 다 참 | **버릴 스킬을 고르는 2단계 화면** (무기 교체와 동일) |
| 교체 화면 생성 실패 | 슬롯 0을 덮어씀 + 경고. 엉뚱한 인덱스가 슬롯으로 해석되는 상태보다 낫다 |
| 이미 장착한 스킬을 또 주는 카드 | 교체 화면이 뜬다. 스킬 카드는 `bStackable=false`가 기본이라 같은 카드는 다시 안 나오지만, 같은 스킬을 주는 **다른** 카드가 있으면 발생 가능 |
| `ESkillType::Persistent` | 발사 시도 시 경고만 (`ECardEffectType::Ability`와 같은 취급) |

## 검증

테스트 스위트가 없으므로 **컴파일 통과 + 수동 PIE 확인**이다.

**개발용 콘솔 명령** (`AGJCharacter`의 `UFUNCTION(Exec)`, 몸통은 컴포넌트 —
컴포넌트에 직접 `Exec`를 달면 콘솔이 못 찾는다):

- `GJEquipSkill <스킬ID> [슬롯]` — 카드를 안 거치고 장착
- `GJSkillInfo` — 슬롯별 장착 스킬, 쿨타임 잔량, 현재 MP, 차징 상태

**PIE 확인 항목**
1. 우클릭 톡 → 작은 구체, 꾹 → 큰 구체 (크기와 데미지가 같이 커짐)
2. MP가 정확히 `MPCost`만큼 줄고, 부족하면 안 나감
3. 쿨타임 중 우클릭이 안 먹음
4. 차징 중 좌클릭·R·1·2·Tab이 전부 무시됨
5. 차징 중 회피 → 구체가 안 나가고 MP도 안 줄음
6. `PierceCount=2`로 적 3마리를 일렬로 세우면 2마리만 맞고 소멸
7. 같은 적이 한 발에 두 번 맞지 않음
8. 연속 발사 시 관통 상태가 이전 발사에 오염되지 않음
9. 차징 중 레벨업 → 카드 화면이 뜨고 차징이 취소됨
10. `Card_Fireball`을 고르면 실제로 장착되고 경고가 안 뜸
11. `GJEquipSkill`로 슬롯 3개를 채운 뒤 스킬 카드를 고르면 **버릴 스킬 3개가 뜨고**, 고른 슬롯이 정확히 교체됨
12. 연속 레벨업 중 스킬 교체가 껴도 남은 카드 선택 횟수가 줄지 않음 (레벨 3번 오르면 카드 화면 총 3번)
13. 스택 불가 스킬 카드가 교체 경로를 탄 뒤 다시 뽑히지 않음

## 알려진 갭 (이번 범위 밖)

- `IA_Skill3`과 `IA_Reload`가 **둘 다 R키**에 매핑돼 있다. Skill3을 구현하면 R을 누를 때 재장전과 스킬이 같이 나간다. Skill3 작업 시 정리 필요
- 스킬 쿨타임·슬롯을 보여주는 HUD가 없다. 어떤 스킬이 어느 키에 있는지 확인하려면 콘솔(`GJSkillInfo`)을 봐야 한다 — 슬롯 교체가 가능해진 만큼 이게 실제로 불편해질 것이다
- 시전 애니메이션·차징 이펙트·발사 이펙트가 없다
- `ESkillType::Persistent` 미구현
