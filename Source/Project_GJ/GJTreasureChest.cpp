#include "GJTreasureChest.h"
#include "GJCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

AGJTreasureChest::AGJTreasureChest()
{
    PrimaryActorTick.bCanEverTick = false;

    ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
    RootComponent = ChestMesh;

    // AGJItemBase와 같은 구조 - 상호작용 범위는 메시 충돌과 별개로 넉넉하게 둔다.
    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(RootComponent);
    InteractionCollision->SetSphereRadius(150.f);
    InteractionCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    InteractionCollision->SetCollisionResponseToAllChannels(ECR_Overlap);
}

void AGJTreasureChest::Interact_Implementation(AGJCharacter* Interactor)
{
    if (bOpened || !Interactor)
    {
        return;
    }

    // 플레이어의 상호작용 입력은 겹친 액터를 찾아 Interact만 부른다.
    // "지금 정말 범위 안인지"는 각 구현부가 스스로 판단한다.
    if (!InteractionCollision->IsOverlappingActor(Interactor))
    {
        return;
    }

    bOpened = true;

    UWorld* World = GetWorld();
    if (World && Contents.Num() > 0)
    {
        const int32 DropCount = FMath::Max(FMath::RandRange(MinDrops, MaxDrops), 0);

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

        for (int32 i = 0; i < DropCount; i++)
        {
            const TSubclassOf<AActor> DropClass = Contents[FMath::RandRange(0, Contents.Num() - 1)];
            if (!DropClass)
            {
                continue;
            }

            // 원형으로 흩뿌린다. 겹쳐 놓으면 하나만 있는 것처럼 보인다.
            const float Angle = 2.f * PI * i / FMath::Max(DropCount, 1);
            const FVector Offset(FMath::Cos(Angle) * DropRadius, FMath::Sin(Angle) * DropRadius, 0.f);

            World->SpawnActor<AActor>(DropClass, GetActorLocation() + Offset, FRotator::ZeroRotator, Params);
        }

        UE_LOG(LogTemp, Log, TEXT("[CHEST] %s 열림 - %d개 드랍"), *GetName(), DropCount);
    }

    OnChestOpened();
}
