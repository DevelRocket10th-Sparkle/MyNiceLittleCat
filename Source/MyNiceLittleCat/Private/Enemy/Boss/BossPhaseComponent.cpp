// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/BossPhaseComponent.h"

#include "Engine/TargetPoint.h"
#include "Engine/World.h"

#include "Cinematics/CinematicSubsystem.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Interfaces/BossPhaseTransitionHandler.h"

UBossPhaseComponent::UBossPhaseComponent()
{
	// 전환 연출 종료는 콜백으로 받으므로 Tick 불필요
	PrimaryComponentTick.bCanEverTick = false;
}

/* 초기화, 시작, 정지 */

bool UBossPhaseComponent::InitializePhases(UBossPhaseConfigData* InConfig)
{
	StopAndInvalidate();

	Config = InConfig;
	CurrentPhaseIndex = INDEX_NONE;

	// 설정 오류를 로그에 표시하기 위한 설명
	FString Error;

	if (!GetOwner()->HasAuthority() || !IsValid(InConfig) || !InConfig->ValidateConfiguration(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[BossPhase] %s: initialization failed: %s"), *GetNameSafe(GetOwner()), *Error);
		return false;
	}

	if (!ValidateMovementRoutes(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[BossPhase] %s: initialization failed: %s"), *GetNameSafe(GetOwner()), *Error);
		return false;
	}

	return true;
}

void UBossPhaseComponent::SetMovementRoutes(const TMap<FName, FBossMovementRoute>& InRoutes)
{
	MovementRoutes = InRoutes;
}

void UBossPhaseComponent::SetTransitionHandler(UObject* InHandler)
{
	// 계약을 구현하지 않은 객체는 받지 않음
	if (InHandler && !InHandler->GetClass()->ImplementsInterface(UBossPhaseTransitionHandler::StaticClass()))
	{
		UE_LOG(LogTemp, Error, TEXT("[BossPhase] %s does not implement IBossPhaseTransitionHandler"), *GetNameSafe(InHandler));
		TransitionHandlerObject.Reset();
		return;
	}

	TransitionHandlerObject = InHandler;
}

// 첫 페이즈 설정 적용 후 체력 조건 평가 허용
bool UBossPhaseComponent::StartPhases()
{
	if (!GetOwner()->HasAuthority() || !Config || !ApplyPhase.IsBound() || CurrentPhaseIndex != INDEX_NONE)
	{
		return false;
	}

	CurrentPhaseIndex = 0;

	// 설정 적용 중에는 피해가 허용되지 않도록 Active 전환을 나중에 수행
	RunState = EBossPhaseRunState::Transitioning;
	if (!ApplyCurrentPhase())
	{
		StopAndInvalidate();
		return false;
	}

	RunState = EBossPhaseRunState::Active;

	// 첫 페이즈 진입 중 즉시 완료된 기믹은 활성화 이후 처리
	if (bPendingGimmickCompletion)
	{
		RequestNextPhase();
	}

	return true;
}

// 요청을 먼저 무효화한 뒤 연출과 상태 정리
void UBossPhaseComponent::StopAndInvalidate()
{
	// 연출 취소에 필요한 번호는 무효화 전에 보관
	const int64 StoppedTransitionId = ActiveTransitionId;

	ActivePhaseExecutionId = 0;
	ActiveTransitionId = 0;
	PendingPhaseIndex = INDEX_NONE;
	bApplyingPhase = false;
	bPendingGimmickCompletion = false;
	RunState = EBossPhaseRunState::Stopped;

	CancelTransitionPresentation(StoppedTransitionId);

	// 경로 순회 상태 초기화 (MovementRoutes는 유지)
	ResetWaypointProgress();
}

/* 페이즈 진행 조건 */

// 현재 활성 전투 페이즈에 대해서만 체력 조건 평가
void UBossPhaseComponent::EvaluateHealth(float Health, float MaxHealth)
{
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!IsActive() || !Phase || Phase->Kind != EBossPhaseKind::Combat)
	{
		return;
	}

	// 하한 0은 마지막 페이즈: 체력으로 다음 페이즈를 요청하지 않음 (사망 처리)
	const bool bReachedFloor = Phase->HealthFloorRatio > 0.f
		&& Health > 0.f && MaxHealth > 0.f
		&& Health <= MaxHealth * Phase->HealthFloorRatio;

	if (bReachedFloor)
	{
		RequestNextPhase();
	}
}

// 현재 배리어와 진입 번호가 일치하는 기믹 완료만 다음 페이즈 요청으로 변환
bool UBossPhaseComponent::RequestNextPhaseFromGimmick(FName PhaseId, int64 PhaseExecutionId)
{
	const FBossPhaseDefinition* Phase = GetCurrentPhase();

	// 아래 조건에서는 거부
	// - 서버가 아니거나 현재 페이즈가 배리어가 아님
	// - 다른 페이즈 또는 이전 진입의 늦은 통지
	// - 다음 페이즈가 없음
	if (!GetOwner()->HasAuthority() || !Phase || Phase->Kind != EBossPhaseKind::Barrier
		|| Phase->PhaseId != PhaseId
		|| PhaseExecutionId == 0 || PhaseExecutionId != ActivePhaseExecutionId
		|| !Config->Phases.IsValidIndex(CurrentPhaseIndex + 1))
	{
		return false;
	}

	// 진입 중 즉시 완료되는 기믹은 설정 적용을 마친 뒤 한 번만 처리
	if (bApplyingPhase && IsTransitioning())
	{
		if (bPendingGimmickCompletion)
		{
			return false;
		}

		bPendingGimmickCompletion = true;
		return true;
	}

	if (!IsActive())
	{
		return false;
	}

	RequestNextPhase();
	return true;
}

// 현재 전투 페이즈에서 허용하는 최소 체력
float UBossPhaseComponent::GetDamageHealthFloor(float MaxHealth) const
{
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!Phase)
	{
		return 0.f;
	}

	// 배리어는 자체 비율 대신 직전 전투의 체력 하한을 유지하여 피해로 진행되지 않게 함
	if (Phase->Kind == EBossPhaseKind::Barrier)
	{
		for (int32 Index = CurrentPhaseIndex - 1; Index >= 0; --Index)
		{
			const FBossPhaseDefinition& PreviousPhase = Config->Phases[Index];
			if (PreviousPhase.Kind == EBossPhaseKind::Combat)
			{
				return PreviousPhase.HealthFloorRatio * MaxHealth;
			}
		}

		return 0.f;
	}

	return Phase->HealthFloorRatio * MaxHealth;
}

/* 전환 */

// 대기 중인 전환 시작
bool UBossPhaseComponent::BeginPendingTransition()
{
	if (!GetOwner()->HasAuthority() || !HasPendingTransition()
		|| !Config || !Config->Phases.IsValidIndex(PendingPhaseIndex))
	{
		return false;
	}

	RunState = EBossPhaseRunState::Transitioning;
	StartTransitionPresentation();

	return true;
}

// 연출이 끝난 현재 전환만 다음 페이즈 적용으로 진행
bool UBossPhaseComponent::NotifyTransitionPresentationFinished(int64 TransitionId)
{
	if (!IsTransitioning() || !bPresentationRunning
		|| TransitionId == 0 || TransitionId != ActiveTransitionId)
	{
		return false;
	}

	bPresentationRunning = false;
	bPresentationByHandler = false;
	PresentationCinematic.Reset();

	UE_LOG(LogTemp, Log, TEXT("[BossPhase] %s: transition presentation finished"),
		*Config->Phases[PendingPhaseIndex].PhaseId.ToString());

	CompleteTransition();
	return true;
}

/* 상태 조회 */

const FBossPhaseDefinition* UBossPhaseComponent::GetCurrentPhase() const
{
	return Config && Config->Phases.IsValidIndex(CurrentPhaseIndex) ? &Config->Phases[CurrentPhaseIndex] : nullptr;
}

FName UBossPhaseComponent::GetCurrentPhaseId() const
{
	// 시작 전에는 유효한 페이즈가 없으므로 None
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	return Phase ? Phase->PhaseId : NAME_None;
}

bool UBossPhaseComponent::GetCurrentWaypoint(FVector& OutLocation) const
{
	const FBossPhaseDefinition* Phase = GetCurrentPhase();

	// 페이즈가 기본 이동과 패턴별 예외 어디에서도 Waypoints를 사용하지 않으면 중단
	if (!IsActive() || !Phase || !Phase->UsesWaypoints())
	{
		return false;
	}

	// 경로나 지정된 경로의 Waypoint가 유효하지 않으면 실패
	const FBossMovementRoute* Route = MovementRoutes.Find(Phase->MovementRouteId);
	if (!Route || !Route->Points.IsValidIndex(CurrentWaypointIndex))
	{
		return false;
	}

	const ATargetPoint* Point = Route->Points[CurrentWaypointIndex].Get();
	if (!IsValid(Point))
	{
		return false;
	}

	OutLocation = Point->GetActorLocation();
	return true;
}

/* IEnemyLocomotionProvider */

int64 UBossPhaseComponent::GetLocomotionContextId() const
{
	// 활성 페이즈가 아니면 기본 이동 없음 → 이동 Task 종료
	return IsActive() ? ActivePhaseExecutionId : 0;
}

bool UBossPhaseComponent::ResolveLocomotion(FName PatternId, FEnemyLocomotionIntent& OutIntent) const
{
	OutIntent = FEnemyLocomotionIntent();

	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!IsActive() || !Phase)
	{
		return false;
	}

	OutIntent.AcceptanceRadius = Phase->MovementAcceptanceRadius;

	// 실행 중 패턴 → 선택된 패턴 → 페이즈 기본 순으로 결정
	switch (Phase->ResolveMovementMode(PatternId))
	{
	case EBossPhaseMovementMode::Stop:
		OutIntent.Mode = EEnemyLocomotionMode::Stop;
		return true;

	case EBossPhaseMovementMode::Chasing:
		OutIntent.Mode = EEnemyLocomotionMode::ChaseTarget;
		return true;

	case EBossPhaseMovementMode::Waypoints:
		OutIntent.Mode = EEnemyLocomotionMode::MoveToLocation;
		return GetCurrentWaypoint(OutIntent.Destination);

	default:
		return false;
	}
}

void UBossPhaseComponent::NotifyLocomotionDestinationReached(int64 ContextId)
{
	// 아래 조건에서 무시
	// - 페이즈가 전환 중이거나 정지 상태
	// - 도착 알림이 현재 페이즈의 알림이 아님
	if (!IsActive() || ContextId == 0 || ContextId != ActivePhaseExecutionId)
	{
		return;
	}

	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (!Phase || !Phase->UsesWaypoints())
	{
		return;
	}

	// 지점이 둘 이상인 경로만 순회
	const FBossMovementRoute* Route = MovementRoutes.Find(Phase->MovementRouteId);
	if (!Route || Route->Points.Num() <= 1)
	{
		return;
	}

	// 처음과 마지막 지점에서 방향을 바꾸는 왕복 순회
	const int32 LastIndex = Route->Points.Num() - 1;
	if (CurrentWaypointIndex >= LastIndex)
	{
		WaypointDirection = -1;
	}
	else if (CurrentWaypointIndex <= 0)
	{
		WaypointDirection = 1;
	}

	CurrentWaypointIndex += WaypointDirection;
}

/* 엔진 이벤트 */

void UBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAndInvalidate();

	ApplyPhase.Unbind();
	OnTransitionRequested.Clear();
	OnTransitionFinished.Clear();
	TransitionHandlerObject.Reset();

	Super::EndPlay(EndPlayReason);
}

/* 페이즈 진행 */

// 다음 페이즈 전환을 중복 없이 요청하고 즉시 보호 처리 통지
void UBossPhaseComponent::RequestNextPhase()
{
	if (!IsActive() || !Config || !Config->Phases.IsValidIndex(CurrentPhaseIndex + 1))
	{
		return;
	}

	// 이전 페이즈의 늦은 기믹 완료/경로 도착 통지를 거부하도록 진입 번호 무효화
	ActivePhaseExecutionId = 0;
	bPendingGimmickCompletion = false;

	PendingPhaseIndex = CurrentPhaseIndex + 1;
	ActiveTransitionId = ++NextTransitionId;
	RunState = EBossPhaseRunState::TransitionPending;

	OnTransitionRequested.Broadcast();
}

// 페이즈별 실행 번호를 부여하고 외부 적용 콜백의 정지 및 동기 완료 처리
bool UBossPhaseComponent::ApplyCurrentPhase()
{
	if (!Config || !Config->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return false;
	}

	ResetWaypointProgress();

	ActivePhaseExecutionId = ++NextPhaseExecutionId;
	const int64 ApplyingExecutionId = ActivePhaseExecutionId;

	bPendingGimmickCompletion = false;
	bApplyingPhase = true;

	const bool bApplied = ApplyPhase.Execute(Config->Phases[CurrentPhaseIndex]);

	bApplyingPhase = false;

	// 적용 중 정지(StopAndInvalidate)가 일어났으면 성공이어도 무효
	return bApplied && IsTransitioning() && ActivePhaseExecutionId == ApplyingExecutionId;
}

/* 전환 연출 */

void UBossPhaseComponent::StartTransitionPresentation()
{
	const int64 TransitionId = ActiveTransitionId;
	const FBossPhaseDefinition& NextPhase = Config->Phases[PendingPhaseIndex];

	bPresentationRunning = true;
	bPresentationByHandler = false;
	PresentationCinematic.Reset();

	/* 1) 레벨 담당자 우선 */

	if (IBossPhaseTransitionHandler* Handler = GetTransitionHandler())
	{
		FBossPhaseTransitionContext Context;
		Context.Boss = Cast<ABossCharacter>(GetOwner());
		Context.TransitionId = TransitionId;
		Context.FromPhaseId = GetCurrentPhaseId();
		Context.ToPhaseId = NextPhase.PhaseId;
		Context.TimeoutSeconds = NextPhase.TransitionTimeout;

		bPresentationByHandler = true;
		if (Handler->BeginPhaseTransition(Context))
		{
			// 담당자가 끝나면 NotifyTransitionPresentationFinished로 돌아옴
			return;
		}
		bPresentationByHandler = false;
	}

	// 담당자 호출 중 정지/완료가 일어났으면 더 진행하지 않음
	if (!bPresentationRunning || ActiveTransitionId != TransitionId)
	{
		return;
	}

	/* 2) 페이즈 설정의 기본 전환 시퀀스 */

	if (NextPhase.bUseTransitionSequence && NextPhase.TransitionSequence)
	{
		if (UCinematicSubsystem* Cinematics = GetWorld()->GetSubsystem<UCinematicSubsystem>())
		{
			FCinematicRequest Request;
			Request.Sequence = NextPhase.TransitionSequence;
			Request.bCutscene = NextPhase.bTransitionIsCutscene;
			Request.bSkippable = NextPhase.bTransitionSkippable;
			Request.EndState = ECinematicEndState::RestoreState;
			Request.TimeoutSeconds = NextPhase.TransitionTimeout;

			// 동적으로 생성한 보스를 시퀀스의 보스 바인딩에 연결 (태그 이름은 보스 BP에서 설정)
			if (const ABossCharacter* Boss = Cast<ABossCharacter>(GetOwner()))
			{
				Request.Bindings.Emplace(Boss->GetSequenceBindingTag(), GetOwner());
			}

			PresentationCinematic = Cinematics->Play(Request, FOnCinematicMarker(),
				FOnCinematicEnded::CreateUObject(this, &UBossPhaseComponent::HandleDefaultPresentationEnded));

			if (PresentationCinematic.IsValid())
			{
				return;
			}
		}

		// 시퀀스 문제로 전투가 멈추지 않도록 연출만 건너뜀
		UE_LOG(LogTemp, Warning, TEXT("[BossPhase] %s: transition sequence failed to start, skipped"),
			*NextPhase.PhaseId.ToString());
	}

	/* 3) 연출 없음: 즉시 완료 */

	NotifyTransitionPresentationFinished(TransitionId);
}

void UBossPhaseComponent::HandleDefaultPresentationEnded(FCinematicHandle Handle, ECinematicEndReason Reason)
{
	if (Handle != PresentationCinematic)
	{
		return;
	}

	// 정상 종료, 스킵, 제한 시간, 외부 정지 모두 연출 종료로 보고 다음 페이즈 진행
	UE_LOG(LogTemp, Log, TEXT("[BossPhase] default transition sequence ended (%s)"), *UEnum::GetValueAsString(Reason));
	NotifyTransitionPresentationFinished(ActiveTransitionId);
}

void UBossPhaseComponent::CancelTransitionPresentation(int64 TransitionId)
{
	if (!bPresentationRunning)
	{
		return;
	}

	// 상태를 먼저 비워서 취소 처리 중 들어오는 완료 통지를 거부
	const bool bByHandler = bPresentationByHandler;
	const FCinematicHandle Cinematic = PresentationCinematic;

	bPresentationRunning = false;
	bPresentationByHandler = false;
	PresentationCinematic.Reset();

	if (bByHandler)
	{
		if (IBossPhaseTransitionHandler* Handler = GetTransitionHandler())
		{
			Handler->CancelPhaseTransition(TransitionId);
		}
	}

	if (Cinematic.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UCinematicSubsystem* Cinematics = World->GetSubsystem<UCinematicSubsystem>())
			{
				Cinematics->Cancel(Cinematic);
			}
		}
	}
}

// 연출이 끝난 전환의 다음 페이즈 확정
void UBossPhaseComponent::CompleteTransition()
{
	const int64 TransitionId = ActiveTransitionId;

	CurrentPhaseIndex = PendingPhaseIndex;
	PendingPhaseIndex = INDEX_NONE;

	const bool bApplied = ApplyCurrentPhase();

	// 적용 중 정지 요청이 발생했다면 이 전환은 이미 무효: 알리지 않음
	if (!IsTransitioning() || ActiveTransitionId != TransitionId)
	{
		return;
	}

	if (!bApplied)
	{
		OnTransitionFinished.Broadcast(TransitionId, false);
		return;
	}

	RunState = EBossPhaseRunState::Active;
	ActiveTransitionId = 0;

	// 진입 중 즉시 완료된 기믹은 완료 통지 전에 다음 전환으로 이어서 요청
	// → 보스는 Active가 아닌 상태를 보고 공격 차단을 유지
	if (bPendingGimmickCompletion)
	{
		RequestNextPhase();
	}

	OnTransitionFinished.Broadcast(TransitionId, true);
}

IBossPhaseTransitionHandler* UBossPhaseComponent::GetTransitionHandler() const
{
	return Cast<IBossPhaseTransitionHandler>(TransitionHandlerObject.Get());
}

/* 이동 경로 */

bool UBossPhaseComponent::ValidateMovementRoutes(FString& OutError) const
{
	OutError.Reset();

	for (const FBossPhaseDefinition& Phase : Config->Phases)
	{
		// 기본 이동과 패턴별 예외 모두 Waypoints를 사용하지 않으면 경로 검사 생략
		if (!Phase.UsesWaypoints())
		{
			continue;
		}

		const FString PhaseName = Phase.PhaseId.ToString();
		const FString RouteName = Phase.MovementRouteId.ToString();

		// 경로가 레벨(보스방 매니저)에 등록되지 않음
		const FBossMovementRoute* Route = MovementRoutes.Find(Phase.MovementRouteId);
		if (!Route || Route->Points.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Phase '%s': route '%s' is not registered or empty."), *PhaseName, *RouteName);
			return false;
		}

		// 지점마다 유효성과 같은 월드인지 검사
		for (int32 PointIndex = 0; PointIndex < Route->Points.Num(); ++PointIndex)
		{
			const ATargetPoint* Point = Route->Points[PointIndex].Get();
			if (!IsValid(Point) || Point->GetWorld() != GetWorld())
			{
				OutError = FString::Printf(TEXT("Phase '%s': route '%s', point [%d] is invalid or in another world."),
					*PhaseName, *RouteName, PointIndex);
				return false;
			}
		}
	}

	return true;
}

void UBossPhaseComponent::ResetWaypointProgress()
{
	CurrentWaypointIndex = 0;
	WaypointDirection = 1;
}
