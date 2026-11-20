#include "Systems/RoomCutawayCache.h"

#include "Characters/Camera/RoomMaskVolume.h"
#include "Components/LocalLightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/PlatformTime.h"

namespace
{
	// 옆면 바깥 방향 (FRoomCutawayCache 옆면 인덱스 순서)
	const FVector SideNormals[FRoomCutawayCache::SideCount] =
	{
		FVector(-1.f, 0.f, 0.f), FVector(1.f, 0.f, 0.f), FVector(0.f, -1.f, 0.f), FVector(0.f, 1.f, 0.f)
	};

	// 부착 관계의 가장 위 액터: 캐릭터 / 펫에 붙은 소품, 조명도 Pawn으로 판정하기 위함
	const AActor* FindAttachRoot(const AActor* Actor)
	{
		const AActor* Root = Actor;
		while (const AActor* Parent = Root->GetAttachParentActor())
		{
			Root = Parent;
		}
		return Root;
	}

	// 방 컷어웨이 후보: 화면에 보이는 메시만, Pawn과 거기 붙은 액터, 방 볼륨, 방에서 제외 지정한 액터는 제외
	bool IsCutawayCandidate(const UPrimitiveComponent* Piece, const AActor* AttachRoot, const ARoomMaskVolume& Room)
	{
		if (!Piece->IsA<UMeshComponent>() || !Piece->IsRegistered() || !Piece->IsVisible() || Piece->bHiddenInGame)
		{
			return false;
		}
		return !AttachRoot->IsA<APawn>() && !AttachRoot->IsA<ARoomMaskVolume>() && !Room.IsCutawayIgnored(AttachRoot);
	}

	// 그림자 컬링 후보 조명: 보이는 상태로 그림자를 그리는 점 / 스포트 조명만. Pawn에 붙은 조명과 방에서 제외 지정한 액터는 제외
	bool IsShadowCullCandidate(const ULocalLightComponent* Light, const AActor* AttachRoot, const ARoomMaskVolume& Room)
	{
		if (!Light->IsRegistered() || !Light->IsVisible() || !Light->CastShadows || Light->AttenuationRadius <= 0.f)
		{
			return false;
		}
		return !AttachRoot->IsA<APawn>() && !Room.IsCutawayIgnored(AttachRoot);
	}

	// 다른 박스 안에 있는 점인지: L / T자 방에서 박스끼리 맞닿은 안쪽 면 조각 제외
	bool IsInsideAnyOtherBox(const FVector& Point, const TArray<FBox>& Boxes, int32 SkipIndex)
	{
		for (int32 i = 0; i < Boxes.Num(); ++i)
		{
			if (i != SkipIndex && Boxes[i].IsInside(Point))
			{
				return true;
			}
		}
		return false;
	}

	// 윗층 조각인지: 중심이 박스 윗면 근처 이상이고, 방 바닥 면적 (+여유) 위에 걸침
	bool IsAboveRoomBox(const FBox& PieceBox, const FBox& Box, const FRoomCutawaySettings& S)
	{
		if (PieceBox.GetCenter().Z < Box.Max.Z - S.TopTolerance)
		{
			return false;
		}
		const FBox Footprint = Box.ExpandBy(FVector(S.TopFootprintMargin, S.TopFootprintMargin, 0.f));
		return PieceBox.Min.X < Footprint.Max.X && PieceBox.Max.X > Footprint.Min.X
			&& PieceBox.Min.Y < Footprint.Max.Y && PieceBox.Max.Y > Footprint.Min.Y;
	}

	// 빛이 닿는 범위 (감쇠 반경 구)가 방 박스 (+여유)와 하나도 겹치지 않으면 그림자 컬링 대상
	void ClassifyLight(ULocalLightComponent* Light, const TArray<FBox>& Boxes, const FRoomCutawaySettings& S, FRoomCutawayCache& Cache)
	{
		const FVector LightCenter = Light->GetComponentLocation();
		const FVector::FReal RadiusSquared = FMath::Square(static_cast<FVector::FReal>(Light->AttenuationRadius));
		for (const FBox& Box : Boxes)
		{
			if (FMath::SphereAABBIntersection(LightCenter, RadiusSquared, Box.ExpandBy(S.LightShadowMargin)))
			{
				return;
			}
		}
		Cache.OutsideShadowLights.Add(Light);
	}

	// 메시 조각 하나를 윗층 / 방 밖 / 옆면으로 분류
	// - 조각마다 한 번만 불리므로 윗층, 방 밖은 중복 검사 없이 추가 (옆면만 박스가 여러 개일 때 같은 면이 겹칠 수 있음)
	void ClassifyPiece(UPrimitiveComponent* Piece, const TArray<FBox>& Boxes, const FRoomCutawaySettings& S, FRoomCutawayCache& Cache)
	{
		const FBox PieceBox = Piece->Bounds.GetBox();
		const FVector PieceCenter = PieceBox.GetCenter();

		/* 방 박스 (XY만 Inset만큼 축소)와 전혀 겹치지 않으면 방 밖 조각 */

		bool bTouchesRoom = false;
		bool bNearRoom = false;
		const FVector Inset(S.OutsideInset, S.OutsideInset, 0.f);
		for (const FBox& Box : Boxes)
		{
			if (FBox(Box.Min + Inset, Box.Max - Inset).Intersect(PieceBox))
			{
				bTouchesRoom = true;
				break;
			}
			bNearRoom |= Box.ExpandBy(S.OutsideRadius).Intersect(PieceBox);
		}

		if (!bTouchesRoom)
		{
			// 윗층 판정에 해당하면 윗층, 아니면 가까운 것만 방 밖
			const bool bIsTop = Boxes.ContainsByPredicate([&](const FBox& Box) { return IsAboveRoomBox(PieceBox, Box, S); });
			if (bIsTop)
			{
				Cache.TopPieces.Add(Piece);
			}
			else if (bNearRoom)
			{
				Cache.OutsidePieces.Add(Piece);
			}
			return;
		}

		/* 방에 걸친 조각: 박스별로 윗층 / 옆면 판정 */

		uint8 AddedSides = 0;
		for (int32 BoxIndex = 0; BoxIndex < Boxes.Num(); ++BoxIndex)
		{
			const FBox& Box = Boxes[BoxIndex];

			// 윗층: 박스 윗면 근처 이상이면 옆면 판정 없이 윗층 여부만 봄
			if (PieceCenter.Z >= Box.Max.Z - S.TopTolerance)
			{
				if (IsAboveRoomBox(PieceBox, Box, S))
				{
					Cache.TopPieces.Add(Piece);
					return;
				}
				continue;
			}
			// 방 바닥보다 아래 (기초, 지면 소품)는 제외
			if (PieceCenter.Z < Box.Min.Z)
			{
				continue;
			}
			// L / T자 방에서 박스끼리 맞닿은 안쪽 면 조각 제외
			if (IsInsideAnyOtherBox(PieceCenter, Boxes, BoxIndex))
			{
				continue;
			}

			for (int32 Side = 0; Side < FRoomCutawayCache::SideCount; ++Side)
			{
				const int32 Axis = Side / 2;          // 0 = X, 1 = Y
				const int32 LateralAxis = 1 - Axis;
				const bool bMinSide = (Side % 2) == 0;

				// 면에서 방 안쪽으로 들어온 깊이: 음수면 면 바깥 (벽 두께, 외벽 장식)
				const double Reach = bMinSide ? PieceBox.Max[Axis] - Box.Min[Axis] : Box.Max[Axis] - PieceBox.Min[Axis];
				// 면을 뚫고 벽 안 / 밖으로 나간 깊이: 벽난로, 문틀처럼 벽에 박힌 메시 판별
				const double Embed = bMinSide ? Box.Min[Axis] - PieceBox.Min[Axis] : PieceBox.Max[Axis] - Box.Max[Axis];
				const bool bAttached = Reach <= S.InnerDepth;
				const bool bEmbedded = Embed >= S.WallEmbedDepth && Reach <= S.MaxEmbeddedReach;
				if ((!bAttached && !bEmbedded) || Reach < -S.OuterDepth)
				{
					continue;
				}
				// 면의 가로 범위와 겹쳐야 함: 옆방 벽까지 번지는 것 방지
				if (PieceBox.Max[LateralAxis] <= Box.Min[LateralAxis] || PieceBox.Min[LateralAxis] >= Box.Max[LateralAxis])
				{
					continue;
				}
				// 박스가 여러 개면 같은 방향 옆면에 두 번 들어가지 않게
				const uint8 SideBit = 1 << Side;
				if (!(AddedSides & SideBit))
				{
					AddedSides |= SideBit;
					Cache.SidePieces[Side].Add(Piece);
				}
			}
		}
	}
}

FRoomCutawayCache FRoomCutawayCache::Build(const UWorld& World, const ARoomMaskVolume& Room, const FRoomCutawaySettings& Settings)
{
	const double StartSeconds = FPlatformTime::Seconds();

	FRoomCutawayCache Cache;
	TArray<FBox> Boxes;
	Room.GetRoomBounds(Boxes);

	TInlineComponentArray<ULocalLightComponent*> Lights;
	TInlineComponentArray<UPrimitiveComponent*> Pieces;

	// 방을 처음 들어갈 때 한 번만 월드 메시를 훑어 면별로 분류
	for (TActorIterator<AActor> It(&World); It; ++It)
	{
		const AActor* Actor = *It;
		const AActor* AttachRoot = FindAttachRoot(Actor);

		Actor->GetComponents(Lights);
		for (ULocalLightComponent* Light : Lights)
		{
			if (IsShadowCullCandidate(Light, AttachRoot, Room))
			{
				ClassifyLight(Light, Boxes, Settings, Cache);
			}
		}

		Actor->GetComponents(Pieces);
		for (UPrimitiveComponent* Piece : Pieces)
		{
			if (IsCutawayCandidate(Piece, AttachRoot, Room))
			{
				ClassifyPiece(Piece, Boxes, Settings, Cache);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[RoomCutaway] %s: top %d, outside %d, -X %d, +X %d, -Y %d, +Y %d, shadow-culled lights %d (%.1f ms)"),
		*Room.GetActorNameOrLabel(), Cache.TopPieces.Num(), Cache.OutsidePieces.Num(),
		Cache.SidePieces[0].Num(), Cache.SidePieces[1].Num(), Cache.SidePieces[2].Num(), Cache.SidePieces[3].Num(),
		Cache.OutsideShadowLights.Num(), (FPlatformTime::Seconds() - StartSeconds) * 1000.0);

	return Cache;
}

uint8 FRoomCutawayCache::GetCameraSideMask(const FVector& ToCamera2D, float FaceDotThreshold)
{
	uint8 Mask = 0;
	for (int32 Side = 0; Side < SideCount; ++Side)
	{
		if (FVector::DotProduct(SideNormals[Side], ToCamera2D) > FaceDotThreshold)
		{
			Mask |= 1 << Side;
		}
	}
	return Mask;
}

void FRoomCutawayCache::CollectPieces(uint8 SideMask, bool bIncludeOutside, TSet<UPrimitiveComponent*>& OutPieces) const
{
	auto AddValid = [&OutPieces](const TArray<TWeakObjectPtr<UPrimitiveComponent>>& Source)
	{
		for (const TWeakObjectPtr<UPrimitiveComponent>& Piece : Source)
		{
			if (UPrimitiveComponent* Valid = Piece.Get())
			{
				OutPieces.Add(Valid);
			}
		}
	};

	AddValid(TopPieces);
	if (bIncludeOutside)
	{
		AddValid(OutsidePieces);
	}
	for (int32 Side = 0; Side < SideCount; ++Side)
	{
		if (SideMask & (1 << Side))
		{
			AddValid(SidePieces[Side]);
		}
	}
}
