/// 작성자: 최요환
/// 작성일자: 2026-10-16
///
/// 플레이어용 상호작용 컴포넌트 구현

#include "Characters/PlayerComponents/InteractionComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Gimmicks/InteractionVolumeComponent.h"
#include "WIdgets/Interaction/InteractionPromptWidget.h"
#include "Components/WidgetComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Engine/LocalPlayer.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
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

	CreatePrompt();

	GetWorld()->GetTimerManager().SetTimer(ScanTimerHandle, this, &UInteractionComponent::RefreshFocus, ScanInterval, true);

	// 테스트용 키 입력: 빙의(컨트롤러 변경) 때마다 그 플레이어 컨트롤러에 입력 컴포넌트를 넣음
	if (bBindInteractKey)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			OwnerPawn->ReceiveControllerChangedDelegate.AddDynamic(this, &UInteractionComponent::HandleOwnerControllerChanged);
			PushKeyInput(OwnerPawn->GetController());
		}
	}
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
	FinishAlign(false);
	SetFocusedActor(nullptr);
	PopKeyInput();

	if (PromptComponent)
	{
		PromptComponent->DestroyComponent();
		PromptComponent = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	PopKeyInput();
	PushKeyInput(NewController);
}

void UInteractionComponent::PushKeyInput(AController* Controller)
{
	APlayerController* PlayerController = Cast<APlayerController>(Controller);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!KeyInputComponent)
	{
		KeyInputComponent = NewObject<UInputComponent>(GetOwner(), TEXT("InteractKeyInput"));
		FInputKeyBinding& Binding = KeyInputComponent->BindKey(InteractKey, IE_Pressed, this, &UInteractionComponent::HandleInteractKey);
		Binding.bConsumeInput = false;
	}

	PlayerController->PushInputComponent(KeyInputComponent);
	KeyInputController = PlayerController;
}

void UInteractionComponent::PopKeyInput()
{
	if (KeyInputController.IsValid() && KeyInputComponent)
	{
		KeyInputController->PopInputComponent(KeyInputComponent);
	}
	KeyInputController.Reset();
}

void UInteractionComponent::HandleInteractKey()
{
	TryInteract();
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

	if (UInteractionPromptWidget* PromptWidget = GetPromptWidget(); PromptWidget && PromptTarget.Get() == Target)
	{
		PromptWidget->NotifyPressed();
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

		// 이동 경로 (남은 구간)
		if (AlignPhase == EAlignPhase::Move)
		{
			FVector Prev = Owner->GetActorLocation();
			for (int32 Index = MovePathIndex; Index < MovePathPoints.Num(); ++Index)
			{
				const FVector Point(MovePathPoints[Index].X, MovePathPoints[Index].Y, Prev.Z);
				DrawDebugLine(GetWorld(), Prev, Point, FColor::Yellow);
				Prev = Point;
			}
		}
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

	const bool bFinalPoint = MovePathIndex >= MovePathPoints.Num() - 1;
	const FVector PointLocation = MovePathPoints.IsValidIndex(MovePathIndex) ? MovePathPoints[MovePathIndex] : CurrentAlignment.TargetLocation;
	const FVector ToTarget = (PointLocation - OwnerLocation) * FVector(1.f, 1.f, 0.f);
	const float Dist = ToTarget.Size();

	// 이번 프레임 이동량보다 가까우면 도착 (넘어가서 왔다 갔다 하는 것 방지)
	const float FrameStep = Pawn->GetVelocity().Size2D() * DeltaTime;
	const bool bArrived = Dist <= FMath::Max(bFinalPoint ? MoveAcceptRadius : PathPointAcceptRadius, FrameStep);

	// 경유 지점은 지나치면서 다음 지점으로
	if (bArrived && !bFinalPoint)
	{
		++MovePathIndex;
		return;
	}

	if (bArrived || PhaseElapsed >= MoveTimeLimit)
	{
		if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
		{
			Movement->StopMovementImmediately();
		}

		if (!bArrived)
		{
			if (bCancelOnMoveFail)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Interaction] 정렬 위치에 도착하지 못해 상호작용을 취소합니다 (제한 %.2f초)"), MoveTimeLimit);
				FinishAlign(false);
				return;
			}
			UE_LOG(LogTemp, Warning, TEXT("[Interaction] 정렬 위치에 도착하지 못해 현재 위치에서 회전합니다 (제한 %.2f초)"), MoveTimeLimit);
		}
		else
		{
			// 남은 오차는 직접 맞춤 (높이는 유지)
			FVector FinalLocation = CurrentAlignment.TargetLocation;
			FinalLocation.Z = OwnerLocation.Z;
			Pawn->SetActorLocation(FinalLocation, true);
		}

		AlignPhase = EAlignPhase::Rotate;
		PhaseElapsed = 0.f;
		return;
	}

	// 이동 입력 잠금 중에도 들어가도록 bForce. 걷기 애니메이션과 이동 방향 회전은 무브먼트가 처리
	Pawn->AddMovementInput(ToTarget / Dist, MoveInputScale, true);
}

void UInteractionComponent::BuildMovePath(const FVector& Start, const FVector& Goal)
{
	MovePathPoints.Reset();
	MovePathIndex = 0;

	// NavMesh 경로 (목표까지 이어진 경로만 사용, 첫 점은 시작 위치라 제외)
	if (bUseNavPath)
	{
		if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
		{
			const UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(GetWorld(), Start, Goal, GetOwner());
			if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 1)
			{
				MovePathPoints.Append(Path->PathPoints.GetData() + 1, Path->PathPoints.Num() - 1);
			}
		}
	}

	// 경로가 없으면 직선. 마지막 점은 NavMesh 투영 위치 대신 정확한 목표 위치
	if (MovePathPoints.IsEmpty())
	{
		MovePathPoints.Add(Goal);
	}
	else
	{
		MovePathPoints.Last() = Goal;
	}

	// 제한 시간 = 경로 길이 / 정렬 이동 속도 + 여유
	float PathLength = 0.f;
	FVector Prev = Start;
	for (const FVector& Point : MovePathPoints)
	{
		PathLength += FVector::Dist2D(Prev, Point);
		Prev = Point;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	const UPawnMovementComponent* Movement = Pawn ? Pawn->GetMovementComponent() : nullptr;
	const float MoveSpeed = Movement ? Movement->GetMaxSpeed() * MoveInputScale : 0.f;
	MoveTimeLimit = (MoveSpeed > KINDA_SMALL_NUMBER ? PathLength / MoveSpeed : 0.f) + MoveTimeoutPadding;
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
	RefreshPrompt();
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

	float OwnerRadius = 0.f;
	float OwnerHalfHeight = 0.f;
	Owner->GetSimpleCollisionCylinder(OwnerRadius, OwnerHalfHeight);

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

		// 액터 중심 대신 감지 기준점(큰 액터는 가장 가까운 표면) 기준으로 판정
		const FVector FocusLocation = IInteractableInterface::Execute_GetInteractFocusLocation(Candidate, const_cast<AActor*>(Owner));
		const FVector ToCandidate = FocusLocation - OwnerLocation;
		const float Dist = ToCandidate.Size2D();

		// 감지 영역(InteractionVolume)이 있는 액터는 반경 대신 캐릭터가 그 영역에 닿아 있어야 감지
		TInlineComponentArray<UInteractionVolumeComponent*> Volumes(Candidate);
		if (Volumes.Num() > 0)
		{
			const bool bTouching = Volumes.ContainsByPredicate([&](const UInteractionVolumeComponent* Volume)
			{
				return Volume->IsTouchingCylinder(OwnerLocation, OwnerRadius, OwnerHalfHeight);
			});
			if (!bTouching)
			{
				continue;
			}
		}
		else if (Dist > InteractRadius)
		{
			continue;
		}

		// 바로 겹쳐 있으면 정면으로 간주
		const float Dot = Dist > KINDA_SMALL_NUMBER ? FVector::DotProduct(OwnerForward, ToCandidate.GetSafeNormal2D()) : 1.f;
		if (Dot < MinDot)
		{
			continue;
		}

		if (bUseVisibilityTrace && !IsVisible(Candidate, FocusLocation))
		{
			continue;
		}

		const float Closeness = InteractRadius > 0.f ? FMath::Max(0.f, 1.f - Dist / InteractRadius) : 1.f;
		const float Score = FacingWeight * Dot + DistanceWeight * Closeness;
		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = Candidate;
		}
	}

	return BestActor;
}

bool UInteractionComponent::IsVisible(const AActor* Target, const FVector& TargetLocation) const
{
	const AActor* Owner = GetOwner();

	FVector EyeLocation;
	FRotator EyeRotation;
	Owner->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(InteractionVisibility), false, Owner);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, EyeLocation, TargetLocation, VisibilityChannel, Params))
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
		BuildMovePath(Owner->GetActorLocation(), Alignment.TargetLocation);
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

	if (bHidePromptWhileAligning)
	{
		HidePrompt();
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

	RefreshPrompt();
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

	// 상태가 바뀌어 동작 텍스트·가능 여부가 달라졌을 수 있으므로 바로 반영 (안내 UI 포함)
	RefreshFocus();
}

void UInteractionComponent::CreatePrompt()
{
	AActor* Owner = GetOwner();
	if (!PromptWidgetClass || !IsValid(Owner) || PromptComponent)
	{
		return;
	}

	PromptComponent = NewObject<UWidgetComponent>(Owner, TEXT("InteractPrompt"));
	PromptComponent->SetWidgetSpace(EWidgetSpace::Screen);
	PromptComponent->SetWidgetClass(PromptWidgetClass);
	PromptComponent->SetPivot(PromptPivot);
	PromptComponent->SetDrawAtDesiredSize(PromptDrawSize.IsNearlyZero());
	if (!PromptDrawSize.IsNearlyZero())
	{
		PromptComponent->SetDrawSize(PromptDrawSize);
	}
	PromptComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PromptComponent->SetGenerateOverlapEvents(false);
	PromptComponent->SetCastShadow(false);
	PromptComponent->SetVisibility(false);
	PromptComponent->SetupAttachment(Owner->GetRootComponent());
	PromptComponent->RegisterComponent();
}

UInteractionPromptWidget* UInteractionComponent::GetPromptWidget() const
{
	return PromptComponent ? Cast<UInteractionPromptWidget>(PromptComponent->GetUserWidgetObject()) : nullptr;
}

void UInteractionComponent::RefreshPrompt()
{
	if (!PromptComponent)
	{
		return;
	}

	AActor* Target = IsAligning() ? AlignTarget.Get() : FocusedActor.Get();
	if (!IsValid(Target) || (IsAligning() && bHidePromptWhileAligning))
	{
		HidePrompt();
		return;
	}

	const FInteractPromptInfo Info = IInteractableInterface::Execute_GetInteractPrompt(Target, GetOwner());
	if (!Info.bShowPrompt)
	{
		HidePrompt();
		return;
	}

	// 대상 루트에 붙여 대상이 움직여도 따라가게 함
	USceneComponent* TargetRoot = Target->GetRootComponent();
	if (TargetRoot && PromptComponent->GetAttachParent() != TargetRoot)
	{
		PromptComponent->AttachToComponent(TargetRoot, FAttachmentTransformRules::KeepWorldTransform);
	}
	PromptComponent->SetWorldLocation(Info.WorldLocation);

	UInteractionPromptWidget* PromptWidget = GetPromptWidget();
	if (PromptWidget)
	{
		const FText KeyText = GetInteractKeyText();
		if (!PromptWidget->GetKeyDisplayText().EqualTo(KeyText) || !PromptWidget->GetActionDisplayText().EqualTo(Info.ActionText))
		{
			PromptWidget->SetPrompt(KeyText, Info.ActionText);
		}
	}

	// 처음 표시하거나 대상이 바뀌면 표시 이벤트
	if (PromptTarget.Get() != Target)
	{
		PromptTarget = Target;
		PromptComponent->SetVisibility(true);
		if (PromptWidget)
		{
			PromptWidget->NotifyShown();
		}
	}
}

void UInteractionComponent::HidePrompt()
{
	// 대상이 파괴돼 PromptTarget이 무효여도 표시 중이면 숨김
	PromptTarget.Reset();
	if (!PromptComponent || !PromptComponent->IsVisible())
	{
		return;
	}

	if (UInteractionPromptWidget* PromptWidget = GetPromptWidget())
	{
		PromptWidget->NotifyHidden();
	}
	PromptComponent->SetVisibility(false);

	// 대상 액터에서 떼어 다시 소유 액터에 붙임 (대상이 사라져도 함께 정리되지 않게)
	if (AActor* Owner = GetOwner(); Owner && Owner->GetRootComponent())
	{
		PromptComponent->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
}

FText UInteractionComponent::GetInteractKeyText() const
{
	// 입력 액션에 매핑된 키 (키보드 키 우선)
	if (InteractAction)
	{
		const APawn* Pawn = Cast<APawn>(GetOwner());
		const APlayerController* PlayerController = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
		const ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		if (const UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			const TArray<FKey> Keys = InputSubsystem->QueryKeysMappedToAction(InteractAction);
			const FKey* Key = Keys.FindByPredicate([](const FKey& Candidate) { return !Candidate.IsGamepadKey(); });
			if (!Key && Keys.Num() > 0)
			{
				Key = &Keys[0];
			}
			if (Key)
			{
				return Key->GetDisplayName(false);
			}
		}
	}

	if (bBindInteractKey && InteractKey.IsValid())
	{
		return InteractKey.GetDisplayName(false);
	}

	return FallbackKeyText;
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
