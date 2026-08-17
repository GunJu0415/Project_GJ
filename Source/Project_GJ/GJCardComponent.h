#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GJGameTypes.h"
#include "GJCardComponent.generated.h"

class AGJCharacter;
class UGJCardSelectWidget;

// 컴포넌트가 지금 무엇을 묻고 있는지. 선택지 위젯은 인덱스만 돌려주므로,
// 그 인덱스가 "뽑힌 카드 목록의 위치"인지 "버릴 무기 슬롯 번호"인지는 이 상태로 판단한다.
// UENUM이 아닌 이유: 리플렉션에 노출할 필요가 없다(블루프린트도 UI도 이 값을 안 본다).
enum class EGJChoiceMode : uint8
{
    None,
    Card,
    WeaponReplace
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECT_GJ_API UGJCardComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UGJCardComponent();

    // 뽑기 결과를 로그로만 출력한다. AGJCharacter의 GJDrawCards 콘솔 명령이 호출한다.
    // 여기에 UFUNCTION(Exec)를 달아도 콘솔이 못 찾아서 폰을 거친다.
    // 카드가 생긴 뒤에도 "지금 풀에서 뭐가 나올 수 있나"를 보는 용도로 남긴다.
    UFUNCTION(BlueprintCallable, Category = "Card")
    void GJDrawCards();

    // 해당 태그를 가진 카드의 등장 가중치에 곱해질 배율을 정한다.
    // 지금은 콘솔에서만 부르지만, 나중에 "직업 카드"나 "먹은 카드 누적"이 붙어도
    // 전부 이 함수 하나로 들어온다 - 두 경로가 같은 맵을 밀어야 배율이 어떻게
    // 합쳐지는지가 한 곳에서 결정된다.
    UFUNCTION(BlueprintCallable, Category = "Card")
    void SetTagWeightMultiplier(FGameplayTag Tag, float Multiplier);

    // 개발용. 레벨업 없이 카드 화면만 띄워본다(일시정지는 걸지 않는다 - Task 4에서 붙는다).
    // Exec를 여기 달면 콘솔이 못 찾는다(Task 2에서 확인됨). AGJCharacter에 창구를 만든다.
    UFUNCTION(BlueprintCallable, Category = "Card")
    void GJShowCards();

protected:
    virtual void BeginPlay() override;

    // 카드 정의 테이블 (DT_CardData). 비어 있으면 카드 시스템 전체가 조용히 꺼진다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    UDataTable* CardTable;

    // 기본 선택지 장수. 코드에 3을 박아두면 "2장짜리 선택" 같은 조정에 컴파일이 필요해진다.
    // 이 값은 원본이라 런타임에 덮어쓰지 않는다 - 보너스는 아래 두 멤버로 얹는다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    int32 NumCardsToDraw = 3;

    // 영구 특성으로 늘어난 선택지 수. 지금은 아무도 안 바꾸지만, 메타 프로그레션(M6)이
    // 붙을 자리를 미리 뚫어둔다. 여기 대신 NumCardsToDraw를 직접 덮으면 원본값을 잃는다.
    UPROPERTY(BlueprintReadWrite, Category = "Card")
    int32 BonusCardSlots = 0;

    // 이 확률로 선택지가 한 장 더 뜬다 (0.2 = 20%). 판정은 뽑기 함수 밖에서 한다 -
    // DrawCards가 "몇 장 뽑을지"까지 정하면 리롤할 때마다 장수가 흔들린다.
    UPROPERTY(BlueprintReadWrite, Category = "Card")
    float ExtraCardChance = 0.f;

    // 태그별 등장 가중치 배율. 플레이어가 타고 있는 트리를 밀어주는 장치다.
    // 카드가 이 태그를 가지고 있으면(자식 태그 포함) 가중치에 배율이 곱해진다.
    UPROPERTY(BlueprintReadOnly, Category = "Card")
    TMap<FGameplayTag, float> TagWeightMultipliers;

    // 배율의 상한 (원래 가중치 대비 몇 배까지 허용할지).
    // 태그가 여러 개 겹치면 배율이 곱해져서 한 카드가 풀을 독점할 수 있다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    float MaxTagWeightMultiplier = 5.f;

    // 이미 고른 bStackable=false 카드. 스택 가능한 카드는 기록할 이유가 없다.
    // 런마다 컴포넌트가 새로 만들어지므로 초기화 코드가 필요 없다.
    TSet<FName> TakenCards;

    // 태그 배율을 적용한 실효 가중치. 테이블의 Weight는 원본이라 건드리지 않는다.
    float GetEffectiveWeight(const FCardData& Card) const;

    // 선택지 화면 클래스 (WBP_CardSelect). 비어 있으면 카드 선택을 건너뛴다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    TSubclassOf<UGJCardSelectWidget> CardSelectWidgetClass;

    UPROPERTY()
    UGJCardSelectWidget* CardSelectWidgetInstance;

    // 선택지를 화면에 띄운다. 위젯 생성이 실패하면 false를 돌려준다 -
    // 호출자는 이때 일시정지를 걸면 안 된다(화면 없이 게임만 멈추는 소프트락이 된다).
    bool OpenChoiceUI(const TArray<FGJChoiceEntry>& Entries);

    // 뽑힌 카드 ID 목록을 표시용 구조체로 바꾼다.
    TArray<FGJChoiceEntry> BuildCardEntries(const TArray<FName>& CardIds) const;

    // 이번에 몇 장 뽑을지 정한다. 확률 판정이 들어있어 호출할 때마다 결과가 다를 수 있으므로,
    // 한 번의 선택 화면에는 한 번만 부른다(리롤은 장수를 다시 굴리지 않는다).
    int32 GetDrawCount() const;

    // 가중 랜덤 비복원 추출로 최대 Count장을 뽑는다.
    // 후보가 부족하면 있는 만큼만, 하나도 없으면 빈 배열을 돌려준다.
    // 부작용이 없다(TakenCards를 건드리지 않는다). 리롤이 이 함수 재호출만으로 되는 이유다.
    TArray<FName> DrawCards(int32 Count) const;

    // 소유자를 AGJCharacter로 캐스팅해서 돌려준다. 다른 액터에 잘못 붙였으면 nullptr.
    AGJCharacter* GetOwnerCharacter() const;
};
