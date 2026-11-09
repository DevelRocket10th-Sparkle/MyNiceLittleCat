#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BossEncounterTrigger.generated.h"

class ABossEncounterManager;
class UBoxComponent;
class UPrimitiveComponent;

// 플레이어가 영역에 들어오면 지정한 보스방 매니저에 전투 시작 요청
// - 매니저를 인스턴스에서 직접 지정 (레벨에 매니저가 여러 개 있어도 대상이 고정됨)
// - 시작 가능 여부(상태, 연출 진행 단계)는 매니저가 판단하므로 트리거는 상태를 갖지 않음
UCLASS()
class MYNICELITTLECAT_API ABossEncounterTrigger : public AActor
{
	GENERATED_BODY()

public:
	// 폰만 감지하는 박스 생성 및 Tick 비활성화
	ABossEncounterTrigger();

protected:
	// 매니저 지정 여부 확인 및 겹침 이벤트 구독
	virtual void BeginPlay() override;
	// 겹침 이벤트 구독 해제 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 살아 있는 플레이어일 때만 매니저에 시작 요청
	void TryRequestStart(AActor* OtherActor);
	// 직접 지정한 매니저 우선, 비어 있으면 같은 레벨(레벨 인스턴스)에 하나뿐인 매니저 사용
	ABossEncounterManager* ResolveEncounterManager() const;

protected:
	// 전투 시작 영역
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 시작 요청을 보낼 보스방 매니저: 비우면 같은 레벨의 매니저를 자동 사용
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Boss")
	TObjectPtr<ABossEncounterManager> EncounterManager;
};
