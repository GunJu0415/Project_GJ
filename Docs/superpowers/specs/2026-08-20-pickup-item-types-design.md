# 습득 아이템 종류 설계 (A2)

**작성일:** 2026-08-20

## 목표

주우면 **즉시 효과가 나는** 아이템과, **대가를 치러야 주는** 아이템을 만든다. 이것들이 생기면 **회복방과 보급방(아이작의 악마방)이 코드 없이 `DT_RoomSpawn` 행 추가만으로** 성립한다.

## 왜 룸 시스템과 별도인가

`AGJHealItem`은 방 없이 레벨에 그냥 떨어뜨려도 검증된다. 룸 스펙에 묶으면 서로 상관없는 두 작업이 엮여서 하나가 막히면 다른 하나도 못 나간다. 실제로 Task A는 이 문서 없이 완결됐다(`Docs/superpowers/specs/2026-08-18-room-system-design.md`).

## 방 클래스는 늘어나지 않는다

| 방 | 어떻게 성립하나 |
|---|---|
| 회복방 | `AGJCombatRoom` + 적 0마리 + `ItemPool = [회복 아이템]` |
| 보급방(악마방) | `AGJCombatRoom` + 적 0마리 + `ItemPool = [악마 아이템]` |

**늘어나는 것은 아이템 클래스뿐이다.** 방은 여전히 "채우고, 전멸을 세고, 문을 여는" 일만 하고 **무엇이 놓이냐만 다르다.** 이것이 룸 설계에서 세운 "값만 다른 것은 데이터지 동작이 아니다"가 실제로 값을 하는 지점이다.

## 클래스 구조

```
AGJItemBase (기존, abstract)     ItemDataHandle -> ItemStat, 범위 확인 후 PickUp 호출
 ├─ AGJItem        (기존)  인벤토리에 넣는다
 ├─ AGJHealItem    (신규)  즉시 회복
 └─ AGJPricedItem  (신규, abstract)  대가를 치러야 준다
      │   virtual bool CanAfford(AGJCharacter*) const
      │   virtual void PayPrice(AGJCharacter*)
      │   virtual FText GetPriceText() const      로그·UI용
      └─ AGJDevilItem   최대 체력으로 지불
```

`AGJPricedItem`을 중간에 두는 이유: 나중에 화폐가 생기면 `AGJShopItem`이 **같은 자리에** 들어오고, `CanAfford`/`PayPrice`만 다르게 구현한다. 지불 → 지급 흐름은 베이스가 한 번만 짠다.

## `AGJHealItem` — 새 데이터가 0이다

`FItemData`에 **`HealAmount`와 `ManaRecoverAmount`가 이미 있고** `AGJCharacter::ApplyConsumableEffect(Heal, Mana)`가 이미 적용한다. `AGJItemBase`가 `OnConstruction`에서 `ItemDataHandle`을 읽어 `ItemStat`에 캐시해 두는 것도 그대로다.

그래서 `AGJItem`과 **데이터가 완전히 같고** 다른 것은 단 하나 — 인벤토리에 넣느냐, 줍는 즉시 적용하느냐.

```cpp
void AGJHealItem::PickUp(AGJCharacter* Picker)
{
    Picker->ApplyConsumableEffect(ItemStat.HealAmount, ItemStat.ManaRecoverAmount);
    Destroy();
}
```

**데이터 테이블에 새 필드를 넣지 않는다.** 넣으면 에디터 재시작이 또 필요하고, 기존 소비 아이템과 값이 두 벌이 된다.

## `AGJDevilItem` — 최대 체력으로 지불

### 최대 체력을 직접 깎지 않는다

`AddStatBonus(FStatModifier)`에 **음수 `Add.MaxHP`**를 넣는다. `MaxHP`를 직접 대입하면 **레벨업 때 `RecalculateStats`가 `BaseStat + StatBonus`로 다시 계산하면서 지운다** — 대가를 치렀는데 레벨업 한 번에 돌려받는 꼴이 된다.

`AddStatBonus`는 이미 카드가 쓰는 경로다(`UGJCardComponent::ApplyCard`의 `StatBonus` 분기). 대가 지불이 그 경로를 그대로 타면 **누적·재계산 규칙이 한 곳에만 있다.**

### 0 체력 좀비 상태는 이미 막혀 있다

`RecalculateStats`가 최대 체력이 내려갈 때 현재 체력도 같이 내리는데, 하한이 `(CurrentHP > 0) ? 1 : 0`이다. 살아 있으면 **절대 0이 되지 않는다.**

주석에 이유까지 적혀 있다 — "스탯 변화는 데미지가 아니다. 사망 판정은 `TakeDamage` 안에만 있어서 죽지는 않고 `IsDead()`만 true가 되는 좀비 상태가 된다." **"최대 체력 -20%" 리스크 카드에서 같은 문제를 이미 풀었다.**

따라서 `AGJDevilItem`은 **현재 체력 가드를 새로 짜지 않는다.**

### `CanAfford`는 최대 체력 하한만 본다

```cpp
bool AGJDevilItem::CanAfford(const AGJCharacter* Buyer) const
{
    return Buyer && (Buyer->GetMaxHP() - MaxHPCost) >= MinMaxHPAfterPurchase;
}
```

`RecalculateStats`가 `MaxHP`를 최소 1로 클램프하긴 하지만 **최대 체력 1은 게임이 성립하지 않는 상태**다. 디자이너가 정하는 하한(`MinMaxHPAfterPurchase`, 기본 20)을 둔다.

### 가격은 `FItemData`가 아니라 액터에 둔다

```cpp
UPROPERTY(EditAnywhere, Category = "Price")
float MaxHPCost = 20.f;

UPROPERTY(EditAnywhere, Category = "Price")
float MinMaxHPAfterPurchase = 20.f;
```

**가격은 아이템 종류의 성질이 아니라 이 배치의 성질이다.** 같은 것이 악마방에선 비싸고 다른 데선 쌀 수 있다. `FItemData`에 넣으면 모든 아이템이 안 쓰는 칼럼을 갖게 되고 에디터 재시작도 또 필요하다.

## 지불 성공 시 무엇을 주나 — 카드 선택 화면

**최대 체력을 팔아 카드를 산다.** 이 게임에서 성장의 화폐는 카드이므로, 아이템을 주면 그냥 좀 좋은 상자와 구분이 안 된다.

`UGJCardComponent`에 공개 진입점을 하나 뺀다:

```cpp
// 카드 선택을 한 번 지급한다. 이미 화면이 떠 있으면 대기열에만 쌓인다.
UFUNCTION(BlueprintCallable, Category = "Card")
void GrantCardChoice();
```

몸통은 지금 `HandleLevelUp` 안에 있는 두 줄 그대로이고, `HandleLevelUp`이 이걸 부르도록 바꾼다.

```cpp
void UGJCardComponent::GrantCardChoice()
{
    PendingChoices++;

    // 이미 화면이 떠 있으면 카운터만 올리고 끝낸다.
    if (CurrentMode == EGJChoiceMode::None)
    {
        ShowNextChoice();
    }
}

void UGJCardComponent::HandleLevelUp(int32 NewLevel)
{
    GrantCardChoice();
}
```

**이 함수는 여기서만 쓰이는 게 아니다.** 개발 가이드 9절에 "스테이지 클리어 시 카드 지급 트리거가 없음 — 대기열 진입점을 공개 함수로 빼면 그쪽에서 부르기만 하면 된다"고 이미 적혀 있다. 진행 구조(Task C)에서 보스방이 그대로 쓴다.

**대기열이 이미 있다는 것이 중요하다.** 화면이 떠 있는 동안 또 지급되면 카운터만 오르고 순서대로 뜬다. 악마 아이템과 레벨업이 겹쳐도 하나가 사라지지 않는다.

## 지불 흐름

```
PickUp(Buyer):
  1. CanAfford(Buyer) 아니면 -> 로그 남기고 아무 일도 안 함 (아이템은 그대로 남는다)
  2. PayPrice(Buyer)
  3. GrantReward(Buyer)     기본 구현: 카드 선택 지급
  4. Destroy()
```

**못 낼 때 조용히 넘어가지 않는다.** 오늘 스킬 쪽에서 조용한 실패로 두 번 시간을 썼다. `[PRICE] 지불 불가: 최대 체력 100 - 20 < 하한 20` 형태로 이유를 남긴다.

**아이템은 그대로 남는다.** 대가를 못 내면 나중에 다시 시도할 수 있어야 한다 — 체력을 회복하거나 최대 체력을 올린 뒤에.

## 안 하는 것

- **가격 표시 UI**. 지금은 로그로만 확인된다. 화면에 띄우려면 월드 위젯이 필요하고 그건 별도 작업이다
- **화폐와 상점 NPC**. `AGJPricedItem` 아래에 `AGJShopItem` 자리만 비워둔다. 골드가 런을 넘어 남는지가 영구 강화(M6)와 얽히는 갈림길이라 게임 방향 결정이 먼저다
- **회복방·보급방 데이터 행**. 클래스가 생기고 나면 테이블 작업만 남으므로 검증 후에 넣는다

## 사용자가 만들 것

| 에셋 | 내용 |
|---|---|
| `BP_HealItem` | `AGJHealItem` 상속. 메시 + `ItemDataHandle`(회복량이 든 행) |
| `BP_DevilItem` | `AGJDevilItem` 상속. 메시 + `MaxHPCost` |
| `DT_ItemData` 행 | 회복 아이템용 행 (`HealAmount` > 0) |

## 검증

테스트 스위트가 없다. **컴파일 통과 + 수동 PIE 확인**이다.

`TestLev`에 두 아이템을 손으로 놓고:

1. 적에게 맞아 체력을 깎은 뒤 **회복 아이템을 주우면 체력이 오르고 아이템이 사라진다**
2. **인벤토리에는 안 들어간다** (즉시 효과라서)
3. **악마 아이템을 주우면 최대 체력이 줄고 카드 선택 화면이 뜬다**
4. 카드를 고른 뒤 **HUD의 최대 체력이 줄어 있다**
5. **레벨업을 해도 줄어든 최대 체력이 돌아오지 않는다** — 이게 `AddStatBonus`를 쓴 이유의 검증이다
6. 최대 체력이 하한 근처일 때 주우면 **아이템이 그대로 남고 로그에 이유가 뜬다**

**5번이 핵심이다.** 최대 체력을 직접 깎았다면 여기서 원상복구되고, 그건 조용히 지나가는 종류의 버그다.

## 미결 사항

없음.
