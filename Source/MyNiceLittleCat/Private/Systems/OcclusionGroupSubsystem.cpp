#include "Systems/OcclusionGroupSubsystem.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

const FName UOcclusionGroupSubsystem::NoGroupTag(TEXT("NoOcclusionGroup"));

namespace OcclusionGroup
{
	// 같은 방향으로 간주하는 법선 내적 최소값 (약 11도 이내)
	constexpr float SameDirectionDot = 0.98f;
}

bool UOcclusionGroupSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UOcclusionGroupSubsystem::CollectGroupMembers(UPrimitiveComponent* HitPiece, const FOcclusionGroupSettings& Settings, TSet<UPrimitiveComponent*>& OutMembers)
{
	if (!IsValid(HitPiece))
	{
		return;
	}
	// 제외 태그가 있는 조각은 묶지 않고 자기 자신만 숨김
	if (HitPiece->ComponentHasTag(NoGroupTag))
	{
		OutMembers.Add(HitPiece);
		return;
	}
	// 이미 계산한 면이면 캐시 사용
	if (const int32* CachedIndex = GroupIndexByPiece.Find(HitPiece))
	{
		AppendGroupMembers(*CachedIndex, OutMembers);
		return;
	}
	const FPieceShape HitShape = CalculatePieceShape(HitPiece, Settings);
	// 기둥, 모서리 등 덩어리 조각은 면을 정의할 수 없으므로 자기 자신만 숨김
	if (!HitShape.bIsPlanar)
	{
		OutMembers.Add(HitPiece);
		return;
	}
	const int32 NewIndex = BuildGroup(HitPiece, HitShape, Settings);
	AppendGroupMembers(NewIndex, OutMembers);
}

void UOcclusionGroupSubsystem::ClearGroupCache()
{
	Groups.Reset();
	GroupIndexByPiece.Reset();
}

UOcclusionGroupSubsystem::FPieceShape UOcclusionGroupSubsystem::CalculatePieceShape(const UPrimitiveComponent* Piece, const FOcclusionGroupSettings& Settings) const
{
	FPieceShape Shape;
	Shape.Center = Piece->Bounds.Origin;

	// 회전을 뺀 로컬 크기: 피벗 위치나 배치 회전이 제각각이어도 얇은 축을 찾을 수 있음
	const FVector LocalExtent = Piece->CalcBounds(FTransform::Identity).BoxExtent * Piece->GetComponentScale().GetAbs();

	// 가장 얇은 축 검색 (0 = X, 1 = Y, 2 = Z)
	int32 ThinnestAxis = 0;
	for (int32 Axis = 1; Axis < 3; ++Axis)
	{
		if (LocalExtent[Axis] < LocalExtent[ThinnestAxis])
		{
			ThinnestAxis = Axis;
		}
	}
	// 나머지 두 축 중 짧은 쪽 = 두 번째로 얇은 축
	float SecondThinnest = TNumericLimits<float>::Max();
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Axis != ThinnestAxis)
		{
			SecondThinnest = FMath::Min(SecondThinnest, LocalExtent[Axis]);
		}
	}
	if (SecondThinnest <= KINDA_SMALL_NUMBER)
	{
		return Shape;
	}

	// 얇은 축이 충분히 얇아야 판으로 간주
	Shape.bIsPlanar = LocalExtent[ThinnestAxis] / SecondThinnest <= Settings.PlanarRatio;
	// EAxis는 X = 1부터 시작하므로 +1
	Shape.Normal = Piece->GetComponentTransform().GetUnitAxis(static_cast<EAxis::Type>(ThinnestAxis + 1));
	return Shape;
}

bool UOcclusionGroupSubsystem::IsSamePlane(const FPieceShape& A, const FPieceShape& B, const FOcclusionGroupSettings& Settings) const
{
	// 180도 돌려 배치한 조각도 같은 면이므로 내적의 절대값으로 비교
	const float DirectionDot = FMath::Abs(FVector::DotProduct(A.Normal, B.Normal));
	if (DirectionDot < OcclusionGroup::SameDirectionDot)
	{
		return false;
	}
	// 법선 방향으로 떨어진 거리: 마주 보는 반대편 벽처럼 방향만 같은 조각 제외
	const float PlaneDistance = FMath::Abs(FVector::DotProduct(B.Center - A.Center, A.Normal));
	return PlaneDistance <= Settings.PlaneTolerance;
}

void UOcclusionGroupSubsystem::FindTouchingPieces(UPrimitiveComponent* Piece, const FOcclusionGroupSettings& Settings, TArray<UPrimitiveComponent*>& OutPieces) const
{
	OutPieces.Reset();
	const FBoxSphereBounds& Bounds = Piece->Bounds;
	const FCollisionShape SearchBox = FCollisionShape::MakeBox(Bounds.BoxExtent + FVector(Settings.AdjacencyTolerance));
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OcclusionGroupOverlap), false);

	// 가림 채널에 반응하는 조각만 검색되므로 바닥, 소품, 캐릭터는 자동 제외
	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByChannel(Overlaps, Bounds.Origin, FQuat::Identity, Settings.TraceChannel, SearchBox, Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		UPrimitiveComponent* Other = Overlap.GetComponent();
		if (IsValid(Other) && Other != Piece)
		{
			OutPieces.AddUnique(Other);
		}
	}
}

int32 UOcclusionGroupSubsystem::BuildGroup(UPrimitiveComponent* SeedPiece, const FPieceShape& SeedShape, const FOcclusionGroupSettings& Settings)
{
	FOcclusionGroup NewGroup;
	// 같은 평면이라 이웃 탐색을 이어갈 조각 목록 (BFS 대기열 겸 결과)
	TArray<UPrimitiveComponent*> PlanarPieces;
	// 한 번 검사한 조각: 중복 검사 방지
	TSet<UPrimitiveComponent*> Visited;
	TArray<UPrimitiveComponent*> TouchingPieces;

	PlanarPieces.Add(SeedPiece);
	Visited.Add(SeedPiece);

	// 대기열을 앞에서부터 처리하면서 같은 평면 이웃을 뒤에 추가
	for (int32 QueueIndex = 0; QueueIndex < PlanarPieces.Num(); ++QueueIndex)
	{
		UPrimitiveComponent* Current = PlanarPieces[QueueIndex];
		NewGroup.Members.Add(Current);

		FindTouchingPieces(Current, Settings, TouchingPieces);
		for (UPrimitiveComponent* Candidate : TouchingPieces)
		{
			if (Visited.Contains(Candidate))
			{
				continue;
			}
			Visited.Add(Candidate);

			// 제외 태그가 있는 조각은 그룹에 포함하지 않음
			if (Candidate->ComponentHasTag(NoGroupTag))
			{
				continue;
			}
			const FPieceShape CandidateShape = CalculatePieceShape(Candidate, Settings);
			// 모서리, 기둥 등 덩어리 조각: 이 면과 같이 숨기되 이웃 탐색은 이어가지 않음 (다른 면으로 번지는 것 방지)
			if (!CandidateShape.bIsPlanar)
			{
				NewGroup.Members.Add(Candidate);
				continue;
			}
			// 직각으로 만나는 벽, 천장 등 다른 면 제외
			if (!IsSamePlane(SeedShape, CandidateShape, Settings))
			{
				continue;
			}
			// 너무 멀리 이어진 조각 제외: 옆방까지 번지는 것 방지
			if (FVector::Dist(SeedShape.Center, CandidateShape.Center) > Settings.MaxGroupRadius)
			{
				continue;
			}
			PlanarPieces.Add(Candidate);
		}
	}

	const int32 NewIndex = Groups.Add(MoveTemp(NewGroup));
	// 판 조각만 캐시에 등록. 덩어리 조각은 여러 면에 같이 속할 수 있으므로 등록하지 않음
	// 이전에 다른 그룹으로 등록된 조각이면 새 그룹으로 덮어씀 (이전 그룹은 더 이상 조회되지 않음)
	for (UPrimitiveComponent* Piece : PlanarPieces)
	{
		GroupIndexByPiece.Add(Piece, NewIndex);
	}
	return NewIndex;
}

void UOcclusionGroupSubsystem::AppendGroupMembers(int32 GroupIndex, TSet<UPrimitiveComponent*>& OutMembers) const
{
	if (!Groups.IsValidIndex(GroupIndex))
	{
		return;
	}
	for (const TWeakObjectPtr<UPrimitiveComponent>& Member : Groups[GroupIndex].Members)
	{
		// 스트리밍 해제 등으로 사라진 조각 제외
		if (Member.IsValid())
		{
			OutMembers.Add(Member.Get());
		}
	}
}
