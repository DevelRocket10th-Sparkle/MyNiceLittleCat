#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "OcclusionGroupSubsystem.generated.h"

class UPrimitiveComponent;

// 조각들을 같은 벽 면 / 천장으로 묶는 판정 기준
USTRUCT(BlueprintType)
struct FOcclusionGroupSettings
{
	GENERATED_BODY()

	// 가림 판정 전용 Trace 채널: Project Settings에서 추가한 CameraOcclusion 채널 선택
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_GameTraceChannel3;
	// 같은 평면으로 간주하는 법선 방향 거리 허용치 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.0"))
	float PlaneTolerance = 20.f;
	// 이웃 조각으로 간주하는 조각 사이 틈 허용치 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.0"))
	float AdjacencyTolerance = 15.f;
	// 처음 맞은 조각 기준 그룹 최대 반경: 일직선으로 이어진 옆방 벽까지 번지는 것 방지 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "100.0"))
	float MaxGroupRadius = 3000.f;
	// 가장 얇은 축 / 두 번째로 얇은 축 비율이 이 값 이하일 때만 판(벽, 천장)으로 간주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Occlusion", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float PlanarRatio = 0.5f;
};

// 카메라를 가린 조각에서 시작해 같은 면에 붙어 있는 조각들을 자동으로 찾아 묶는 월드 단위 캐시
// 방마다 수동 등록 없이, 조각의 방향 / 평면 위치 / 인접 여부만으로 그룹 판정
UCLASS()
class MYNICELITTLECAT_API UOcclusionGroupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 그룹 묶기에서 제외할 조각의 컴포넌트 태그: 붙어 있어도 자기 자신만 숨김
	static const FName NoGroupTag;

	// 맞은 조각이 속한 면 전체를 OutMembers에 추가. 판이 아니거나 제외 태그가 있으면 자기 자신만 추가
	void CollectGroupMembers(UPrimitiveComponent* HitPiece, const FOcclusionGroupSettings& Settings, TSet<UPrimitiveComponent*>& OutMembers);
	// 바닥 / 천장처럼 위아래를 향한 판 조각인지 판정: 층 높이 조건 적용 대상 구분용
	bool IsHorizontalPiece(const UPrimitiveComponent* Piece, const FOcclusionGroupSettings& Settings) const;
	// 레벨 구성이 바뀌었을 때 (스트리밍 등) 그룹 캐시 초기화
	UFUNCTION(BlueprintCallable, Category = "Occlusion")
	void ClearGroupCache();

protected:
	// 게임 / PIE 월드에서만 생성
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	// 조각 하나의 평면 정보
	struct FPieceShape
	{
		// 가장 얇은 축의 월드 방향 = 벽 / 천장의 법선
		FVector Normal = FVector::UpVector;
		// 월드 바운드 중심
		FVector Center = FVector::ZeroVector;
		// 얇은 판 형태 여부: 기둥, 모서리 조각 등 덩어리는 false
		bool bIsPlanar = false;
	};

	// 같은 면으로 묶인 조각 목록
	struct FOcclusionGroup
	{
		TArray<TWeakObjectPtr<UPrimitiveComponent>> Members;
	};

	// 로컬 바운드와 스케일로 조각의 법선 및 판 여부 계산
	FPieceShape CalculatePieceShape(const UPrimitiveComponent* Piece, const FOcclusionGroupSettings& Settings) const;
	// 두 조각이 같은 방향 및 같은 평면 위에 있는지 판정
	bool IsSamePlane(const FPieceShape& A, const FPieceShape& B, const FOcclusionGroupSettings& Settings) const;
	// 조각 바운드를 허용치만큼 넓혀 겹치는 다른 조각 검색
	void FindTouchingPieces(UPrimitiveComponent* Piece, const FOcclusionGroupSettings& Settings, TArray<UPrimitiveComponent*>& OutPieces) const;
	// 시작 조각에서 같은 평면의 이웃을 따라가며 (BFS) 새 그룹 생성 후 그룹 인덱스 반환
	int32 BuildGroup(UPrimitiveComponent* SeedPiece, const FPieceShape& SeedShape, const FOcclusionGroupSettings& Settings);
	// 그룹 멤버 중 유효한 조각만 OutMembers에 추가
	void AppendGroupMembers(int32 GroupIndex, TSet<UPrimitiveComponent*>& OutMembers) const;

	// 지금까지 만든 그룹 목록
	TArray<FOcclusionGroup> Groups;
	// 판 조각별 소속 그룹 인덱스: 같은 면의 조각이 다시 맞으면 재계산 없이 사용
	TMap<TWeakObjectPtr<UPrimitiveComponent>, int32> GroupIndexByPiece;
};
