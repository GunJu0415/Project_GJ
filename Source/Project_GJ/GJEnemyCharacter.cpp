#include "GJEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AGJEnemyCharacter::AGJEnemyCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // 1. 맵에 배치되거나 스폰될 때 자동으로 AI Controller가 빙의하도록 설정
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    // 2. 샌드백 역할이므로 플레이어나 무기와 충돌할 수 있도록 캡슐 콜리전 설정
    GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));

    // 3. 적 캐릭터가 플레이어와 밀리지 않도록 무게나 회전 세팅 (선택 사항)
    GetCharacterMovement()->bOrientRotationToMovement = true;
}

void AGJEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
}

void AGJEnemyCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}