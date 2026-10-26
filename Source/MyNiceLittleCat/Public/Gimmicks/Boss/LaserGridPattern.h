/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 보스 2페이즈(70~30%)의 바닥 광역 레이저 패턴입니다. 경기장을 가로·세로 줄(레인)로 나누고, 줄 단위로 예고 후 레이저를 쏩니다.
/// 보스가 직접 쏘지 않는 패턴이라 경기장 중앙에 배치하는 액터로 두고, 보스 페이즈가 StartPattern/StopPattern만 호출합니다.
/// 공격 1회(줄 하나): 줄 전체에 사각형 경고 데칼(AAttackWarningDecal) 표시 → WarningTime 뒤 줄 한쪽 끝에서 반대쪽 끝으로
/// FireDuration 동안 레이저 발사(ULaserBeamComponent, 빔 굵기 = 줄 폭). 경고 진행도는 레이저가 나오는 쪽에서 차오릅니다.
/// 축: X = 레이저가 X 방향으로 지나감(줄은 Y 위치로 구분), Y = 레이저가 Y 방향으로 지나감. 모두 이 액터의 로컬 축 기준입니다.
/// 줄 고르기(TargetMode): TargetPlayer(플레이어가 서 있는 줄, CrossChance 확률로 가로·세로 동시) / Random / Sweep(끝에서부터 차례로) / Cross(항상 가로·세로)
/// StrikeInterval마다 다음 공격을 시작하므로 앞 공격이 끝나기 전에 다음 예고가 겹칠 수 있습니다 (동시 공격 수는 MaxConcurrentStrikes).
/// Sequence 모드: Sequences(ULaserGridSequenceData)의 Steps를 순서대로 실행합니다. 단계마다 축·줄·나오는 쪽·다음까지 시간을 지정하고,
/// 시퀀스가 끝나면 RestAfter만큼 쉬고 목록에서 다음 시퀀스를 고릅니다 (bRandomSequenceOrder면 무작위, 아니면 차례로).
/// 1회 실행(PlaySequenceOnce): 시퀀스 하나만 실행하고 마지막 레이저까지 끝나면 OnSequenceFinished를 알립니다 (RestAfter는 쓰지 않음).
/// 보스 패턴 GA(UGA_BossLaserGrid)가 이 방식으로 사용합니다. 반복 실행 중에는 시작할 수 없고, StopPattern은 두 방식 모두 정리합니다.
/// 보스 지점(화살표): BossStandPoint = 격자 패턴 동안 보스가 서 있는 곳(격자 바깥), BossBeamPoint = 단일 레이저 패턴 때 보스가 들어오는 곳(격자 안쪽).
/// 화살표 방향이 도착 후 보스가 바라보는 방향이며, 레벨 인스턴스에서 컴포넌트를 옮겨 조정합니다.
/// TODO: 레이저 그래픽(지금은 디버그 라인), 경고·발사 사운드

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gimmicks/Boss/LaserGridTypes.h"
#include "LaserGridPattern.generated.h"

class UArrowComponent;
class UBoxComponent;
class ULaserBeamComponent;
class AAttackWarningDecal;
class ULaserGridSequenceData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLaserGridStrike, ELaserGridAxis, Axis, int32, LaneIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLaserGridSequenceFinished, ULaserGridSequenceData*, Sequence);

// 공격 1회가 쓰는 레이저·경고 묶음 (풀로 돌려 씀)
USTRUCT()
struct FLaserGridSlot
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ULaserBeamComponent> Laser;

	UPROPERTY()
	TObjectPtr<AAttackWarningDecal> Warning;

	bool bBusy = false;

	// 예고한 공격 (발사 때 사용)
	ELaserGridAxis Axis = ELaserGridAxis::X;

	int32 LaneIndex = 0;

	// true면 축의 + 쪽에서 - 쪽으로 발사
	bool bReversed = false;

	float FireDuration = 0.f;

	FTimerHandle TimerHandle;
};

UCLASS()
class MYNICELITTLECAT_API ALaserGridPattern : public AActor
{
	GENERATED_BODY()

public:
	ALaserGridPattern();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category = "LaserGrid")
	void StartPattern();

	// 반복을 멈추고 진행 중인 예고·레이저도 모두 정리
	UFUNCTION(BlueprintCallable, Category = "LaserGrid")
	void StopPattern();

	// 시퀀스 하나만 실행 (Sequence가 비면 Sequences 목록에서 고름). 이미 실행 중이거나 시퀀스가 없으면 false
	// DamagePerHitOverride가 0 이상이면 이번 실행의 레이저 피해량으로 사용 (보스 공격력 연동), 음수면 레이저 기본값
	UFUNCTION(BlueprintCallable, Category = "LaserGrid")
	bool PlaySequenceOnce(ULaserGridSequenceData* Sequence = nullptr, float DamagePerHitOverride = -1.f);

	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	bool IsPatternActive() const { return bPatternActive; }

	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	bool IsPlayingOnce() const { return bPlayingOnce; }

	// 줄 하나를 바로 공격 (예고부터 시작, 빈 슬롯이 없으면 false)
	UFUNCTION(BlueprintCallable, Category = "LaserGrid")
	bool StrikeLane(ELaserGridAxis Axis, int32 LaneIndex);

	// 해당 축 공격의 줄 개수
	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	int32 GetLaneCount(ELaserGridAxis Axis) const;

	// 월드 위치가 속한 줄 번호 (영역 밖이면 가장 가까운 줄)
	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	int32 GetLaneIndexAt(ELaserGridAxis Axis, const FVector& WorldLocation) const;

	// 격자 패턴 동안 보스가 서 있을 지점 (바닥, 월드)
	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	FTransform GetBossStandTransform() const;

	// 단일 레이저 패턴 때 보스가 들어올 지점 (바닥, 월드)
	UFUNCTION(BlueprintPure, Category = "LaserGrid")
	FTransform GetBossBeamTransform() const;

	// 테스트용: 패턴 시작/정지
	UFUNCTION(CallInEditor, Category = "LaserGrid|Test")
	void TogglePattern();

public:
	// 예고 시작 시
	UPROPERTY(BlueprintAssignable, Category = "LaserGrid")
	FOnLaserGridStrike OnStrikeWarned;

	// 레이저 발사 시
	UPROPERTY(BlueprintAssignable, Category = "LaserGrid")
	FOnLaserGridStrike OnStrikeFired;

	// 1회 실행한 시퀀스의 마지막 레이저까지 끝났을 때 (StopPattern으로 멈추면 오지 않음)
	UPROPERTY(BlueprintAssignable, Category = "LaserGrid")
	FOnLaserGridSequenceFinished OnSequenceFinished;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// StrikeInterval마다 TargetMode에 따라 공격할 줄을 고름
	void HandleStrikeTick();

	// 예고부터 시작하는 공격 1회 (모든 공격이 거치는 곳)
	bool StartStrike(ELaserGridAxis Axis, int32 LaneIndex, bool bReversed, float InWarningTime, float InFireDuration);

	// Sequence 모드: 다음 시퀀스를 골라 처음부터 실행
	void StartNextSequence();

	// Sequences 목록에서 다음 시퀀스를 고름 (bRandomSequenceOrder 규칙, 없으면 nullptr)
	ULaserGridSequenceData* PickNextSequence();

	// 1회 실행: 모든 단계를 시작했고 진행 중인 공격이 없으면 완료 처리
	void TryFinishSequenceOnce();

	// Sequence 모드: 현재 단계부터 DelayToNext가 0인 단계까지 한 번에 실행하고 다음 단계 타이머를 걺
	void RunSequenceSteps();

	// 단계의 줄 고르기 규칙으로 줄 번호를 구함 (영역 밖이면 INDEX_NONE)
	int32 ResolveStepLane(const FLaserGridStep& Step) const;

	void FireSlot(int32 SlotIndex);

	void ReleaseSlot(int32 SlotIndex);

	int32 FindFreeSlot() const;

	// 줄 폭 방향(줄을 구분하는 축)의 영역 절반 크기
	float GetCrossHalfSize(ELaserGridAxis Axis) const;

	// 레이저 진행 방향의 영역 절반 크기
	float GetAlongHalfSize(ELaserGridAxis Axis) const;

	// 줄 중심(바닥, 월드)과 레이저 진행 방향(월드)
	void GetLaneGeometry(ELaserGridAxis Axis, int32 LaneIndex, bool bReversed, FVector& OutCenter, FVector& OutDirection) const;

	APawn* GetTargetPawn() const;

	static ELaserGridAxis OtherAxis(ELaserGridAxis Axis) { return Axis == ELaserGridAxis::X ? ELaserGridAxis::Y : ELaserGridAxis::X; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	// 경기장 영역 표시 (콜리전 없음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> ArenaBox;

	// 격자 패턴 동안 보스 대기 지점 (격자 바깥, 화살표 = 바라볼 방향)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> BossStandPoint;

	// 단일 레이저 패턴 때 보스 지점 (격자 안쪽)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> BossBeamPoint;

	// 경기장 절반 크기 (로컬 X·Y, cm). 액터 위치 = 경기장 중앙 바닥
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Area", meta = (ClampMin = "50.0"))
	FVector2D ArenaHalfSize = FVector2D(1000.f, 1000.f);

	// 줄 폭 (cm). 빔 굵기와 경고 폭도 이 값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Area", meta = (ClampMin = "10.0"))
	float LaneWidth = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Pattern")
	ELaserGridTargetMode TargetMode = ELaserGridTargetMode::TargetPlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Pattern")
	bool bAllowAxisX = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Pattern")
	bool bAllowAxisY = true;

	// TargetPlayer: 가로·세로 동시 공격 확률
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CrossChance = 0.25f;

	// true면 레이저가 나오는 쪽(줄의 어느 끝)을 무작위로, false면 항상 축의 - 쪽에서 (Sequence 모드는 단계의 FireSide 사용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Pattern")
	bool bRandomFireSide = true;

	// Sequence 모드에서 실행할 공격 순서 목록
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Sequence")
	TArray<TObjectPtr<ULaserGridSequenceData>> Sequences;

	// true면 시퀀스를 무작위로 고름 (같은 시퀀스 연속은 피함), false면 목록 순서대로 반복
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Sequence")
	bool bRandomSequenceOrder = true;

	// 다음 공격까지 간격 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Timing", meta = (ClampMin = "0.1", Units = "s"))
	float StrikeInterval = 1.2f;

	// 예고 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Timing", meta = (ClampMin = "0.0", Units = "s"))
	float WarningTime = 1.2f;

	// 발사 시간 (초). 레이저 DamageInterval(1초)보다 짧으면 한 번 지나갈 때 한 번만 맞음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Timing", meta = (ClampMin = "0.05", Units = "s"))
	float FireDuration = 0.5f;

	// true면 StartPattern 직후 첫 공격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Timing")
	bool bStrikeOnStart = true;

	// true면 BeginPlay에서 바로 StartPattern (테스트용, 보스전에선 보통 false)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Timing")
	bool bAutoStart = false;

	// 동시에 진행할 수 있는 공격 수 (레이저 컴포넌트 수)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Laser", meta = (ClampMin = "1", ClampMax = "16"))
	int32 MaxConcurrentStrikes = 10;

	// 빔 중심 높이 (바닥 기준). 빔 반지름(줄 폭 절반)보다 높아야 바닥에 막히지 않음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Laser", meta = (ClampMin = "0.0"))
	float BeamHeight = 110.f;

	// 줄 경고 데칼 (사각형 머티리얼 BP). 비어 있으면 경고 없이 대기 후 발사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Warning")
	TSubclassOf<AAttackWarningDecal> WarningDecalClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "LaserGrid|Warning")
	FLinearColor WarningColor = FLinearColor(1.f, 0.05f, 0.f);

private:
	UPROPERTY(Transient)
	TArray<FLaserGridSlot> Slots;

	bool bPatternActive = false;

	FTimerHandle StrikeTimerHandle;

	// Sweep 진행 상태
	ELaserGridAxis SweepAxis = ELaserGridAxis::X;
	int32 SweepLane = 0;

	// Sequence 진행 상태
	UPROPERTY(Transient)
	TObjectPtr<ULaserGridSequenceData> CurrentSequence;

	int32 SequenceIndex = INDEX_NONE;

	int32 StepIndex = 0;

	// PlaySequenceOnce로 실행 중
	bool bPlayingOnce = false;

	// 1회 실행 중 모든 단계를 시작함 (남은 공격이 끝나기를 기다리는 중)
	bool bSequenceStepsDone = false;

	// 레이저 기본 피해량 (BeginPlay에서 레이저 컴포넌트 값으로 저장)
	float DefaultDamagePerHit = 1.f;

	// 이번 1회 실행의 피해량 (음수면 기본값)
	float ActiveDamageOverride = -1.f;
};
