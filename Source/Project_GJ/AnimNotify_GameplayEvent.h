#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_GameplayEvent.generated.h"

// 1. 에디터에서 선택할 수 있도록 Enum(열거형)을 만듭니다.
UENUM(BlueprintType)
enum class EGameplayNotifyType : uint8
{
    Fire            UMETA(DisplayName = "발사 (Fire)"),
    Footstep        UMETA(DisplayName = "발소리 (Footstep)"),
    Reload          UMETA(DisplayName = "장전 (Reload)"),
    ShellEject      UMETA(DisplayName = "탄피 배출 (Shell Eject)"),
    MeleeHit        UMETA(DisplayName = "근접 타격 (Melee Hit)")
};

UCLASS()
class PROJECT_GJ_API UAnimNotify_GameplayEvent : public UAnimNotify
{
    GENERATED_BODY()

public:
    // 2. 애니메이터/기획자가 에디터에서 선택할 수 있게 프로퍼티로 노출합니다.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Notify")
    EGameplayNotifyType NotifyType;

    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;
};