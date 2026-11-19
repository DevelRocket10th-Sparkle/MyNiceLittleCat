// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/CinematicTrailDecalComponent.h"
#include "Cinematics/CinematicLog.h"

#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Tracks/MovieScene3DTransformTrack.h"

#include "Cinematics/CinematicSubsystem.h"

UCinematicTrailDecalComponent::UCinematicTrailDecalComponent()
{
	// 호출 시점에만 동작하므로 Tick 불필요
	PrimaryComponentTick.bCanEverTick = false;
}

void UCinematicTrailDecalComponent::PrecomputeTrail(ULevelSequence* Sequence)
{
	if (Sequence)
	{
		FindOrBuildTrail(Sequence);
	}
}

int32 UCinematicTrailDecalComponent::SpawnTrail(ULevelSequence* Sequence, const FTransform& SequenceOrigin,
	const TArray<FVector>& ExtraLocalPoints, const TArray<const AActor*>& IgnoredActors)
{
	UWorld* World = GetWorld();
	if (!World || !DecalMaterial || !Sequence)
	{
		UE_LOG(LogCinematic, Warning, TEXT("[TrailDecal] %s: skipped (material %d, sequence %d)"),
			*GetNameSafe(GetOwner()), DecalMaterial != nullptr, Sequence != nullptr);
		return 0;
	}

	/* 배치할 지점 목록: 경로 샘플 + 추가 지점 */

	TArray<FTrailPoint> Points = FindOrBuildTrail(Sequence);

	for (const FVector& ExtraPoint : ExtraLocalPoints)
	{
		// 추가 지점은 가장 가까운 경로 지점을 대상이 지나는 시각에 나타나게 함
		double Seconds = 0.0;
		double BestDistanceSquared = TNumericLimits<double>::Max();
		for (const FTrailPoint& PathPoint : Points)
		{
			const double DistanceSquared = FVector::DistSquared2D(PathPoint.Local, ExtraPoint);
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				Seconds = PathPoint.Seconds;
			}
		}

		Points.Add({ ExtraPoint + FVector(0.f, 0.f, ExtraPointHeight), Seconds });
	}

	/* 바닥 검사 설정 */

	FCollisionQueryParams Query(SCENE_QUERY_STAT(CinematicTrailFloor), false);
	for (const AActor* IgnoredActor : IgnoredActors)
	{
		if (IgnoredActor)
		{
			Query.AddIgnoredActor(IgnoredActor);
		}
	}
	const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);

	/* 지점마다 바닥을 찾아 데칼 배치 */

	int32 SpawnedCount = 0;

	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		// 같은 간격의 발자국처럼 보이지 않도록 지점마다 좌우로 조금씩 흩뿌림
		const FVector Center = SequenceOrigin.TransformPosition(Points[Index].Local);
		const FVector Side = FVector(FMath::Sin(Index * 2.1f), FMath::Cos(Index * 1.7f), 0.f) * SideJitter;
		const FVector TraceStart = Center + Side;
		const FVector TraceEnd = TraceStart - FVector(0.f, 0.f, FloorTraceDistance);

		// 정적 바닥 우선, 없으면 가시성 채널로 한 번 더 검사
		FHitResult Floor;
		const bool bHit = World->LineTraceSingleByObjectType(Floor, TraceStart, TraceEnd, StaticOnly, Query)
			|| World->LineTraceSingleByChannel(Floor, TraceStart, TraceEnd, ECC_Visibility, Query);

		// 벽 안에서 시작했거나 기울어진 면이면 건너뜀
		if (!bHit || Floor.bStartPenetrating || Floor.ImpactNormal.Z < MinFloorNormalZ)
		{
			continue;
		}

		// 황금비 분수로 크기와 회전을 불규칙하게 (같은 시퀀스면 항상 같은 모양)
		const float Alpha = FMath::Frac(Index * 0.618f);
		const float Size = FMath::Lerp(MinDecalSize, MaxDecalSize, Alpha);
		const FRotator Rotation = (-Floor.ImpactNormal).Rotation() + FRotator(0.f, 0.f, Index * 71.f);

		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, DecalMaterial,
			FVector(DecalDepth, Size, Size), Floor.ImpactPoint, Rotation);
		if (!Decal)
		{
			continue;
		}

		// 화면 크기에 따른 사라짐을 끄고(멀리서도 보이게), 대상이 지나가는 시각에 맞춰 페이드인
		Decal->SetFadeScreenSize(0.f);
		Decal->SetFadeIn(static_cast<float>(Points[Index].Seconds), FadeInSeconds);

		SpawnedDecals.Add(Decal);
		++SpawnedCount;
	}

	UE_LOG(LogCinematic, Log, TEXT("[TrailDecal] %s: points %d, decals %d"), *Sequence->GetName(), Points.Num(), SpawnedCount);
	return SpawnedCount;
}

void UCinematicTrailDecalComponent::RevealAll()
{
	for (const TWeakObjectPtr<UDecalComponent>& Decal : SpawnedDecals)
	{
		if (Decal.IsValid())
		{
			Decal->SetFadeIn(0.f, 0.f);
		}
	}
}

void UCinematicTrailDecalComponent::ClearTrail()
{
	for (const TWeakObjectPtr<UDecalComponent>& Decal : SpawnedDecals)
	{
		if (Decal.IsValid())
		{
			Decal->DestroyComponent();
		}
	}

	SpawnedDecals.Reset();
}

void UCinematicTrailDecalComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearTrail();
	CachedTrails.Reset();

	Super::EndPlay(EndPlayReason);
}

const TArray<UCinematicTrailDecalComponent::FTrailPoint>& UCinematicTrailDecalComponent::FindOrBuildTrail(ULevelSequence* Sequence)
{
	if (const TArray<FTrailPoint>* Cached = CachedTrails.Find(Sequence))
	{
		return *Cached;
	}

	return CachedTrails.Add(Sequence, SampleTrail(Sequence));
}

TArray<UCinematicTrailDecalComponent::FTrailPoint> UCinematicTrailDecalComponent::SampleTrail(ULevelSequence* Sequence) const
{
	TArray<FTrailPoint> Points;

	UMovieScene* MovieScene = Sequence ? Sequence->GetMovieScene() : nullptr;
	if (!MovieScene)
	{
		return Points;
	}

	/* 1) 태그가 붙은 바인딩의 트랜스폼 트랙에서 위치 채널(X, Y, Z) 찾기 */

	TArrayView<FMovieSceneDoubleChannel* const> LocationChannels;

	for (const FMovieSceneObjectBindingID& BindingId : Sequence->FindBindingsByTag(BindingTag))
	{
		const FMovieSceneBinding* Binding = MovieScene->FindBinding(BindingId.GetGuid());
		if (!Binding)
		{
			continue;
		}

		for (UMovieSceneTrack* Track : Binding->GetTracks())
		{
			const UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(Track);
			if (!TransformTrack || TransformTrack->GetAllSections().IsEmpty())
			{
				continue;
			}

			// 트랜스폼 섹션의 첫 3개 double 채널이 위치 X, Y, Z
			UMovieSceneSection* Section = TransformTrack->GetAllSections()[0];
			TArrayView<FMovieSceneDoubleChannel* const> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
			if (Channels.Num() >= 3)
			{
				LocationChannels = Channels;
			}
			break;
		}

		if (LocationChannels.Num() >= 3)
		{
			break;
		}
	}

	if (LocationChannels.Num() < 3)
	{
		UE_LOG(LogCinematic, Warning, TEXT("[TrailDecal] %s: no transform track on binding tag '%s'"),
			*Sequence->GetName(), *BindingTag.ToString());
		return Points;
	}

	/* 2) 샘플링 구간: 시작/끝 마커, 없으면 시퀀스 전체 */

	const FFrameRate TickResolution = MovieScene->GetTickResolution();
	const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
	const TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();
	const double RangeStartSeconds = TickResolution.AsSeconds(PlaybackRange.GetLowerBoundValue());
	const double RangeEndSeconds = TickResolution.AsSeconds(PlaybackRange.GetUpperBoundValue());

	double FromSeconds = 0.0;
	double ToSeconds = RangeEndSeconds - RangeStartSeconds;

	for (const FCinematicMarker& Marker : UCinematicSubsystem::ReadMarkers(Sequence))
	{
		if (Marker.Label.Equals(StartMarkerLabel, ESearchCase::IgnoreCase))
		{
			FromSeconds = Marker.Seconds;
		}
		else if (Marker.Label.Equals(EndMarkerLabel, ESearchCase::IgnoreCase))
		{
			ToSeconds = Marker.Seconds;
		}
	}

	// 재생 시작 기준 시각의 위치 계산
	auto EvaluateLocation = [&](double Seconds)
	{
		const FFrameTime Time = (RangeStartSeconds + Seconds) * TickResolution;

		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		LocationChannels[0]->Evaluate(Time, X);
		LocationChannels[1]->Evaluate(Time, Y);
		LocationChannels[2]->Evaluate(Time, Z);

		return FVector(X, Y, Z);
	};

	/* 3) 표시 프레임 간격으로 이동 거리를 누적하며 Spacing마다 한 점씩 기록 */

	const double Step = DisplayRate.AsInterval();
	FVector LastLocation = EvaluateLocation(FromSeconds);

	// 첫 데칼이 구간 시작 직후에 나오도록 누적 거리를 미리 채워 둠
	float Travelled = Spacing * 0.6f;
	float NextGap = Spacing;

	for (double Seconds = FromSeconds + Step; Seconds <= ToSeconds + KINDA_SMALL_NUMBER; Seconds += Step)
	{
		const FVector CurrentLocation = EvaluateLocation(Seconds);
		Travelled += FVector::Dist2D(LastLocation, CurrentLocation);
		LastLocation = CurrentLocation;

		if (Travelled < NextGap)
		{
			continue;
		}

		Travelled = 0.f;
		Points.Add({ CurrentLocation, Seconds });

		// 일정한 발자국처럼 보이지 않도록 다음 간격을 0.7~1.3배로 불규칙하게
		NextGap = Spacing * (0.7f + 0.6f * FMath::Frac(Points.Num() * 0.618f));
	}

	return Points;
}
