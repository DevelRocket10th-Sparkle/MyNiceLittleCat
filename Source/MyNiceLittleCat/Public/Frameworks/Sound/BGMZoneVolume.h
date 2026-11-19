// 작성자: 우진
// 작성일자: 2026-11-18

/// 맵 내부 구역별 BGM 볼륨이에요
/// 플레이어가 볼륨 안으로 들어오면 ZoneSound를, 나가면 이전 곡(레벨 기본곡 등)으로 복귀해요
/// 겹치는 Zone은 Priority가 높은 쪽이 우선합니다. (예: 마을 0, 마을 안 상점 10)
/// 의도: 한 맵 안에서도 구역마다 다른 곡을 배치만으로 지정하고 싶었어요

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BGMZoneVolume.generated.h"

class APawn;
class APlayerController;

UCLASS()
class MYNICELITTLECAT_API ABGMZoneVolume : public AActor
{
	GENERATED_BODY()
	
public:	
	ABGMZoneVolume();

protected:
	virtual void BeginPlay() override;

	// (스트리밍/레벨 인스턴스 언로드 시 EndOverlap이 안 올 수 있어서 여기서 확실히 정리)
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

// ========================================================================
protected:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 플레이어가 조종하는 Pawn인지 (펫·적 등 AI Pawn은 제외)
	bool IsPlayerPawn(const AActor* Actor) const;

	// 시작 시 플레이어가 이미 박스 안에 있는지 위치로 직접 확인
	// (이어하기/체크포인트 스폰은 BeginPlay 전에 일어나서 오버랩 이벤트를 놓칠 수 있음)
	void CheckInitialPlayerInside();

	// 월드 위치가 박스 내부인지 (회전·스케일 반영)
	bool IsLocationInsideBox(const FVector& WorldLocation) const;

	// 플레이어 컨트롤러의 빙의 변경 이벤트 구독 (중복 구독 방지)
	void BindPossessionChanged();

	// 빙의가 바뀔 때마다(체크포인트 이동/사망 후 리스폰) 새 폰 위치로 존 안/밖 재판정
	// 의도: 같은 레벨 리스폰은 "폰 파괴 → 새 폰 스폰 → 빙의" 순서라
	//       파괴 때 EndOverlap, 스폰 때 BeginOverlap 모두 IsPlayerControlled()가 false여서 놓침
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

	// 실제 진입/이탈 처리 (서브시스템 호출 + 상태 갱신)
	void EnterWithPawn(APawn* Pawn);
	void ExitNow();

// ========================================================================
protected:
	UPROPERTY(VisibleAnywhere, Category = "BGM")
	TObjectPtr<class UBoxComponent> Box;

	// 이 구역에서 틀 곡
	UPROPERTY(EditAnywhere, Category = "BGM")
	TObjectPtr<USoundBase> ZoneSound;

	// 겹칠 때 우선순위 (높을수록 우선)
	UPROPERTY(EditAnywhere, Category = "BGM")
	int32 Priority = 0;

	// 존 진입/이탈 시 크로스페이드 시간 (음수면 Project Settings의 DefaultFadeTime 사용)
	UPROPERTY(EditAnywhere, Category = "BGM")
	float FadeTime = -1.f;

private:
	// 지금 플레이어가 안에 있는지 (EndPlay 정리 판단용)
	bool bPlayerInside = false;

	// 들어올 때의 플레이어 폰: 나갈 때 컨트롤러가 이미 떨어져 있어도(파괴 직전) 같은 폰인지로 이탈 판정
	TWeakObjectPtr<APawn> InsidePawn;

	// 빙의 변경 이벤트를 구독한 컨트롤러 (EndPlay에서 해제)
	TWeakObjectPtr<APlayerController> BoundController;
};
