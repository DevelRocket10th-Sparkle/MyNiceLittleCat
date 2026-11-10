#include "Systems/CameraOcclusionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Characters/PlayerComponents/LockOnComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FName StairSurfaceTag(TEXT("OcclusionStairSurface"));

	FName GetCeilingFace(const UPrimitiveComponent* Piece)
	{
		for (const FName Tag : Piece->ComponentTags)
		{
			if (Tag.ToString().StartsWith(TEXT("OcclusionWallFace.Ceiling")))
			{
				return Tag;
			}
		}
		return NAME_None;
	}
}

UCameraOcclusionComponent::UCameraOcclusionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCameraOcclusionComponent::SetOcclusionSuspended(bool bSuspended)
{
	if (bOcclusionSuspended == bSuspended)
		return;

	bOcclusionSuspended = bSuspended;

	// 컷신 시작 전에 숨긴 메시 복원
	if (bOcclusionSuspended)
	{
		RestoreAllPieces();
	}
}

void UCameraOcclusionComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	Camera = Owner->FindComponentByClass<UCameraComponent>();
	LockOn = Owner->FindComponentByClass<ULockOnComponent>();
	GroupSubsystem = GetWorld()->GetSubsystem<UOcclusionGroupSubsystem>();
	if (!Camera.IsValid() || !GroupSubsystem.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] CameraOcclusion: camera or group subsystem missing"), *GetNameSafe(Owner));
		return;
	}
	GetWorld()->GetTimerManager().SetTimer(UpdateTimerHandle, this, &UCameraOcclusionComponent::UpdateOcclusion, UpdateInterval, true);
}

void UCameraOcclusionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimerHandle);
	}
	RestoreAllPieces();
	Super::EndPlay(EndPlayReason);
}

void UCameraOcclusionComponent::UpdateOcclusion()
{
	// 재생 중 숨기지 않도록 제한
	if (bOcclusionSuspended)
		return;

	// 화면을 보는 로컬 플레이어에서만 의미 있음
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}
	if (!Camera.IsValid() || !GroupSubsystem.IsValid())
	{
		return;
	}
	const FVector CameraLocation = Camera->GetComponentLocation();

	// 1. 대상별 Sweep으로 시야를 가린 조각 수집
	TArray<AActor*> Targets;
	CollectTargets(Targets);
	TSet<UPrimitiveComponent*> OccludingPieces;
	for (const AActor* Target : Targets)
	{
		SweepToTarget(CameraLocation, Target, OccludingPieces);
	}

	// 3. 새로 가린 조각은 숨기고, 이미 숨긴 조각은 가림 시각만 갱신
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	for (UPrimitiveComponent* Piece : OccludingPieces)
	{
		if (FHiddenPiece* Hidden = HiddenPieces.Find(Piece))
		{
			Hidden->LastOccludedTime = CurrentTime;
		}
		else
		{
			HidePiece(Piece, CurrentTime);
		}
	}

	// 4. 일정 시간 가리지 않은 조각 복구
	RestoreExpiredPieces(CurrentTime);
}

void UCameraOcclusionComponent::CollectTargets(TArray<AActor*>& OutTargets) const
{
	OutTargets.Add(GetOwner());
	if (!bIncludeLockedTarget || !LockOn.IsValid() || !LockOn->IsLockedOn())
	{
		return;
	}
	AActor* LockedTarget = LockOn->GetLockedTarget();
	if (IsValid(LockedTarget))
	{
		OutTargets.Add(LockedTarget);
	}
}

void UCameraOcclusionComponent::SweepToTarget(const FVector& CameraLocation, const AActor* Target, TSet<UPrimitiveComponent*>& OutOccludingPieces) const
{
	if (!IsValid(Target))
	{
		return;
	}
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToCamera = CameraLocation - TargetLocation;
	const float Distance = ToCamera.Size();
	if (Distance <= TargetEndOffset)
	{
		return;
	}
	// 대상 위치에서 카메라 쪽으로 TargetEndOffset만큼 당긴 지점까지만 Sweep
	const FVector SweepEnd = TargetLocation + ToCamera / Distance * TargetEndOffset;

	// 대상 발 높이: 캐릭터는 캡슐 아랫면, 그 외 액터는 단순 콜리전 원기둥 아랫면
	float CollisionRadius = 0.f;
	float CollisionHalfHeight = 0.f;
	Target->GetSimpleCollisionCylinder(CollisionRadius, CollisionHalfHeight);
	const float TargetFootZ = TargetLocation.Z - CollisionHalfHeight;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraOcclusionSweep), false, GetOwner());
	Params.AddIgnoredActor(Target);

	// 벽 / 천장은 가림 채널에 Overlap으로 반응하므로 첫 조각에서 멈추지 않고 경로상의 조각 전부 수집
	TArray<FHitResult> Hits;
	TSet<UPrimitiveComponent*> WallGroupPieces;
	GetWorld()->SweepMultiByChannel(Hits, CameraLocation, SweepEnd, FQuat::Identity,
		GroupSettings.TraceChannel, FCollisionShape::MakeSphere(SweepRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		UPrimitiveComponent* HitPiece = Hit.GetComponent();
		if (!IsValid(HitPiece))
		{
			continue;
		}
		if (HitPiece->ComponentHasTag(StairSurfaceTag))
		{
			// 경사진 계단은 밟고 있는 구간을 남기고, 머리 위에서 가리는 부분만 처리한다.
			if (Hit.ImpactPoint.Z < TargetFootZ + CeilingMinHeight)
			{
				continue;
			}

			const FVector TargetFoot(TargetLocation.X, TargetLocation.Y, TargetFootZ);
			FHitResult SupportHit;
			if (HitPiece->LineTraceComponent(SupportHit, TargetFoot + FVector(0.f, 0.f, 30.f),
				TargetFoot - FVector(0.f, 0.f, 10.f), Params))
			{
				continue;
			}
		}
		else if (!IsAboveTargetFloor(HitPiece, TargetFootZ))
		{
			continue;
		}
		TSet<UPrimitiveComponent*>& Pieces = GetCeilingFace(HitPiece).IsNone() ? WallGroupPieces : OutOccludingPieces;
		GroupSubsystem->CollectGroupMembers(HitPiece, GroupSettings, Pieces);
	}

	// 외벽은 여러 방에 걸치므로 벽과 함께 숨길 천장은 대상 바로 위의 면으로 제한한다.
	TMap<FName, FBox> CeilingBounds;
	TMap<UPrimitiveComponent*, FName> LinkedCeilingPieces;
	for (UPrimitiveComponent* Piece : WallGroupPieces)
	{
		if (!IsAboveTargetFloor(Piece, TargetFootZ))
		{
			continue;
		}
		const FName CeilingFace = GetCeilingFace(Piece);
		if (CeilingFace.IsNone())
		{
			OutOccludingPieces.Add(Piece);
			continue;
		}
		CeilingBounds.FindOrAdd(CeilingFace, FBox(ForceInit)) += Piece->Bounds.GetBox();
		LinkedCeilingPieces.Add(Piece, CeilingFace);
	}

	FName NearestCeiling = NAME_None;
	double NearestCeilingZ = TNumericLimits<double>::Max();
	for (const TPair<FName, FBox>& Entry : CeilingBounds)
	{
		if (Entry.Value.IsInsideOrOnXY(TargetLocation) && Entry.Value.Min.Z < NearestCeilingZ)
		{
			NearestCeiling = Entry.Key;
			NearestCeilingZ = Entry.Value.Min.Z;
		}
	}
	for (const TPair<UPrimitiveComponent*, FName>& Entry : LinkedCeilingPieces)
	{
		if (Entry.Value == NearestCeiling)
		{
			OutOccludingPieces.Add(Entry.Key);
		}
	}

	if (bDrawDebug)
	{
		DrawDebugLine(GetWorld(), CameraLocation, SweepEnd, FColor::Yellow, false, UpdateInterval);
		DrawDebugSphere(GetWorld(), SweepEnd, SweepRadius, 12, FColor::Yellow, false, UpdateInterval);
		for (const FHitResult& Hit : Hits)
		{
			DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 12.f, FColor::Red, false, UpdateInterval);
		}
	}
}

bool UCameraOcclusionComponent::IsAboveTargetFloor(const UPrimitiveComponent* Piece, float TargetFootZ) const
{
	// 벽은 아랫면이 발 높이와 같으므로 높이 조건을 적용하지 않음
	if (!GroupSubsystem->IsHorizontalPiece(Piece, GroupSettings))
	{
		return true;
	}
	const float PieceBottomZ = Piece->Bounds.Origin.Z - Piece->Bounds.BoxExtent.Z;
	return PieceBottomZ >= TargetFootZ + CeilingMinHeight;
}

void UCameraOcclusionComponent::HidePiece(UPrimitiveComponent* Piece, float CurrentTime)
{
	FHiddenPiece& Hidden = HiddenPieces.Add(Piece);
	Hidden.bWasRenderInMainPass = Piece->bRenderInMainPass;
	Hidden.bWasRenderInDepthPass = Piece->bRenderInDepthPass;
	Hidden.LastOccludedTime = CurrentTime;
	// 화면에서만 숨김: 그림자, 콜리전은 유지되어 실내 음영이 바뀌지 않고 Sweep에도 계속 잡힘
	Piece->SetRenderInMainPass(false);
	// 메인 패스를 꺼도 깊이 패스는 기본으로 켜져 있어, 끄지 않으면 색 없는 깊이만 남아 검은 면으로 가림
	Piece->SetRenderInDepthPass(false);
}

void UCameraOcclusionComponent::RestorePiece(UPrimitiveComponent* Piece, const FHiddenPiece& Hidden)
{
	Piece->SetRenderInMainPass(Hidden.bWasRenderInMainPass);
	Piece->SetRenderInDepthPass(Hidden.bWasRenderInDepthPass);
}

void UCameraOcclusionComponent::RestoreExpiredPieces(float CurrentTime)
{
	for (TMap<TWeakObjectPtr<UPrimitiveComponent>, FHiddenPiece>::TIterator It = HiddenPieces.CreateIterator(); It; ++It)
	{
		UPrimitiveComponent* Piece = It.Key().Get();
		// 스트리밍 해제 등으로 사라진 조각은 목록에서만 제거
		if (!Piece)
		{
			It.RemoveCurrent();
			continue;
		}
		const FHiddenPiece& Hidden = It.Value();
		if (CurrentTime - Hidden.LastOccludedTime < RestoreDelay)
		{
			continue;
		}
		RestorePiece(Piece, Hidden);
		It.RemoveCurrent();
	}
}

void UCameraOcclusionComponent::RestoreAllPieces()
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, FHiddenPiece>& Pair : HiddenPieces)
	{
		UPrimitiveComponent* Piece = Pair.Key.Get();
		if (Piece)
		{
			RestorePiece(Piece, Pair.Value);
		}
	}
	HiddenPieces.Reset();
}
