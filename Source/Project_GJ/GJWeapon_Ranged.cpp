#include "GJWeapon_Ranged.h"
#include "GJProjectile.h"

AGJWeapon_Ranged::AGJWeapon_Ranged()
{
    PoolSize = 30; // 기본 탄창 30발
}

void AGJWeapon_Ranged::BeginPlay()
{
    Super::BeginPlay();
    CreateProjectilePool();
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

void AGJWeapon_Ranged::FireWeapon()
{
    // 캐릭터가 바라보는 방향 (또는 마우스 타겟팅 방향)
    FVector ShootDirection = GetInstigator()->GetActorForwardVector();

    // 무기 메시에 Muzzle(총구) 소켓이 있다면 그 위치를, 없다면 무기 위치를 사용
    FVector MuzzleLocation = WeaponMesh->DoesSocketExist(FName("Muzzle")) ?
        WeaponMesh->GetSocketLocation(FName("Muzzle")) : GetActorLocation();

    // 1. 풀에서 총알 하나 꺼내기
    AGJProjectile* ProjectileToFire = GetAvailableProjectile();

    if (ProjectileToFire)
    {
        // 2. 총구 위치와 쏘는 방향 세팅
        ProjectileToFire->SetActorLocationAndRotation(MuzzleLocation, ShootDirection.Rotation());

        // 3. 발사!
        ProjectileToFire->FireInDirection(ShootDirection);

        // [선택 사항] 격발 사운드 재생, 총구 이펙트(Muzzle Flash) 생성 로직 추가 가능
    }
}