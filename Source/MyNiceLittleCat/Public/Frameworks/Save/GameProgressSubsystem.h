/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 게임 진행상태(체크포인트·진행 플래그) 관리와 저장·로드를 담당하는 게임 인스턴스 서브시스템입니다.
/// 게임 인스턴스에 붙어 맵이 바뀌어도 유지되고, 초기화 시 세이브 슬롯이 있으면 자동으로 로드합니다.
/// ACheckpoint가 상호작용 시 SetCheckpoint를 호출하고, 부활은 RespawnPlayer로 처리합니다.
/// RespawnPlayer: 기존 폰 제거 → 게임 모드 RestartPlayer(게임 모드별 처리 유지, 예: 펫 재연결)
///                → 현재 맵에 체크포인트가 있으면 그 위치로 텔레포트 (없으면 PlayerStart 그대로)
/// 사망 → 부활 호출 연결은 플레이어 담당과 합의 필요 (TODO(합의)), 지금은 테스트 액터 키로 호출합니다.
/// TODO: 사망~부활 연출(페이드 등)과 저장·부활 UI 알림은 추후 보완

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Frameworks/Save/GameProgressSaveGame.h"
#include "GameProgressSubsystem.generated.h"

class AController;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCheckpointChanged, FName, CheckpointId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProgressSaved, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRespawned, AController*, Controller, APawn*, NewPawn);

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

	// 기존 폰을 제거하고 다시 스폰 (체크포인트가 있으면 그 위치로)
	UFUNCTION(BlueprintCallable, Category = "Progress|Checkpoint")
	bool RespawnPlayer(AController* Controller);

	// ---- 진행 플래그 ----

	UFUNCTION(BlueprintCallable, Category = "Progress|Flag")
	void SetProgressFlag(FName Flag, bool bEnabled = true);

	UFUNCTION(BlueprintPure, Category = "Progress|Flag")
	bool HasProgressFlag(FName Flag) const;

	// ---- 저장·로드 ----

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

	UPROPERTY(BlueprintAssignable, Category = "Progress")
	FOnPlayerRespawned OnPlayerRespawned;

	// 세이브 슬롯 (TODO(합의): 슬롯 여러 개 / 이어하기 UI 생기면 조정)
	FString SaveSlotName = TEXT("GameProgress");
	int32 SaveUserIndex = 0;

private:
	static FString GetLevelName(const UObject* WorldContextObject);

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameProgressSaveGame> CurrentSave;
};
