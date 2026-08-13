# 카드 선택 시스템 (M2.6) 설계

## 1. 배경 / 문제

레벨업이 동작하지만(M2) **성장의 선택지가 없다.** 레벨이 오르면 데이터 테이블의 다음 행 스탯을 그대로 받을 뿐이라, 같은 런을 두 번 해도 캐릭터가 똑같아진다. 로그라이트의 재미는 매 런마다 다른 조합을 쌓는 데서 나오는데 그 축이 통째로 비어 있다.

`AGJCharacter::OnLevelUp` 델리게이트는 M2에서 이 자리를 위해 미리 만들어 뒀고, **구독자가 없어 신호만 쏘고 아무 일도 안 일어나는 상태**다. M2.5의 `AddStatBonus`도 호출자가 없다. 이 설계가 그 둘을 잇는다.

## 2. 범위

**포함**
- `FCardData` + `DT_CardData` — 카드를 데이터로 정의
- 가중 랜덤 3장 뽑기와 풀 필터링
- 레벨업 시 카드 선택 화면(일시정지 모달)
- 연속 레벨업 대기열
- 효과 적용: 스탯 보너스, 무기 획득

**제외**
- **능력 카드의 실제 동작** — 파이어볼 같은 액티브 스킬은 스킬 슬롯/쿨다운/MP 소모/입력 바인딩이 전부 없다(M2.7). 효과 종류 enum에 자리만 잡고 스텁으로 둔다
- 리롤 / 스킵 — 카드가 한 장도 없을 때의 자동 건너뛰기는 있지만, 플레이어가 선택을 거부하는 기능은 없다
- 희귀도 등급의 시각적 표현(색 테두리 등) — `Weight`로 확률만 조절한다
- 카드 선택 연출/애니메이션
- 스테이지 클리어 시 카드 지급 — 진행 구조(M5)가 생긴 뒤

## 3. 어디에 사는가 — `UGJCardComponent`

**새 액터 컴포넌트로 만들어 `AGJCharacter`에 붙인다.**

`GJCharacter.cpp`는 이미 1236줄이다. 카드 풀·뽑기·대기열·위젯 관리를 얹으면 1400줄에 가까워지고, 이미 이동/조준/닷지/콤보/무기스왑/재장전/인벤토리를 다 들고 있는 파일에 관심사가 하나 더 붙는다. 프로젝트에 `UGJInventoryComponent`, `UCharacterStateComponent` 선례가 있으므로 같은 패턴을 따른다.

경계는 이렇다: **캐릭터는 `OnLevelUp`을 쏘기만 하고 카드를 모른다.** 컴포넌트가 그걸 구독해 나머지를 전부 처리하고, 효과를 적용할 때만 캐릭터의 공개 API(`AddStatBonus`, `PickUpWeapon`, `GetWeaponInSlot`)를 부른다. 세 함수 모두 이미 `public`이라 캐릭터 쪽에 새로 열어줄 것이 없다.

컴포넌트는 `BeginPlay`에서 소유자를 `AGJCharacter`로 캐스팅해 `OnLevelUp`에 바인딩한다. 캐스팅이 실패하면(다른 액터에 잘못 붙인 경우) 경고를 찍고 아무 일도 하지 않는다.

한 번에 뽑을 장수는 `EditDefaultsOnly int32 NumCardsToDraw = 3`으로 둔다. 3이 코드에 박혀 있으면 "2장짜리 선택" 같은 조정을 코드 수정으로 해야 한다.

## 4. 데이터 구조

`FItemData`가 이미 쓰는 "타입 enum + 페이로드 필드, 안 쓰는 필드는 무시" 패턴을 따른다.

```cpp
UENUM(BlueprintType)
enum class ECardEffectType : uint8
{
    StatBonus,    // StatEffect를 쓴다
    GrantWeapon,  // WeaponClass를 쓴다
    Ability       // 미구현 - M2.7까지 자리만
};

USTRUCT(BlueprintType)
struct FCardData : public FTableRowBase   // 행 이름 = 카드 ID
{
    FText DisplayName;
    FText Description;
    UTexture2D* Icon;

    ECardEffectType EffectType = ECardEffectType::StatBonus;

    // EffectType == StatBonus일 때 AddStatBonus로 넘어간다.
    // M2.5에서 FStatModifier를 BlueprintType + EditAnywhere로 만든 게 이걸 위한 것이다.
    FStatModifier StatEffect;

    // EffectType == GrantWeapon일 때 스폰할 무기 클래스
    TSubclassOf<AGJWeaponBase> WeaponClass;

    // false면 한 번 고른 뒤 풀에서 영구 제외된다. 무기나 고유 효과용.
    bool bStackable = true;

    // 가중 랜덤의 가중치. 0 이하면 절대 안 뽑힌다(카드를 임시로 끄는 용도로 쓸 수 있다).
    float Weight = 1.f;
};
```

## 5. 뽑기 규칙

전체 행에서 **가중 랜덤으로 최대 `NumCardsToDraw`장**, 한 번의 뽑기 안에서는 중복 없음(비복원 추출).

구현은 누적합 방식이다: 후보들의 `Weight` 총합을 구하고 `[0, 총합)` 구간의 난수를 뽑아 누적합이 그 값을 넘는 첫 카드를 고른다. 고른 카드는 **후보 목록에서 제거하고 총합을 다시 계산**한 뒤 다음 장을 뽑는다. 총합을 갱신하지 않으면 이미 뽑힌 카드의 가중치가 구간에 남아 뽑기가 실패하는 구멍이 생긴다.

**제외 조건** — 뽑기 후보를 만들 때 다음을 걸러낸다:

| 조건 | 이유 |
|---|---|
| `Weight <= 0` | 임시로 꺼둔 카드 |
| `bStackable == false`이고 이미 먹은 카드 | 중복 방지 |
| `GrantWeapon`인데 무기 슬롯 2개가 다 참 | 안 걸러내면 "주우면 들고 있던 걸 바닥에 떨구는" 동작이 카드 선택에서 튀어나온다. 카드는 이득이어야지 교환이면 안 된다 |
| `GrantWeapon`인데 `WeaponClass`가 비어 있음 | 데이터 미입력 |
| `StatBonus`인데 `StatEffect`가 전부 0 | 데이터 미입력. 골라도 아무 일이 없어 플레이어가 손해를 본다 |

**후보가 3장 미만이면 있는 만큼만** 보여준다. **0장이면 카드 화면을 아예 건너뛴다** — 빈 창이 떠서 진행이 막히면 안 된다.

먹은 카드는 `bStackable == false`인 경우에만 `TSet<FName> TakenCards`에 기록한다. 스택 가능한 카드는 기록할 이유가 없다.

## 6. 흐름 제어 — 대기열

M2 검증에서 확인했듯 **킬 한 번에 레벨 2→5**가 실제로 일어난다. `AddEXP`의 `while` 루프가 `OnLevelUp`을 4번 연달아 동기적으로 쏜다. 카드 화면을 한 번만 띄우면 플레이어가 보상 3번을 잃는다.

```
OnLevelUp (연속 N번)
  → PendingChoices++
  → 위젯이 안 떠 있으면 ShowNextChoice()

ShowNextChoice()
  PendingChoices == 0 → 일시정지 해제, FInputModeGameOnly(ConsumeCaptureMouseDown=false), 종료
  카드 뽑기
    0장 → PendingChoices--, 다시 ShowNextChoice()   (재귀가 아니라 루프로 구현: 풀이 비면 무한 재귀가 된다)
  위젯 생성/표시 + SetGamePaused(true) + FInputModeUIOnly(SetWidgetToFocus) + bIsAutoFiring=false

카드 클릭
  → 효과 적용 → PendingChoices-- → 위젯 RemoveFromParent → ShowNextChoice()
```

`OnLevelUp`이 여러 번 들어와도 **카운터만 올라간다.** 첫 브로드캐스트에서 위젯이 뜨고 게임이 멈추지만, `AddEXP`의 루프는 동기적으로 계속 돌면서 카운터를 4까지 올린다. 재진입 문제가 없다.

**인벤토리 모달에서 얻은 교훈 셋을 그대로 가져온다:**
- 열 때 `bIsAutoFiring = false` — 안 하면 닫은 뒤 무한 연사가 된다
- `FInputModeUIOnly` + `SetWidgetToFocus` — 포커스가 뷰포트로 새지 않게
- 닫을 때 `FInputModeGameOnly` + `SetConsumeCaptureMouseDown(false)` — 안 하면 닫은 직후 첫 클릭이 씹힌다

**인벤토리와 다른 점: 닫기 키가 없다.** 반드시 한 장 골라야 넘어간다.

**소프트락 방지**: 위젯 클래스가 비어 있거나 위젯 생성이 실패하면 **일시정지를 걸지 않고** 경고를 찍은 뒤 대기열을 비운다. 에디터 작업이 아직 안 된 상태에서 레벨업하면 게임이 멈춘 채 아무것도 안 뜨는 상황이 되는데, 그건 원인 추적이 가장 어려운 종류의 증상이다.

**일시정지와 적 파괴 타이머**: 카드 선택은 적의 `HandleDeath` 콜스택 안에서 시작된다. 적은 `DestroyDelay`(기본 2초) 타이머로 파괴되는데, 일시정지 중에는 타이머가 멈췄다가 해제 후 이어진다. 문제없지만 PIE에서 확인한다.

## 7. UI

| 클래스 | WBP | 바인딩 |
|---|---|---|
| `UGJCardSelectWidget` | `WBP_CardSelect` | `BindWidget UHorizontalBox* CardContainer`, `EditDefaultsOnly TSubclassOf<UGJCardWidget> CardWidgetClass` |
| `UGJCardWidget` | `WBP_Card` | `BindWidget`: `UImage* IconImage`, `UTextBlock* NameText`, `UTextBlock* DescText`, `UButton* SelectButton` |

카드 3개는 인벤토리 슬롯과 같이 **런타임에 `CreateWidget` + `AddChild`** 한다(`GJInventoryWidget.cpp`의 `SlotWidgetClass` 패턴). 뽑히는 장수가 1~3으로 달라져도 위젯을 고칠 필요가 없다.

`UGJCardWidget`은 `SelectButton->OnClicked`를 받아 자기 카드 ID를 담은 델리게이트를 쏘고, `UGJCardSelectWidget`이 그걸 모아 컴포넌트로 올린다. 카드 위젯은 효과를 모르고 ID만 안다.

값 갱신 로직은 전부 C++에 두고 블루프린트는 이름 맞는 위젯만 배치하는 기존 규칙(7절)을 따른다.

## 8. 효과 적용

```
StatBonus   → Character->AddStatBonus(Data.StatEffect)
GrantWeapon → 캐릭터 위치에 WeaponClass를 SpawnActor(Owner/Instigator = 캐릭터)
              → Character->PickUpWeapon(생성된 무기)
Ability     → UE_LOG 경고. 스킬 시스템(M2.7) 전까지 미구현
```

`Ability`를 조용히 무시하지 않고 **경고를 찍는 이유**: 데이터 테이블에 능력 카드를 넣어두고 "왜 안 먹지?"로 헤매는 걸 막기 위해서다. 뽑기 단계에서 거르지 않고 적용 단계에서 경고하는 쪽을 택한 건, 능력 카드가 테이블에 있으면 UI에는 보여서 M2.7 작업 시 바로 확인할 수 있게 하기 위함이다.

`PickUpWeapon`은 슬롯이 비어 있으면 채우고, 다 찼으면 현재 무기를 떨어뜨린다. 뽑기 단계에서 슬롯이 꽉 찬 경우를 이미 걸러내므로 카드 경로에서는 후자가 발생하지 않는다.

## 9. 검증

테스트 스위트가 없으므로 **컴파일 통과 + 수동 PIE**다.

1. 레벨업 시 카드 3장이 뜨고 게임이 멈추는가
2. 카드를 고르면 효과가 적용되고 게임이 재개되는가 — 스탯 카드는 HUD 체력/로그로, 무기 카드는 손에 들린 무기로 확인
3. **연속 레벨업**: `GJAddBonus RequiredEXP -90 0` 후 적 하나 처치 → 카드 화면이 **연달아 여러 번** 뜨는가
4. `bStackable=false` 카드를 고른 뒤 다시 레벨업 → 그 카드가 **더는 안 나오는가**
5. 무기 슬롯 2개를 채운 뒤 레벨업 → 무기 카드가 **후보에서 빠지는가**
6. 카드를 다 소진해 후보가 0장이 되면 → 카드 화면 없이 **그냥 넘어가는가**(멈추지 않는가)
7. 위젯 클래스를 비운 채 레벨업 → **게임이 멈추지 않고** 경고만 뜨는가
8. 카드 선택 후 마우스 클릭이 바로 먹는가(첫 클릭 씹힘 없음), 연사가 안 걸려 있는가
9. 죽고 새 런 시작 → `TakenCards`가 비워져 제외됐던 카드가 다시 나오는가

9번은 컴포넌트가 캐릭터와 함께 새로 생성되므로 초기화 코드 없이 성립한다. EXP·스탯 보너스와 같은 메커니즘이다.

## 10. 에디터 작업

M2.5는 에디터 작업이 없었지만 이번엔 있다.

- `DT_CardData` 생성 + 카드 입력(아이콘 텍스처 지정 포함)
- `WBP_CardSelect`, `WBP_Card` 두 개 제작
- `BP_GJCharacter`에 `UGJCardComponent` 추가하고 데이터 테이블·위젯 클래스 지정

MCP가 연결되어 있으므로 **위젯 배치와 데이터 테이블 입력은 자동화할 수 있다.** 아이콘으로 쓸 텍스처는 사용자가 정한다(현재 프로젝트에 아이콘용 에셋이 없어 자리표시자가 필요하다).

새 `UCLASS`(`UGJCardComponent`, 위젯 두 개)를 만들 때는 **라이브 코딩만으로는 부모 클래스 목록에 안 뜨는 경우가 있어** 에디터 재시작이 필요할 수 있다(개발 가이드 10절).

## 11. 후속 관계

- **M2.5 스탯 보너스 레이어** — 선행. `FStatModifier`와 `AddStatBonus`를 그대로 쓴다
- **M2.7 액티브 스킬** — `ECardEffectType::Ability`가 실제로 동작하게 만든다. 효과 enum에 자리를 잡아뒀으므로 그때 데이터 테이블 스키마는 안 바뀐다
- **M5 스테이지 진행** — 스테이지 클리어 시에도 카드를 주려면 `OnLevelUp` 외의 트리거가 필요하다. 컴포넌트의 진입점(`PendingChoices++` → `ShowNextChoice()`)을 공개 함수로 두면 그쪽에서 부르기만 하면 된다
- **M6 영구 특성** — 영구 재화로 찍는 특성도 결국 스탯 보너스지만, 저장소가 다르고 런 시작 전에 적용되므로 카드 시스템과는 별개 경로다
