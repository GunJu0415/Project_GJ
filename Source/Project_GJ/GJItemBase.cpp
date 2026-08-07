#include "GJItemBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GJCharacter.h"

AGJItemBase::AGJItemBase()
{
    PrimaryActorTick.bCanEverTick = false;

    ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
    SetRootComponent(ItemMesh);
    // 메시 자체는 콜리전 없이 비주얼 전용
    ItemMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 아이템 자체의 물리적 충돌 판정용 (작게 유지 - 지금은 상호작용 판정에 안 씀)
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
    CollisionComp->SetupAttachment(ItemMesh);
    CollisionComp->InitSphereRadius(50.f);
    CollisionComp->SetCollisionProfileName(TEXT("Trigger"));

    // 상호작용(습득) 범위 - 물리 충돌(CollisionComp)과 분리된, 더 넉넉한 크기
    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(ItemMesh);
    InteractionCollision->InitSphereRadius(150.f);
    // "Trigger" 프로필: 아무것도 막지 않고(Block 없음) 캐릭터(Pawn)와는 겹침(Overlap)만 발생시킴 -
    // 필드에 놓인 아이템이 플레이어 이동을 물리적으로 막지 않으면서 범위 판정만 하기 위함
    InteractionCollision->SetCollisionProfileName(TEXT("Trigger"));
}

void AGJItemBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    if (!ItemDataHandle.IsNull())
    {
        if (FItemData* RowData = ItemDataHandle.GetRow<FItemData>(TEXT("Item Initialization")))
        {
            ItemStat = *RowData;

            if (RowData->ItemMeshAsset)
            {
                ItemMesh->SetStaticMesh(RowData->ItemMeshAsset);
            }
        }
    }
}

void AGJItemBase::Interact_Implementation(AGJCharacter* Interactor)
{
    if (Interactor && InteractionCollision->IsOverlappingActor(Interactor))
    {
        PickUp(Interactor);
    }
}

void AGJItemBase::PickUp(AGJCharacter* Picker)
{
    // 베이스는 빈 구현 - 실제 습득 처리는 서브클래스(AGJItem)에서 구현
}
