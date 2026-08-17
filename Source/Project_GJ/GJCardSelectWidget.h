#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJGameTypes.h"
#include "GJCardSelectWidget.generated.h"

class UHorizontalBox;
class UGJCardWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChoiceSelectedSignature, int32, ChoiceIndex);

UCLASS()
class PROJECT_GJ_API UGJCardSelectWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 선택지 목록을 받아 카드 위젯을 그만큼 만들어 늘어놓는다.
    // 카드 선택(최대 3개)과 무기 교체(2개) 양쪽이 이 함수를 쓴다.
    void ShowChoices(const TArray<FGJChoiceEntry>& Choices);

    UPROPERTY(BlueprintAssignable, Category = "Card")
    FOnChoiceSelectedSignature OnChoiceSelected;

protected:
    UPROPERTY(meta = (BindWidget))
    UHorizontalBox* CardContainer;

    // WBP_Card를 지정한다. 인벤토리의 SlotWidgetClass와 같은 패턴이다.
    UPROPERTY(EditDefaultsOnly, Category = "Card")
    TSubclassOf<UGJCardWidget> CardWidgetClass;

    UFUNCTION()
    void HandleCardClicked(int32 ChoiceIndex);
};
