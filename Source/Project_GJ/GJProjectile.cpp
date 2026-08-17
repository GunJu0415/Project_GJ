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
    // 관통 구체용. 비관통일 때는 프로필이 BlockAllDynamic이라 오버랩이 안 생기므로 무해하다.
    CollisionComp->SetGenerateOverlapEvents(true);
    CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AGJProjectile::OnOverlapBegin);
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

void AGJProjectile::FireInDirection(const FVector& ShootDirection, float InDamage, float InSpeed, float InRange,
                                    float InScale, int32 InPierceCount)
{
    bIsActive = true;
    Damage = InDamage;
    RemainingPierce = InPierceCount;
    HitActors.Reset();

    // 콜리전과 메시가 같이 커진다. 스케일 대신 SphereRadius를 직접 만지면 메시가 안 따라온다.
    SetActorScale3D(FVector(FMath::Max(InScale, 0.01f)));

    // 숨김 해제 및 충돌 켜기
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);

    // 콜리전 설정은 SetActorEnableCollision 뒤에 해야 덮어써지지 않는다.
    if (InPierceCount != 0)
    {
        // 관통이면 폰을 Block으로 두면 안 된다. bShouldBounce=false라 블로킹 히트가 나는 순간
        // ProjectileMovement가 그 자리에서 멈춰서, Deactivate를 안 해도 구체가 적 앞에 박힌다.
        // 벽(WorldStatic)은 여전히 막아야 하므로 채널별로 따로 준다.
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);
        CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
        CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    }
    else
    {
        // 총알은 기존 동작 그대로. 풀에서 재사용되므로 매번 되돌려 놓아야 한다 -
        // 관통 스킬이 쓰고 반납한 구체를 총알이 집어가면 벽만 막는 채로 굳는다.
        CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CollisionComp->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    }

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

    // 풀로 돌아가는 객체다. 안 지우면 다음 발사가 이 적을 못 때린다.
    HitActors.Reset();
    RemainingPierce = 0;

    // 사거리 타이머가 아직 남아있다면 취소 (충돌로 먼저 비활성화된 경우, 이후 재사용 중에 뒤늦게 터지는 것 방지)
    GetWorldTimerManager().ClearTimer(RangeTimerHandle);

    // 화면에서 숨기고 충돌, 이동 연산을 끕니다 (Destroy 대신 사용)
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
    SetActorTickEnabled(false);
    ProjectileMovement->Deactivate();
    ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AGJProjectile::HandleTouch(AActor* OtherActor)
{
    // 자기 자신, 쏜 주체, 이미 때린 대상은 건너뛴다.
    if (!bIsActive || !OtherActor || OtherActor == this || OtherActor == GetInstigator())
    {
        return;
    }

    if (HitActors.Contains(OtherActor))
    {
        return;
    }

    // 맞은 대상이 AGJBaseCharacter(플레이어 또는 적)인지 확인합니다. 실패하면 nullptr.
    AGJBaseCharacter* HitCharacter = Cast<AGJBaseCharacter>(OtherActor);
    if (!HitCharacter)
    {
        return;
    }

    HitActors.Add(OtherActor);

    UGameplayStatics::ApplyDamage(
        HitCharacter,
        Damage,
        GetInstigatorController(), // Instigator가 없어도 안전하게 nullptr 반환 (GetInstigator()->GetController()는 Instigator가 없으면 크래시남)
        this,
        UDamageType::StaticClass()
    );

    // 관통 없음 -> 첫 적에서 소멸
    if (RemainingPierce == 0)
    {
        Deactivate();
        return;
    }

    // -1은 무한이므로 줄이지 않는다. 줄이면 -2, -3으로 내려가 "남았는지" 판정이 뒤집힌다.
    if (RemainingPierce > 0)
    {
        RemainingPierce--;
        if (RemainingPierce == 0)
        {
            // 관통 횟수를 다 쓴 뒤에도 이번 적은 이미 맞혔다. 여기서 끝낸다.
            Deactivate();
        }
    }
}

void AGJProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
    // 관통 구체는 폰을 오버랩으로 통과하므로 여기 들어오는 건 벽이다.
    // 비관통 구체(총알)는 폰도 여기로 들어온다.
    HandleTouch(OtherActor);

    // 벽에 맞았으면 HandleTouch가 아무것도 안 했으므로 여기서 끈다.
    // 투사체 비활성화 (오브젝트 풀링 사용 중이므로 Destroy() 대신 Deactivate 사용!)
    if (bIsActive)
    {
        Deactivate();
    }
}

void AGJProjectile::OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
                                   int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 관통 구체가 적을 통과할 때만 들어온다. 벽은 Block이라 OnHit으로 간다.
    HandleTouch(OtherActor);
}
