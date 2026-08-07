#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJHealthBarWidget.generated.h"

class UProgressBar;

UCLASS()
class PROJECT_GJ_API UGJHealthBarWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "HealthBar")
    void UpdateHealth(float CurrentHP, float MaxHP);

protected:
    // WBP 디자이너에서 이 이름과 똑같은 Progress Bar 위젯을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HealthProgressBar;
};
