/// 작성자: 최요환
/// 작성일자: 2026-10-22
///
/// 보스 페이즈와 배리어·자성 코인 기믹을 잇는 컴포넌트입니다.
/// 보스(ABossCharacter)에 붙이면 보스가 IBossPhaseGimmickInterface로 찾아 호출합니다 (보스당 1개).
/// - ReceiveLevelGimmickReferences: 매니저가 넘긴 PhaseId별 레벨 액터(배리어)와 영역 중심 저장
/// - EnterBossPhase: 배리어 페이즈면 PhaseId의 배리어를 켜고 붕괴를 구독, CoinPatternPhaseIds에 있으면 같은 액터의
///   UMagneticCoinPatternComponent 시작. 배리어 페이즈인데 배리어가 없으면 false (설정 오류로 전투 실패 처리)
/// - 배리어 붕괴 시 진입 때 받은 PhaseId·실행 번호로 보스에 NotifyPhaseGimmickCompleted
/// - StopBossGimmicks: 구독 해제, 남은 배리어 끄기, 코인 패턴 정지 (여러 번 호출해도 안전)
/// TODO(합의): PhaseId 이름(Barrier70 등)은 임시, 최종 보스 데이터 머지 후 테스트하며 조정

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemy/Boss/Components/BossPhaseGimmickInterface.h"
#include "BossGimmickComponent.generated.h"

class ABarrier;
class UMagneticCoinPatternComponent;

UCLASS(ClassGroup = (Gimmick), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UBossGimmickComponent : public UActorComponent, public IBossPhaseGimmickInterface
{
	GENERATED_BODY()

public:
	UBossGimmickComponent();

	// IBossPhaseGimmickInterface
	virtual bool EnterBossPhase_Implementation(FName PhaseId, int64 PhaseExecutionId) override;
	virtual void StopBossGimmicks_Implementation() override;
	virtual void ReceiveLevelGimmickReferences_Implementation(const TMap<FName, AActor*>& PhaseActors, AActor* AreaCenter) override;

	// 현재 진행 중인 배리어 (없으면 nullptr)
	UFUNCTION(BlueprintPure, Category = "BossGimmick")
	ABarrier* GetActiveBarrier() const { return ActiveBarrier; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBarrierBroken(ABarrier* Barrier);

	// 진행 중인 기믹 정리 (인터페이스 호출과 EndPlay 공용)
	void StopAll();

	// 소유 액터의 코인 패턴 컴포넌트 (없으면 nullptr)
	UMagneticCoinPatternComponent* GetCoinPattern() const;

	// 자성 코인 패턴을 켤 페이즈
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	TArray<FName> CoinPatternPhaseIds = { TEXT("Barrier70") };

	// 정지 시 남은 코인 제거
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	bool bClearCoinsOnStop = true;

	// 정지 시 아직 깨지지 않은 배리어 끄기 (생성장치도 정지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	bool bDeactivateBarrierOnStop = true;

private:
	// 매니저가 넘긴 PhaseId별 레벨 액터
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TMap<FName, TObjectPtr<AActor>> PhaseActors;

	// 매니저가 넘긴 영역 중심 (코인 스폰 기준)
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TObjectPtr<AActor> AreaCenter;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TObjectPtr<ABarrier> ActiveBarrier;

	// 진입 때 받은 값: 완료 통지에 그대로 전달
	FName ActivePhaseId;
	int64 ActivePhaseExecutionId = 0;
};
