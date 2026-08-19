#include "GJBoxRoom.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

AGJBoxRoom::AGJBoxRoom()
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (CubeFinder.Succeeded())
    {
        CubeMesh = CubeFinder.Object;
    }
}

void AGJBoxRoom::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RebuildGeometry();
}

void AGJBoxRoom::RebuildGeometry()
{
    // 이전에 만든 것을 먼저 지운다. 안 그러면 파라미터를 고칠 때마다 벽이 겹쳐 쌓인다.
    for (UStaticMeshComponent* Part : GeneratedParts)
    {
        if (Part)
        {
            Part->DestroyComponent();
        }
    }
    GeneratedParts.Reset();

    if (!CubeMesh)
    {
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: CubeMesh가 없어 지오메트리를 만들지 못했습니다."), *GetName());
        return;
    }

    const float T = WallThickness;

    // 바깥 치수 = 안쪽 + 양쪽 벽 두께. 모서리가 딱 맞물린다.
    const float SpanX = InteriorSize.X + T * 2.f;
    const float SpanY = InteriorSize.Y + T * 2.f;

    // 벽 중심은 안쪽 끝에서 두께의 절반만큼 더 바깥.
    const float WallOffX = InteriorSize.X * 0.5f + T * 0.5f;
    const float WallOffY = InteriorSize.Y * 0.5f + T * 0.5f;
    const float WallCenterZ = WallHeight * 0.5f;

    // 바닥 윗면이 정확히 Z=0이 되게 절반만큼 내린다. 스폰 포인트가 Z=100이라
    // 적이 1m 위에서 떨어지며 안정적으로 착지한다.
    AddBox(TEXT("Floor"),
        FVector(SpanX, SpanY, FloorThickness),
        FVector(0.f, 0.f, -FloorThickness * 0.5f));

    AddBox(TEXT("Wall_West"),  FVector(T, SpanY, WallHeight), FVector(-WallOffX, 0.f, WallCenterZ));
    AddBox(TEXT("Wall_East"),  FVector(T, SpanY, WallHeight), FVector( WallOffX, 0.f, WallCenterZ));
    AddBox(TEXT("Wall_South"), FVector(SpanX, T, WallHeight), FVector(0.f, -WallOffY, WallCenterZ));

    // 북쪽만 문 폭만큼 비우고 두 조각으로 나눈다. Task 3의 출구가 이 구멍에 들어간다.
    const float SegLen = (SpanX - DoorWidth) * 0.5f;
    if (SegLen > 0.f)
    {
        const float SegCenter = (DoorWidth + SegLen) * 0.5f;
        AddBox(TEXT("Wall_North_L"), FVector(SegLen, T, WallHeight), FVector(-SegCenter, WallOffY, WallCenterZ));
        AddBox(TEXT("Wall_North_R"), FVector(SegLen, T, WallHeight), FVector( SegCenter, WallOffY, WallCenterZ));
    }
    else
    {
        // 문이 벽보다 넓으면 북쪽이 통째로 뚫린다. 조용히 넘어가면 왜 뚫렸는지 못 찾는다.
        UE_LOG(LogTemp, Warning, TEXT("[ROOM] %s: DoorWidth(%.0f)가 벽 길이(%.0f)보다 넓어 북쪽 벽이 없습니다."),
            *GetName(), DoorWidth, SpanX);
    }
}

UStaticMeshComponent* AGJBoxRoom::AddBox(FName ComponentName, const FVector& Size, const FVector& Location)
{
    UStaticMeshComponent* Box = NewObject<UStaticMeshComponent>(this, ComponentName);
    if (!Box)
    {
        return nullptr;
    }

    Box->SetStaticMesh(CubeMesh);
    Box->SetupAttachment(RootComponent);

    // Task B가 방을 런타임에 스폰하므로 Static이면 "이동한 스태틱 컴포넌트" 경고가 난다.
    Box->SetMobility(EComponentMobility::Movable);

    Box->SetRelativeLocation(Location);

    // 엔진 기본 큐브가 100 유닛 정육면체라, 원하는 크기를 100으로 나눈 값이 스케일이다.
    Box->SetRelativeScale3D(Size / 100.f);

    Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Box->SetCollisionProfileName(TEXT("BlockAll"));

    Box->RegisterComponent();

    GeneratedParts.Add(Box);
    return Box;
}
