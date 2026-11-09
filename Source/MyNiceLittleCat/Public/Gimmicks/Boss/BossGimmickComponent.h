/// 작성자: 최요환
/// 작성일자: 2026-10-22
///
/// 보스 페이즈와 배리어·자성 코인 기믹을 잇는 컴포넌트입니다.
/// 보스(ABossCharacter)에 붙이면 보스가 IBossPhaseGimmickInterface로 찾아 호출합니다 (보스당 1개).
/// - ReceiveLevelGimmickReferences: 매니저가 넘긴 PhaseId별 레벨 액터(배리어)와 영역 중심 저장
/// - EnterBossPhase: 배리어 페이즈면 PhaseId의 배리어를 켜고(bAttachBarrierToBoss면 보스에 붙여 따라가게 함) 붕괴를 구독.
///   CoinPatternPhases에 PhaseId가 있으면 코인 패턴 시작 (전투 페이즈도 진입 호출이 오므로 보스 공격과 동시에 진행 가능)
///   - 값(레벨 액터 키)이 있으면 매니저가 넘긴 그 액터(AMagneticCoinPatternActor 등)의 UMagneticCoinPatternComponent, 영역 중심은 그 액터
///   - 값이 None이면 보스 자신의 UMagneticCoinPatternComponent, 영역 중심은 매니저의 GimmickAreaCenter
///   배리어 페이즈인데 배리어가 없으면 false (설정 오류로 전투 실패 처리)
/// - 배리어 붕괴 시 진입 때 받은 PhaseId·실행 번호로 보스에 NotifyPhaseGimmickCompleted
/// - StopBossGimmicks: 구독 해제, 남은 배리어 끄기, 코인 패턴 정지 (여러 번 호출해도 안전)
/// - GetPhaseActor: 패턴 GA가 PhaseId 등 키로 레벨 액터(격자 레이저 등)를 찾을 때 사용
/// - 레이저 교대: LaserAlternationPhaseIds 페이즈에서는 격자(UGA_BossLaserGrid)와 단일(UGA_BossLaserBeam) 레이저가 순번대로만 실행됨.
///   각 GA의 CanActivateAbility가 IsLaserTurnAllowed로 확인하고, 정상 종료 시 CompleteLaserTurn으로 순번을 넘김 (취소는 넘기지 않음).
///   페이즈 진입·정지 때 FirstLaserTurn으로 초기화
/// - 반격 피해 (2026-11-06): 던진 자성 코인·반사된 레이저가 보스에 맞으면 TryApplyCounterDamage로 피해 GE(SetByCaller Data.Damage)를 보스 ASC에 적용.
///   종류별 허용 페이즈(CoinDamagePhaseIds·ReflectedLaserDamagePhaseIds)에서만, 배리어가 켜져 있으면 무시 (bIgnoreWhileBarrierActive)
/// TODO(합의): 보스 쪽에 패턴 순서·쿨다운 기능이 생기면 레이저 교대는 그쪽으로 옮김
/// TODO(합의): PhaseId 이름(Barrier70 등)은 임시, 최종 보스 데이터 머지 후 테스트하며 조정

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemy/Interfaces/BossPhaseGimmickInterface.h"
#include "BossGimmickComponent.generated.h"

class ABarrier;
class UMagneticCoinPatternComponent;
class UGameplayEffect;
class UBossGimmickComponent;

// 레이저 교대 페이즈에서 다음에 실행할 레이저 패턴
UENUM(BlueprintType)
enum class EBossLaserTurn : uint8
{
	Grid	UMETA(DisplayName = "Grid"),
	Beam	UMETA(DisplayName = "Beam"),
};

// 플레이어가 보스 공격·기믹을 되돌려 주는 피해 종류
UENUM(BlueprintType)
enum class EBossCounterDamageType : uint8
{
	MagneticCoin	UMETA(DisplayName = "Magnetic Coin"),
	ReflectedLaser	UMETA(DisplayName = "Reflected Laser"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnBossCounterDamaged, UBossGimmickComponent*, Component, AActor*, Source, EBossCounterDamageType, Type, float, Damage);

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

	// 매니저가 넘긴 레벨 액터 (키는 PhaseId, 없으면 nullptr). 패턴 GA가 격자 레이저 등 레벨 기믹을 찾을 때 사용
	UFUNCTION(BlueprintPure, Category = "BossGimmick")
	AActor* GetPhaseActor(FName Key) const;

	// 현재 페이즈가 레이저 교대 페이즈인지
	UFUNCTION(BlueprintPure, Category = "BossGimmick|Laser")
	bool IsLaserAlternationPhase() const;

	// 이 레이저 패턴을 지금 실행해도 되는지 (교대 페이즈가 아니면 항상 true)
	UFUNCTION(BlueprintPure, Category = "BossGimmick|Laser")
	bool IsLaserTurnAllowed(EBossLaserTurn Turn) const;

	// 레이저 패턴이 정상 종료됐을 때 순번을 다음으로 넘김 (교대 페이즈이고 자기 차례였을 때만)
	UFUNCTION(BlueprintCallable, Category = "BossGimmick|Laser")
	void CompleteLaserTurn(EBossLaserTurn FinishedTurn);

	UFUNCTION(BlueprintPure, Category = "BossGimmick|Laser")
	EBossLaserTurn GetLaserTurn() const { return CurrentLaserTurn; }

	// 지금 이 종류의 반격 피해를 받는지 (허용 페이즈 + 배리어 꺼짐)
	UFUNCTION(BlueprintPure, Category = "BossGimmick|CounterDamage")
	bool CanReceiveCounterDamage(EBossCounterDamageType Type) const;

	// 반격 피해 적용. 받을 수 없는 상태면 false (코인은 false면 사라지지 않음)
	// DamageOverride > 0이면 종류별 기본 피해량 대신 사용
	UFUNCTION(BlueprintCallable, Category = "BossGimmick|CounterDamage")
	bool TryApplyCounterDamage(AActor* Source, EBossCounterDamageType Type, float DamageOverride = -1.f);

	// 반격 피해가 들어갔을 때 (연출·UI용)
	UPROPERTY(BlueprintAssignable, Category = "BossGimmick|CounterDamage")
	FOnBossCounterDamaged OnCounterDamaged;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBarrierBroken(ABarrier* Barrier);

	// 진행 중인 기믹 정리 (인터페이스 호출과 EndPlay 공용)
	void StopAll();

	// 페이즈 진입 시 코인 패턴 시작 (CoinPatternPhases에 없으면 아무것도 안 함)
	void StartCoinPattern(FName PhaseId);

	// 자성 코인 패턴을 켤 페이즈: PhaseId → 매니저 PhaseGimmickActors의 코인 패턴 액터 키 (None이면 보스 자신의 컴포넌트)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	TMap<FName, FName> CoinPatternPhases = { { TEXT("Barrier70"), NAME_None } };

	// 정지 시 남은 코인 제거
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	bool bClearCoinsOnStop = true;

	// 정지 시 아직 깨지지 않은 배리어 끄기 (생성장치도 정지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	bool bDeactivateBarrierOnStop = true;

	// 배리어를 켤 때 보스에 붙여 보스를 감싸고 따라가게 함 (끄면 레벨에 놓인 자리 그대로 켜짐)
	// 높이는 배리어의 FollowOffset, 크기는 배리어 인스턴스 스케일로 조정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick")
	bool bAttachBarrierToBoss = true;

	// 격자·단일 레이저를 번갈아 쓰는 페이즈 (TODO(합의): 2페이즈 PhaseId 이름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|Laser")
	TArray<FName> LaserAlternationPhaseIds = { TEXT("CombatPhase2") };

	// 교대 페이즈에서 처음 실행할 레이저
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|Laser")
	EBossLaserTurn FirstLaserTurn = EBossLaserTurn::Grid;

	// 반격 피해 GE (SetByCaller Data.Damage로 피해량을 받는 GE). 비어 있으면 적 Damage 어트리뷰트에 피해량을 더하는 즉시 GE를 코드로 만들어 적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage")
	TSubclassOf<UGameplayEffect> CounterDamageEffectClass;

	// 던진 자성 코인 피해를 받는 페이즈
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage")
	TArray<FName> CoinDamagePhaseIds = { TEXT("CombatPhase3") };

	// 반사된 레이저 피해를 받는 페이즈 (2·3페이즈 같은 경험)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage")
	TArray<FName> ReflectedLaserDamagePhaseIds = { TEXT("CombatPhase2"), TEXT("CombatPhase3") };

	// 코인 1개 피해량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage", meta = (ClampMin = "0.0"))
	float CoinDamage = 5.f;

	// 반사 레이저 피해량 (ReflectedLaserDamageInterval마다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage", meta = (ClampMin = "0.0"))
	float ReflectedLaserDamage = 3.f;

	// 반사 레이저 피해 최소 간격(초). 반사된 빛이 계속 닿아도 이 간격마다 한 번만 피해
	// (빔이 잠깐 끊겼다 다시 닿아도, 레이저가 여러 개여도 보스 기준으로 셈)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage", meta = (ClampMin = "0.0"))
	float ReflectedLaserDamageInterval = 1.f;

	// 배리어(현재 페이즈 배리어 또는 매니저가 넘긴 배리어 중 하나라도)가 켜져 있으면 반격 피해 무시
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BossGimmick|CounterDamage")
	bool bIgnoreWhileBarrierActive = true;

	bool IsAnyBarrierActive() const;

private:
	// 마지막 반사 레이저 피해 시각 (-1 = 아직 없음)
	float LastReflectedLaserDamageTime = -1.f;

	// 매니저가 넘긴 PhaseId별 레벨 액터
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TMap<FName, TObjectPtr<AActor>> PhaseActors;

	// 매니저가 넘긴 영역 중심 (코인 스폰 기준)
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TObjectPtr<AActor> AreaCenter;

	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TObjectPtr<ABarrier> ActiveBarrier;

	// 현재 페이즈에서 켠 코인 패턴 (보스 자신 또는 레벨 액터의 컴포넌트)
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick")
	TObjectPtr<UMagneticCoinPatternComponent> ActiveCoinPattern;

	// 진입 때 받은 값: 완료 통지에 그대로 전달
	FName ActivePhaseId;
	int64 ActivePhaseExecutionId = 0;

	// 레이저 교대 순번
	UPROPERTY(VisibleInstanceOnly, Transient, Category = "BossGimmick|Laser")
	EBossLaserTurn CurrentLaserTurn = EBossLaserTurn::Grid;
};
