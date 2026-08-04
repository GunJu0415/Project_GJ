#include "GJProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AGJProjectile::AGJProjectile()
{
    PrimaryActorTick.bCanEverTick = false; // 투사체는 틱을 끌수록 최적화에 좋습니다.

    // 1. 충돌체(Sphere) 세팅
    CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
    CollisionComp->InitSphereRadius(5.0f);
    CollisionComp->BodyInstance.SetCollisionProfileName("Projectile");
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

    bIsActive = false;
}

void AGJProjectile::BeginPlay()
{
    Super::BeginPlay();
    // 시작 시 풀에 보관하기 위해 비활성화 상태로 둡니다.
    Deactivate();
}

void AGJProjectile::FireInDirection(const FVector& ShootDirection)
{
    bIsActive = true;

    // 숨김 해제 및 충돌 켜기
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);

    // 날아가기 시작
    ProjectileMovement->SetVelocityInLocalSpace(ShootDirection * ProjectileMovement->InitialSpeed);
    ProjectileMovement->Activate();
}

void AGJProjectile::Deactivate()
{
    bIsActive = false;

    // 화면에서 숨기고 충돌, 이동 연산을 끕니다 (Destroy 대신 사용)
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    ProjectileMovement->Deactivate();
    ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AGJProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // [데미지 처리 로직은 여기에 추가]
    // 예: UGameplayStatics::ApplyDamage(OtherActor, DamageAmount, ...);

    // 데미지를 준 후, 파괴하지 않고 풀로 돌려보냅니다(비활성화).
    Deactivate();
}