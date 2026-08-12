#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GJInteractable.h"
#include "GJRunPortal.generated.h"

class UStaticMeshComponent;
class USphereComponent;
class AGJCharacter;

// 허브에 놓는 "런 시작" 포탈. 아이템(AGJItemBase)/무기(AGJWeaponBase)와 똑같은
// IGJInteractable 방식을 쓰므로 플레이어 쪽 입력 코드는 손댈 필요가 없다.
UCLASS()
class PROJECT_GJ_API AGJRunPortal : public AActor, public IGJInteractable
{
    GENERATED_BODY()

public:
    AGJRunPortal();

    // IGJInteractable - 상호작용 범위 안에서 상호작용 입력을 받으면 새 런을 시작한다
    virtual void Interact_Implementation(AGJCharacter* Interactor) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USceneComponent* RootComp;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UStaticMeshComponent* PortalMesh;

    // 상호작용 판정 범위 - AGJItemBase의 InteractionCollision과 동일한 "Trigger" 프로필.
    // 아이템보다 넉넉하게 잡아서 포탈 근처에 서기만 해도 반응하게 한다.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USphereComponent* InteractionCollision;
};
