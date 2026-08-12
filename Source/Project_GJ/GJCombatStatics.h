#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GJCombatStatics.generated.h"

// 데미지 공식의 단일 소스. 공격 계산은 공격자가, 방어 경감은 맞는 쪽(TakeDamage)이 호출하지만
// 공식 자체는 이 파일 한 곳에만 존재한다 - 밸런스를 조정할 때 여기만 보면 된다.
//
// 전체 공식:
//   공격데미지 = 무기데미지 x (1 + 공격력/100) x 치명타배율
//   최종데미지 = 공격데미지 x 100/(100 + 방어력)
UCLASS()
class PROJECT_GJ_API UGJCombatStatics : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // 공격 측 계산 - 공격력 배율을 곱하고 치명타를 굴린다.
    // CritChance는 0.0~1.0 범위다(0.25 = 25%). bOutWasCritical로 치명타 여부를 돌려준다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static float CalculateOutgoingDamage(
        float BaseDamage,
        float AttackPower,
        float CritChance,
        float CritMultiplier,
        bool& bOutWasCritical);

    // 방어 측 경감 - 체감형이라 방어력을 아무리 올려도 100% 무효화에 도달하지 않는다.
    UFUNCTION(BlueprintCallable, Category = "Combat")
    static float ApplyDefense(float IncomingDamage, float Defense);
};
