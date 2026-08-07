#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJPlayerHUDWidget.generated.h"

class UProgressBar;

UCLASS()
class PROJECT_GJ_API UGJPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateHP(float CurrentHP, float MaxHP);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateMP(float CurrentMP, float MaxMP);

protected:
    // WBP 디자이너에서 이 이름들과 똑같은 Progress Bar 위젯을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HPBar;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* MPBar;
};
