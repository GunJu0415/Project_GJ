# 전투 스탯과 데미지 공식 — 설계 문서

> 작성일: 2026-08-08
> 대상: Project_GJ (UE 5.8, C++ 우선 + 얇은 블루프린트 레이어)
> 상태: 승인됨, 구현 계획 작성 대기
> 선행: M1 런 루프 (`2026-08-08-run-loop-design.md`) 완료

---

## 1. 배경과 문제

현재 이 게임에는 **데미지 계산이라는 것이 없다.** 공격자가 넘긴 숫자가 그대로 HP에서 빠진다.

- `AGJBaseCharacter::TakeDamage()`는 받은 값을 그대로 `CurrentHP`에서 차감한다. 경감도, 치명타도, 어떤 변형도 없다.
- 데미지 값은 투사체가 `WeaponStat.BaseDamage`, 적이 `EnemyStat.AttackDamage`를 그대로 쓴다. **캐릭터 스탯을 전혀 거치지 않는다.**
- `FCharacterStat::BaseAttackPower`는 선언되어 있지만 **코드에서 한 번도 읽히지 않는다.**
- 플레이어의 이동 속도는 **어디에서도 설정하지 않는다.** `UCharacterMovementComponent`의 엔진 기본값(600)을 그대로 쓰고 있다. 적만 `ApplyEnemyStat()`에서 `MaxWalkSpeed`를 설정한다.

그 결과 캐릭터를 성장시키거나 적의 강함을 조절할 수단이 무기 데이터 하나뿐이다. M2(인런 성장, EXP/레벨업)를 만들려면 **레벨이 올랐을 때 실제로 무엇이 강해지는지**가 먼저 정의되어야 한다.

**이 작업의 목표는 방어력·이동속도·쿨타임감소·치명타 스탯을 추가하고, 이들을 엮는 데미지 공식을 한 곳에 정의하는 것이다.**

---

## 2. 범위

### 포함

- `AGJBaseCharacter`에 공용 전투 스탯 추가 (방어력, 치명타 확률/배율)
- `FCharacterStat`에 방어력·이동속도·쿨타임감소·치명타 추가
- `FEnemyStat`에 방어력·치명타 추가
- 데미지 공식을 담는 `UGJCombatStatics` 신설
- 기존 데미지 경로(플레이어 사격, 적 근접 공격, `TakeDamage`)에 공식 연결
- 플레이어 이동 속도를 데이터 테이블 값으로 실제 적용

### 제외

| 항목 | 사유 |
|---|---|
| EXP·레벨업 | M2. 이 작업은 M2가 조작할 **대상**을 만드는 것이다 |
| 쿨타임 감소의 실제 적용 | 스킬 시스템이 아직 없다. 필드만 추가한다(3.3절 참고) |
| 치명타 표시 UI(데미지 폰트 등) | 요청 범위 밖. 다만 4.4절에 확장 지점을 명시한다 |
| 근접 무기 히트 판정 | M7. 여전히 원거리만 데미지를 준다 |

---

## 3. 스탯 배치

### 3.1 `AGJBaseCharacter` — 플레이어·적 공용 런타임 스탯

`TakeDamage()`가 플레이어든 적이든 **동일하게** 방어력을 적용해야 하므로, 방어력은 공용 베이스에 있어야 한다. 이미 `MaxHP`/`CurrentHP`가 쓰는 방식과 같다 — 베이스에 선언하고 각자 자기 데이터 테이블에서 채운다.

| 필드 | 타입 | 기본값 | 용도 |
|---|---|---|---|
| `Defense` | `float` | 0 | 받는 데미지 경감. `TakeDamage`가 읽는다 |
| `CritChance` | `float` | 0 | 치명타 확률 (0.0~1.0) |
| `CritMultiplier` | `float` | 2.0 | 치명타 시 데미지 배율 |

### 3.2 `FCharacterStat` — 플레이어 (`DT_CharacterStat`, 행 = 레벨)

| 필드 | 기본값 | 비고 |
|---|---|---|
| `Defense` | 0 | |
| `MoveSpeed` | 600 | 엔진 기본값과 같은 값을 명시적 기본으로 둔다. 지금까지 암묵적으로 쓰던 값을 데이터로 끌어올리는 것이므로 **기존 플레이 감각이 바뀌지 않는다** |
| `CooldownReduction` | 0 | 스킬 시스템 전까지 미사용 (3.3절) |
| `CritChance` | 0 | |
| `CritMultiplier` | 2.0 | |

### 3.3 쿨타임 감소를 지금 넣는 이유

`CooldownReduction`은 **이번 작업에서 어디에도 연결되지 않는다.** 적용 대상이 될 스킬 시스템이 아직 없기 때문이다.

미사용 필드를 늘리는 것은 일반적으로 피해야 하지만(`RequiredEXP`, `BaseAttackPower`가 이미 그런 상태였다), 여기서는 지금 넣는 편이 낫다고 판단했다. 이 프로젝트에서 `USTRUCT` 레이아웃 변경은 **실제 비용이 있다** — 해당 구조체를 참조하는 UMG 블루프린트 그래프의 핀이 깨져 에디터 재시작이 필요했던 이력이 개발 가이드에 기록되어 있고, 데이터 테이블의 기존 행에도 새 칸을 채워야 한다. 스탯 4개를 한 번에 넣으면 이 마이그레이션이 **한 번으로 끝난다.**

구현 시 헤더 주석에 "스킬 시스템 전까지 미사용"을 명시하고, 개발 가이드의 미사용 필드 목록에도 올린다.

### 3.4 `FEnemyStat` — 적 (`DT_EnemyStat`)

| 필드 | 기본값 | 비고 |
|---|---|---|
| `Defense` | 0 | 적마다 단단함을 다르게 줄 수 있다 |
| `CritChance` | 0 | |
| `CritMultiplier` | 2.0 | |

적에게는 `MoveSpeed`가 이미 있고, 쿨타임 감소는 의미가 없어 추가하지 않는다.

### 3.5 적용 지점

두 곳 모두 **이미 존재하는 함수**라 새로운 흐름이 생기지 않는다.

- `AGJCharacter::UpdateCharacterStat(NewLevel)` — 지금도 레벨별로 `MaxHP`/`MaxMP`를 채운다. 여기에 `Defense`/`CritChance`/`CritMultiplier`를 채우고 `GetCharacterMovement()->MaxWalkSpeed`도 설정한다.
- `AGJEnemyCharacter::ApplyEnemyStat()` — 지금도 `MaxWalkSpeed`를 포함해 스탯을 채운다. 여기에 나머지를 추가한다.

---

## 4. 데미지 공식

### 4.1 공식

```
공격 측:  공격데미지 = 무기데미지 × (1 + 공격력/100) × 치명타배율
방어 측:  최종데미지 = 공격데미지 × 100/(100 + 방어력)
```

치명타배율은 확률로 굴려 터지면 `CritMultiplier`, 아니면 `1.0`이다.

방어력 경감은 **체감형(diminishing returns)**이다. 방어력을 아무리 올려도 100% 무효화에 도달하지 않으며, 올릴수록 추가 효율이 서서히 줄어든다.

| 방어력 | 경감률 | 무기 15 기준 실제 데미지 |
|---:|---:|---:|
| 0 | 0% | 15.0 |
| 50 | 33% | 10.0 |
| 100 | 50% | 7.5 |
| 200 | 67% | 5.0 |
| 400 | 80% | 3.0 |

방어력 100마다 "체력이 1배씩 더 있는" 것과 같은 효과다.

### 4.2 계산 책임의 분리

공격 부분은 **공격자가**, 방어 부분은 **맞는 쪽이** 계산한다. 각자 자기 스탯만 알면 되고, 무엇보다 방어력이 `TakeDamage` 한 곳에 있으므로 **앞으로 어떤 데미지 소스가 추가되어도**(장판, 도트, 폭발) 경감이 자동으로 적용된다. 공격자가 최종값까지 계산하는 방식은 데미지 소스가 늘어날 때마다 방어력 적용을 빠뜨릴 위험이 있어 채택하지 않았다.

### 4.3 `UGJCombatStatics` (신규)

```
Source/Project_GJ/GJCombatStatics.h / .cpp   — UBlueprintFunctionLibrary
```

| 함수 | 역할 |
|---|---|
| `CalculateOutgoingDamage(BaseDamage, AttackPower, CritChance, CritMultiplier, bOutWasCritical)` | 공격 측 계산 + 치명타 굴림. 치명타 여부를 out 파라미터로 돌려준다 |
| `ApplyDefense(IncomingDamage, Defense)` | 방어 측 경감 + 최소 데미지 보장 |

공격과 방어가 서로 다른 지점에서 호출되지만 **공식 자체는 이 파일 하나에 모인다.** 밸런스를 조정할 때 여기만 보면 된다.

### 4.4 흐름

```
[플레이어 사격]
AGJWeapon_Ranged::Fire()
  → CalculateOutgoingDamage(WeaponStat.BaseDamage, 캐릭터 공격력, 캐릭터 치명타 확률/배율)
  → 계산된 값을 총알에 실어 발사
  → AGJProjectile::OnHit() → ApplyDamage(대상, 계산된 데미지)

[적 근접 공격]
AGJEnemyCharacter::ApplyAttackDamage()
  → CalculateOutgoingDamage(AttackDamage, 0, 적 치명타 확률/배율)
  → ApplyDamage(플레이어, 계산된 데미지)

[공통 — 맞는 쪽]
AGJBaseCharacter::TakeDamage()
  → ApplyDefense(받은 데미지, 내 Defense)     ← 방어력은 여기 한 곳에서만 적용
  → CurrentHP 차감 → OnDamaged 브로드캐스트
```

**무기가 캐릭터 스탯을 읽는 방법**: `AGJWeapon_Ranged::Fire()`는 `GetInstigator()`로 자신을 든 캐릭터를 얻을 수 있다(`OnPickedUp`에서 `SetInstigator`를 하므로 항상 유효하다). 이를 `AGJCharacter`로 캐스팅해 공격력과 치명타 스탯을 읽는다. 다만 `CurrentCharacterStat`은 `protected`이므로 **읽기용 getter를 추가해야 한다**:

- `AGJCharacter::GetBaseAttackPower()` — `CurrentCharacterStat.BaseAttackPower` 반환
- 치명타 확률/배율은 `AGJBaseCharacter`의 `CritChance`/`CritMultiplier`가 이미 `BlueprintReadOnly`이므로 별도 getter가 필요 없다면 그대로 쓴다. 접근이 막혀 있으면 같은 방식으로 getter를 추가한다.

캐스팅이 실패하면(무기를 든 주체가 `AGJCharacter`가 아니면) 공격력 0, 치명타 0으로 처리해 무기 기본 데미지만 나가게 한다.

**적은 공격력 배율을 쓰지 않는다.** `EnemyStat.AttackDamage`가 이미 최종 공격력이므로 `AttackPower`에 0을 넘긴다(`× (1 + 0/100)` = 그대로).

**치명타 여부는 대상에게 전달되지 않는다.** `CalculateOutgoingDamage`가 `bOutWasCritical`을 돌려주지만 공격자 쪽에서만 알 수 있다. 나중에 치명타 데미지 폰트 같은 UI를 붙이려면 커스텀 `FDamageEvent`로 이 정보를 실어 보내야 한다 — 지금은 요청 범위 밖이라 만들지 않는다.

### 4.5 최소 데미지 보장

`ApplyDefense`는 경감 결과가 1.0 미만으로 내려가지 않도록 **하한을 1.0으로 둔다.**

방어력이 극단적으로 높을 때 데미지가 0.4 같은 값이 되면 사실상 무적이 되는데, 이 상태는 "왜 안 죽지?"라는 형태로 나타나 원인을 찾기 어렵다. 최소 1을 보장하면 아무리 단단해도 언젠가는 죽으므로 그런 교착이 생기지 않는다.

**단, 들어온 데미지 자체가 1보다 작으면 하한을 적용하지 않는다.** 그렇지 않으면 0.5짜리 약한 공격이 방어력을 거치면서 오히려 1로 **증가**하는 역전이 생긴다. 정확히는 하한값이 `FMath::Min(1.0f, IncomingDamage)`이며, 경감 결과가 이 값보다 작을 때만 끌어올린다. 즉 방어력은 어떤 경우에도 데미지를 늘리지 않는다.

---

## 5. 검증 방법

이 프로젝트에는 자동화된 테스트 스위트가 없다. 검증은 **컴파일 통과 + 수동 PIE 확인**이다.

1. Live Coding 컴파일 통과
2. `DT_CharacterStat`의 방어력을 0 → 100으로 바꾸고, 적에게 맞았을 때 **받는 데미지가 절반**이 되는지 (HP 바 감소 폭으로 확인)
3. `CritChance`를 1.0(100%)으로 놓고 적을 쐈을 때 **데미지가 2배**로 들어가는지 (적 체력바 감소 폭)
4. `MoveSpeed`를 300으로 바꾸면 실제로 **이동이 느려지는지**
5. `DT_EnemyStat`의 방어력을 올리면 **적이 덜 죽는지**
6. 방어력을 비정상적으로 높게(예: 10000) 넣어도 **데미지가 1은 들어가는지**(최소 데미지 보장 확인)
7. `BaseAttackPower`를 100으로 올리면 **데미지가 2배**가 되는지 (`× (1 + 100/100)`)

---

## 6. 필요한 에디터 작업

| 작업 | 비고 |
|---|---|
| `DT_CharacterStat`의 기존 행에 새 칸 값 채우기 | 구조체 필드가 늘어나므로 기존 행에 빈 칸이 생긴다 |
| `DT_EnemyStat`의 기존 행에 새 칸 값 채우기 | 위와 동일 |
| (필요 시) 에디터 재시작 | `USTRUCT` 레이아웃 변경 후 UMG 블루프린트 핀이 깨질 수 있다 |

> ⚠️ `FWeaponStat`을 참조하는 `WBP_AmmoUI`의 `Break WeaponStat` 노드가 과거에 이 문제로 깨진 이력이 있다. 이번에는 `FWeaponStat`을 건드리지 않지만, `FCharacterStat`/`FEnemyStat`을 참조하는 블루프린트가 있다면 같은 증상이 날 수 있다.

---

## 7. 후속 작업과의 관계

이 작업은 **M2(인런 성장)의 전제**다. 레벨업이 무엇을 올릴지가 여기서 정의된 스탯 집합으로 결정된다.

```
전투 스탯 + 데미지 공식  ← 이 문서
 └─ M2 인런 성장 (EXP/레벨업)   — 레벨이 오르면 이 스탯들이 오른다
      └─ M6 메타 프로그레션        — 영구 강화도 같은 스탯을 올린다

M7 근접 무기 + 히트 판정  — 새 데미지 소스가 생기지만,
                            방어력이 TakeDamage에 있으므로 자동으로 경감이 적용된다
```

`CooldownReduction`은 스킬 시스템이 생길 때 연결된다. 그 시점까지는 데이터에만 존재한다.
