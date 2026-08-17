#include "GJCardComponent.h"
#include "GJCharacter.h"
#include "GJWeaponBase.h"
#include "GJCardSelectWidget.h"
#include "Engine/DataTable.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

UGJCardComponent::UGJCardComponent()
{
    // 카드 로직은 전부 이벤트 구동(레벨업, 버튼 클릭)이라 매 프레임 할 일이 없다.
    PrimaryComponentTick.bCanEverTick = false;
}

void UGJCardComponent::BeginPlay()
{
    Super::BeginPlay();
}

AGJCharacter* UGJCardComponent::GetOwnerCharacter() const
{
    return Cast<AGJCharacter>(GetOwner());
}

// StatBonus 카드인데 효과가 전부 0이면 골라도 아무 일이 없어 플레이어가 손해를 본다.
// 데이터 미입력을 뽑기 단계에서 걸러내기 위한 판정이다.
static bool IsStatEffectEmpty(const FStatModifier& Modifier)
{
    auto AllZero = [](const FStatValues& V)
    {
        return V.MaxHP == 0.f && V.MaxMP == 0.f && V.BaseAttackPower == 0.f
            && V.RequiredEXP == 0.f && V.Defense == 0.f && V.MoveSpeed == 0.f
            && V.CooldownReduction == 0.f && V.CritChance == 0.f && V.CritMultiplier == 0.f;
    };
    return AllZero(Modifier.Add) && AllZero(Modifier.Percent);
}

void UGJCardComponent::SetTagWeightMultiplier(FGameplayTag Tag, float Multiplier)
{
    if (!Tag.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("SetTagWeightMultiplier: 유효하지 않은 태그입니다."));
        return;
    }

    // 배율 1은 "아무 영향 없음"이므로 맵에 남겨둘 이유가 없다. 안 지우면 시간이 지날수록
    // 맵이 무의미한 항목으로 불어나고, 로그로 상태를 볼 때 뭐가 실제로 작동 중인지 안 보인다.
    if (FMath::IsNearlyEqual(Multiplier, 1.f))
    {
        TagWeightMultipliers.Remove(Tag);
    }
    else
    {
        TagWeightMultipliers.Add(Tag, FMath::Max(Multiplier, 0.f));
    }
}

float UGJCardComponent::GetEffectiveWeight(const FCardData& Card) const
{
    float Weight = Card.Weight;

    // 카드의 태그가 아니라 배율 항목을 기준으로 순회한다. 카드 기준으로 돌면
    // Tree.Fire와 Tree.Fire.Shotgun을 둘 다 가진 카드가 Tree.Fire 배율을 두 번 먹는다.
    for (const TPair<FGameplayTag, float>& Pair : TagWeightMultipliers)
    {
        // HasTag는 계층 매칭이다 - 카드가 Tree.Fire.Shotgun만 가지고 있어도
        // Tree.Fire 배율에 걸린다.
        if (Card.CardTags.HasTag(Pair.Key))
        {
            Weight *= Pair.Value;
        }
    }

    return FMath::Clamp(Weight, 0.f, Card.Weight * MaxTagWeightMultiplier);
}

int32 UGJCardComponent::GetDrawCount() const
{
    int32 Count = NumCardsToDraw + BonusCardSlots;

    if (ExtraCardChance > 0.f && FMath::FRand() < ExtraCardChance)
    {
        Count++;
    }

    // 0장이 되면 선택 화면이 빈 채로 떠서 진행이 막힌다. 데이터를 어떻게 넣든 최소 1장은 보장한다.
    return FMath::Max(Count, 1);
}

TArray<FName> UGJCardComponent::DrawCards(int32 Count) const
{
    TArray<FName> Result;

    if (!CardTable)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: CardTable이 비어있어 카드를 뽑을 수 없습니다."));
        return Result;
    }

    // 1. 후보 수집
    TArray<FName> Candidates;
    TArray<float> Weights;

    for (const FName& RowName : CardTable->GetRowNames())
    {
        const FCardData* Row = CardTable->FindRow<FCardData>(RowName, TEXT("DrawCards"), false);
        if (!Row)
        {
            continue;
        }

        // 가중치 0 이하 = 임시로 꺼둔 카드. 태그 배율이 아니라 테이블 원본값으로 판정한다 -
        // 꺼둔 카드가 배율 때문에 되살아나면 안 된다.
        if (Row->Weight <= 0.f)
        {
            continue;
        }

        // 스택 불가인데 이미 먹은 카드
        if (!Row->bStackable && TakenCards.Contains(RowName))
        {
            continue;
        }

        // 데이터가 비어 있는 행은 뽑아도 의미가 없다.
        // 무기 슬롯이 꽉 찼다는 이유로는 거르지 않는다 - 그 경우 카드를 고른 뒤
        // 어느 무기를 버릴지 플레이어가 정한다.
        if (Row->EffectType == ECardEffectType::GrantWeapon && !Row->WeaponClass)
        {
            continue;
        }
        if (Row->EffectType == ECardEffectType::StatBonus && IsStatEffectEmpty(Row->StatEffect))
        {
            continue;
        }
        // Ability는 거르지 않는다. 테이블에 있으면 UI에는 보이고, 고르면 적용 단계에서
        // 경고가 찍힌다(M2.7 작업 시 바로 확인할 수 있게).

        Candidates.Add(RowName);
        Weights.Add(GetEffectiveWeight(*Row));
    }

    // 2. 가중 랜덤 비복원 추출
    const int32 DrawCount = FMath::Min(Count, Candidates.Num());
    for (int32 Draw = 0; Draw < DrawCount; Draw++)
    {
        // 매 반복마다 총합을 다시 구한다. 뽑힌 카드를 목록에서 빼고도 총합을 그대로 쓰면
        // 이미 사라진 가중치가 구간에 남아, 난수가 그 구간에 떨어졌을 때 엉뚱한 카드가
        // 뽑히거나 마지막 카드로 몰린다.
        float TotalWeight = 0.f;
        for (float W : Weights)
        {
            TotalWeight += W;
        }
        if (TotalWeight <= 0.f)
        {
            break;
        }

        const float Roll = FMath::FRandRange(0.f, TotalWeight);
        float Accum = 0.f;
        int32 PickedIndex = Candidates.Num() - 1;  // 부동소수 오차로 루프를 못 빠져나갈 때의 안전값
        for (int32 i = 0; i < Candidates.Num(); i++)
        {
            Accum += Weights[i];
            if (Roll < Accum)
            {
                PickedIndex = i;
                break;
            }
        }

        Result.Add(Candidates[PickedIndex]);
        Candidates.RemoveAt(PickedIndex);
        Weights.RemoveAt(PickedIndex);
    }

    return Result;
}

void UGJCardComponent::GJDrawCards()
{
    const TArray<FName> Drawn = DrawCards(GetDrawCount());

    if (Drawn.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJDrawCards: 뽑을 수 있는 카드가 없습니다 (테이블 비었거나 전부 제외됨)."));
        return;
    }

    FString Joined;
    for (const FName& Id : Drawn)
    {
        if (!Joined.IsEmpty())
        {
            Joined += TEXT(", ");
        }
        Joined += Id.ToString();
    }

    // 지금 걸려 있는 태그 배율도 같이 찍는다. 뽑기 결과가 치우쳐 보일 때
    // 그게 배율 탓인지 그냥 운인지 구분하려면 둘을 같은 줄에서 봐야 한다.
    FString TagInfo;
    for (const TPair<FGameplayTag, float>& Pair : TagWeightMultipliers)
    {
        if (!TagInfo.IsEmpty())
        {
            TagInfo += TEXT(", ");
        }
        TagInfo += FString::Printf(TEXT("%s x%.1f"), *Pair.Key.ToString(), Pair.Value);
    }
    if (TagInfo.IsEmpty())
    {
        TagInfo = TEXT("없음");
    }

    UE_LOG(LogTemp, Log, TEXT("GJDrawCards: %d장 -> %s (이미 먹은 고유카드 %d개, 태그 배율: %s)"),
        Drawn.Num(), *Joined, TakenCards.Num(), *TagInfo);
}

TArray<FGJChoiceEntry> UGJCardComponent::BuildCardEntries(const TArray<FName>& CardIds) const
{
    TArray<FGJChoiceEntry> Entries;
    if (!CardTable)
    {
        return Entries;
    }

    for (const FName& Id : CardIds)
    {
        const FCardData* Row = CardTable->FindRow<FCardData>(Id, TEXT("BuildCardEntries"), false);
        if (!Row)
        {
            continue;
        }

        FGJChoiceEntry Entry;
        Entry.DisplayName = Row->DisplayName;
        Entry.Description = Row->Description;
        Entry.Icon = Row->Icon;
        Entries.Add(Entry);
    }

    return Entries;
}

bool UGJCardComponent::OpenChoiceUI(const TArray<FGJChoiceEntry>& Entries)
{
    AGJCharacter* Character = GetOwnerCharacter();
    if (!Character)
    {
        return false;
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC)
    {
        return false;
    }

    if (!CardSelectWidgetInstance)
    {
        if (!CardSelectWidgetClass)
        {
            UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: CardSelectWidgetClass가 비어있어 카드 선택을 건너뜁니다."));
            return false;
        }
        CardSelectWidgetInstance = CreateWidget<UGJCardSelectWidget>(PC, CardSelectWidgetClass);
        if (!CardSelectWidgetInstance)
        {
            UE_LOG(LogTemp, Warning, TEXT("GJCardComponent: 카드 선택 위젯 생성에 실패했습니다."));
            return false;
        }
    }

    CardSelectWidgetInstance->ShowChoices(Entries);

    if (!CardSelectWidgetInstance->IsInViewport())
    {
        CardSelectWidgetInstance->AddToViewport();
    }

    return true;
}

void UGJCardComponent::GJShowCards()
{
    const TArray<FName> Drawn = DrawCards(GetDrawCount());
    if (Drawn.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: 뽑을 수 있는 카드가 없습니다."));
        return;
    }

    if (!OpenChoiceUI(BuildCardEntries(Drawn)))
    {
        UE_LOG(LogTemp, Warning, TEXT("GJShowCards: 화면을 띄우지 못했습니다."));
    }
}
