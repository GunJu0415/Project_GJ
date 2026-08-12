#include "GJRunPortal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GJCharacter.h"
#include "GJGameInstance.h"

AGJRunPortal::AGJRunPortal()
{
    PrimaryActorTick.bCanEverTick = false;

    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(RootComp);

    PortalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PortalMesh"));
    PortalMesh->SetupAttachment(RootComponent);
    // 메시는 비주얼 전용 - 플레이어가 포탈에 부딪혀 막히지 않게 한다
    PortalMesh->SetCollisionProfileName(TEXT("NoCollision"));

    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(RootComponent);
    InteractionCollision->InitSphereRadius(200.f);
    InteractionCollision->SetCollisionProfileName(TEXT("Trigger"));
}

void AGJRunPortal::Interact_Implementation(AGJCharacter* Interactor)
{
    // "지금 정말 범위 안인지"는 각 구현부가 스스로 판단한다 (IGJInteractable의 규칙)
    if (!Interactor || !InteractionCollision->IsOverlappingActor(Interactor))
    {
        return;
    }

    if (UGJGameInstance* GJGameInstance = Cast<UGJGameInstance>(GetGameInstance()))
    {
        GJGameInstance->StartNewRun();
    }
}
