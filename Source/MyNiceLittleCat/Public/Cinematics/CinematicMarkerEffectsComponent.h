// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (AStageEncounterDirector에서 분리)
* 시퀀스 마커 Event:<이름>[:Off], Cue:<이름>의 효과 처리 (연출 담당자, BP 연출 액터가 같이 사용)
* - Event:<이름>  → EventReceivers의 액터에 기믹 신호(IActivatableInterface) Activate, :Off면 Deactivate
* - Cue:<이름>    → CinematicCues에 등록한 소리/이펙트 재생 (시퀀스를 다시 생성해도 레벨의 연출 설정은 유지)
* - 붙여서 재생한 반복 연출은 StopAll(초기화, 처치, 종료)로 정지
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Cinematics/CinematicTypes.h"
#include "Frameworks/Feedback/FeedbackCue.h"
#include "CinematicMarkerEffectsComponent.generated.h"

class USceneComponent;

// 컷 마커 Cue:<이름>의 연출 기준 위치
UENUM(BlueprintType)
enum class EStageCueTarget : uint8
{
	Stage	UMETA(DisplayName = "Stage (스테이지 로컬 좌표)"),
	Boss	UMETA(DisplayName = "Boss (보스 메시)"),
	Player	UMETA(DisplayName = "Player (플레이어 메시)"),
	Actor	UMETA(DisplayName = "Actor (레벨 액터)")
};

// 컷 마커 Cue:<이름> 하나가 재생할 소리와 이펙트
USTRUCT(BlueprintType)
struct FStageCinematicCue
{
	GENERATED_BODY()

	// 재생할 소리와 이펙트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FFeedbackCue Feedback;

	// 기준 위치: 스테이지 좌표, 보스, 플레이어, 지정 액터
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	EStageCueTarget Target = EStageCueTarget::Stage;

	// Target이 Actor일 때 기준 액터 (계단 막이, 대문 등 레벨에 배치된 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (EditCondition = "Target == EStageCueTarget::Actor", EditConditionHides))
	TObjectPtr<AActor> TargetActor;

	// Target이 Boss/Player일 때 붙일 메시 소켓 (None이면 메시 원점 = 발밑)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FName Socket;

	// Stage: 스테이지 로컬 좌표 / 그 외: 기준 위치에서 기준의 로컬 방향으로 더할 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	FVector Offset = FVector::ZeroVector;

	// 기준을 따라 움직이도록 붙여서 재생 (Stage 기준은 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	bool bAttach = false;

	// 붙여서 재생한 반복 연출을 끌 시간 (0이면 이펙트가 스스로 끝날 때까지, 초기화/처치/종료 때는 항상 정지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bAttach"))
	float StopAfterSeconds = 0.f;

	// 스킵으로 한꺼번에 전달될 때도 재생할지 (기본: 지나간 연출은 재생하지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cue")
	bool bPlayWhenSkipped = false;
};

// Cue 기준 위치를 정할 때 쓰는 현재 상황 (소유 액터가 채움)
struct FCinematicCueAnchors
{
	// Stage 기준 좌표계 (레벨 인스턴스 변환)
	FTransform StageTransform = FTransform::Identity;

	// Boss/Player 기준 메시 (없으면 nullptr)
	USceneComponent* BossMesh = nullptr;
	USceneComponent* PlayerMesh = nullptr;
};

UCLASS(ClassGroup = (Cinematic), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UCinematicMarkerEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	UCinematicMarkerEffectsComponent();

	// Event·Cue 마커면 처리하고 true (등록되지 않은 이름이어도 이 컴포넌트의 마커면 true)
	bool HandleMarker(const FCinematicMarker& Marker, const FCinematicCueAnchors& Anchors);

	// 붙여서 재생 중인 컷 연출 정지 (초기화, 처치, 종료)
	void StopAll();

	// BP 연출용: Stage 기준은 소유 액터의 레벨 변환, Boss는 없음, Player는 플레이어 0의 메시
	UFUNCTION(BlueprintCallable, Category = "Cinematic|Markers")
	bool HandleMarkerForOwner(const FCinematicMarker& Marker);

protected :

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	void HandleEventMarker(const FCinematicMarkerArgs& Args);
	void HandleCueMarker(const FCinematicMarkerArgs& Args, bool bReachedByPlayback, const FCinematicCueAnchors& Anchors);

public :

	// 마커 Event:<이름>을 받을 액터: IActivatableInterface 구현 액터 (기믹, 또는 인터페이스를 추가한 BP)
	// - 스킵하면 종료 시점에 한꺼번에 전달되므로 받는 쪽은 여러 번 와도 안전해야 함 (기믹은 같은 상태면 무시)
	UPROPERTY(EditAnywhere, Category = "Cinematic|Markers")
	TMap<FName, TObjectPtr<AActor>> EventReceivers;

	// 마커 Cue:<이름>이 재생할 소리와 이펙트 (키: 이름)
	// - 시퀀스에는 마커만 두고 에셋은 여기서 지정: 시퀀스를 다시 생성해도 연출 설정 유지
	// - 등록하지 않은 이름의 마커는 무시 (자리만 만들어 둔 마커)
	UPROPERTY(EditAnywhere, Category = "Cinematic|Markers")
	TMap<FName, FStageCinematicCue> CinematicCues;

private :

	// 붙여서 재생 중인 컷 연출
	TArray<FFeedbackInstance> ActiveCueInstances;
};
