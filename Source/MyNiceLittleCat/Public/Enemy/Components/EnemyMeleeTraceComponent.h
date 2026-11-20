/*
* 작성자: 전선현
* 수정 일자: 2026.11.13. (대상 판정을 EnemyTargetUtils로 이동, 이벤트 이름 OnPlayerHit -> OnTargetHit)
* 수정 일자: 2026.11.20. (구 Sweep 디버그 그리기를 공용 함수로, 디버그 표시 기본값 끔)
* 근접 공격의 소켓 Sweep 판정 실행 상태 (캐릭터별)
* - 판정 소스: "Body"는 캐릭터 메시 자동 등록, "Weapon" 등은 RegisterHitSource로 등록하거나 부착 메시에서 자동 검색
* - 판정 구간(BeginWindow ~ EndWindow) 동안 매 프레임 소켓의 이전 위치 -> 현재 위치를 구로 Sweep
* - 피해 적용은 하지 않고 OnTargetHit으로 GA에 알림 (검색과 피해 처리 분리)
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemy/Data/EnemyMeleePatternSettings.h"
#include "EnemyMeleeTraceComponent.generated.h"

class UMeshComponent;
class USkeletalMeshComponent;
struct FHitResult;

// 판정에 대상이 맞음: 같은 공격 요청에서 대상마다 한 번만 방송
DECLARE_MULTICAST_DELEGATE_OneParam(FEnemyMeleeTargetHit, const FHitResult&);

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	// 판정 구간에만 Tick, 애니메이션 갱신 이후(PostPhysics)에 소켓 위치 샘플링
	UEnemyMeleeTraceComponent();

	/* 판정 소스 등록 */

	// 판정 소스 이름과 메시 연결: 공격 중 교체하면 진행 중인 판정은 중단
	UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
	void RegisterHitSource(FName SourceName, UMeshComponent* Mesh);

	UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
	void UnregisterHitSource(FName SourceName);

	UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
	UMeshComponent* GetHitSource(FName SourceName) const;

	/* 공격 실행 (근접 GA가 호출) */

	// 공격 시작 전 소켓 목록 준비: 소스 또는 소켓이 하나라도 없으면 false
	bool PrepareAttack(const TArray<TObjectPtr<UEnemyMeleeTraceSettings>>& Definitions, int64 RequestId);

	// 판정 구간 시작/종료 (NotifyState 객체로 구간 식별)
	void BeginWindow(const UObject* NotifyKey);
	void EndWindow(const UObject* NotifyKey);

	// 판정 종료: ExpectedRequestId가 0이 아니면 해당 요청일 때만
	void StopAttack(int64 ExpectedRequestId = 0);

	// 판정 구간 동안 소켓 Sweep
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	
	// 캐릭터 메시를 Body 소스로 등록
	virtual void BeginPlay() override;

	// 판정, 구독, 소스 정리
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	// 판정할 소켓 하나
	struct FTracePoint
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		FName SourceName;
		FName Socket;
		FVector PreviousPosition = FVector::ZeroVector;
		float Radius = 0.f;

		// 자동 검색한 무기: 공격 중 몸에서 떨어지면 판정 중단
		bool bRequireBodyAttachment = false;
	};

	// 판정 중 강제로 바꾼 메시 애니메이션 갱신 설정 (종료 시 복원)
	struct FPoseSetting
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		uint8 PreviousOption = 0;
	};

	// 모든 소켓의 이전 위치 -> 현재 위치 Sweep 후 대상 알림
	void SampleSockets();

	// 디버그 표시
	void DrawSweepDebug(const FTracePoint& Point, const FVector& Current) const;

private :

	// 소스 이름 -> 메시
	TMap<FName, TWeakObjectPtr<UMeshComponent>> Sources;

	// 현재 공격의 판정 소켓
	TArray<FTracePoint> Points;

	// 판정 중 변경한 메시 설정
	TArray<FPoseSetting> PoseSettings;

	// 현재 공격에서 이미 맞은 대상
	TSet<TWeakObjectPtr<AActor>> HitActors;

	// 진행 중인 판정 구간 (NotifyState)
	TWeakObjectPtr<const UObject> ActiveNotify;

	// 현재 공격 요청 번호
	int64 ActiveRequestId = 0;

	// 판정 상태 세대: 알림 콜백 중 판정이 중단/재시작되면 이전 루프를 멈추는 기준
	uint64 Generation = 0;

public:

	FEnemyMeleeTargetHit OnTargetHit;

	// 구를 Start → End로 옮기며 검사한 영역(캡슐)과 끝 위치의 구를 그림: 소켓 판정과 돌진 판정 표시에서 함께 사용
	static void DrawSphereSweep(const UWorld* World, const FVector& Start, const FVector& End, float Radius, const FColor& Color);

	// 활성 소켓 Sweep(청록) 및 RushAttack 돌진 Sweep(주황)을 표시: 판정을 조정할 때 캐릭터 BP에서 켬
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
	bool bDrawDebugTraces = false;

};
