#include "Systems/CameraOcclusionComponent.h"
#include "Characters/PlayerComponents/LockOnComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

UCameraOcclusionComponent::UCameraOcclusionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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
	TSet<UPrimitiveComponent*> HitPieces;
	for (const AActor* Target : Targets)
	{
		SweepToTarget(CameraLocation, Target, HitPieces);
	}

	// 2. 맞은 조각을 같은 면 전체로 확장
	TSet<UPrimitiveComponent*> OccludingPieces;
	for (UPrimitiveComponent* HitPiece : HitPieces)
	{
		GroupSubsystem->CollectGroupMembers(HitPiece, GroupSettings, OccludingPieces);
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

void UCameraOcclusionComponent::SweepToTarget(const FVector& CameraLocation, const AActor* Target, TSet<UPrimitiveComponent*>& OutHitPieces) const
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

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CameraOcclusionSweep), false, GetOwner());
	Params.AddIgnoredActor(Target);

	// 벽 / 천장은 가림 채널에 Overlap으로 반응하므로 첫 조각에서 멈추지 않고 경로상의 조각 전부 수집
	TArray<FHitResult> Hits;
	GetWorld()->SweepMultiByChannel(Hits, CameraLocation, SweepEnd, FQuat::Identity,
		GroupSettings.TraceChannel, FCollisionShape::MakeSphere(SweepRadius), Params);
	for (const FHitResult& Hit : Hits)
	{
		UPrimitiveComponent* HitPiece = Hit.GetComponent();
		if (IsValid(HitPiece))
		{
			OutHitPieces.Add(HitPiece);
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

void UCameraOcclusionComponent::HidePiece(UPrimitiveComponent* Piece, float CurrentTime)
{
	FHiddenPiece& Hidden = HiddenPieces.Add(Piece);
	Hidden.bWasRenderInMainPass = Piece->bRenderInMainPass;
	Hidden.LastOccludedTime = CurrentTime;
	// 화면에서만 숨김: 그림자, 콜리전은 유지되어 실내 음영이 바뀌지 않고 Sweep에도 계속 잡힘
	Piece->SetRenderInMainPass(false);
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
		Piece->SetRenderInMainPass(Hidden.bWasRenderInMainPass);
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
			Piece->SetRenderInMainPass(Pair.Value.bWasRenderInMainPass);
		}
	}
	HiddenPieces.Reset();
}
