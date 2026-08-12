#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJGameOverWidget.generated.h"

class UButton;
class UTextBlock;

// 런이 끝났을 때 뜨는 게임오버 화면. 기존 UGJPlayerHUDWidget/UGJInventoryWidget과 동일하게
// C++ 베이스 + BindWidget 패턴을 쓴다 - 디자이너에서는 이름이 맞는 위젯만 배치하면 되고
// 블루프린트 이벤트 그래프 작업이 필요 없다.
UCLASS()
class PROJECT_GJ_API UGJGameOverWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 방금 끝난 도전의 번호를 표시한다 (게임 모드가 위젯을 띄운 직후 호출)
    UFUNCTION(BlueprintCallable, Category = "Run")
    void SetRunCount(int32 RunCount);

protected:
    virtual void NativeOnInitialized() override;

    // 디자이너에서 이 이름과 똑같은 Button을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UButton* ReturnToHubButton;

    // 선택 사항 - 몇 번째 도전이었는지 표시
    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* RunCountText;

    UFUNCTION()
    void OnReturnToHubClicked();
};
