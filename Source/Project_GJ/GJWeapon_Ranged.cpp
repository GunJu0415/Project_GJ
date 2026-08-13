#include "GJWeapon_Ranged.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h" // 무기 메시에 따라 Static/Skeletal 포함
#include "GJProjectile.h"
#include "GJCombatStatics.h"
#include "GJCharacter.h"

AGJWeapon_Ranged::AGJWeapon_Ranged()
{
    PoolSize = 30; // 기본 탄창 30발
}

void AGJWeapon_Ranged::BeginPlay()
{
    Super::BeginPlay();
    CreateProjectilePool();

    // 탄창을 가득 채운 상태로 시작
    CurrentAmmo = WeaponStat.MagazineSize;
}

void AGJWeapon_Ranged::CreateProjectilePool()
{
    if (!ProjectileClass) return;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = GetInstigator(); // 총을 쏜 캐릭터

    for (int32 i = 0; i < PoolSize; i++)
    {
        // 총알을 생성하고 배열에 담아둡니다.
        AGJProjectile* NewProjectile = GetWorld()->SpawnActor<AGJProjectile>(ProjectileClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
        if (NewProjectile)
        {
            // 투사체 내부의 BeginPlay에서 자동으로 Deactivate() 되며 숨겨집니다.
            ProjectilePool.Add(NewProjectile);
        }
    }
}

AGJProjectile* AGJWeapon_Ranged::GetAvailableProjectile()
{
    // 풀을 순회하며 비활성화(쉬고 있는) 총알을 찾습니다.
    for (AGJProjectile* Projectile : ProjectilePool)
    {
        if (Projectile && !Projectile->IsActive())
        {
            return Projectile;
        }
    }

    // 만약 30발이 모두 화면에 날아가고 있다면 nullptr 반환 (필요시 여기서 동적 추가 생성 가능)
    return nullptr;
}

bool AGJWeapon_Ranged::CanReload() const
{
    return !bIsReloading && CurrentAmmo < WeaponStat.MagazineSize;
}

void AGJWeapon_Ranged::StartReload(int32 InBulletsToRefill)
{
    bIsReloading = true;
    PendingRefillAmount = InBulletsToRefill;
}

void AGJWeapon_Ranged::FinishReload()
{
    CurrentAmmo = FMath::Clamp(CurrentAmmo + PendingRefillAmount, 0, WeaponStat.MagazineSize);
    PendingRefillAmount = 0;
    bIsReloading = false;

    OnAmmoChanged.Broadcast(CurrentAmmo, WeaponStat.MagazineSize);
}

void AGJWeapon_Ranged::Fire()
{
    // 재장전 중에는 발사 불가
    if (bIsReloading)
    {
        return;
    }

    // 연사 속도 제한: WeaponStat.FireInterval(초)마다 최대 1발
    const float CurrentTime = GetWorld()->GetTimeSeconds();
    if (CurrentTime - LastFireTime < WeaponStat.FireInterval)
    {
        return;
    }

    // 탄창이 비었으면 발사 불가 (재장전은 R 입력으로 별도 처리)
    if (CurrentAmmo <= 0)
    {
        return;
    }

    // 디버깅용 화면 출력 (노란색 텍스트, 3초 유지)
    if (GEngine)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Pooling Fire function called!"));
    }

    // 1. 발사 방향 세팅
    FVector ShootDirection = GetInstigator()->GetActorForwardVector();

    // 2. 총구 위치 세팅 (소켓 이름은 블루프린트 설정과 반드시 일치해야 합니다!)
    FVector MuzzleLocation = WeaponMesh->DoesSocketExist(FName("MuzzleSocket")) ?
        WeaponMesh->GetSocketLocation(FName("MuzzleSocket")) : GetActorLocation();

    // 3. 풀에서 총알 하나 꺼내기
    AGJProjectile* ProjectileToFire = GetAvailableProjectile();

    if (ProjectileToFire)
    {
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());

        // 총알 풀은 무기의 BeginPlay에서 만들어지는데, 필드에 놓여 있다가 주운 무기는 그 시점에
        // 주인이 없어서 풀 전체가 인스티게이터 nullptr로 굳어버린다(OnPickedUp의 SetInstigator는
        // 무기 액터에만 걸린다). 그러면 OnHit이 넘기는 가해자 컨트롤러가 null이 되어 적 처치
        // 경험치를 줄 대상을 찾지 못하고, 자기 피격 방지도 동작하지 않는다.
        // 발사 시점에 현재 주인으로 갱신하면 스왑/픽업/드랍 어느 경로든 항상 맞는다.
        ProjectileToFire->SetInstigator(GetInstigator());

        // 무기를 든 캐릭터의 공격력/치명타를 반영해 실제 발사 데미지를 계산한다.
        // OnPickedUp에서 SetInstigator를 하므로 플레이어가 든 무기는 항상 여기서 캐릭터를 찾을 수 있다.
        float AttackPower = 0.f;
        float CritChance = 0.f;
        float CritMultiplier = 1.f;
        if (AGJCharacter* OwningCharacter = Cast<AGJCharacter>(GetInstigator()))
        {
            AttackPower = OwningCharacter->GetBaseAttackPower();
            CritChance = OwningCharacter->CritChance;
            CritMultiplier = OwningCharacter->CritMultiplier;
        }

        // 캐스팅이 실패하면 위 기본값 그대로라 무기 기본 데미지만 나간다(배율 1배, 치명타 없음)
        bool bWasCritical = false;
        const float OutgoingDamage = UGJCombatStatics::CalculateOutgoingDamage(
            WeaponStat.BaseDamage, AttackPower, CritChance, CritMultiplier, bWasCritical);

        ProjectileToFire->FireInDirection(ShootDirection, OutgoingDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range);

        CurrentAmmo--;
        LastFireTime = CurrentTime;

        OnAmmoChanged.Broadcast(CurrentAmmo, WeaponStat.MagazineSize);
    }
    else
    {
        // 풀에 남은 총알이 없을 때 화면 출력 (빨간색 텍스트, 3초 유지)
        if (GEngine)
        {
            //GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Failed to get projectile from pool!"));
        }
    }
}
