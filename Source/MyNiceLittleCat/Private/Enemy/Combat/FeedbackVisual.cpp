#include "Enemy/Combat/FeedbackVisual.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

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

	FVisualDesign GetDesign(EFeedbackVisual Style)
	{
		const FLinearColor Amber(3.4f, 0.74f, 0.12f);
		const FLinearColor Ivory(5.f, 3.6f, 1.6f);
		const FLinearColor Crimson(3.8f, 0.045f, 0.20f);
		const FLinearColor Violet(1.45f, 0.23f, 3.6f);
		const FLinearColor Lavender(3.5f, 2.4f, 5.f);
		const FLinearColor Jade(0.24f, 2.6f, 1.2f);
		switch (Style)
		{
		case EFeedbackVisual::Landing:    return { Ring, Amber, Ivory, 0.68f, 250.f, 105.f, 24 };
		case EFeedbackVisual::BossLanding: return { Ring, FLinearColor(0.22f, 1.8f, 3.4f),
			FLinearColor(1.6f, 3.7f, 5.4f), 0.90f, 300.f, 240.f, 24 };
		case EFeedbackVisual::Shatter:    return { Fracture, Violet, Lavender, 1.05f, 280.f, 180.f, 28 };
		case EFeedbackVisual::DeathBurst: return { Fracture, Violet, Lavender, 0.48f, 100.f, 68.f, 14 };
		case EFeedbackVisual::Dissolve:   return { Rise, Violet * 0.5f, Lavender, 0.75f, 85.f, 130.f, 13 };
		case EFeedbackVisual::RushStart:  return { Ring, Amber, Ivory, 0.42f, 116.f, 45.f, 16 };
		case EFeedbackVisual::RushTrail:  return { Trail, Crimson, Amber, 2.0f, 138.f, 46.f, 8 };
		case EFeedbackVisual::GoldGather: return { Gather, Jade, Ivory, 0.65f, 112.f, 75.f, 14 };
		case EFeedbackVisual::GoldFlash:  return { Fan, Amber, Ivory, 0.19f, 91.f, 25.f, 7 };
		case EFeedbackVisual::GoldTrail:  return { Trail, Amber, Ivory, 6.f, 72.f, 13.f, 6 };
		case EFeedbackVisual::GoldImpact: return { Fan, Amber, Ivory, 0.36f, 73.f, 48.f, 10 };
		case EFeedbackVisual::Arrival:    return { Orbit, Amber * 0.45f, Ivory * 0.35f, 2.2f, 245.f, 108.f, 15 };
		case EFeedbackVisual::Unlock:     return { Gather, Amber, Ivory, 0.72f, 96.f, 76.f, 14 };
		case EFeedbackVisual::Stumble:    return { Ring, Amber * 0.6f, Ivory * 0.55f, 0.38f, 72.f, 25.f, 12 };
		case EFeedbackVisual::Warning:    return { Orbit, Crimson, Amber, 1.f, 155.f, 100.f, 19 };
		case EFeedbackVisual::Staff:      return { Fan, Violet, Lavender, 0.56f, 126.f, 78.f, 16 };
		case EFeedbackVisual::Levitate:   return { Orbit, Violet, Lavender, 2.15f, 110.f, 170.f, 24 };
		default:                         return { Fan, Amber, Ivory, 0.30f, 79.f, 45.f, 12 };
		}
	}

	FTransform MakeShard(const FVector& Position, const FVector& Heading, float Length, float Width, float Depth)
	{
		return FTransform(Heading.Rotation(), Position,
			FVector(Length, Width, Depth).ComponentMax(FVector(0.05)) / 100.f);
	}
}

EFeedbackVisual ResolveFeedbackVisual(const UNiagaraSystem* Effect)
{
	if (!Effect || !Effect->GetOutermost()->GetName().StartsWith(TEXT("/Game/Game/VFX/Niagara/Enemy/")))
	{
		return EFeedbackVisual::None;
	}

	static const TMap<FName, EFeedbackVisual> Styles = {
		{ TEXT("NS_Enemy_HitImpact_v1"), EFeedbackVisual::Impact },
		{ TEXT("NS_Enemy_DeathBurst_v1"), EFeedbackVisual::DeathBurst },
		{ TEXT("NS_Enemy_DeathPoof_v1"), EFeedbackVisual::Dissolve },
		{ TEXT("NS_Enemy_GroundSlam_v1"), EFeedbackVisual::Landing },
		{ TEXT("NS_Enemy_RushDust_v1"), EFeedbackVisual::RushStart },
		{ TEXT("NS_Enemy_RushTrail_v1"), EFeedbackVisual::RushTrail },
		{ TEXT("NS_MoneyGun_Appear_v1"), EFeedbackVisual::GoldGather },
		{ TEXT("NS_MoneyGun_Muzzle_v1"), EFeedbackVisual::GoldFlash },
		{ TEXT("NS_MoneyProjectile_Trail_v1"), EFeedbackVisual::GoldTrail },
		{ TEXT("NS_MoneyProjectile_Impact_v1"), EFeedbackVisual::GoldImpact }
	};
	return Styles.FindRef(Effect->GetFName());
}

EFeedbackVisual ResolveStageFeedbackVisual(FName Cue, const UNiagaraSystem* Effect)
{
	if (!Effect || (!Effect->GetOutermost()->GetName().StartsWith(TEXT("/Game/Game/VFX/Niagara/Stage3/"))
		&& ResolveFeedbackVisual(Effect) == EFeedbackVisual::None))
	{
		return EFeedbackVisual::None;
	}
	static const TMap<FName, EFeedbackVisual> Styles = {
		{ TEXT("Arrival"), EFeedbackVisual::Arrival },
		{ TEXT("GateUnlock"), EFeedbackVisual::Unlock },
		{ TEXT("BossLand"), EFeedbackVisual::BossLanding },
		{ TEXT("PlayerStumble"), EFeedbackVisual::Stumble },
		{ TEXT("Phase1Start"), EFeedbackVisual::Warning },
		{ TEXT("ImpactReplay"), EFeedbackVisual::BossLanding },
		{ TEXT("BarrierCollapse"), EFeedbackVisual::Shatter },
		{ TEXT("StaffFlourish"), EFeedbackVisual::Staff },
		{ TEXT("RedWarning"), EFeedbackVisual::Warning },
		{ TEXT("StairBreak"), EFeedbackVisual::Shatter },
		{ TEXT("BossLevitate"), EFeedbackVisual::Levitate }
	};
	return Styles.FindRef(Cue);
}

AFeedbackVisual::AFeedbackVisual()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	SetCanBeDamaged(false);
	SetReplicates(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ShardMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Emissive(
		TEXT("/Game/Game/VFX/Materials/M_FeedbackGlow.M_FeedbackGlow"));
	GlowMaterial = Emissive.Object;

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
		Layer->SetStaticMesh(ShardMesh.Object);
		Layer->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Layer->SetGenerateOverlapEvents(false);
		Layer->SetCanEverAffectNavigation(false);
		Layer->SetCastShadow(false);
		Layer->SetReceivesDecals(false);
	}
}

AFeedbackVisual* AFeedbackVisual::Play(const UObject* Context, EFeedbackVisual InStyle, const FTransform& Transform,
	USceneComponent* Anchor, FName Socket)
{
	UWorld* World = Context ? Context->GetWorld() : nullptr;
	if (InStyle == EFeedbackVisual::None || !World || World->GetNetMode() == NM_DedicatedServer)
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

void AFeedbackVisual::Initialize(EFeedbackVisual InStyle)
{
	Style = InStyle;
	const FVisualDesign Design = GetDesign(Style);
	Duration = Design.Duration;
	Radius = Design.Radius;
	Lift = Design.Lift;
	Motion = Design.Motion;
	if (Style == EFeedbackVisual::Landing || Style == EFeedbackVisual::BossLanding
		|| Style == EFeedbackVisual::Shatter || Style == EFeedbackVisual::GoldFlash)
	{
		FlashIntensity = Style == EFeedbackVisual::Shatter ? 2200.f : 900.f;
		Flash->SetLightColor(Design.Color.GetClamped());
		Flash->SetAttenuationRadius(Design.Radius * 1.5f);
	}

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
	const float Travel = 1.f - FMath::Square(1.f - Progress);
	const float Tail = 1.f - FMath::SmoothStep(0.36f, 1.f, Progress);
	if (FlashIntensity > 0.f)
	{
		Flash->SetIntensity(FlashIntensity * FMath::Square(1.f - FMath::Clamp(Seconds / 0.18f, 0.f, 1.f)));
	}
	const int32 Count = OuterTransforms.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Fraction = float(Index) / Count;
		const float Angle = 2.f * PI * Fraction;
		const float Variation = 0.65f + 0.35f * FMath::Frac(Index * 0.618034f);
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0);
		const FVector Tangent(-Radial.Y, Radial.X, 0);
		FVector Position;
		FVector Heading;
		FVector AccentPosition;
		float Length = 0.f;
		float Width = 0.f;
		float AccentSize = 0.f;

		switch (Motion)
		{
		case Ring:
			Position = Radial * Radius * (0.22f + Travel * 0.78f) + FVector(0, 0, 5);
			Heading = Tangent;
			Length = 2.f * PI * Radius / Count * (0.16f + Travel * 0.68f) * Tail;
			Width = 7.f * Tail;
			AccentPosition = Radial * Radius * Travel * Variation * (Style == EFeedbackVisual::BossLanding ? 0.65f : 1.f)
				+ FVector(0, 0, 8.f + 4.f * Lift * Variation * Progress * (1.f - Progress));
			AccentSize = (Style == EFeedbackVisual::BossLanding ? 43.f : 17.f) * Tail * Variation;
			break;
		case Fracture:
			Position = Radial * Radius * (0.10f + Travel * Variation)
				+ FVector(0, 0, Lift * (0.18f + 2.6f * Progress * (1.f - Progress) + Fraction * 0.25f));
			Heading = Radial.RotateAngleAxis(120.f * Progress + Index * 21.f, Tangent);
			Length = (13.f + 25.f * Variation) * Tail;
			Width = 6.f * Variation * Tail;
			AccentPosition = Position * 0.72f + FVector(0, 0, 12);
			AccentSize = 15.f * Tail;
			break;
		case Orbit:
		{
			const float Spin = Angle + Seconds * (Style == EFeedbackVisual::Arrival ? 0.6f : 2.7f);
			const float Envelope = FMath::SmoothStep(0.f, 0.12f, Progress) * Tail;
			const FVector OrbitDirection(FMath::Cos(Spin), FMath::Sin(Spin), 0);
			const float Height = Style == EFeedbackVisual::Levitate
				? FMath::Lerp(0.08f, 0.87f, Fraction) + Travel * 0.25f
				: Fraction - 0.42f + Travel * 0.33f;
			Position = OrbitDirection * Radius * (0.60f + Fraction * 0.40f) + FVector(0, 0, Lift * Height);
			Heading = FVector(-OrbitDirection.Y, OrbitDirection.X, 0.25);
			Length = (Style == EFeedbackVisual::Arrival ? 13.f : 46.f) * Envelope;
			Width = 5.f * Envelope;
			AccentPosition = Position * 0.81f;
			AccentSize = 9.f * Envelope;
			break;
		}
		case Trail:
		{
			const float Phase = FMath::Frac(Fraction + Seconds * 3.5f);
			const float Taper = (1.f - Phase) * Tail;
			Position = FVector(-Radius * (0.18f + Phase), Radial.X * Lift * Phase, Radial.Y * Lift * Phase);
			Heading = FVector::ForwardVector;
			Length = (18.f + 40.f * Phase) * Taper;
			Width = 5.f * Taper;
			AccentPosition = Position * 0.72f;
			AccentSize = 9.f * Taper;
			break;
		}
		case Gather:
			Position = Radial * Radius * FMath::Lerp(1.f, 0.12f, Travel)
				+ FVector(0, 0, Lift * (Fraction - 0.30f) * (1.f - Progress));
			Heading = -Radial;
			Length = 31.f * Variation * Tail;
			Width = 5.f * Tail;
			AccentPosition = Radial * Radius * 0.42f * Travel;
			AccentSize = 11.f * Tail;
			break;
		case Rise:
			Position = Radial * Radius * Variation * FMath::Lerp(0.4f, 1.f, Travel)
				+ FVector(0, 0, Lift * (0.2f + Travel * Variation));
			Heading = (Radial * 0.25f + FVector::UpVector).GetSafeNormal();
			Length = 25.f * Variation * Tail;
			Width = 5.f * Tail;
			AccentPosition = Position * 0.76f + FVector(0, 0, 12);
			AccentSize = 8.f * Tail;
			break;
		default:
		{
			const FVector Direction = FVector(0.7, Radial.X * 0.85, Radial.Y * Lift / FMath::Max(Radius, 1.f)).GetSafeNormal();
			Position = Direction * Radius * Travel * Variation;
			Heading = Direction;
			Length = (16.f + 34.f * Variation) * Tail;
			Width = 6.f * Tail;
			AccentPosition = Position * 0.55f;
			AccentSize = 19.f * Tail;
			break;
		}
		}

		OuterTransforms[Index] = MakeShard(Position, Heading, Length, Width, Width * 0.65f);
		CoreTransforms[Index] = MakeShard(AccentPosition, Motion == Ring ? Radial + FVector::UpVector : Heading,
			AccentSize, AccentSize * 0.29f, AccentSize * 0.29f);
	}
}
