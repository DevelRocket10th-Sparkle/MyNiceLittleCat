/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 게임 진행상태(체크포인트·진행 플래그) 관리와 저장·로드를 담당하는 게임 인스턴스 서브시스템입니다.
/// 게임 인스턴스에 붙어 맵이 바뀌어도 유지되고, 초기화 시 세이브 슬롯이 있으면 자동으로 로드합니다.
/// ACheckpoint가 상호작용 시 SetCheckpoint를 호출합니다.
/// 리스폰(언제·어디서 다시 스폰할지)은 AMainGameMode가 담당하고, 위치는 GetRespawnTransform으로 조회합니다.
/// TODO: 저장 UI 알림은 추후 보완

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Frameworks/Save/GameProgressSaveGame.h"
#include "GameProgressSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointChanged, FName, CheckpointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgressSaved, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayerRested);
// 세이브 직전 (맵의 기믹 등이 현재 상태를 세이브 데이터에 기록)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnCollectSaveData, UGameProgressSaveGame*);

UCLASS()
class MYNICELITTLECAT_API UGameProgressSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// ---- 체크포인트 ----

	// 현재 맵의 체크포인트를 등록 (bSave면 바로 슬롯에 저장)
	UFUNCTION(BlueprintCallable, Category = "Progress|Checkpoint", meta = (WorldContext = "WorldContextObject"))
	void SetCheckpoint(const UObject* WorldContextObject, FName CheckpointId, const FTransform& RespawnTransform, bool bSave = true);

	UFUNCTION(BlueprintPure, Category = "Progress|Checkpoint")
	const FCheckpointSaveData& GetCheckpointData() const { return CurrentSave->LastCheckpoint; }

	// 마지막 체크포인트가 현재 맵에 있고 Id가 같은지
	UFUNCTION(BlueprintPure, Category = "Progress|Checkpoint", meta = (WorldContext = "WorldContextObject"))
	bool IsCurrentCheckpoint(const UObject* WorldContextObject, FName CheckpointId) const;

	// 현재 맵에 등록된 체크포인트의 부활 위치 (없으면 false)
	UFUNCTION(BlueprintCallable, Category = "Progress|Checkpoint", meta = (WorldContext = "WorldContextObject"))
	bool GetRespawnTransform(const UObject* WorldContextObject, FTransform& OutTransform) const;

	// 체크포인트를 발견 목록에 추가 (이미 있으면 갱신). 새로 발견했으면 true
	UFUNCTION(BlueprintCallable, Category = "Progress|Checkpoint", meta = (WorldContext = "WorldContextObject"))
	bool DiscoverCheckpoint(const UObject* WorldContextObject, FName CheckpointId, const FText& DisplayName, const FTransform& RespawnTransform);

	UFUNCTION(BlueprintPure, Category = "Progress|Checkpoint")
	const TArray<FCheckpointSaveData>& GetDiscoveredCheckpoints() const { return CurrentSave->DiscoveredCheckpoints; }

	UFUNCTION(BlueprintCallable, Category = "Progress|Checkpoint")
	void UnlockCheckpoint(FName LevelName, FName CheckpointId, const FText& DisplayName, bool bSave = true);

	// 저장된 체크포인트 데이터를 그대로 마지막 체크포인트로 지정 (다른 맵 체크포인트로 빠른 이동할 때)
	void SetCheckpointData(const FCheckpointSaveData& Data, bool bSave = true);

	// ---- 진행 플래그 ----

	UFUNCTION(BlueprintCallable, Category = "Progress|Flag")
	void SetProgressFlag(FName Flag, bool bEnabled = true);

	UFUNCTION(BlueprintPure, Category = "Progress|Flag")
	bool HasProgressFlag(FName Flag) const;

	// ---- 기믹 상태 ----

	// 현재 맵 기믹의 켜짐 상태 기록 (보통 세이브 직전 OnCollectSaveData에서 호출)
	UFUNCTION(BlueprintCallable, Category = "Progress|Gimmick", meta = (WorldContext = "WorldContextObject"))
	void SetGimmickState(const UObject* WorldContextObject, FName GimmickId, bool bActivated);

	// 현재 맵 기믹의 저장된 켜짐 상태 (기록이 없으면 false)
	UFUNCTION(BlueprintCallable, Category = "Progress|Gimmick", meta = (WorldContext = "WorldContextObject"))
	bool GetGimmickState(const UObject* WorldContextObject, FName GimmickId, bool& bOutActivated) const;

	// ---- 저장·로드 ----

	// 슬롯에 저장. 체크포인트 상호작용 때 호출되고, 자동 저장 등 다른 시점에도 어디서든 호출 가능
	// 저장 직전 OnCollectSaveData를 브로드캐스트해 맵 기믹 상태를 모음
	UFUNCTION(BlueprintCallable, Category = "Progress|Save")
	bool SaveProgress();

	// 슬롯에서 로드 (슬롯이 없거나 실패하면 false, 현재 데이터 유지)
	UFUNCTION(BlueprintCallable, Category = "Progress|Save")
	bool LoadProgress();

	// 슬롯 삭제 + 현재 데이터 초기화 (새 게임)
	UFUNCTION(BlueprintCallable, Category = "Progress|Save")
	void ResetProgress();

	UFUNCTION(BlueprintPure, Category = "Progress|Save")
	bool HasSaveSlot() const;

	UFUNCTION(BlueprintPure, Category = "Progress|Save")
	UGameProgressSaveGame* GetCurrentSave() const { return CurrentSave; }

public:
	// 등록된 체크포인트가 바뀌면 브로드캐스트 (맵의 체크포인트 액터들이 활성 표시 갱신)
	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnCheckpointChanged OnCheckpointChanged;

	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnProgressSaved OnProgressSaved;

	// 체크포인트에서 휴식했을 때 (포션 충전, 몬스터 리셋 등을 여기에 연결)
	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnPlayerRested OnPlayerRested;

	// 세이브 직전 (C++ 전용)
	FOnCollectSaveData OnCollectSaveData;

	// 세이브 슬롯 (TODO(합의): 슬롯 여러 개 / 이어하기 UI 생기면 조정)
	FString SaveSlotName = TEXT("GameProgress");
	int32 SaveUserIndex = 0;

private:
	static FString GetLevelName(const UObject* WorldContextObject);
	static FName MakeGimmickKey(const UObject* WorldContextObject, FName GimmickId);

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameProgressSaveGame> CurrentSave;
};
