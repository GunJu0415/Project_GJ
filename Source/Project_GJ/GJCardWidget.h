#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJGameTypes.h"
#include "GJCardWidget.generated.h"

class UImage;
class UTextBlock;
class UButton;

// 이 선택지가 목록에서 몇 번째인지를 돌려준다. 카드 ID가 아니라 인덱스인 이유는
// 같은 위젯을 무기 교체(인덱스 = 버릴 슬롯 번호)에도 쓰기 때문이다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCardWidgetClickedSignature, int32, ChoiceIndex);

UCLASS()
class PROJECT_GJ_API UGJCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 표시 내용을 채우고 자기 인덱스를 기억한다.
    void Setup(int32 InChoiceIndex, const FGJChoiceEntry& Entry);

    UPROPERTY(BlueprintAssignable, Category = "Card")
    FOnCardWidgetClickedSignature OnCardClicked;

protected:
    virtual void NativeConstruct() override;

    // WBP_Card를 이 태스크에서 새로 만들므로 strict BindWidget을 쓴다.
    // (기존 WBP에 바인딩을 추가할 때만 BindWidgetOptional이 필요하다 - 그때는 에디터 작업
    //  전까지 WBP 컴파일이 깨지기 때문. 여기선 위젯과 WBP가 같이 만들어진다.)
    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* NameText;

    UPROPERTY(meta = (BindWidget))
    UTextBlock* DescText;

    UPROPERTY(meta = (BindWidget))
    UButton* SelectButton;

    int32 ChoiceIndex = INDEX_NONE;

    UFUNCTION()
    void HandleButtonClicked();
};
