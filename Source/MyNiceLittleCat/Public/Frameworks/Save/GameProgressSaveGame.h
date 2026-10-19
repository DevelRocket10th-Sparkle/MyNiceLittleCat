/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 게임 진행상태 세이브 데이터입니다.
/// UGameProgressSubsystem이 슬롯에 저장·로드합니다. 직접 만들지 말고 서브시스템을 통해 사용합니다.
/// 지금은 마지막 체크포인트와 진행 플래그(이름 집합)만 저장합니다.
/// 저장 항목이 늘어나면 여기에 UPROPERTY를 추가하고 SaveVersion을 올립니다.
/// TODO(합의): 저장할 데이터 목록 (플레이어 HP·능력 해금, 기믹·보스 진행 등)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameProgressSaveGame.generated.h"

// 마지막으로 상호작용한 체크포인트
USTRUCT(BlueprintType)
struct FCheckpointSaveData
{
	GENERATED_BODY()

	// 체크포인트를 등록한 적이 있는지
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	bool bValid = false;

	// 체크포인트가 있는 맵 이름 (PIE 접두사 제외)
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FString LevelName;

	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FName CheckpointId;

	// 부활 위치 (발 위치 기준) · 방향
	UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
	FTransform RespawnTransform;
};

UCLASS()
class MYNICELITTLECAT_API UGameProgressSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	// 세이브 구조 버전 (항목 추가·변경 시 올림)
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	int32 SaveVersion = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FDateTime SavedTime;

	UPROPERTY(BlueprintReadOnly, Category = "Save")
	FCheckpointSaveData LastCheckpoint;

	// 진행 플래그 (스테이지 클리어, 보스 처치 등). 이름 규칙은 TODO(합의)
	UPROPERTY(BlueprintReadOnly, Category = "Save")
	TSet<FName> ProgressFlags;
};
