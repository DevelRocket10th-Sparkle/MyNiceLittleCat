#include "Systems/CameraOcclusionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Characters/PlayerComponents/LockOnComponent.h"
#include "Characters/Camera/RoomMaskVolume.h"
#include "Components/MeshComponent.h"
#include "Components/LocalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

namespace
{
	// 방 컷어웨이 후보: 화면에 보이는 메시만, 캐릭터 / 펫 등 Pawn과 거기 붙은 액터, 방에서 제외 지정한 액터는 제외
	bool IsRoomCutawayCandidate(const UPrimitiveComponent* Piece, const AActor* Owner, const ARoomMaskVolume* Room)
	{
		if (!Piece->IsA<UMeshComponent>() || !Piece->IsRegistered() || !Piece->IsVisible() || Piece->bHiddenInGame)
		{
			return false;
		}
		const AActor* AttachRoot = Owner;
		while (AttachRoot->GetAttachParentActor())
		{
			AttachRoot = AttachRoot->GetAttachParentActor();
		}
		return !AttachRoot->IsA<APawn>() && !AttachRoot->IsA<ARoomMaskVolume>() && !Room->IsCutawayIgnored(AttachRoot);
	}

	// 그림자 컬링 후보 조명: 보이는 상태로 그림자를 그리는 점 / 스포트 조명만. 캐릭터 / 펫 등 Pawn에 붙은 조명과 방에서 제외 지정한 액터는 제외
	bool IsLightShadowCullCandidate(const ULocalLightComponent* Light, const AActor* Owner, const ARoomMaskVolume* Room)
	{
		if (!Light->IsRegistered() || !Light->IsVisible() || !Light->CastShadows || Light->AttenuationRadius <= 0.f)
		{
			return false;
		}
		const AActor* AttachRoot = Owner;
		while (AttachRoot->GetAttachParentActor())
		{
			AttachRoot = AttachRoot->GetAttachParentActor();
		}
		return !AttachRoot->IsA<APawn>() && !Room->IsCutawayIgnored(AttachRoot);
	}

	bool IsInsideAnyOtherBox(const FVector& Point, const TArray<FBox>& Boxes, int32 SkipIndex)
	{
		for (int32 i = 0; i < Boxes.Num(); ++i)
		{
			if (i != SkipIndex && Boxes[i].IsInside(Point))
			{
				return true;
			}
		}
		return false;
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
	// 컷신 카메라는 다른 방도 비추므로 꺼 둔 조명 그림자도 복원
	if (bOcclusionSuspended)
	{
		RestoreAllPieces();
		RestoreCulledLightShadows();
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
	RestoreCulledLightShadows();
	RoomCaches.Reset();
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

	// 1. 방 안이면 카메라 쪽 벽면 / 천장 / 윗층 / 방 밖을 방 기준으로 수집
	ARoomMaskVolume::UpdatePlayerLocation(OwnerPawn->GetActorLocation());
	TSet<UPrimitiveComponent*> OccludingPieces;
	CollectRoomCutaway(CameraLocation, OccludingPieces);

	// 2. 방 밖이면 대상별 Sweep으로 시야를 가린 조각 수집
	//    방 안에서는 Sweep을 쓰지 않음: 가까이 간 가구 / 계단이 사라지는 것 방지
	if (!bInCutawayRoom)
	{
		TArray<AActor*> Targets;
		CollectTargets(Targets);
		for (const AActor* Target : Targets)
		{
			SweepToTarget(CameraLocation, Target, OccludingPieces);
		}
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
	TSet<UPrimitiveComponent*> GroupPieces;
	GetWorld()->SweepMultiByChannel(Hits, CameraLocation, SweepEnd, FQuat::Identity,
		GroupSettings.TraceChannel, FCollisionShape::MakeSphere(SweepRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		UPrimitiveComponent* HitPiece = Hit.GetComponent();
		if (!IsValid(HitPiece))
		{
			continue;
		}
		if (!IsAboveTargetFloor(HitPiece, TargetFootZ))
		{
			continue;
		}
		// 대상이 밟고 있는 조각 (계단, 경사로)은 발 높이 아래로 맞은 경우 숨기지 않음
		if (Hit.ImpactPoint.Z < TargetFootZ + CeilingMinHeight)
		{
			const FVector TargetFoot(TargetLocation.X, TargetLocation.Y, TargetFootZ);
			FHitResult SupportHit;
			if (HitPiece->LineTraceComponent(SupportHit, TargetFoot + FVector(0.f, 0.f, 30.f),
				TargetFoot - FVector(0.f, 0.f, 10.f), Params))
			{
				continue;
			}
		}
		// 맞은 조각과 같은 평면에 이어진 조각까지 묶어서 숨김
		GroupPieces.Reset();
		GroupSubsystem->CollectGroupMembers(HitPiece, GroupSettings, GroupPieces);
		for (UPrimitiveComponent* Piece : GroupPieces)
		{
			if (IsAboveTargetFloor(Piece, TargetFootZ))
			{
				OutOccludingPieces.Add(Piece);
			}
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

void UCameraOcclusionComponent::CollectRoomCutaway(const FVector& CameraLocation, TSet<UPrimitiveComponent*>& OutOccludingPieces)
{
	bInCutawayRoom = false;
	if (!RoomCutaway.bEnabled)
	{
		RestoreCulledLightShadows();
		return;
	}
	ARoomMaskVolume* Room = ARoomMaskVolume::GetCurrentRoom();
	if (!Room || Room->GetWorld() != GetWorld() || !Room->IsCutawayEnabled())
	{
		// 방 밖 (마당, 옥상)에서는 창 너머 실내가 보일 수 있으므로 조명 그림자를 모두 원래대로 유지
		RestoreCulledLightShadows();
		return;
	}
	bInCutawayRoom = true;
	const FRoomCutawayCache& Cache = GetOrBuildRoomCache(Room);

	// 방에 닿지 않는 조명의 그림자 끄기 (방이 바뀔 때만 실제 변경)
	ApplyLightShadowCulling(Room, Cache);

	// 천장 / 윗층, 방 밖 (옆방, 건물 밖)은 카메라 각도와 관계없이 항상 숨김
	for (const TWeakObjectPtr<UPrimitiveComponent>& Piece : Cache.TopPieces)
	{
		if (Piece.IsValid())
		{
			OutOccludingPieces.Add(Piece.Get());
		}
	}
	if (RoomCutaway.bHideOutsideRoom)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Piece : Cache.OutsidePieces)
		{
			if (Piece.IsValid())
			{
				OutOccludingPieces.Add(Piece.Get());
			}
		}
	}

	// 옆면은 플레이어 기준 카메라가 있는 쪽만 숨김
	static const FVector SideNormals[FRoomCutawayCache::SideCount] =
	{
		FVector(-1.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), FVector(0.f, 1.f, 0.f)
	};
	const FVector ToCamera = (CameraLocation - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	for (int32 Side = 0; Side < FRoomCutawayCache::SideCount; ++Side)
	{
		if (FVector::DotProduct(SideNormals[Side], ToCamera) <= RoomCutaway.FaceDotThreshold)
		{
			continue;
		}
		for (const TWeakObjectPtr<UPrimitiveComponent>& Piece : Cache.SidePieces[Side])
		{
			if (Piece.IsValid())
			{
				OutOccludingPieces.Add(Piece.Get());
			}
		}
	}

	if (bDrawDebug)
	{
		TArray<FBox> Boxes;
		Room->GetRoomBounds(Boxes);
		for (const FBox& Box : Boxes)
		{
			DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Cyan, false, UpdateInterval);
		}
	}
}

const FRoomCutawayCache& UCameraOcclusionComponent::GetOrBuildRoomCache(ARoomMaskVolume* Room)
{
	if (const FRoomCutawayCache* Found = RoomCaches.Find(Room))
	{
		return *Found;
	}
	FRoomCutawayCache& Cache = RoomCaches.Add(Room);

	TArray<FBox> Boxes;
	Room->GetRoomBounds(Boxes);
	const FRoomCutawaySettings& S = RoomCutaway;

	// 방을 처음 들어갈 때 한 번만 월드 메시를 훑어 면별로 분류
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		const AActor* Actor = *It;

		// 조명: 빛이 닿는 범위 (감쇠 반경 구)가 방 박스 (+여유)와 하나도 겹치지 않으면 그림자 컬링 대상
		TInlineComponentArray<ULocalLightComponent*> Lights;
		Actor->GetComponents(Lights);
		for (ULocalLightComponent* Light : Lights)
		{
			if (!IsLightShadowCullCandidate(Light, Actor, Room))
			{
				continue;
			}
			const FVector LightCenter = Light->GetComponentLocation();
			const FVector::FReal RadiusSquared = FMath::Square(static_cast<FVector::FReal>(Light->AttenuationRadius));
			bool bReachesRoom = false;
			for (const FBox& Box : Boxes)
			{
				if (FMath::SphereAABBIntersection(LightCenter, RadiusSquared, Box.ExpandBy(S.LightShadowMargin)))
				{
					bReachesRoom = true;
					break;
				}
			}
			if (!bReachesRoom)
			{
				Cache.OutsideShadowLights.Add(Light);
			}
		}

		TInlineComponentArray<UPrimitiveComponent*> Pieces;
		Actor->GetComponents(Pieces);
		for (UPrimitiveComponent* Piece : Pieces)
		{
			if (!IsRoomCutawayCandidate(Piece, Actor, Room))
			{
				continue;
			}
			const FBox PieceBox = Piece->Bounds.GetBox();
			const FVector PieceCenter = PieceBox.GetCenter();

			// 방 박스 (XY만 Inset만큼 축소)와 전혀 겹치지 않으면 방 밖 조각
			bool bTouchesRoom = false;
			bool bNearRoom = false;
			for (const FBox& Box : Boxes)
			{
				const FVector Inset(S.OutsideInset, S.OutsideInset, 0.f);
				if (FBox(Box.Min + Inset, Box.Max - Inset).Intersect(PieceBox))
				{
					bTouchesRoom = true;
					break;
				}
				bNearRoom |= Box.ExpandBy(S.OutsideRadius).Intersect(PieceBox);
			}
			if (!bTouchesRoom)
			{
				// 윗층 판정에 해당하면 TopPieces로, 아니면 OutsidePieces로 분류
				bool bIsTop = false;
				for (const FBox& Box : Boxes)
				{
					const FBox Footprint = Box.ExpandBy(FVector(S.TopFootprintMargin, S.TopFootprintMargin, 0.f));
					if (PieceCenter.Z >= Box.Max.Z - S.TopTolerance &&
						PieceBox.Min.X < Footprint.Max.X && PieceBox.Max.X > Footprint.Min.X &&
						PieceBox.Min.Y < Footprint.Max.Y && PieceBox.Max.Y > Footprint.Min.Y)
					{
						bIsTop = true;
						break;
					}
				}
				if (bIsTop)
				{
					Cache.TopPieces.AddUnique(Piece);
				}
				else if (bNearRoom)
				{
					Cache.OutsidePieces.AddUnique(Piece);
				}
				continue;
			}

			for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
			{
				const FBox& Box = Boxes[BoxIndex];

				// 윗층: 중심이 박스 윗면 근처 이상이고, 방 바닥 면적 (+여유) 위에 걸친 메시
				if (PieceCenter.Z >= Box.Max.Z - S.TopTolerance)
				{
					const FBox Footprint = Box.ExpandBy(FVector(S.TopFootprintMargin, S.TopFootprintMargin, 0.f));
					if (PieceBox.Min.X < Footprint.Max.X && PieceBox.Max.X > Footprint.Min.X &&
						PieceBox.Min.Y < Footprint.Max.Y && PieceBox.Max.Y > Footprint.Min.Y)
					{
						Cache.TopPieces.AddUnique(Piece);
						break;
					}
					continue;
				}
				// 방 바닥보다 아래 (기초, 지면 소품)는 제외
				if (PieceCenter.Z < Box.Min.Z)
				{
					continue;
				}
				// L / T자 방에서 박스끼리 맞닿은 안쪽 면 조각 제외
				if (IsInsideAnyOtherBox(PieceCenter, Boxes, BoxIndex))
				{
					continue;
				}

				for (int32 Side = 0; Side < FRoomCutawayCache::SideCount; ++Side)
				{
					const int32 Axis = Side / 2;          // 0 = X, 1 = Y
					const int32 LateralAxis = 1 - Axis;
					const bool bMinSide = (Side % 2) == 0;

					// 면에서 방 안쪽으로 들어온 깊이: 음수면 면 바깥 (벽 두께, 외벽 장식)
					const double Reach = bMinSide ? PieceBox.Max[Axis] - Box.Min[Axis] : Box.Max[Axis] - PieceBox.Min[Axis];
					// 면을 뚫고 벽 안 / 밖으로 나간 깊이: 벽난로, 문틀처럼 벽에 박힌 메시 판별
					const double Embed = bMinSide ? Box.Min[Axis] - PieceBox.Min[Axis] : PieceBox.Max[Axis] - Box.Max[Axis];
					const bool bAttached = Reach <= S.InnerDepth;
					const bool bEmbedded = Embed >= S.WallEmbedDepth && Reach <= S.MaxEmbeddedReach;
					if ((!bAttached && !bEmbedded) || Reach < -S.OuterDepth)
					{
						continue;
					}
					// 면의 가로 범위와 겹쳐야 함: 옆방 벽까지 번지는 것 방지
					if (PieceBox.Max[LateralAxis] <= Box.Min[LateralAxis] || PieceBox.Min[LateralAxis] >= Box.Max[LateralAxis])
					{
						continue;
					}
					Cache.SidePieces[Side].AddUnique(Piece);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RoomCutaway] %s: top %d, outside %d, -X %d, +X %d, -Y %d, +Y %d, shadow-culled lights %d"), *Room->GetActorNameOrLabel(),
		Cache.TopPieces.Num(), Cache.OutsidePieces.Num(), Cache.SidePieces[0].Num(), Cache.SidePieces[1].Num(), Cache.SidePieces[2].Num(), Cache.SidePieces[3].Num(),
		Cache.OutsideShadowLights.Num());
	return Cache;
}

void UCameraOcclusionComponent::ApplyLightShadowCulling(ARoomMaskVolume* Room, const FRoomCutawayCache& Cache)
{
	if (!RoomCutaway.bCullOutsideLightShadows)
	{
		RestoreCulledLightShadows();
		return;
	}
	// 같은 방이면 이미 적용된 상태: 매 판정마다 렌더 상태를 다시 만들지 않음
	if (LightCullRoom.Get() == Room)
	{
		return;
	}
	// 이전 방 기준으로 끈 조명 복구 후 새 방 기준으로 다시 끔
	RestoreCulledLightShadows();
	LightCullRoom = Room;
	for (const TWeakObjectPtr<ULocalLightComponent>& WeakLight : Cache.OutsideShadowLights)
	{
		ULocalLightComponent* Light = WeakLight.Get();
		// 다른 기능이 이미 그림자를 끈 조명은 건드리지 않음: 복구 때 켜 버리지 않도록 목록에 넣지 않음
		if (!Light || !Light->CastShadows)
		{
			continue;
		}
		Light->SetCastShadows(false);
		CulledShadowLights.Add(Light);
	}
}

void UCameraOcclusionComponent::RestoreCulledLightShadows()
{
	for (const TWeakObjectPtr<ULocalLightComponent>& WeakLight : CulledShadowLights)
	{
		if (ULocalLightComponent* Light = WeakLight.Get())
		{
			Light->SetCastShadows(true);
		}
	}
	CulledShadowLights.Reset();
	LightCullRoom.Reset();
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
