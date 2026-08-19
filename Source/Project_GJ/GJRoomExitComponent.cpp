#include "GJRoomExitComponent.h"
#include "Components/PrimitiveComponent.h"

UGJRoomExitComponent::UGJRoomExitComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGJRoomExitComponent::SetBlocked(bool bBlocked)
{
    // 표시는 자식까지 전파된다. 문짝 메시는 이 아래에 붙는다.
    SetVisibility(bBlocked, /*bPropagateToChildren=*/true);

    // 콜리전은 전파되지 않으므로 직접 순회한다. 표시만 끄면 보이지 않는 벽이 남는다.
    TArray<USceneComponent*> Children;
    GetChildrenComponents(/*bIncludeAllDescendants=*/true, Children);

    for (USceneComponent* Child : Children)
    {
        if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Child))
        {
            Prim->SetCollisionEnabled(bBlocked ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
}
