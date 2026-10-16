/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 플레이어용 상호작용 컴포넌트 구현

#include "Characters/PlayerComponents/InteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"

UInteractionComponent::UInteractionComponent()
{
	// 정렬 중에만 Tick
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UInteractionComponent::RefreshFocus, ScanInterval, true);
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	FinishAlign(false);
	SetFocusedActor(nullptr);

	Super::EndPlay(EndPlayReason);
}

bool UInteractionComponent::TryInteract()
{
	if (IsAligning())
	{
		return false;
	}

	// 입력 시점에 한 번 더 갱신 (스캔 간격 사이에 대상이 바뀐 경우)
	RefreshFocus();

	AActor* Target = FocusedActor.Get();
	if (!IsValid(Target))
	{
		return false;
	}

	AActor* Owner = GetOwner();
	const FInteractAlignment Alignment = IInteractableInterface::Execute_GetInteractAlignment(Target, Owner);
	if (Alignment.Mode == EInteractAlignMode::None)
	{
		ExecuteInteract(Target);
	}
	else
	{
		BeginAlign(Target, Alignment);
	}
	return true;
}

void UInteractionComponent::CancelInteract()
{
	FinishAlign(false);
}

void UInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* Owner = GetOwner();
	if (!AlignTarget.IsValid() || !IsValid(Owner))
	{
		FinishAlign(false);
		return;
	}

	PhaseElapsed += DeltaTime;

	if (bDrawDebugAlign)
	{
		const FVector DebugLocation(CurrentAlignment.TargetLocation.X, CurrentAlignment.TargetLocation.Y, Owner->GetActorLocation().Z);
		DrawDebugSphere(GetWorld(), DebugLocation, MoveAcceptRadius * 2.f, 8, FColor::Cyan);
		DrawDebugDirectionalArrow(GetWorld(), DebugLocation, DebugLocation + CurrentAlignment.TargetRotation.Vector() * 60.f, 20.f, FColor::Cyan);
	}

	if (AlignPhase == EAlignPhase::Move)
	{
		TickMove(DeltaTime);
	}
	else if (AlignPhase == EAlignPhase::Rotate)
	{
		TickRotate(DeltaTime);
	}
}

void UInteractionComponent::TickMove(float DeltaTime)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	const FVector OwnerLocation = Pawn->GetActorLocation();
	const FVector ToTarget = (CurrentAlignment.TargetLocation - OwnerLocation) * FVector(1.f, 1.f, 0.f);
	const float Dist = ToTarget.Size();

	// 이번 프레임 이동량보다 가까우면 도착 (넘어가서 왔다 갔다 하는 것 방지)
	const float FrameStep = Pawn->GetVelocity().Size2D() * DeltaTime;
	const bool bArrived = Dist <= FMath::Max(MoveAcceptRadius, FrameStep);

	if (bArrived || PhaseElapsed >= MoveTimeout)
	{
		if (bArrived)
		{
			// 남은 오차는 직접 맞춤 (높이는 유지)
			FVector FinalLocation = CurrentAlignment.TargetLocation;
			FinalLocation.Z = OwnerLocation.Z;
			Pawn->SetActorLocation(FinalLocation, true);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Interaction] 정렬 위치에 도착하지 못해 현재 위치에서 회전합니다 (남은 거리 %.1f)"), Dist);
		}

		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}

		AlignPhase = EAlignPhase::Rotate;
		PhaseElapsed = 0.f;
		return;
	}

	// 이동 입력 잠금 중에도 들어가도록 bForce. 걷기 애니메이션과 이동 방향 회전은 무브먼트가 처리
	Pawn->AddMovementInput(ToTarget / Dist, MoveInputScale, true);
}

void UInteractionComponent::TickRotate(float DeltaTime)
{
	AActor* Owner = GetOwner();
	const FRotator Current = Owner->GetActorRotation();
	const FRotator Target(0.f, CurrentAlignment.TargetRotation.Yaw, 0.f);

	const FRotator NewRotation = FMath::RInterpConstantTo(Current, Target, DeltaTime, RotateSpeed);
	Owner->SetActorRotation(NewRotation);

	if (FMath::Abs(FRotator::NormalizeAxis(NewRotation.Yaw - Target.Yaw)) <= 1.f)
	{
		Owner->SetActorRotation(Target);
		FinishAlign(true);
	}
}

void UInteractionComponent::RefreshFocus()
{
	// 정렬 중에는 대상 고정
	if (IsAligning())
	{
		return;
	}

	SetFocusedActor(FindBestCandidate());
}

AActor* UInteractionComponent::FindBestCandidate() const
{
	const AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	const FVector OwnerLocation = Owner->GetActorLocation();
	const FVector OwnerForward = Owner->GetActorForwardVector().GetSafeNormal2D();
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(MaxFacingAngle));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionScan), false, Owner);
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, OwnerLocation, FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),
		FCollisionShape::MakeSphere(InteractRadius), Params);

	AActor* BestActor = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	TSet<const AActor*> Visited;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!IsValid(Candidate) || Visited.Contains(Candidate))
		{
			continue;
		}
		Visited.Add(Candidate);

		if (!Candidate->Implements<UInteractableInterface>()
			|| !IInteractableInterface::Execute_CanInteract(Candidate, const_cast<AActor*>(Owner)))
		{
			continue;
		}

		const FVector ToCandidate = Candidate->GetActorLocation() - OwnerLocation;
		const float Dist = ToCandidate.Size2D();
		if (Dist > InteractRadius)
		{
			continue;
		}

		// 바로 겹쳐 있으면 정면으로 간주
		const float Dot = Dist > KINDA_SMALL_NUMBER ? FVector::DotProduct(OwnerForward, ToCandidate.GetSafeNormal2D()) : 1.f;
		if (Dot < MinDot)
		{
			continue;
		}

		if (bUseVisibilityTrace && !IsVisible(Candidate))
		{
			continue;
		}

		const float Closeness = InteractRadius > 0.f ? 1.f - Dist / InteractRadius : 1.f;
		const float Score = FacingWeight * Dot + DistanceWeight * Closeness;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
		}
	}

	return BestActor;
}

bool UInteractionComponent::IsVisible(const AActor* Target) const
{
	const AActor* Owner = GetOwner();

	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionVisibility), false, Owner);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, Target->GetActorLocation(), VisibilityChannel, Params))
	{
		return true;
	}
	return Hit.GetActor() == Target;
}

void UInteractionComponent::SetFocusedActor(AActor* NewFocus)
{
	AActor* OldFocus = FocusedActor.Get();
	if (OldFocus == NewFocus)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (IsValid(OldFocus) && OldFocus->Implements<UInteractableInterface>())
	{
		IInteractableInterface::Execute_OnFocusEnd(OldFocus, Owner);
	}

	FocusedActor = NewFocus;

	if (IsValid(NewFocus))
	{
		IInteractableInterface::Execute_OnFocusBegin(NewFocus, Owner);
	}

	OnFocusChanged.Broadcast(NewFocus, OldFocus);
}

void UInteractionComponent::BeginAlign(AActor* Target, const FInteractAlignment& Alignment)
{
	AActor* Owner = GetOwner();

	AlignTarget = Target;
	CurrentAlignment = Alignment;
	PhaseElapsed = 0.f;

	const APawn* Pawn = Cast<APawn>(Owner);
	UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;

	if (Alignment.Mode == EInteractAlignMode::SnapToPoint && Movement)
	{
		// 걸어서 이동 (달리던 속도는 이어서 사용)
		AlignPhase = EAlignPhase::Move;
	}
	else
	{
		// 무브먼트가 없는 액터는 위치를 바로 맞춤
		if (Alignment.Mode == EInteractAlignMode::SnapToPoint)
		{
			FVector FinalLocation = Alignment.TargetLocation;
			FinalLocation.Z = Owner->GetActorLocation().Z;
			Owner->SetActorLocation(FinalLocation, true);
		}

		// 달리던 관성으로 미끄러지지 않게 정지 후 제자리 회전
		if (Movement)
		{
			Movement->StopMovementImmediately();
		}
		AlignPhase = EAlignPhase::Rotate;
	}

	if (bLockMoveInputWhileAligning)
	{
		SetMoveInputLocked(true);
	}

	SetComponentTickEnabled(true);
}

void UInteractionComponent::FinishAlign(bool bExecute)
{
	AActor* Target = AlignTarget.Get();

	AlignTarget.Reset();
	AlignPhase = EAlignPhase::None;
	SetComponentTickEnabled(false);
	SetMoveInputLocked(false);

	if (bExecute && IsValid(Target))
	{
		ExecuteInteract(Target);
	}
}

void UInteractionComponent::ExecuteInteract(AActor* Target)
{
	AActor* Owner = GetOwner();

	// 정렬하는 동안 상태가 바뀌었을 수 있으므로 다시 확인
	if (!IInteractableInterface::Execute_CanInteract(Target, Owner))
	{
		return;
	}

	IInteractableInterface::Execute_Interact(Target, Owner);
	OnInteractExecuted.Broadcast(Target);
}

void UInteractionComponent::SetMoveInputLocked(bool bLocked)
{
	if (bMoveInputLocked == bLocked)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	AController* Controller = Pawn ? Pawn->GetController() : nullptr;
	if (!Controller)
	{
		return;
	}

	// SetIgnoreMoveInput은 카운터 방식이라 잠금/해제를 짝 맞춰 호출
	Controller->SetIgnoreMoveInput(bLocked);
	bMoveInputLocked = bLocked;
}
