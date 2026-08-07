#include "GJWeapon_Ranged.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h" // 무기 메시에 따라 Static/Skeletal 포함
#include "GJProjectile.h"

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
        // 데이터 테이블에서 가져온 데미지/속도/사거리 그대로 발사합니다.
        ProjectileToFire->FireInDirection(ShootDirection, WeaponStat.BaseDamage, WeaponStat.ProjectileSpeed, WeaponStat.Range);

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
