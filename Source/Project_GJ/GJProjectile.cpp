#include "GJProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "GJBaseCharacter.h"
#include "Kismet/GameplayStatics.h"

AGJProjectile::AGJProjectile()
{
    PrimaryActorTick.bCanEverTick = false; // 투사체는 틱을 끌수록 최적화에 좋습니다.

    // 1. 충돌체(Sphere) 세팅
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    // "Projectile"이라는 콜리전 프로필은 이 프로젝트(엔진 기본 + DefaultEngine.ini) 어디에도 등록되어 있지 않아서
    // 무효 처리되고 있었습니다. 실제로 존재하는 "BlockAllDynamic" 프로필로 교체합니다.
    CollisionComp->BodyInstance.SetCollisionProfileName("BlockAllDynamic");
    CollisionComp->OnComponentHit.AddDynamic(this, &AGJProjectile::OnHit); // 충돌 이벤트 바인딩
    RootComponent = CollisionComp;

    // 2. 외형(Mesh) 세팅
    MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
    MeshComp->SetupAttachment(CollisionComp);
    MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision); // 메시는 충돌 연산 X

    // 3. 발사체 이동 컴포넌트 (핵심)
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
    ProjectileMovement->UpdatedComponent = CollisionComp;
    ProjectileMovement->InitialSpeed = 3000.f;
    ProjectileMovement->MaxSpeed = 3000.f;
    ProjectileMovement->bRotationFollowsVelocity = true; // 날아가는 방향으로 자동 회전
    ProjectileMovement->bShouldBounce = false;
    ProjectileMovement->ProjectileGravityScale = 0.f; // 중력 영향 제거 -> 포물선 없이 직선으로 날아감

    bIsActive = false;
}

void AGJProjectile::BeginPlay()
{
    Super::BeginPlay();
    // 시작 시 풀에 보관하기 위해 비활성화 상태로 둡니다.
    Deactivate();
}

void AGJProjectile::FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange)
{
    bIsActive = true;
    Damage = InDamage;

    // 숨김 해제 및 충돌 켜기
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);

    // 무기 테이블에서 가져온 속도로 직선 비행 (방향 벡터를 정규화한 뒤 속도를 곱함)
    const float Speed = (InSpeed > 0.f) ? InSpeed : ProjectileMovement->InitialSpeed;
    ProjectileMovement->MaxSpeed = Speed;
    ProjectileMovement->Velocity = ShootDirection.GetSafeNormal() * Speed;
    ProjectileMovement->Activate();

    // 사거리(Range)만큼 날아가면 자동 비활성화. 등속 직선 비행이므로 (사거리 / 속도) = 도달 시간.
    GetWorldTimerManager().ClearTimer(RangeTimerHandle);
    if (InRange > 0.f && Speed > 0.f)
    {
        const float FlightTime = InRange / Speed;
        GetWorldTimerManager().SetTimer(RangeTimerHandle, this, &AGJProjectile::Deactivate, FlightTime, false);
    }
}

void AGJProjectile::Deactivate()
{
    bIsActive = false;

    // 사거리 타이머가 아직 남아있다면 취소 (충돌로 먼저 비활성화된 경우, 이후 재사용 중에 뒤늦게 터지는 것 방지)
    GetWorldTimerManager().ClearTimer(RangeTimerHandle);

    // 화면에서 숨기고 충돌, 이동 연산을 끕니다 (Destroy 대신 사용)
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    ProjectileMovement->Deactivate();
    ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AGJProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 부딪힌 대상이 유효하고, 자기 자신이 아니며, 총을 쏜 주체가 아닐 때만 데미지 적용
    if (OtherActor && OtherActor != this && OtherActor != GetInstigator())
    {
        // 맞은 대상이 AGJBaseCharacter(플레이어 또는 적)인지 확인합니다. 실패하면 nullptr.
        AGJBaseCharacter* HitCharacter = Cast<AGJBaseCharacter>(OtherActor);

        if (HitCharacter)
        {
            UGameplayStatics::ApplyDamage(
                HitCharacter,
                Damage,
                GetInstigatorController(), // Instigator가 없어도 안전하게 nullptr 반환 (GetInstigator()->GetController()는 Instigator가 없으면 크래시남)
                this,
                UDamageType::StaticClass()
            );
        }
    }

    // 투사체 비활성화 (오브젝트 풀링 사용 중이므로 Destroy() 대신 Deactivate 사용!)
    Deactivate();
}
