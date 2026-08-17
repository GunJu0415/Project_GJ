# 스킬 상태 표시 설계

**작성일**: 2026-08-17
**범위**: HUD 스킬 아이콘 3개 + 쿨타임 차오름, 인벤토리 스킬 페이지(드래그 교환), 차징 중 구체 미리보기.
**전제**: M2.7 액티브 스킬 시스템(`UGJSkillComponent`, `DT_SkillData`)이 이미 동작한다.

## 목표

지금은 어느 키에 무슨 스킬이 있는지, 쿨타임이 얼마나 남았는지 **콘솔(`GJSkillInfo`)로만** 알 수 있다. 슬롯 교체가 가능해진 만큼 이건 실제로 불편하다. 화면에서 보이게 만든다.

## 범위

**만드는 것**
- `UGJSkillIconWidget` — HUD 아이콘 하나 (아이콘 + 키 라벨 + 쿨타임 차오름)
- `UGJSkillSlotWidget` — 인벤토리 스킬 칸 하나 (드래그로 슬롯 교환)
- `AGJCharacter::ChargeOrbMesh` — 차징 중 총구 앞에서 커지는 구체
- 컴포넌트 API: `GetCooldownRatio`, `OnSkillSlotsChanged`, `SwapSkillSlots`

**안 만드는 것**
- MP 부족 시 아이콘 흐리게 표시 — 쿨타임과 표시가 겹쳐 헷갈린다. 필요해지면 나중에
- 스킬 아이콘 아트 — 기존 텍스처를 자리표시자로 쓴다
- 차징 진행도 바 — 구체 크기 자체가 진행도를 보여준다
- 시전 애니메이션, 발사 이펙트, 사운드

## 결정 사항과 근거

### 갱신 경로를 둘로 나눈다

| 무엇 | 어떻게 | 왜 |
|---|---|---|
| 슬롯 내용(아이콘, 이름) | `OnSkillSlotsChanged` 델리게이트 | 가끔 바뀐다. 매 프레임 테이블을 다시 읽을 이유가 없다 |
| 쿨타임 비율 | 위젯 `NativeTick`에서 `GetCooldownRatio` 폴링 | 매 프레임 바뀐다 |

**위젯이 틱하는 것은 컴포넌트의 "틱 안 쓰기" 결정과 충돌하지 않는다.** 컴포넌트는 여전히 시각 비교만 하고, 그 결과를 물어보는 쪽이 UI일 뿐이다. UMG에서 위젯 틱은 정상적인 방식이고 아이콘 3개는 비용이 없다.

### C++은 비율만 주고, 그리는 방법은 WBP가 정한다

```cpp
void UGJSkillIconWidget::SetCooldownRatio(float Ratio);  // 0~1
```

시계방향 차오름은 **방사형 마스크 머티리얼**이 필요하고, 아래서 위로는 `ProgressBar` 하나로 끝난다. 이 경계로 자르면 **방사형을 먼저 시도해보고 실패해도 C++이 한 줄도 안 바뀐다.** 나중에 방사형으로 올릴 때도 WBP만 고치면 된다.

구현 순서: 방사형 머티리얼을 MCP로 시도 → 안 되면 `ProgressBar`(`BarFillType = BottomToTop`)로 진행.

### 슬롯 교환은 쿨타임도 같이 옮긴다

**이걸 놓치면 교환이 쿨타임 초기화 수단이 된다.** 파이어볼을 쓰고 인벤토리를 열어 Q 칸으로 옮기면 쿨타임이 사라지는 식이다.

`SwapSkillSlots(A, B)`는 `EquippedSkills`와 `CooldownEndTime`을 **함께** 맞바꾼다.

카드로 새 스킬을 받을 때(`EquipSkillInSlot`)는 반대로 쿨타임을 0으로 **초기화한다** — 그건 없던 스킬이 새로 들어오는 것이라 이전 쿨타임을 물려받을 이유가 없고, "교체가 손해가 되면 안 된다"는 원칙에도 맞는다. 둘은 다른 동작이므로 다른 함수다.

### 차징 구체의 메시는 `ProjectileClass` CDO에서 읽는다

`FSkillData`에 별도 필드를 두지 않는다. **차징 중 보이는 구체와 실제로 날아가는 구체가 자동으로 같아지기** 때문이다. 따로 두면 언젠가 둘이 어긋나고, 데이터를 고칠 때까지 아무도 모른다.

`AGJProjectile`에 메시 getter 하나만 연다:
```cpp
FORCEINLINE UStaticMeshComponent* GetMeshComp() const { return MeshComp; }
```

### 차징 구체는 액터가 아니라 캐릭터의 컴포넌트다

스폰/파괴가 필요 없고, 캐릭터를 따라다니는 게 공짜다. 위치는 캐릭터 기준 `MuzzleOffset` — **구체가 실제로 발사되는 바로 그 자리**라 "여기서 나가겠구나"가 그대로 읽힌다.

`UGJSkillComponent`는 `UActorComponent`라 트랜스폼이 없어서 자식 메시를 못 단다. 그래서 메시는 `AGJCharacter`가 소유하고 스킬 컴포넌트가 조작한다.

### 틱은 차징 중에만 켠다

`SetComponentTickEnabled(true)`를 차징 시작에, `false`를 발사·취소에 건다. 평소엔 여전히 틱이 안 돌아 원래 설계 의도가 유지된다.

구체 크기는 발사와 **같은 공식**을 쓴다: `BaseScale × (1 + (MaxChargeMultiplier - 1) × 비율)`. 그래야 미리보기 크기가 실제 발사 크기와 일치한다.

## 컴포넌트 API 추가

```cpp
// UGJSkillComponent

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSkillSlotsChangedSignature);

// 슬롯 내용이 바뀔 때마다 방송. HUD와 인벤토리가 구독한다.
UPROPERTY(BlueprintAssignable, Category = "Skill")
FOnSkillSlotsChangedSignature OnSkillSlotsChanged;

// 0 = 준비됨, 1 = 방금 썼음. 빈 슬롯도 0.
UFUNCTION(BlueprintPure, Category = "Skill")
float GetCooldownRatio(int32 SlotIndex) const;

// 두 슬롯의 스킬과 쿨타임을 함께 맞바꾼다.
UFUNCTION(BlueprintCallable, Category = "Skill")
void SwapSkillSlots(int32 SlotA, int32 SlotB);

// 슬롯 키 이름("우클릭"/"Q"/"F"). UI가 라벨에 쓴다.
UFUNCTION(BlueprintPure, Category = "Skill")
static FText GetSlotKeyLabel(int32 SlotIndex);
```

`GetSlotKeyLabel`을 `static`으로 두는 이유: 지금 키 이름이 `LogSkillInfo`와 카드 교체 화면(`GJCardComponent`) 두 곳에 각각 하드코딩돼 있다. HUD와 인벤토리까지 더하면 네 곳이 된다. **한 곳으로 모은다.**

## HUD 스킬 바

```
WBP_PlayerHUD
  RootCanvas
    StatusRow (HorizontalBox)
      PortraitBox
      StatusBox (VerticalBox)
        HPBar
        MPBar
        EXPBar
        SkillRow (HorizontalBox)     <- 신규
          SkillIcon1  (우클릭)
          SkillIcon2  (Q)
          SkillIcon3  (F)
```

`UGJPlayerHUDWidget`에 `BindWidgetOptional`로 `SkillIcon1/2/3`을 추가한다. EXP 바와 같은 이유로 Optional이다 — strict로 두면 C++이 먼저 들어간 순간 WBP 컴파일이 깨져 에디터 작업 전까지 게임이 안 돈다.

### `UGJSkillIconWidget`

| 바인딩 | 타입 | 필수 | 용도 |
|---|---|---|---|
| `IconImage` | `UImage` | strict | 스킬 아이콘. 빈 슬롯이면 숨김 |
| `KeyText` | `UTextBlock` | Optional | "우클릭" / "Q" / "F" |
| `CooldownBar` | `UProgressBar` | Optional | 아래서 위로 차오르는 방식일 때 |
| `CooldownImage` | `UImage` | Optional | 방사형 머티리얼 방식일 때 |

**쿨타임 표시를 두 개로 나눠 받는 이유:** 하나를 `UWidget`으로 받으면 매번 캐스팅해야 하고, `ProgressBar`는 `SetPercent`, 머티리얼 `Image`는 스칼라 파라미터라 호출이 아예 다르다. **둘 다 Optional로 두고 붙어 있는 쪽만 갱신하면**, WBP에서 어느 방식을 쓰든 C++이 그대로다.

```cpp
// 무기 슬롯 위젯과 같은 진입점. 슬롯 번호를 기억하고, 델리게이트를 구독하고, 즉시 한 번 그린다.
void SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter);

protected:
    void RefreshSkill();               // 델리게이트가 부른다 (아이콘/키 라벨)
    void SetCooldownRatio(float Ratio); // NativeTick이 부른다
    virtual void NativeTick(const FGeometry&, float) override;
```

**빈 슬롯도 키 라벨은 보여준다.** 아이콘만 숨기고 칸은 남긴다 — 그래야 "F 자리가 비어 있다"는 걸 안다.

### 누가 `SetSlotData`를 부르나

`UGJPlayerHUDWidget::InitializeSkillIcons(AGJCharacter*)`가 아이콘 3개에 각각 `SetSlotData(0/1/2, 캐릭터)`를 부른다. 인벤토리의 `InitializeInventory`와 같은 모양이고, 캐릭터가 HUD를 만드는 자리에서 한 번 호출한다.

아이콘 위젯이 직접 `GetOwningPlayerPawn()`을 캐스팅하지 않는 이유: 위젯 생성 시점에 폰이 아직 없을 수 있고, 그러면 조용히 아무것도 안 그리는 상태로 굳는다. 명시적으로 넘겨주면 그 실패가 없다.

## 인벤토리 스킬 페이지

무기 페이지와 완전히 같은 패턴이다.

```cpp
// UGJInventoryWidget
UPROPERTY(meta = (BindWidgetOptional))
UGJSkillSlotWidget* SkillSlotWidget1;   // 2, 3도 동일

void RefreshSkillSlots();   // RefreshWeaponSlots와 같은 자리에서 호출
```

### `UGJSkillSlotWidget`

`UGJWeaponSlotWidget`을 그대로 따른다:
- `SetSlotData(int32 SlotIndex, AGJCharacter* OwningCharacter)`
- `NativeOnDragDetected` → `UGJSkillSlotDragOp`(출발 인덱스만 담음)
- `NativeOnDrop` → `SwapSkillSlots(출발, 도착)`
- 바인딩: `IconImage`(strict), `KeyText`(Optional)

**클릭에는 아무 동작도 없다.** 무기 칸은 클릭이 "장착"이지만 스킬은 세 개가 항상 활성이라 장착이라는 개념이 없다. 드래그만 받는다.

## 차징 구체

```cpp
// AGJCharacter (생성자)
ChargeOrbMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChargeOrbMesh"));
ChargeOrbMesh->SetupAttachment(RootComponent);
ChargeOrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
ChargeOrbMesh->SetVisibility(false);
```

캐릭터는 접근자만 연다(`GetChargeOrbMesh()`). **켜고 끄고 키우는 것은 전부 스킬 컴포넌트가 한다** — 캐릭터는 스킬을 모른다는 원칙 그대로다.

```
차징 시작
  ├ ProjectileClass(없으면 DefaultProjectileClass) CDO에서 메시·머티리얼을 읽어 설정
  ├ 상대 위치 = MuzzleOffset
  ├ 스케일 = BaseScale x 1.0
  ├ 보이게 + 컴포넌트 틱 켜기
매 틱
  └ 스케일 = BaseScale x (1 + (MaxChargeMultiplier - 1) x 현재비율)
발사 / 취소
  └ 숨기고 틱 끄기
```

메시를 못 찾으면(클래스가 비었거나 CDO 메시가 없음) **연출만 건너뛰고 발사는 정상 동작한다.** 시각 요소가 게임플레이를 막으면 안 된다.

## 엣지 케이스

| 상황 | 처리 |
|---|---|
| 빈 슬롯 | 아이콘 숨김, 키 라벨은 유지, 쿨타임 비율 0 |
| 슬롯 교환으로 쿨타임 회피 | `SwapSkillSlots`가 쿨타임도 함께 교환 |
| 교환 중 차징 | 인벤토리를 여는 순간 차징이 취소되므로 발생 불가. `SwapSkillSlots`는 그래도 인덱스 유효성만 검사 |
| 차징 중 사망/회피/카드 화면 | 기존 `CancelCharge` 경로에서 구체도 함께 숨김 |
| `ProjectileClass`가 비어 있음 | 구체 연출 생략, 발사는 정상 |
| HUD 위젯이 없는 상태 | `BindWidgetOptional`이라 널 체크로 통과 |
| 스킬 컴포넌트를 못 찾음 | 위젯이 조용히 아무것도 안 그린다 |

## 검증

전부 PIE 수동 확인이다.

1. `GJEquipSkill Skill_Fireball` → **HUD 첫 칸에 아이콘이 뜨고** 나머지 두 칸은 키 라벨만 보인다
2. 우클릭 발사 → **첫 칸이 가득 찼다가 3초에 걸쳐 비워진다**
3. 쿨타임 중 우클릭 → 안 나가고, 표시도 계속 줄어든다
4. `GJEquipSkill Skill_Fireball 2` → **세 번째 칸(F)에 아이콘이 즉시 나타난다** (델리게이트 동작)
5. Tab → 스킬 페이지에 칸 3개, 첫 칸에 아이콘
6. 파이어볼을 **Q 칸으로 드래그** → 자리가 바뀌고 **HUD도 즉시 반영**된다
7. 파이어볼을 쓴 직후(쿨타임 중) 인벤토리에서 다른 칸으로 옮기고 닫기 → **쿨타임이 그대로 남아 있다** (초기화되면 버그)
8. 우클릭을 누르고 있으면 **캐릭터 앞에 구체가 나타나 점점 커진다**
9. 뗐을 때 **날아가는 구체 크기가 방금 보던 크기와 같다**
10. 회피로 차징 취소 → **구체가 사라진다**
11. 차징 중 레벨업 → 카드 화면이 뜨고 구체가 사라진다

## 알려진 갭 (이번 범위 밖)

- 쿨타임 표시가 방사형이 아니라 아래서 위로 차오르는 형태일 수 있다(머티리얼 저작 결과에 따라). C++ 변경 없이 나중에 교체 가능
- 스킬 아이콘이 자리표시자 텍스처다
- MP가 부족할 때 아이콘에 표시가 없다 — 눌러도 안 나가는 이유를 화면에서 알 수 없다
- 차징 구체에 이펙트·머티리얼 연출이 없다. 발사될 구체의 메시를 그대로 키울 뿐이다
- 인벤토리 스킬 칸에 마우스를 올려도 설명 툴팁이 없다
