// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 시퀀스에서 움직이는 대상의 경로를 따라 데칼을 배치하는 컴포넌트 (핏자국, 발자국 등)
* - 경로는 시퀀스 바인딩의 트랜스폼 키에서 직접 계산 → 시퀀서에서 경로를 고치면 데칼도 따라감
* - 대상이 각 지점을 지나는 시각에 맞춰 페이드인, 스킵하면 RevealAll()로 즉시 표시
* - 경로 계산은 PrecomputeTrail()로 로딩 시점에 미리 해 둘 수 있음
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UObject/ObjectKey.h"
#include "CinematicTrailDecalComponent.generated.h"

class ULevelSequence;
class UMaterialInterface;
class UDecalComponent;

UCLASS(ClassGroup = (Cinematic), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UCinematicTrailDecalComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	// Tick 없이 호출 시점에만 동작
	UCinematicTrailDecalComponent();

	// 시퀀스의 경로를 미리 계산해 캐시: 재생 시점에 채널 샘플링 비용이 들지 않게 함
	void PrecomputeTrail(ULevelSequence* Sequence);

	// 경로를 따라 데칼 배치 후 배치한 개수 반환
	// - SequenceOrigin: 시퀀스 키 좌표계 → 월드 좌표계 변환 (레벨 인스턴스 오프셋)
	// - ExtraLocalPoints: 경로 외에 추가로 찍을 지점 (시퀀스 로컬 좌표), 가장 가까운 경로 지점의 시각에 나타남
	// - IgnoredActors: 바닥 검사에서 무시할 액터 (보스, 플레이어 캡슐)
	int32 SpawnTrail(ULevelSequence* Sequence, const FTransform& SequenceOrigin,
		const TArray<FVector>& ExtraLocalPoints, const TArray<const AActor*>& IgnoredActors);

	// 페이드인 대기 중인 데칼을 즉시 표시 (스킵 시)
	void RevealAll();

	// 배치한 데칼 전부 제거 (전투 초기화)
	void ClearTrail();

protected :

	// 남은 데칼 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	// 경로 위 한 점: 시퀀스 로컬 좌표(캡슐 중심)와 대상이 그 점을 지나는 시각
	struct FTrailPoint
	{
		FVector Local = FVector::ZeroVector;
		double Seconds = 0.0;
	};

	// 캐시에서 경로를 찾고 없으면 계산
	const TArray<FTrailPoint>& FindOrBuildTrail(ULevelSequence* Sequence);

	// 바인딩 태그가 붙은 대상의 트랜스폼 키를 시작/끝 마커 구간에서 일정 간격으로 샘플링
	TArray<FTrailPoint> SampleTrail(ULevelSequence* Sequence) const;

protected :

	/* 데칼 */

	// 배치할 데칼 재질
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	// 데칼 투영 깊이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail", meta = (ClampMin = "1.0", Units = "cm"))
	float DecalDepth = 30.f;

	// 데칼 크기 범위: 지점마다 불규칙하게 선택
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail", meta = (ClampMin = "1.0", Units = "cm"))
	float MinDecalSize = 22.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail", meta = (ClampMin = "1.0", Units = "cm"))
	float MaxDecalSize = 38.f;

	// 대상이 지나간 뒤 나타나는 데 걸리는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail", meta = (ClampMin = "0.0", Units = "s"))
	float FadeInSeconds = 0.15f;

	/* 경로 */

	// 경로를 따라갈 시퀀스 바인딩의 태그
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Source")
	FName BindingTag = TEXT("Boss");

	// 데칼 구간 시작/끝 마커 라벨: 없으면 시퀀스 처음/끝
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Source")
	FString StartMarkerLabel = TEXT("Blood:Start");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Source")
	FString EndMarkerLabel = TEXT("Blood:End");

	// 경로 길이 기준 데칼 간격: 실제 간격은 0.7~1.3배로 불규칙
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Placement", meta = (ClampMin = "10.0", Units = "cm"))
	float Spacing = 650.f;

	// 경로 좌우로 흩뿌리는 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Placement", meta = (ClampMin = "0.0", Units = "cm"))
	float SideJitter = 45.f;

	// 추가 지점은 바닥 좌표로 지정하므로 캡슐 중심 높이만큼 올려서 바닥 검사
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Placement", meta = (Units = "cm"))
	float ExtraPointHeight = 160.f;

	// 캡슐 중심에서 아래로 바닥을 찾는 거리: 공중 구간은 바닥이 멀어 자동 제외
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Placement", meta = (ClampMin = "1.0", Units = "cm"))
	float FloorTraceDistance = 420.f;

	// 이보다 기울어진 면(벽, 계단 옆면)에는 배치하지 않음: 바닥 법선의 Z 최소값
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trail|Placement", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinFloorNormalZ = 0.65f;

private :

	// 시퀀스별 계산한 경로 캐시: 시퀀스는 다른 곳에서 참조하므로 키는 약한 참조
	TMap<TObjectKey<ULevelSequence>, TArray<FTrailPoint>> CachedTrails;

	// 배치한 데칼
	TArray<TWeakObjectPtr<UDecalComponent>> SpawnedDecals;
};
