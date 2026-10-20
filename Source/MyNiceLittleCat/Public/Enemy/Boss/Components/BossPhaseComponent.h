// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.20.
* 체력 하한, 페이즈 전환 요청, 현재 페이즈 관리
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayAbilitySpec.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "Enemy/Boss/Datas/BossPhaseConfigData.h"
#include "BossPhaseComponent.generated.h"

class UAbilitySystemComponent;
struct FAbilityEndedData;

// 현재 페이즈와 별개로 관리하는 전환 진행 상태
UENUM(BlueprintType)
enum class EBossPhaseRunState : uint8 { Stopped, Active, TransitionPending, Transitioning };

// BossCharacter가 페이즈별 기믹 설정을 적용하고 성공 여부를 반환
DECLARE_DELEGATE_RetVal_OneParam(bool, FApplyBossPhase, const FBossPhaseDefinition&);


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYNICELITTLECAT_API UBossPhaseComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UBossPhaseComponent();

	// 서버에서 페이즈 설정을 검사하고 전환 GA를 부여
	bool InitializePhases(UAbilitySystemComponent* InASC, UBossPhaseConfigData* InConfig);
	// 첫 페이즈에 진입: 정지 후 재사용하지 않고 보스를 재생성하여 초기화
	// 첫 페이즈 설정 적용 후 체력 조건 평가 허용
	bool StartPhases();
	// 전투 페이즈의 체력 하한 도달 여부 검사
	// 현재 전투 페이즈의 하한 체력 도달 여부 확인
	void EvaluateHealth(float Health, float MaxHealth);
	// Getter: 현재 페이즈가 실제 기믹을 실행하지 않는 로그 전용 항목인지 확인
	bool IsCurrentPhaseLogOnly() const;
	// 대기 중인 전환 시작: 성공하면 요청 ID를 반환
	// 대기 중인 요청의 전환 GA 시작
	bool BeginPendingTransition(int64& OutTransitionId);
	// 요청 ID가 일치하는 전환 결과 조회
	// 현재 전환 번호와 일치하는 결과만 반환
	EEnemyPatternResult GetTransitionResult(int64 TransitionId) const;
	// 정상 종료된 전환에 한해 다음 페이즈 설정 적용
	// 정상 종료한 전환에 한해 다음 페이즈 설정 확정
	bool CompleteTransition(int64 TransitionId);
	// 실행 번호 무효화 후 전환 GA, 제한 시간 및 이벤트 구독 정리
	// 요청을 먼저 무효화한 뒤 타이머, 구독 및 능력 실행 정리
	void StopAndInvalidate();
	// 현재 전투 페이즈의 피해 적용 하한 체력 반환
	// 현재 전투 페이즈에서 허용하는 최소 체력 반환
	float GetDamageHealthFloor(float MaxHealth) const;
	// 현재 페이즈 설정 조회: 초기화 전에는 nullptr
	const FBossPhaseDefinition* GetCurrentPhase() const;
	// 일반 공격 및 체력 조건 평가가 가능한 상태인지 확인
	bool IsActive() const { return RunState == EBossPhaseRunState::Active; }
	// BT가 처리해야 하는 전환 요청이 있는지 확인
	bool HasPendingTransition() const { return RunState == EBossPhaseRunState::TransitionPending; }
	// 전환 GA 또는 페이즈 적용 처리가 진행 중인지 확인
	bool IsTransitioning() const { return RunState == EBossPhaseRunState::Transitioning; }
	// Getter: 현재 페이즈 ID
	UFUNCTION(BlueprintPure, Category = "Boss|Phase")
	FName GetCurrentPhaseId() const;
	// 전환 요청 즉시 Boss가 공격 및 피해를 차단하도록 알리는 이벤트
	FSimpleMulticastDelegate OnTransitionRequested;
	// 구체적인 배리어 및 코인 시작은 Boss가 담당하는 단일 적용 함수
	FApplyBossPhase ApplyPhase;

protected:
	// Called when the game starts
	// 컴포넌트 초기화 후 실행 준비
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;


	// 액터 제거 시 전환 실행 및 직접 부여한 능력 정리
	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :
	// 현재 페이즈 다음 인덱스의 전환을 중복 없이 요청
	// 다음 페이즈 전환을 중복 없이 요청하고 즉시 보호 처리 통지
	void RequestNextPhase();
	// 현재 전환 GA의 종료만 받아 정상 완료 또는 취소 기록
	// 현재 능력의 종료 이벤트만 받아 완료 또는 취소 기록
	void HandleAbilityEnded(const FAbilityEndedData& EndedData);
	// 전환 종료 누락을 실패 처리하고 GA 취소
	void HandleTransitionTimeout();
	// 이 컴포넌트가 부여한 전환 GA만 제거
	// 이 컴포넌트가 부여한 GA를 중복 없이 서버에서 제거
	void ClearGrantedAbilities();

	// 공유 페이즈 설정 데이터
	UPROPERTY(Transient)
	TObjectPtr<UBossPhaseConfigData> Config;
	// 보스의 능력 실행 컴포넌트에 대한 약한 참조
	TWeakObjectPtr<UAbilitySystemComponent> ASC;
	// 다음 페이즈 인덱스별 전환 GA 핸들
	TMap<int32, FGameplayAbilitySpecHandle> TransitionHandles;
	// 현재 전환에서 실행한 GA 핸들
	FGameplayAbilitySpecHandle ActiveTransitionHandle;
	// ASC 종료 이벤트 구독 핸들
	FDelegateHandle AbilityEndedDelegateHandle;
	// 전환 제한 시간 타이머
	FTimerHandle TransitionTimeoutHandle;
	// 현재 적용된 페이즈 인덱스
	int32 CurrentPhaseIndex = INDEX_NONE;
	// 다음으로 적용할 페이즈 인덱스
	int32 PendingPhaseIndex = INDEX_NONE;
	// 전환 번호 발급용: 정지 시에도 이전 번호로 되돌리지 않음
	int64 NextTransitionId = 0;
	// 현재 전환 요청 번호: 0은 유효한 요청 없음
	int64 ActiveTransitionId = 0;
	// 전환 대기 및 실행 상태
	EBossPhaseRunState RunState = EBossPhaseRunState::Stopped;
	// 현재 전환 요청의 결과
	EEnemyPatternResult TransitionResult = EEnemyPatternResult::Invalid;
};
