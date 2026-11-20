// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.18.
* 수정 일자: 2026.11.20. (연출 종류를 큐의 ProceduralVisual로 받음, 메시·머티리얼을 프로젝트 설정에서 지정, 움직임별 함수 분리)
* 나이아가라 대신 쓰는 절차적 피드백 연출 (발광 조각 메시를 인스턴스로 그리는 짧은 연출)
* - EnemyFeedback::PlayAtLocation/PlayAttached가 큐의 ProceduralVisual이 None이 아닐 때 생성
* - 메시와 머티리얼은 UFeedbackVisualSettings(프로젝트 설정 → Game → Feedback Visual)에서 지정
* - 연출 종류마다 움직임(Ring, Fracture, Orbit 등)과 색·크기·시간이 정해져 있고, 수명이 끝나면 스스로 사라짐
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "FeedbackVisual.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UPointLightComponent;

UCLASS(NotBlueprintable, Transient)
class AFeedbackVisual : public AActor
{
	GENERATED_BODY()

public :

	AFeedbackVisual();

	// 연출 생성: None이거나 데디케이티드 서버면 생성하지 않고 nullptr (호출 쪽은 나이아가라로 대체)
	// - Anchor를 주면 그 컴포넌트(소켓)에 붙고, 붙은 액터가 사라지면 함께 사라짐
	static AFeedbackVisual* Play(const UObject* Context, EFeedbackVisualStyle Style, const FTransform& Transform,
		USceneComponent* Anchor = nullptr, FName Socket = NAME_None);

	virtual void Tick(float DeltaSeconds) override;

private :

	// 조각 하나의 이번 프레임 모양: 바깥 조각(색)과 안쪽 강조 조각(밝은 색)
	struct FShardPose
	{
		FVector Position = FVector::ZeroVector;
		FVector Heading = FVector::ForwardVector;
		FVector AccentPosition = FVector::ZeroVector;
		float Length = 0.f;
		float Width = 0.f;
		float AccentSize = 0.f;
	};

	// 조각 계산에 공통으로 쓰는 값 (조각 번호와 연출 진행도)
	struct FShardInput
	{
		int32 Index = 0;
		int32 Count = 1;
		float Fraction = 0.f;
		float Angle = 0.f;
		float Variation = 1.f;
		FVector Radial = FVector::ForwardVector;
		FVector Tangent = FVector::RightVector;
		float Progress = 0.f;
		float Travel = 0.f;
		float Tail = 1.f;
	};

	/* 초기화와 갱신 */

	// 연출 종류의 디자인 값 적용, 메시·머티리얼 설정, 조각 인스턴스 생성, 수명 지정
	void Initialize(EFeedbackVisualStyle InStyle);

	// 진행도에 맞춰 섬광 세기와 모든 조각의 변환 계산
	void UpdateVisual();

	/* 움직임별 조각 모양 */

	// 바닥을 따라 퍼지는 고리 (착지, 돌진 시작, 비틀거림)
	FShardPose PoseRing(const FShardInput& In) const;

	// 위로 튀며 회전하는 파편 (파괴, 사망)
	FShardPose PoseFracture(const FShardInput& In) const;

	// 주위를 도는 조각 (도착, 경고, 공중 부양)
	FShardPose PoseOrbit(const FShardInput& In) const;

	// 뒤로 흐르는 꼬리 (돌진 궤적, 금화 궤적)
	FShardPose PoseTrail(const FShardInput& In) const;

	// 안쪽으로 모여드는 조각 (금화 모임, 잠금 해제)
	FShardPose PoseGather(const FShardInput& In) const;

	// 위로 떠오르며 흩어지는 조각 (시체 사라짐)
	FShardPose PoseRise(const FShardInput& In) const;

	// 앞쪽 부채꼴로 터지는 조각 (타격, 금화 섬광·충돌, 지팡이)
	FShardPose PoseFan(const FShardInput& In) const;

	/* 컴포넌트 */

	// 바깥 조각 (연출 색)
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Outer;

	// 안쪽 강조 조각 (밝은 색)
	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Core;

	// 시작 순간 짧은 섬광 (착지, 파괴 등 일부 연출만)
	UPROPERTY()
	TObjectPtr<UPointLightComponent> Flash;

	/* 상태 */

	EFeedbackVisualStyle Style = EFeedbackVisualStyle::None;
	TArray<FTransform> OuterTransforms;
	TArray<FTransform> CoreTransforms;
	float Seconds = 0.f;
	float Duration = 0.f;
	float Radius = 0.f;
	float Lift = 0.f;
	float FlashIntensity = 0.f;
	uint8 Motion = 0;
};
