#include "AnimNotify_GameplayEvent.h"
#include "GJCharacter.h"
#include "Components/SkeletalMeshComponent.h"

void UAnimNotify_GameplayEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    // 언리얼 스타일: 선언과 동시에 유효성 검사
    if (AGJCharacter* Character = Cast<AGJCharacter>(MeshComp->GetOwner()))
    {
        // 3. 에디터에서 선택한 타입에 따라 캐릭터의 알맞은 함수를 호출!
        switch (NotifyType)
        {
        case EGameplayNotifyType::Fire:
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("NotifyCome!!!"));
            Character->PerformFire();

            break;

        case EGameplayNotifyType::Footstep:
            // Character->PlayFootstepSound(); // 나중에 추가할 함수
            break;

        case EGameplayNotifyType::Reload:
            // Character->PerformReload(); // 나중에 추가할 함수
            break;

        case EGameplayNotifyType::MeleeHit:
            // Character->PerformMeleeHit(); // 나중에 추가할 함수
            break;

        default:
            break;
        }
    }
}