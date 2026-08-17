#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJSkillIconWidget.generated.h"

class UImage;
class UTextBlock;
class UProgressBar;
class AGJCharacter;

// HUD의 스킬 아이콘 하나. 슬롯 번호를 기억하고 그 슬롯의 스킬과 쿨타임을 그린다.
// 디자이너에서 WBP_PlayerHUD에 3개 배치하고 각각 SetSlotData(0/1/2, 캐릭터)를 받는다.
UCLASS()
class PROJECT_GJ_API UGJSkillIconWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 무기 슬롯 위젯과 같은 진입점. 슬롯을 기억하고 델리게이트를 구독한 뒤 즉시 한 번 그린다.
    UFUNCTION(BlueprintCallable, Category = "Skill")
    void SetSlotData(int32 InSlotIndex, AGJCharacter* InOwningCharacter);

protected:
    // 쿨타임은 매 프레임 바뀌므로 틱에서 폴링한다. 아이콘 3개짜리라 비용이 없고,
    // 컴포넌트를 틱시키지 않으므로 "틱 안 쓰기" 설계도 그대로다.
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    virtual void NativeDestruct() override;

    // 슬롯 내용이 바뀔 때만 호출된다(아이콘/키 라벨).
    UFUNCTION()
    void RefreshSkill();

    void SetCooldownRatio(float Ratio);

    // 이번 태스크에서 WBP를 같이 만드므로 아이콘만 strict로 둔다.
    UPROPERTY(meta = (BindWidget))
    UImage* IconImage;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* KeyText;

    // 쿨타임 표시를 두 개로 나눠 받는다. ProgressBar는 SetPercent, 머티리얼 Image는
    // 스칼라 파라미터라 호출이 아예 다르다. 둘 다 Optional로 두고 붙어 있는 쪽만
    // 갱신하면, WBP에서 아래서 위로든 방사형이든 골라도 여기가 안 바뀐다.
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* CooldownBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UImage* CooldownImage;

    // CooldownImage에 물린 머티리얼의 이 스칼라 파라미터에 0~1을 넣는다.
    UPROPERTY(EditDefaultsOnly, Category = "Skill")
    FName CooldownParamName = FName("Progress");

    UPROPERTY()
    UMaterialInstanceDynamic* CooldownMID;

    int32 SlotIndex = INDEX_NONE;

    UPROPERTY()
    AGJCharacter* OwningCharacter;
};
