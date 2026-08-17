#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GJPlayerHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UGJSkillIconWidget;
class AGJCharacter;

UCLASS()
class PROJECT_GJ_API UGJPlayerHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateHP(float CurrentHP, float MaxHP);

    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateMP(float CurrentMP, float MaxMP);

    // RequiredEXP는 "이번 레벨의 목표치"다(누적 총량이 아님). 경험치 바는 이번 레벨의 진행도만 그린다.
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void UpdateEXP(float CurrentEXP, float RequiredEXP, int32 Level);

    // 스킬 아이콘 3개에 슬롯 번호와 캐릭터를 넘긴다. 캐릭터가 HUD를 만들 때 한 번 부른다.
    // 아이콘이 GetOwningPlayerPawn()을 직접 캐스팅하지 않는 이유: 위젯 생성 시점에 폰이
    // 아직 없을 수 있고, 그러면 조용히 아무것도 안 그리는 상태로 굳는다.
    UFUNCTION(BlueprintCallable, Category = "HUD")
    void InitializeSkillIcons(AGJCharacter* InCharacter);

protected:
    // WBP 디자이너에서 이 이름들과 똑같은 Progress Bar 위젯을 추가해야 자동으로 바인딩됨
    UPROPERTY(meta = (BindWidget))
    UProgressBar* HPBar;

    UPROPERTY(meta = (BindWidget))
    UProgressBar* MPBar;

    // HP/MP와 달리 BindWidgetOptional이다. strict BindWidget으로 두면 C++이 먼저 들어간 순간
    // WBP_PlayerHUD 컴파일이 깨져서, 에디터에서 위젯을 배치할 때까지 게임이 정상 동작하지 않는다.
    // 이 프로젝트는 C++ 변경과 에디터 작업이 항상 시차를 두고 일어나므로 Optional이 맞다.
    UPROPERTY(meta = (BindWidgetOptional))
    UProgressBar* EXPBar;

    UPROPERTY(meta = (BindWidgetOptional))
    UTextBlock* LevelText;

    // EXPBar와 같은 이유로 Optional이다 - strict로 두면 C++이 먼저 들어간 순간
    // WBP_PlayerHUD 컴파일이 깨져서 에디터 작업 전까지 게임이 안 돈다.
    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillIconWidget* SkillIcon1;

    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillIconWidget* SkillIcon2;

    UPROPERTY(meta = (BindWidgetOptional))
    UGJSkillIconWidget* SkillIcon3;
};
