// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Combat/FeedbackVisual.h"
#include "Enemy/Combat/FeedbackVisualSettings.h"
#include "Enemy/EnemyLog.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	enum EMotion : uint8 { Ring, Fracture, Fan, Orbit, Trail, Gather, Rise };

	struct FVisualDesign
	{
		EMotion Motion;
		FLinearColor Color;
		FLinearColor Accent;
		float Duration;
		float Radius;
		float Lift;
		int32 Count;
	};

	FVisualDesign GetDesign(EFeedbackVisualStyle Style)
	{
		const FLinearColor Amber(3.4f, 0.74f, 0.12f);
		const FLinearColor Ivory(5.f, 3.6f, 1.6f);
		const FLinearColor Crimson(3.8f, 0.045f, 0.20f);
		const FLinearColor Violet(1.45f, 0.23f, 3.6f);
		const FLinearColor Lavender(3.5f, 2.4f, 5.f);
		const FLinearColor Jade(0.24f, 2.6f, 1.2f);
		switch (Style)
		{
		case EFeedbackVisualStyle::Landing:    return { Ring, Amber, Ivory, 0.68f, 250.f, 105.f, 24 };
		case EFeedbackVisualStyle::BossLanding: return { Ring, FLinearColor(0.22f, 1.8f, 3.4f),
			FLinearColor(1.6f, 3.7f, 5.4f), 0.90f, 300.f, 240.f, 24 };
		case EFeedbackVisualStyle::Shatter:    return { Fracture, Violet, Lavender, 1.05f, 280.f, 180.f, 28 };
		case EFeedbackVisualStyle::DeathBurst: return { Fracture, Violet, Lavender, 0.48f, 100.f, 68.f, 14 };
		case EFeedbackVisualStyle::Dissolve:   return { Rise, Violet * 0.5f, Lavender, 0.75f, 85.f, 130.f, 13 };
		case EFeedbackVisualStyle::RushStart:  return { Ring, Amber, Ivory, 0.42f, 116.f, 45.f, 16 };
		case EFeedbackVisualStyle::RushTrail:  return { Trail, Crimson, Amber, 2.0f, 138.f, 46.f, 8 };
		case EFeedbackVisualStyle::GoldGather: return { Gather, Jade, Ivory, 0.65f, 112.f, 75.f, 14 };
		case EFeedbackVisualStyle::GoldFlash:  return { Fan, Amber, Ivory, 0.19f, 91.f, 25.f, 7 };
		case EFeedbackVisualStyle::GoldTrail:  return { Trail, Amber, Ivory, 6.f, 72.f, 13.f, 6 };
		case EFeedbackVisualStyle::GoldImpact: return { Fan, Amber, Ivory, 0.36f, 73.f, 48.f, 10 };
		case EFeedbackVisualStyle::Arrival:    return { Orbit, Amber * 0.45f, Ivory * 0.35f, 2.2f, 245.f, 108.f, 15 };
		case EFeedbackVisualStyle::Unlock:     return { Gather, Amber, Ivory, 0.72f, 96.f, 76.f, 14 };
		case EFeedbackVisualStyle::Stumble:    return { Ring, Amber * 0.6f, Ivory * 0.55f, 0.38f, 72.f, 25.f, 12 };
		case EFeedbackVisualStyle::Warning:    return { Orbit, Crimson, Amber, 1.f, 155.f, 100.f, 19 };
		case EFeedbackVisualStyle::Staff:      return { Fan, Violet, Lavender, 0.56f, 126.f, 78.f, 16 };
		case EFeedbackVisualStyle::Levitate:   return { Orbit, Violet, Lavender, 2.15f, 110.f, 170.f, 24 };
		default:                         return { Fan, Amber, Ivory, 0.30f, 79.f, 45.f, 12 };
		}
	}

	FTransform MakeShard(const FVector& Position, const FVector& Heading, float Length, float Width, float Depth)
	{
		return FTransform(Heading.Rotation(), Position,
			FVector(Length, Width, Depth).ComponentMax(FVector(0.05)) / 100.f);
	}
}

AFeedbackVisual::AFeedbackVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetCanBeDamaged(false);
	SetReplicates(false);

	Outer = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Outer"));
	SetRootComponent(Outer);
	Core = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Core"));
	Core->SetupAttachment(Outer);
	Flash = CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));
	Flash->SetupAttachment(Outer);
	Flash->SetCastShadows(false);
	Flash->SetIntensity(0.f);
	Flash->SetRelativeLocation(FVector(0, 0, 28));

	for (UInstancedStaticMeshComponent* Layer : { Outer.Get(), Core.Get() })
	{
		Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Layer->SetGenerateOverlapEvents(false);
		Layer->SetCanEverAffectNavigation(false);
		Layer->SetCastShadow(false);
		Layer->SetReceivesDecals(false);
	}
}

AFeedbackVisual* AFeedbackVisual::Play(const UObject* Context, EFeedbackVisualStyle InStyle, const FTransform& Transform,
	USceneComponent* Anchor, FName Socket)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	if (InStyle == EFeedbackVisualStyle::None || !World || World->GetNetMode() == NM_DedicatedServer)
	{
		return nullptr;
	}

	AFeedbackVisual* Visual = World->SpawnActor<AFeedbackVisual>(AFeedbackVisual::StaticClass(), Transform);
	if (!Visual)
	{
		return nullptr;
	}

	if (Anchor)
	{
		Visual->AttachToComponent(Anchor, FAttachmentTransformRules::KeepWorldTransform, Socket);
		Visual->SetOwner(Anchor->GetOwner());
	}
	Visual->Initialize(InStyle);
	return Visual;
}

void AFeedbackVisual::Initialize(EFeedbackVisualStyle InStyle)
{
	Style = InStyle;
	const FVisualDesign Design = GetDesign(Style);
	Duration = Design.Duration;
	Radius = Design.Radius;
	Lift = Design.Lift;
	Motion = Design.Motion;
	if (Style == EFeedbackVisualStyle::Landing || Style == EFeedbackVisualStyle::BossLanding
		|| Style == EFeedbackVisualStyle::Shatter || Style == EFeedbackVisualStyle::GoldFlash)
	{
		FlashIntensity = Style == EFeedbackVisualStyle::Shatter ? 2200.f : 900.f;
		Flash->SetLightColor(Design.Color.GetClamped());
		Flash->SetAttenuationRadius(Design.Radius * 1.5f);
	}

	// 메시와 머티리얼은 프로젝트 설정에서 (처음 쓸 때 로드, 이후 로드된 에셋 재사용)
	const UFeedbackVisualSettings* Settings = GetDefault<UFeedbackVisualSettings>();
	UStaticMesh* ShardMesh = Settings->ShardMesh.LoadSynchronous();
	UMaterialInterface* GlowMaterial = Settings->GlowMaterial.LoadSynchronous();
	if (!ShardMesh || !GlowMaterial)
	{
		UE_LOG(LogEnemy, Warning, TEXT("[FeedbackVisual] ShardMesh or GlowMaterial is not set in Project Settings > Feedback Visual"));
	}
	Outer->SetStaticMesh(ShardMesh);
	Core->SetStaticMesh(ShardMesh);

	if (UMaterialInstanceDynamic* Material = Outer->CreateDynamicMaterialInstance(0, GlowMaterial))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Design.Color);
	}
	if (UMaterialInstanceDynamic* Material = Core->CreateDynamicMaterialInstance(0, GlowMaterial))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Design.Accent);
	}

	OuterTransforms.Init(FTransform::Identity, Design.Count);
	CoreTransforms.Init(FTransform::Identity, Design.Count);
	UpdateVisual();
	Outer->AddInstances(OuterTransforms, false, false, false);
	Core->AddInstances(CoreTransforms, false, false, false);
	SetLifeSpan(Duration);
}

void AFeedbackVisual::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Seconds += DeltaSeconds;
	if (GetOwner() && !GetAttachParentActor())
	{
		Destroy();
		return;
	}
	UpdateVisual();
	Outer->BatchUpdateInstancesTransforms(0, OuterTransforms, false, true, true);
	Core->BatchUpdateInstancesTransforms(0, CoreTransforms, false, true, true);
}

void AFeedbackVisual::UpdateVisual()
{
	const float Progress = FMath::Clamp(Seconds / Duration, 0.f, 1.f);

	// 시작 섬광: 0.18초 동안 제곱으로 꺼짐
	if (FlashIntensity > 0.f)
	{
		Flash->SetIntensity(FlashIntensity * FMath::Square(1.f - FMath::Clamp(Seconds / 0.18f, 0.f, 1.f)));
	}

	FShardInput In;
	In.Count = OuterTransforms.Num();
	In.Progress = Progress;
	In.Travel = 1.f - FMath::Square(1.f - Progress);
	In.Tail = 1.f - FMath::SmoothStep(0.36f, 1.f, Progress);

	for (int32 Index = 0; Index < In.Count; ++Index)
	{
		// 조각마다 원 위의 방향과 황금비로 흩은 크기 변화
		In.Index = Index;
		In.Fraction = float(Index) / In.Count;
		In.Angle = 2.f * PI * In.Fraction;
		In.Variation = 0.65f + 0.35f * FMath::Frac(Index * 0.618034f);
		In.Radial = FVector(FMath::Cos(In.Angle), FMath::Sin(In.Angle), 0);
		In.Tangent = FVector(-In.Radial.Y, In.Radial.X, 0);

		FShardPose Pose;
		switch (Motion)
		{
		case Ring:		Pose = PoseRing(In); break;
		case Fracture:	Pose = PoseFracture(In); break;
		case Orbit:		Pose = PoseOrbit(In); break;
		case Trail:		Pose = PoseTrail(In); break;
		case Gather:	Pose = PoseGather(In); break;
		case Rise:		Pose = PoseRise(In); break;
		default:		Pose = PoseFan(In); break;
		}

		OuterTransforms[Index] = MakeShard(Pose.Position, Pose.Heading, Pose.Length, Pose.Width, Pose.Width * 0.65f);
		CoreTransforms[Index] = MakeShard(Pose.AccentPosition, Motion == Ring ? In.Radial + FVector::UpVector : Pose.Heading,
			Pose.AccentSize, Pose.AccentSize * 0.29f, Pose.AccentSize * 0.29f);
	}
}

/* 움직임별 조각 모양 */

AFeedbackVisual::FShardPose AFeedbackVisual::PoseRing(const FShardInput& In) const
{
	// 보스 착지는 강조 조각을 안쪽에 크게
	const bool bBoss = Style == EFeedbackVisualStyle::BossLanding;

	FShardPose Pose;
	Pose.Position = In.Radial * Radius * (0.22f + In.Travel * 0.78f) + FVector(0, 0, 5);
	Pose.Heading = In.Tangent;
	Pose.Length = 2.f * PI * Radius / In.Count * (0.16f + In.Travel * 0.68f) * In.Tail;
	Pose.Width = 7.f * In.Tail;
	Pose.AccentPosition = In.Radial * Radius * In.Travel * In.Variation * (bBoss ? 0.65f : 1.f)
		+ FVector(0, 0, 8.f + 4.f * Lift * In.Variation * In.Progress * (1.f - In.Progress));
	Pose.AccentSize = (bBoss ? 43.f : 17.f) * In.Tail * In.Variation;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseFracture(const FShardInput& In) const
{
	FShardPose Pose;
	Pose.Position = In.Radial * Radius * (0.10f + In.Travel * In.Variation)
		+ FVector(0, 0, Lift * (0.18f + 2.6f * In.Progress * (1.f - In.Progress) + In.Fraction * 0.25f));
	Pose.Heading = In.Radial.RotateAngleAxis(120.f * In.Progress + In.Index * 21.f, In.Tangent);
	Pose.Length = (13.f + 25.f * In.Variation) * In.Tail;
	Pose.Width = 6.f * In.Variation * In.Tail;
	Pose.AccentPosition = Pose.Position * 0.72f + FVector(0, 0, 12);
	Pose.AccentSize = 15.f * In.Tail;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseOrbit(const FShardInput& In) const
{
	// 도착은 느리게 돌고 작게, 공중 부양은 아래에서 위로 층층이
	const float Spin = In.Angle + Seconds * (Style == EFeedbackVisualStyle::Arrival ? 0.6f : 2.7f);
	const float Envelope = FMath::SmoothStep(0.f, 0.12f, In.Progress) * In.Tail;
	const FVector OrbitDirection(FMath::Cos(Spin), FMath::Sin(Spin), 0);
	const float Height = Style == EFeedbackVisualStyle::Levitate
		? FMath::Lerp(0.08f, 0.87f, In.Fraction) + In.Travel * 0.25f
		: In.Fraction - 0.42f + In.Travel * 0.33f;

	FShardPose Pose;
	Pose.Position = OrbitDirection * Radius * (0.60f + In.Fraction * 0.40f) + FVector(0, 0, Lift * Height);
	Pose.Heading = FVector(-OrbitDirection.Y, OrbitDirection.X, 0.25);
	Pose.Length = (Style == EFeedbackVisualStyle::Arrival ? 13.f : 46.f) * Envelope;
	Pose.Width = 5.f * Envelope;
	Pose.AccentPosition = Pose.Position * 0.81f;
	Pose.AccentSize = 9.f * Envelope;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseTrail(const FShardInput& In) const
{
	// 조각마다 위상을 달리해 뒤로 흘러가며 가늘어짐
	const float Phase = FMath::Frac(In.Fraction + Seconds * 3.5f);
	const float Taper = (1.f - Phase) * In.Tail;

	FShardPose Pose;
	Pose.Position = FVector(-Radius * (0.18f + Phase), In.Radial.X * Lift * Phase, In.Radial.Y * Lift * Phase);
	Pose.Heading = FVector::ForwardVector;
	Pose.Length = (18.f + 40.f * Phase) * Taper;
	Pose.Width = 5.f * Taper;
	Pose.AccentPosition = Pose.Position * 0.72f;
	Pose.AccentSize = 9.f * Taper;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseGather(const FShardInput& In) const
{
	FShardPose Pose;
	Pose.Position = In.Radial * Radius * FMath::Lerp(1.f, 0.12f, In.Travel)
		+ FVector(0, 0, Lift * (In.Fraction - 0.30f) * (1.f - In.Progress));
	Pose.Heading = -In.Radial;
	Pose.Length = 31.f * In.Variation * In.Tail;
	Pose.Width = 5.f * In.Tail;
	Pose.AccentPosition = In.Radial * Radius * 0.42f * In.Travel;
	Pose.AccentSize = 11.f * In.Tail;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseRise(const FShardInput& In) const
{
	FShardPose Pose;
	Pose.Position = In.Radial * Radius * In.Variation * FMath::Lerp(0.4f, 1.f, In.Travel)
		+ FVector(0, 0, Lift * (0.2f + In.Travel * In.Variation));
	Pose.Heading = (In.Radial * 0.25f + FVector::UpVector).GetSafeNormal();
	Pose.Length = 25.f * In.Variation * In.Tail;
	Pose.Width = 5.f * In.Tail;
	Pose.AccentPosition = Pose.Position * 0.76f + FVector(0, 0, 12);
	Pose.AccentSize = 8.f * In.Tail;
	return Pose;
}

AFeedbackVisual::FShardPose AFeedbackVisual::PoseFan(const FShardInput& In) const
{
	const FVector Direction = FVector(0.7, In.Radial.X * 0.85, In.Radial.Y * Lift / FMath::Max(Radius, 1.f)).GetSafeNormal();

	FShardPose Pose;
	Pose.Position = Direction * Radius * In.Travel * In.Variation;
	Pose.Heading = Direction;
	Pose.Length = (16.f + 34.f * In.Variation) * In.Tail;
	Pose.Width = 6.f * In.Tail;
	Pose.AccentPosition = Pose.Position * 0.55f;
	Pose.AccentSize = 19.f * In.Tail;
	return Pose;
}
