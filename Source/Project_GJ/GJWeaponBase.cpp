#include "GJWeaponBase.h"
#include "Components/SphereComponent.h"
#include "GJCharacter.h"

AGJWeaponBase::AGJWeaponBase()
{
    // 무기 자체는 매 프레임 Tick을 돌 필요가 없으므로 최적화를 위해 false 설정
    PrimaryActorTick.bCanEverTick = false;

    // 루트 컴포넌트 생성 및 설정
    RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(RootComp);

    // 무기 메시 컴포넌트 생성 및 부착
    WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
    WeaponMesh->SetupAttachment(RootComponent);

    // 무기 자체의 물리 콜리전은 불필요한 경우가 많아 NoCollision으로 초기화
    // (실제 타격 판정은 추후 Anim Notify나 별도의 오버랩 박스로 처리)
    WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));

    // 필드에 놓였을 때 E로 주울 수 있게 하는 상호작용 판정 범위 (AGJItemBase와 동일한 방식) -
    // 장착되고 나면 OnPickedUp()에서 꺼버려서 캐릭터 손에 들려있는 동안은 판정에 안 걸림
    InteractionCollision = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionCollision"));
    InteractionCollision->SetupAttachment(RootComponent);
    InteractionCollision->InitSphereRadius(150.f);
    InteractionCollision->SetCollisionProfileName(TEXT("Trigger"));
}

void AGJWeaponBase::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    // 데이터 테이블 핸들이 비어있지 않은지 검사
    if (!WeaponDataHandle.IsNull())
    {
        // 테이블에서 해당 Row Name의 데이터를 가져옴
        FWeaponStat* RowData = WeaponDataHandle.GetRow<FWeaponStat>(TEXT("Weapon Initialization"));

        if (RowData)
        {
            // 1. 스탯 데이터 덮어쓰기
            WeaponStat = *RowData;

            // 2. 공격 몽타주 덮어쓰기
            AttackMontage = RowData->AttackMontageAsset;

            // 3. 재장전 몽타주 덮어쓰기 (아직 에셋이 없으면 nullptr로 남고, 사용하는 쪽에서 ReloadTime 타이머로 대체함)
            ReloadMontage = RowData->ReloadMontageAsset;

            // 4. 무기 스왑 몽타주 덮어쓰기 (비어있으면 스왑 시 몽타주 없이 즉시 교체됨)
            SwapMontage = RowData->SwapMontageAsset;

            // 5. 무기 메시 덮어쓰기 (테이블에 에셋이 할당되어 있다면 즉시 에디터 모델링 변경)
            if (RowData->WeaponMeshAsset)
            {
                WeaponMesh->SetSkeletalMeshAsset(RowData->WeaponMeshAsset);
            }
        }
    }
}

void AGJWeaponBase::Interact_Implementation(AGJCharacter* Interactor)
{
    if (Interactor)
    {
        Interactor->PickUpWeapon(this);
    }
}

void AGJWeaponBase::OnPickedUp(AGJCharacter* NewOwner)
{
    InteractionCollision->SetCollisionProfileName(TEXT("NoCollision"));
    SetOwner(NewOwner);
    SetInstigator(NewOwner);
}

void AGJWeaponBase::OnDropped(const FVector& DropLocation)
{
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
    SetActorLocation(DropLocation);
    SetActorHiddenInGame(false);
    InteractionCollision->SetCollisionProfileName(TEXT("Trigger"));
}

void AGJWeaponBase::Fire()
{

}
