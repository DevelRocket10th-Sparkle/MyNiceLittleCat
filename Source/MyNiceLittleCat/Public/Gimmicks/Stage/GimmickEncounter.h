/// 작성자: 최요환
/// 작성일자: 2026-11-03
///
/// 플레이어가 영역에 들어오면 방을 잠그고, 지정한 몬스터를 모두 잡으면 다시 여는 잠금 전투 기믹입니다.
/// 시작: TriggerVolume 안에 플레이어가 들어오면(CheckInterval마다 검사, 레이저를 막지 않게 콜리전 없음) LockActors(입구 문 등)에 Deactivate를 보내 닫습니다.
/// 클리어: Enemies가 모두 사망(AEnemyCharacter::OnDeathStarted)하면 LockActors에 Activate를 보내 다시 열고,
///         이 기믹을 켜서(SetActivated) LinkedActors(출구 문 등)에 신호를 보냅니다.
/// 플레이어가 전투 중 사망하면(bResetOnPlayerDeath) 잠금만 풀고 대기 상태로 돌아갑니다. 이미 잡은 몬스터는 그대로입니다.
/// Enemies가 비어 있으면 BeginPlay에 TriggerVolume 안에 있는 몬스터를 자동으로 모읍니다.
/// bSaveState(기본 켜짐)로 클리어 상태를 저장하고, 저장된 클리어 상태로 시작하면 남은 몬스터를 제거합니다.
/// TODO(합의): 몬스터 재등장 규칙(휴식 시 리셋 등)이 정해지면 리셋 범위 조정

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/GimmickActorBase.h"
#include "GimmickEncounter.generated.h"

class UBoxComponent;
class AEnemyCharacter;
class APlayerCharacter;

UCLASS()
class MYNICELITTLECAT_API AGimmickEncounter : public AGimmickActorBase
{
	GENERATED_BODY()

public:
	AGimmickEncounter();

	// 전투 시작 (방 잠금). 남은 몬스터가 없으면 바로 클리어
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Encounter")
	void StartEncounter(AActor* EncounterInstigator);

	// 전투 중단 (잠금 해제, 대기 상태로)
	UFUNCTION(BlueprintCallable, Category = "Gimmick|Encounter")
	void ResetEncounter();

	UFUNCTION(BlueprintPure, Category = "Gimmick|Encounter")
	bool IsEncounterActive() const { return bEncounterActive; }

	UFUNCTION(BlueprintPure, Category = "Gimmick|Encounter")
	int32 GetRemainingEnemyCount() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 영역 안 몬스터 수집 (Enemies가 비었을 때)
	void CollectEnemiesInVolume();

	void BindEnemies();
	void UnbindEnemies();
	void ClearEncounter();
	void SetLockActorsOpen(bool bOpen);
	void BindPlayer(APlayerCharacter* Player);
	void UnbindPlayer();

	// 영역 안 플레이어 검사 (대기 중에만 CheckInterval마다)
	void StartWatching();
	void StopWatching();
	void CheckForPlayer();

	bool IsInsideVolume(const FVector& WorldLocation) const;

	void HandleEnemyDeathStarted(AEnemyCharacter* Enemy);

	UFUNCTION()
	void HandlePlayerDied();

	// 전투 시작 (문 닫힘 연출, 음악 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Encounter", meta = (DisplayName = "On Encounter Started"))
	void K2_OnEncounterStarted();

	// 모든 몬스터 처치
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Encounter", meta = (DisplayName = "On Encounter Cleared"))
	void K2_OnEncounterCleared();

	// 플레이어 사망 등으로 중단
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick|Encounter", meta = (DisplayName = "On Encounter Reset"))
	void K2_OnEncounterReset();

protected:
	// 전투 영역 (플레이어가 들어오면 시작, 비어 있는 Enemies 자동 수집 범위). 콜리전 없이 주기 검사
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerVolume;

	// 플레이어 진입 검사 주기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Encounter", meta = (ClampMin = "0.02", Units = "s"))
	float CheckInterval = 0.2f;

	// 잡아야 하는 몬스터 (비면 BeginPlay에 TriggerVolume 안 몬스터를 모음)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Encounter")
	TArray<TObjectPtr<AEnemyCharacter>> Enemies;

	// 전투 중 닫을 대상 (입구 문 등). 시작 때 Deactivate, 클리어·중단 때 Activate
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Encounter")
	TArray<TObjectPtr<AActor>> LockActors;

	// 전투 중 플레이어가 사망하면 잠금을 풀고 대기 상태로
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Encounter")
	bool bResetOnPlayerDeath = true;

	// 저장된 클리어 상태로 시작하면 남은 몬스터 제거
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Encounter")
	bool bDestroyEnemiesWhenRestoredCleared = true;

private:
	bool bEncounterActive = false;

	// 아직 살아 있는 몬스터
	TArray<TWeakObjectPtr<AEnemyCharacter>> AliveEnemies;

	TWeakObjectPtr<APlayerCharacter> BoundPlayer;

	FTimerHandle WatchTimer;
};
