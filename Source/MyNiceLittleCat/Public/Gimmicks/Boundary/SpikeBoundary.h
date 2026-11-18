// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.18.
* 스플라인을 따라 가시를 깔고, 같은 선에 보이지 않는 벽을 세워 플레이어 이동 영역을 제한하는 액터
* - 레벨에서 스플라인 점을 옮겨 경계 모양을 그림 (닫힌 루프): 어떤 아레나 모양에도 맞출 수 있음
* - 가시: 메시 후보 목록에서 가시마다 무작위로 골라 인스턴스로 배치 (콜리전 없음), 간격·줄 수·크기·회전 무작위 조절
* - 벽: 스플라인을 일정 길이로 나눈 큐브 인스턴스, 게임에서는 숨김, Pawn 채널만 막음 (투사체·레이저·카메라는 통과)
* - 메시는 BP 자식(BP_SpikeBoundary)에서 지정: 코드에 에셋 경로를 두지 않음
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpikeBoundary.generated.h"

class USplineComponent;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;

UCLASS(Blueprintable)
class MYNICELITTLECAT_API ASpikeBoundary : public AActor
{
	GENERATED_BODY()

public :

	ASpikeBoundary();

	// 에디터에서 값이나 스플라인을 바꿀 때마다 가시와 벽을 다시 만듦
	virtual void OnConstruction(const FTransform& Transform) override;

	// 가시와 벽 다시 생성
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Spike Boundary")
	void Rebuild();

protected :

	/* 컴포넌트 */

	// 경계선: 점을 옮겨 모양을 그림 (닫힌 루프)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spike Boundary")
	TObjectPtr<USplineComponent> BoundarySpline;

	// 경계선에 세우는 보이지 않는 벽 (Pawn만 막음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Spike Boundary")
	TObjectPtr<UInstancedStaticMeshComponent> Walls;

	/* 가시 */

	// 가시 메시 후보: 가시마다 하나를 무작위로 고름 (메시마다 인스턴스 컴포넌트 하나)
	// - 피벗이 어디든 메시 바닥이 경계선 높이에 오도록 배치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	TArray<TObjectPtr<UStaticMesh>> SpikeMeshes;

	// 가시 머티리얼 (비우면 메시 기본 머티리얼)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	TObjectPtr<UMaterialInterface> SpikeMaterial;

	// 경계선을 따라 가시 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes", meta = (ClampMin = "10.0", Units = "cm"))
	float SpikeSpacing = 80.f;

	// 가시 크기: 메시 비율을 유지하려면 세 축을 같게 둠
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	FVector SpikeScale = FVector(1.f, 1.f, 1.f);

	// 크기 무작위 비율 (0.2면 ±20%)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes", meta = (ClampMin = "0.0", ClampMax = "0.9"))
	float ScaleJitter = 0.2f;

	// 가시마다 회전 무작위 (끄면 경계선 방향으로 정렬)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	bool bRandomYaw = true;

	// 줄과 직각 방향 위치 무작위 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes", meta = (ClampMin = "0.0", Units = "cm"))
	float PositionJitter = 15.f;

	// 가시 줄 수: 2 이상이면 경계선 바깥쪽으로 여러 줄
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes", meta = (ClampMin = "1", ClampMax = "10"))
	int32 SpikeRows = 2;

	// 줄 사이 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes", meta = (ClampMin = "0.0", Units = "cm"))
	float RowSpacing = 60.f;

	// 줄을 쌓는 방향 반전: 바깥 줄이 영역 안쪽으로 생기면 켬 (스플라인 그린 방향에 따라 다름)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	bool bFlipRowSide = false;

	// 무작위 시드: 같은 값이면 매번 같은 배치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Spikes")
	int32 RandomSeed = 1234;

	/* 벽 */

	// 보이지 않는 벽 사용 (끄면 가시만 깔림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls")
	bool bBlockPawns = true;

	// 벽 메시 (기본 큐브처럼 100cm 크기 메시를 기준으로 늘림)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls")
	TObjectPtr<UStaticMesh> WallMesh;

	// 벽 높이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls", meta = (ClampMin = "10.0", Units = "cm"))
	float WallHeight = 600.f;

	// 벽 두께
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls", meta = (ClampMin = "1.0", Units = "cm"))
	float WallThickness = 40.f;

	// 곡선 구간을 나눌 벽 조각 길이 (짧을수록 곡선에 잘 맞음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls", meta = (ClampMin = "20.0", Units = "cm"))
	float WallSegmentLength = 200.f;

	// 게임에서도 벽을 보이게 함 (위치 확인용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spike Boundary|Walls")
	bool bShowWallsInGame = false;

private :

	void BuildSpikes();
	void BuildWalls();

	// 가시 메시마다 만든 인스턴스 컴포넌트 (생성 스크립트 컴포넌트라 다시 만들 때 엔진이 정리)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> SpikeComponents;
};
