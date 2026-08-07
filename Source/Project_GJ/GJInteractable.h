#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GJInteractable.generated.h"

class AGJCharacter;

UINTERFACE(BlueprintType)
class PROJECT_GJ_API UGJInteractable : public UInterface
{
    GENERATED_BODY()
};

// 상호작용 가능한 모든 것(아이템, 나중에 문/버튼 등)이 구현하는 인터페이스.
// 플레이어의 상호작용 입력은 이 인터페이스를 구현한 액터를 찾아 Interact()만 호출하고,
// "지금 정말 상호작용해도 되는 범위인지"는 각 구현부(액터)가 스스로 판단함.
class PROJECT_GJ_API IGJInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, Category = "Interaction")
    void Interact(AGJCharacter* Interactor);
};
