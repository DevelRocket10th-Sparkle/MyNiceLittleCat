// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Boss/Components/BossPhaseComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

// Sets default values for this component's properties
UBossPhaseComponent::UBossPhaseComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
// 컴포넌트 초기화 후 실행 준비
void UBossPhaseComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UBossPhaseComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


bool UBossPhaseComponent::InitializePhases(UAbilitySystemComponent* InASC, UBossPhaseConfigData* InConfig)
{
	StopAndInvalidate();
	ClearGrantedAbilities();
	ASC = InASC;
	Config = InConfig;
	CurrentPhaseIndex = INDEX_NONE;
	// 설정 오류를 로그에 표시하기 위한 설명
	FString Error;
	if (!GetOwner()->HasAuthority() || !InASC || !InConfig || !InConfig->ValidateConfiguration(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Phase initialization failed: %s"), *Error);
		return false;
	}
	for (int32 Index = 0; Index < Config->Phases.Num(); ++Index)
	{
		// 해당 인덱스 진입 전 실행할 전환 능력 설정
		const FBossPhaseDefinition& Phase = Config->Phases[Index];
		if (Config->IsLogOnlyPhase(Index) || !Phase.TransitionAbility) continue;
		// 전환마다 고유 핸들을 사용하여 이전 전환 종료와 구분
		const FGameplayAbilitySpecHandle Handle = InASC->GiveAbility(
			FGameplayAbilitySpec(Phase.TransitionAbility, 1, INDEX_NONE, this));
		if (!Handle.IsValid()) { ClearGrantedAbilities(); return false; }
		TransitionHandles.Add(Index, Handle);
	}
	AbilityEndedDelegateHandle = InASC->OnAbilityEnded.AddUObject(this, &UBossPhaseComponent::HandleAbilityEnded);
	return true;
}

// 첫 페이즈 설정 적용 후 체력 조건 평가 허용
bool UBossPhaseComponent::StartPhases()
{
	if (!GetOwner()->HasAuthority() || !Config || !ASC.IsValid() || !ApplyPhase.IsBound()
		|| CurrentPhaseIndex != INDEX_NONE) return false;
	CurrentPhaseIndex = 0;
	// 설정 적용 중에는 피해가 허용되지 않도록 Active 전환을 나중에 수행
	RunState = EBossPhaseRunState::Transitioning;
	if (!IsCurrentPhaseLogOnly() && !ApplyPhase.Execute(Config->Phases[0])) { StopAndInvalidate(); return false; }
	RunState = EBossPhaseRunState::Active;
	return true;
}

const FBossPhaseDefinition* UBossPhaseComponent::GetCurrentPhase() const
{
	return Config && Config->Phases.IsValidIndex(CurrentPhaseIndex) ? &Config->Phases[CurrentPhaseIndex] : nullptr;
}

FName UBossPhaseComponent::GetCurrentPhaseId() const
{
	// 초기화 전에는 유효한 페이즈가 없으므로 None 반환
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	return Phase ? Phase->PhaseId : NAME_None;
}

// 현재 전투 페이즈에서 허용하는 최소 체력 반환
float UBossPhaseComponent::GetDamageHealthFloor(float MaxHealth) const
{
	// 배리어에서는 체력 하한 대신 BossCharacter의 피해 차단을 사용
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	return Phase && !IsCurrentPhaseLogOnly() ? Phase->HealthFloorRatio * MaxHealth : 0.f;
}

// 현재 전투 페이즈의 하한 체력 도달 여부 확인
void UBossPhaseComponent::EvaluateHealth(float Health, float MaxHealth)
{
	// 현재 활성 전투 페이즈에 대해서만 체력 조건 평가
	const FBossPhaseDefinition* Phase = GetCurrentPhase();
	if (IsActive() && Phase && !IsCurrentPhaseLogOnly()
		&& Phase->HealthFloorRatio > 0.f && Health > 0.f && MaxHealth > 0.f
		&& Health <= MaxHealth * Phase->HealthFloorRatio) RequestNextPhase();
}



// 다음 페이즈 전환을 중복 없이 요청하고 즉시 보호 처리 통지
void UBossPhaseComponent::RequestNextPhase()
{
	if (!IsActive() || !Config || !Config->Phases.IsValidIndex(CurrentPhaseIndex + 1)) return;
	PendingPhaseIndex = CurrentPhaseIndex + 1;
	ActiveTransitionId = ++NextTransitionId;
	TransitionResult = EEnemyPatternResult::Running;
	RunState = EBossPhaseRunState::TransitionPending;
	OnTransitionRequested.Broadcast();
}

// 대기 중인 요청의 전환 GA 시작
bool UBossPhaseComponent::BeginPendingTransition(int64& OutTransitionId)
{
	OutTransitionId = 0;
	if (!HasPendingTransition() || !ASC.IsValid()) return false;
	RunState = EBossPhaseRunState::Transitioning;
	OutTransitionId = ActiveTransitionId;
	// 전환 연출이 비어 있으면 Task의 다음 Tick에서 바로 페이즈 적용
	const FGameplayAbilitySpecHandle* Handle = TransitionHandles.Find(PendingPhaseIndex);
	if (!Handle) { TransitionResult = EEnemyPatternResult::Completed; return true; }
	ActiveTransitionHandle = *Handle;
	if (!ASC->TryActivateAbility(ActiveTransitionHandle)) TransitionResult = EEnemyPatternResult::Failed;
	if (TransitionResult == EEnemyPatternResult::Running)
		GetWorld()->GetTimerManager().SetTimer(TransitionTimeoutHandle, this,
			&UBossPhaseComponent::HandleTransitionTimeout, Config->Phases[PendingPhaseIndex].TransitionTimeout, false);
	return true;
}

// 현재 전환 번호와 일치하는 결과만 반환
EEnemyPatternResult UBossPhaseComponent::GetTransitionResult(int64 TransitionId) const
{
	return IsTransitioning() && TransitionId != 0 && TransitionId == ActiveTransitionId
		? TransitionResult : EEnemyPatternResult::Invalid;
}

// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
void UBossPhaseComponent::HandleAbilityEnded(const FAbilityEndedData& EndedData)
{
	if (!IsTransitioning() || TransitionResult != EEnemyPatternResult::Running
		|| EndedData.AbilitySpecHandle != ActiveTransitionHandle) return;
	GetWorld()->GetTimerManager().ClearTimer(TransitionTimeoutHandle);
	TransitionResult = EndedData.bWasCancelled ? EEnemyPatternResult::Cancelled : EEnemyPatternResult::Completed;
}

void UBossPhaseComponent::HandleTransitionTimeout()
{
	if (!IsTransitioning() || TransitionResult != EEnemyPatternResult::Running) return;
	TransitionResult = EEnemyPatternResult::Failed;
	if (ASC.IsValid()) ASC->CancelAbilityHandle(ActiveTransitionHandle);
}

// 정상 종료한 전환에 한해 다음 페이즈 설정 확정
bool UBossPhaseComponent::CompleteTransition(int64 TransitionId)
{
	if (GetTransitionResult(TransitionId) != EEnemyPatternResult::Completed || !ApplyPhase.IsBound()) return false;
	CurrentPhaseIndex = PendingPhaseIndex;
	PendingPhaseIndex = INDEX_NONE;
	// 연속된 미담당 페이즈를 한 번씩 기록하고 다음 구현된 페이즈로 이동
	while (Config->IsLogOnlyPhase(CurrentPhaseIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[BossPhase][LogOnly] %s skipped"), *Config->Phases[CurrentPhaseIndex].PhaseId.ToString());
		if (CurrentPhaseIndex == Config->Phases.Num() - 1) break;
		++CurrentPhaseIndex;
	}
	if (!IsCurrentPhaseLogOnly() && !ApplyPhase.Execute(Config->Phases[CurrentPhaseIndex])) return false;
	// 기믹 적용 중 정지 요청이 발생했다면 다시 활성화하지 않음
	if (!IsTransitioning() || ActiveTransitionId != TransitionId) return false;
	RunState = EBossPhaseRunState::Active;
	ActiveTransitionId = 0;
	return true;
}

// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
void UBossPhaseComponent::StopAndInvalidate()
{
	RunState = EBossPhaseRunState::Stopped;
	ActiveTransitionId = 0;
	PendingPhaseIndex = INDEX_NONE;
	TransitionResult = EEnemyPatternResult::Invalid;
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(TransitionTimeoutHandle);
	if (ASC.IsValid())
	{
		ASC->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		for (const auto& Pair : TransitionHandles) ASC->CancelAbilityHandle(Pair.Value);
	}
	AbilityEndedDelegateHandle.Reset();
}

// 이 컴포넌트가 부여한 GA를 중복 없이 서버에서 제거
void UBossPhaseComponent::ClearGrantedAbilities()
{
	if (ASC.IsValid() && GetOwner()->HasAuthority())
		for (const auto& Pair : TransitionHandles) ASC->ClearAbility(Pair.Value);
	TransitionHandles.Reset();
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void UBossPhaseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAndInvalidate();
	ClearGrantedAbilities();
	ApplyPhase.Unbind();
	OnTransitionRequested.Clear();
	Super::EndPlay(EndPlayReason);
}

// 현재 페이즈가 다른 담당자의 로그 전용 항목인지 확인
bool UBossPhaseComponent::IsCurrentPhaseLogOnly() const
{
	return Config && Config->IsLogOnlyPhase(CurrentPhaseIndex);
}
