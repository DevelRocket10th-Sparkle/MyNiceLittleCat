/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 실제 게임용 게임 모드입니다.
/// 플레이어 스폰·리스폰 규칙과 펫 스폰(ATestPetGameMode에서 이식)을 담당합니다.
/// 스폰 위치: 현재 맵에 등록된 체크포인트(UGameProgressSubsystem)가 있으면 그 RespawnPoint, 없으면 PlayerStart.
/// 리스폰: APlayerCharacter::OnPlayerDied 구독 → OnPlayerDeath → RespawnDelay(사망 모습) 뒤
/// 화면 전환(UScreenTransitionSubsystem: 입력 잠금 + 어둡게) → 완전히 어두울 때 RespawnPlayer(기존 폰 제거 후 RestartPlayer) → 밝게 → 입력 해제.
/// bUseRespawnTransition을 끄거나 화면 전환을 재생할 수 없으면 RespawnDelay 뒤 바로 RespawnPlayer.
/// 체크포인트 데이터·저장은 UGameProgressSubsystem, 이 클래스는 언제·어디서 다시 스폰할지만 정합니다.
/// TODO: 사망 UI 알림·효과음, 부활 직후 무적은 추후 보완

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Pet/PetTypes.h"
#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "MainGameMode.generated.h"

class APetCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerRespawned, AController*, Controller, APawn*, NewPawn);

UCLASS()
class MYNICELITTLECAT_API AMainGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AMainGameMode();

	virtual void RestartPlayer(AController* NewPlayer) override;

	// 플레이어 사망 알림. RespawnDelay 뒤 화면 전환과 함께 RespawnPlayer 호출 (bAutoRespawn일 때)
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	void OnPlayerDeath(AController* DeadController);

	// 기존 폰을 제거하고 다시 스폰 (체크포인트가 있으면 그 위치)
	UFUNCTION(BlueprintCallable, Category = "Respawn")
	bool RespawnPlayer(AController* Controller);

	UFUNCTION(BlueprintPure, Category = "Pet")
	APetCharacter* GetPet() const { return Pet; }

public:
	UPROPERTY(BlueprintAssignable, Category = "Respawn")
	FOnPlayerRespawned OnPlayerRespawned;

protected:
	// 현재 맵 체크포인트의 스폰 트랜스폼 (캡슐 절반 높이 보정 포함). 없으면 false
	bool GetCheckpointSpawnTransform(AController* Controller, FTransform& OutTransform);

	// 펫이 없으면 스폰하고, 새 플레이어 폰과 다시 연결
	void SetupPet(AController* Controller);

	// APlayerCharacter::OnPlayerDied 수신
	UFUNCTION()
	void HandlePlayerDied();

	// RespawnDelay가 끝난 뒤 호출. 화면 전환을 쓰면 어두워진 순간 RespawnPlayer, 아니면 바로 RespawnPlayer
	void BeginRespawn(AController* Controller);

protected:
	// 사망 후 자동 리스폰
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn")
	bool bAutoRespawn = true;

	// 사망 후 부활(화면 전환)을 시작할 때까지 시간 (사망 몽타주·래그돌을 보여 주는 시간)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (ClampMin = "0.0", EditCondition = "bAutoRespawn"))
	float RespawnDelay = 2.f;

	// 부활할 때 화면 전환(입력 잠금 + 어둡게 → 부활 → 밝게) 사용. 끄면 RespawnDelay 뒤 바로 부활
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (EditCondition = "bAutoRespawn"))
	bool bUseRespawnTransition = true;

	// 부활 화면 전환 설정 (시간·색·입력 잠금·소리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn", meta = (EditCondition = "bAutoRespawn && bUseRespawnTransition"))
	FScreenTransitionSettings RespawnTransition;

	// 게임 시작(첫 스폰)에도 저장된 체크포인트에서 시작 (이어하기). false면 첫 스폰은 PlayerStart
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Respawn")
	bool bSpawnAtCheckpointOnStart = true;

	UPROPERTY(EditDefaultsOnly, Category = "Pet")
	TSubclassOf<APetCharacter> PetClass;

	// 펫 초기 행동 (FPetInitializationData 세팅값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet")
	EPetBehavior PetInitialBehavior = EPetBehavior::Idle;

private:
	UPROPERTY(Transient)
	TObjectPtr<APetCharacter> Pet;

	FTimerHandle RespawnTimerHandle;

	// 사망 알림을 구독한 플레이어의 컨트롤러
	TWeakObjectPtr<AController> RespawnController;

	// RespawnPlayer 중인지 (첫 스폰과 리스폰 구분)
	bool bRespawning = false;
};
