#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossEncounterManager.generated.h"

class ABossCharacter;
class APlayerCharacter;
class USceneComponent;

// 보스방의 준비, 전투, 초기화, 클리어 및 재도전 대기 상태
UENUM(BlueprintType)
enum class EBossEncounterState : uint8 { Ready, Running, Resetting, Cleared, Waiting };

// 플레이어 사망 시 보스를 유지하며 대기하고, 명시적 초기화나 전환 실패 시 재생성
UCLASS()
class MYNICELITTLECAT_API ABossEncounterManager : public AActor
{
	GENERATED_BODY()
public :
	// 위치 지정용 루트 생성 및 Tick 비활성화
	ABossEncounterManager();
	// 연출 종료 후 전달받은 플레이어를 타깃으로 첫 페이즈 시작
	UFUNCTION(BlueprintCallable, Category = "Boss")
	bool StartEncounter(APlayerCharacter* Player);
	// 기존 보스 및 기믹 정리 후 새 보스로 체력과 페이즈 초기화
	// 보스와 기믹 정리 후 새 보스를 생성하여 체력 및 페이즈 초기화
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void ResetEncounter();
protected :
	// 컴포넌트 초기화 후 실행 준비
	virtual void BeginPlay() override;
	// 모든 콜백을 무효화하고 남아 있는 보스 및 타이머 정리
	// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 현재 보스 사망 시 클리어 처리: 새 보스를 생성하지 않음
	UFUNCTION()
	void HandleBossDefeated(ABossCharacter* Boss);
	// 전환 실패 등의 이벤트를 받아 다음 Tick에 초기화
	UFUNCTION()
	void HandleBossFailed(ABossCharacter* Boss);
	// 플레이어가 사망하거나 제거되면 보스를 유지하고 재도전 대기
	void CheckPlayer();
	// 사망 이벤트에서 즉시 전투를 중지하여 실패 콜백의 재생성보다 우선 처리
	UFUNCTION()
	void HandlePlayerDied();
	void EnterWaitingState();
	void ClearEncounterPlayer();
	// 레벨 기믹 참조를 주입한 뒤 보스 생성 완료
	// 레벨 기믹 참조를 먼저 전달한 뒤 보스 생성과 초기화 확인
	bool SpawnBoss();
	// 이벤트 해제 및 전투 정리 후 기존 보스 제거
	// 이벤트 해제 및 전투 정리 후 현재 보스 제거
	void RemoveCurrentBoss();

	// 레벨 배치 위치를 제공하는 루트 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<USceneComponent> SceneRoot;
	// 몽타주, 코인 및 페이즈 설정을 지정한 보스 Blueprint 클래스
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	TSubclassOf<ABossCharacter> BossClass;
	// 이번 보스방 실행에서 생성한 보스
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<ABossCharacter> CurrentBoss;
	// 보스방의 현재 진행 상태
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Boss")
	EBossEncounterState EncounterState = EBossEncounterState::Resetting;
private :
	// 이번 전투의 단일 플레이어
	UPROPERTY(Transient)
	TObjectPtr<APlayerCharacter> EncounterPlayer;
	// 생존 확인 반복 타이머
	FTimerHandle PlayerCheckHandle;
	// 다음 Tick 초기화 예약을 취소하기 위한 타이머
	FTimerHandle ResetTimerHandle;
	// 새 전투 및 초기화 시 증가하는 세션 번호
	int64 EncounterGeneration = 0;
};
