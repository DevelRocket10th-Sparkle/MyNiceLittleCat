// Fill out your copyright notice in the Description page of Project Settings.


#include "Gimmicks/Boundary/SpikeBoundary.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Materials/MaterialInterface.h"

#include "Enemy/Combat/EnemyTargetUtils.h"

namespace
{
	// 이 액터가 만든 가시 컴포넌트 표시: 다시 만들 때 이전 가시를 찾아 지움 (패키지 빌드에서 Transient 배열이 비어 있어도)
	const FName GeneratedSpikeTag(TEXT("GeneratedBoundarySpike"));
}

ASpikeBoundary::ASpikeBoundary()
{
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 기본 모양: 10m 정사각형 (선형 점, 닫힌 루프)
	BoundarySpline = CreateDefaultSubobject<USplineComponent>(TEXT("BoundarySpline"));
	BoundarySpline->SetupAttachment(RootComponent);
	BoundarySpline->ClearSplinePoints(false);
	const FVector Corners[] = { FVector(-500.f, -500.f, 0.f), FVector(500.f, -500.f, 0.f), FVector(500.f, 500.f, 0.f), FVector(-500.f, 500.f, 0.f) };
	for (const FVector& Corner : Corners)
	{
		BoundarySpline->AddSplinePoint(Corner, ESplineCoordinateSpace::Local, false);
	}
	for (int32 Index = 0; Index < BoundarySpline->GetNumberOfSplinePoints(); ++Index)
	{
		BoundarySpline->SetSplinePointType(Index, ESplinePointType::Linear, false);
	}
	BoundarySpline->SetClosedLoop(true, true);

	// 벽: Pawn만 막고 나머지(투사체, 레이저 트레이스, 카메라)는 통과
	Walls = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Walls"));
	Walls->SetupAttachment(RootComponent);
	Walls->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Walls->SetCollisionObjectType(ECC_WorldStatic);
	Walls->SetCollisionResponseToAllChannels(ECR_Ignore);
	Walls->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	Walls->SetCastShadow(false);
	Walls->SetNotifyRigidBodyCollision(true);
	Walls->SetHiddenInGame(true);
}

void ASpikeBoundary::BeginPlay()
{
	Super::BeginPlay();

	// 패키지에 저장된 액터도 가시를 다시 생성
	Rebuild();

	if ((bDamageOnTouch || bKnockback) && bBlockPawns)
	{
		// 캐릭터 이동이 벽에 막힐 때 들어오는 충돌 이벤트
		Walls->OnComponentHit.AddDynamic(this, &ASpikeBoundary::HandleWallHit);
	}
}

void ASpikeBoundary::HandleWallHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent,
	FVector NormalImpulse, const FHitResult& Hit)
{
	// 플레이어가 조종하는 폰만: 보스·적(AI)과 동전 같은 물리 오브젝트는 벽에 막히기만 함
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn || !Pawn->IsPlayerControlled() || !EnemyTargetUtils::IsAttackableTarget(OtherActor))
	{
		return;
	}

	// 피격·넉백 방향: 닿은 지점 → 대상 (경계 안쪽으로 밀려나는 방향)
	FVector Direction = (OtherActor->GetActorLocation() - Hit.ImpactPoint).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		Direction = -OtherActor->GetActorForwardVector().GetSafeNormal2D();
	}

	// 피해: 대상별 대기 시간마다 한 번
	const double Now = GetWorld()->GetTimeSeconds();
	const double* LastTime = LastDamageTimes.Find(OtherActor);
	if (bDamageOnTouch && (!LastTime || Now - *LastTime >= DamageCooldown))
	{
		LastDamageTimes.Add(OtherActor, Now);
		EnemyTargetUtils::ApplyAttackHit(OtherActor, TouchDamage, Direction, this);
	}

	// 넉백: 닿을 때마다 (피해 처리의 플레이어 넉백보다 나중에 호출되어 이 값이 적용됨)
	if (bKnockback)
	{
		if (ACharacter* Character = Cast<ACharacter>(OtherActor))
		{
			Character->LaunchCharacter(Direction * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpwardStrength), true, true);
		}
	}
}

void ASpikeBoundary::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void ASpikeBoundary::Rebuild()
{
	BuildSpikes();
	BuildWalls();
}

void ASpikeBoundary::BuildSpikes()
{
	// 로드 후 Transient 배열이 비어 있어도 기존 가시를 찾아 정리
	TArray<UInstancedStaticMeshComponent*> ExistingComponents;
	GetComponents(ExistingComponents);

	for (UInstancedStaticMeshComponent* Component : ExistingComponents)
	{
		if (IsValid(Component)
			&& Component != Walls
			&& (SpikeComponents.Contains(Component)
				|| Component->ComponentHasTag(GeneratedSpikeTag)
				|| (Component->CreationMethod == EComponentCreationMethod::UserConstructionScript
					&& SpikeMeshes.Contains(Component->GetStaticMesh())
					&& Component->GetCollisionEnabled() == ECollisionEnabled::NoCollision
					)
				)
			)
		{
			Component->DestroyComponent();
		}
	}
	SpikeComponents.Reset();

	// 유효한 메시마다 인스턴스 컴포넌트 하나: 같은 메시 가시는 한 번에 그려짐
	TArray<UStaticMesh*> Meshes;
	for (UStaticMesh* Mesh : SpikeMeshes)
	{
		if (!Mesh)
		{
			continue;
		}

		UInstancedStaticMeshComponent* Component = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transactional);

		RemoveOwnedComponent(Component);
		Component->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		AddOwnedComponent(Component);
		Component->ComponentTags.Add(GeneratedSpikeTag);
		
		Component->SetupAttachment(RootComponent);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		if (SpikeMaterial)
		{
			for (int32 Slot = 0; Slot < Mesh->GetStaticMaterials().Num(); ++Slot)
			{
				Component->SetMaterial(Slot, SpikeMaterial);
			}
		}
		Component->RegisterComponent();

		Meshes.Add(Mesh);
		SpikeComponents.Add(Component);
	}

	if (Meshes.IsEmpty())
	{
		return;
	}

	const float Length = BoundarySpline->GetSplineLength();
	const int32 Count = FMath::FloorToInt(Length / FMath::Max(SpikeSpacing, 10.f));
	const float RowSide = bFlipRowSide ? -1.f : 1.f;
	FRandomStream Stream(RandomSeed);

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Distance = Index * SpikeSpacing;
		const FVector Location = BoundarySpline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
		const FVector Right = BoundarySpline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);
		const float SplineYaw = BoundarySpline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local).Yaw;

		for (int32 Row = 0; Row < FMath::Max(SpikeRows, 1); ++Row)
		{
			// 바깥 줄은 반 칸씩 엇갈려 빈틈을 줄임
			const float Along = (Row % 2 == 1) ? SpikeSpacing * 0.5f : 0.f;
			const FVector Tangent = BoundarySpline->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::Local);

			// 메시 무작위 선택
			const int32 MeshIndex = Stream.RandRange(0, Meshes.Num() - 1);
			// 메시 바닥(로컬 최소 Z)이 경계선 높이에 오도록 올림
			const float MeshBottom = Meshes[MeshIndex]->GetBounds().GetBox().Min.Z;

			const float Jitter = PositionJitter > 0.f ? Stream.FRandRange(-PositionJitter, PositionJitter) : 0.f;
			const float ScaleFactor = 1.f + Stream.FRandRange(-ScaleJitter, ScaleJitter);
			const float Yaw = bRandomYaw ? Stream.FRandRange(0.f, 360.f) : SplineYaw;

			const FVector InstanceScale = SpikeScale * ScaleFactor;
			FVector InstanceLocation = Location
				+ Tangent * Along
				+ Right * (Row * RowSpacing * RowSide + Jitter);
			InstanceLocation.Z -= MeshBottom * InstanceScale.Z;

			SpikeComponents[MeshIndex]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), InstanceLocation, InstanceScale), false);
		}
	}
}

void ASpikeBoundary::BuildWalls()
{
	Walls->ClearInstances();
	Walls->SetStaticMesh(WallMesh);
	Walls->SetHiddenInGame(!bShowWallsInGame);
	Walls->SetCollisionEnabled(bBlockPawns ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);

	// 지정한 채널만 막고 나머지는 통과
	Walls->SetCollisionResponseToAllChannels(ECR_Ignore);
	for (const TEnumAsByte<ECollisionChannel>& Channel : BlockedChannels)
	{
		Walls->SetCollisionResponseToChannel(Channel, ECR_Block);
	}

	if (!WallMesh || !bBlockPawns)
	{
		return;
	}

	// 메시 크기를 기준으로 늘림 (기본 큐브는 100cm)
	const FVector MeshSize = WallMesh->GetBounds().GetBox().GetSize();
	if (MeshSize.X <= KINDA_SMALL_NUMBER || MeshSize.Y <= KINDA_SMALL_NUMBER || MeshSize.Z <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector MeshCenter = WallMesh->GetBounds().GetBox().GetCenter();

	const float Length = BoundarySpline->GetSplineLength();
	const float SegmentLength = FMath::Max(WallSegmentLength, 20.f);
	const int32 SegmentCount = FMath::CeilToInt(Length / SegmentLength);

	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Start = Index * SegmentLength;
		const float End = FMath::Min(Start + SegmentLength, Length);
		if (End - Start < 1.f)
		{
			continue;
		}

		const FVector P0 = BoundarySpline->GetLocationAtDistanceAlongSpline(Start, ESplineCoordinateSpace::Local);
		const FVector P1 = BoundarySpline->GetLocationAtDistanceAlongSpline(End, ESplineCoordinateSpace::Local);
		const FVector Direction = P1 - P0;
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		// 조각 사이 틈이 없도록 두께만큼 겹침
		const FVector Scale((Direction.Size() + WallThickness) / MeshSize.X, WallThickness / MeshSize.Y, WallHeight / MeshSize.Z);
		const FRotator Rotation = Direction.Rotation();
		const FVector Center = (P0 + P1) * 0.5f + FVector(0.f, 0.f, WallHeight * 0.5f);

		// 메시 피벗이 중심이 아니어도 벽 중심이 경계선 위에 오도록 보정
		const FVector Location = Center - Rotation.RotateVector(MeshCenter * Scale);

		Walls->AddInstance(FTransform(Rotation, Location, Scale), false);
	}
}
