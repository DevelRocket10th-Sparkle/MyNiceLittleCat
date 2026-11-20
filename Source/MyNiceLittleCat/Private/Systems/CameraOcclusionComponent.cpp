#include "Systems/CameraOcclusionComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "Characters/PlayerComponents/LockOnComponent.h"
#include "Characters/Camera/RoomMaskVolume.h"
#include "Components/LocalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Cinematics/CinematicSubsystem.h"

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

	// 컷신 카메라가 벽 너머를 비출 때 플레이어 기준 가림 처리가 화면을 가리지 않도록 컷신 동안 정지
	// - 컷신 도중 리스폰된 폰도 맞도록 현재 상태를 바로 반영
	if (UCinematicSubsystem* Cinematics = GetWorld()->GetSubsystem<UCinematicSubsystem>())
	{
		CutsceneStateChangedHandle = Cinematics->OnCutsceneStateChanged.AddUObject(this, &UCameraOcclusionComponent::SetOcclusionSuspended);
		SetOcclusionSuspended(Cinematics->IsCutscenePlaying());
	}
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
		if (UCinematicSubsystem* Cinematics = World->GetSubsystem<UCinematicSubsystem>())
			Cinematics->OnCutsceneStateChanged.Remove(CutsceneStateChangedHandle);
	}
	CutsceneStateChangedHandle.Reset();
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
	const float CurrentTime = GetWorld()->GetTimeSeconds();

	// 1. 방 안이면 카메라 쪽 벽면 / 천장 / 윗층 / 방 밖을 방 기준으로 숨김 (방이나 카메라 쪽 면이 바뀔 때만 다시 모음)
	ARoomMaskVolume::UpdatePlayerLocation(OwnerPawn->GetActorLocation());
	const bool bInCutawayRoom = UpdateRoomCutaway(CameraLocation, CurrentTime);

	// 2. 방 밖이면 대상별 Sweep으로 시야를 가린 조각 수집
	//    방 안에서는 Sweep을 쓰지 않음: 가까이 간 가구 / 계단이 사라지는 것 방지
	if (!bInCutawayRoom)
	{
		TArray<AActor*> Targets;
		CollectTargets(Targets);

		TSet<UPrimitiveComponent*> OccludingPieces;
		for (const AActor* Target : Targets)
		{
			SweepToTarget(CameraLocation, Target, OccludingPieces);
		}
		MarkOccluded(OccludingPieces, CurrentTime);
	}

	// 3. 일정 시간 가리지 않은 조각 복구 (방 컷어웨이가 잡고 있는 조각 제외)
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

bool UCameraOcclusionComponent::UpdateRoomCutaway(const FVector& CameraLocation, float CurrentTime)
{
	ARoomMaskVolume* Room = RoomCutaway.bEnabled ? ARoomMaskVolume::GetCurrentRoom() : nullptr;
	if (!Room || Room->GetWorld() != GetWorld() || !Room->IsCutawayEnabled())
	{
		// 방 밖 (마당, 옥상)에서는 창 너머 실내가 보일 수 있으므로 조명 그림자를 모두 원래대로 유지
		ReleaseRoomPieces(CurrentTime);
		RestoreCulledLightShadows();
		return false;
	}

	const FRoomCutawayCache& Cache = GetOrBuildRoomCache(Room);

	// 방에 닿지 않는 조명의 그림자 끄기 (방이 바뀔 때만 실제 변경)
	ApplyLightShadowCulling(Room, Cache);

	if (bDrawDebug)
	{
		TArray<FBox> Boxes;
		Room->GetRoomBounds(Boxes);
		for (const FBox& Box : Boxes)
		{
			DrawDebugBox(GetWorld(), Box.GetCenter(), Box.GetExtent(), FColor::Cyan, false, UpdateInterval);
		}
	}

	// 옆면은 플레이어 기준 카메라가 있는 쪽만 숨김
	const FVector ToCamera = (CameraLocation - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	const uint8 SideMask = FRoomCutawayCache::GetCameraSideMask(ToCamera, RoomCutaway.FaceDotThreshold);

	// 방, 카메라 쪽 면, 방 밖 숨김 설정이 그대로면 이미 숨긴 상태: 판정마다 수천 개 조각을 다시 모으지 않음
	if (bHasRoomHeldPieces && HeldRoom.Get() == Room && HeldSideMask == SideMask && bHeldOutsideRoom == RoomCutaway.bHideOutsideRoom)
	{
		return true;
	}

	// 천장 / 윗층, 방 밖 (옆방, 건물 밖)은 카메라 각도와 관계없이 항상 숨김
	TSet<UPrimitiveComponent*> RoomPieces;
	Cache.CollectPieces(SideMask, RoomCutaway.bHideOutsideRoom, RoomPieces);
	HoldRoomPieces(RoomPieces, CurrentTime);

	HeldRoom = Room;
	HeldSideMask = SideMask;
	bHeldOutsideRoom = RoomCutaway.bHideOutsideRoom;
	return true;
}

const FRoomCutawayCache& UCameraOcclusionComponent::GetOrBuildRoomCache(ARoomMaskVolume* Room)
{
	if (const FRoomCutawayCache* Found = RoomCaches.Find(Room))
	{
		return *Found;
	}
	return RoomCaches.Add(Room, FRoomCutawayCache::Build(*GetWorld(), *Room, RoomCutaway));
}

void UCameraOcclusionComponent::HoldRoomPieces(const TSet<UPrimitiveComponent*>& RoomPieces, float CurrentTime)
{
	// 이전에 잡고 있던 조각 중 이번에 빠진 것은 놓음: RestoreDelay 뒤 복구 (카메라 쪽 면이 바뀔 때 깜빡임 방지)
	if (bHasRoomHeldPieces)
	{
		for (TPair<TWeakObjectPtr<UPrimitiveComponent>, FHiddenPiece>& Pair : HiddenPieces)
		{
			if (Pair.Value.bHeldByRoom && !RoomPieces.Contains(Pair.Key.Get()))
			{
				Pair.Value.bHeldByRoom = false;
				Pair.Value.LastOccludedTime = CurrentTime;
				bHasReleasingPieces = true;
			}
		}
	}

	// 새로 들어온 조각은 숨기고, 전부 방이 잡고 있는 것으로 표시
	for (UPrimitiveComponent* Piece : RoomPieces)
	{
		FHiddenPiece* Hidden = HiddenPieces.Find(Piece);
		if (!Hidden)
		{
			Hidden = &HidePiece(Piece, CurrentTime);
		}
		Hidden->bHeldByRoom = true;
		Hidden->LastOccludedTime = CurrentTime;
	}

	bHasRoomHeldPieces = !RoomPieces.IsEmpty();
}

void UCameraOcclusionComponent::ReleaseRoomPieces(float CurrentTime)
{
	HeldRoom.Reset();
	if (!bHasRoomHeldPieces)
	{
		return;
	}

	// 방을 나가면 잡고 있던 조각을 놓음: 바깥 Sweep과 같은 RestoreDelay 뒤 복구
	for (TPair<TWeakObjectPtr<UPrimitiveComponent>, FHiddenPiece>& Pair : HiddenPieces)
	{
		if (Pair.Value.bHeldByRoom)
		{
			Pair.Value.bHeldByRoom = false;
			Pair.Value.LastOccludedTime = CurrentTime;
		}
	}
	bHasRoomHeldPieces = false;
	bHasReleasingPieces = true;
}

void UCameraOcclusionComponent::MarkOccluded(const TSet<UPrimitiveComponent*>& OccludingPieces, float CurrentTime)
{
	// 새로 가린 조각은 숨기고, 이미 숨긴 조각은 가림 시각만 갱신
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

	if (!OccludingPieces.IsEmpty())
	{
		bHasReleasingPieces = true;
	}
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

UCameraOcclusionComponent::FHiddenPiece& UCameraOcclusionComponent::HidePiece(UPrimitiveComponent* Piece, float CurrentTime)
{
	FHiddenPiece& Hidden = HiddenPieces.Add(Piece);
	Hidden.bWasRenderInMainPass = Piece->bRenderInMainPass;
	Hidden.bWasRenderInDepthPass = Piece->bRenderInDepthPass;
	Hidden.LastOccludedTime = CurrentTime;
	// 화면에서만 숨김: 그림자, 콜리전은 유지되어 실내 음영이 바뀌지 않고 Sweep에도 계속 잡힘
	Piece->SetRenderInMainPass(false);
	// 메인 패스를 꺼도 깊이 패스는 기본으로 켜져 있어, 끄지 않으면 색 없는 깊이만 남아 검은 면으로 가림
	Piece->SetRenderInDepthPass(false);
	return Hidden;
}

void UCameraOcclusionComponent::RestorePiece(UPrimitiveComponent* Piece, const FHiddenPiece& Hidden)
{
	Piece->SetRenderInMainPass(Hidden.bWasRenderInMainPass);
	Piece->SetRenderInDepthPass(Hidden.bWasRenderInDepthPass);
}

void UCameraOcclusionComponent::RestoreExpiredPieces(float CurrentTime)
{
	// 방 안에서 면이 그대로면 복구할 조각이 없음: 숨긴 조각 수천 개를 판정마다 훑지 않음
	if (!bHasReleasingPieces)
	{
		return;
	}

	bool bStillReleasing = false;
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
		if (Hidden.bHeldByRoom)
		{
			continue;
		}
		if (CurrentTime - Hidden.LastOccludedTime < RestoreDelay)
		{
			bStillReleasing = true;
			continue;
		}
		RestorePiece(Piece, Hidden);
		It.RemoveCurrent();
	}
	bHasReleasingPieces = bStillReleasing;
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

	// 다음 판정에서 방 조각을 처음부터 다시 숨김 (컷신이 끝난 뒤 등)
	HeldRoom.Reset();
	bHasRoomHeldPieces = false;
	bHasReleasingPieces = false;
}
