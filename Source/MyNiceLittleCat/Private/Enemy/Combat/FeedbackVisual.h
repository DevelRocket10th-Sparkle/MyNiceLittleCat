#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FeedbackVisual.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UNiagaraSystem;
class UPointLightComponent;

enum class EFeedbackVisual : uint8
{
	None, Impact, Landing, BossLanding, Shatter, DeathBurst, Dissolve, RushStart, RushTrail,
	GoldGather, GoldFlash, GoldTrail, GoldImpact, Arrival, Unlock,
	Stumble, Warning, Staff, Levitate
};

EFeedbackVisual ResolveFeedbackVisual(const UNiagaraSystem* Effect);
EFeedbackVisual ResolveStageFeedbackVisual(FName Cue, const UNiagaraSystem* Effect);

UCLASS(NotBlueprintable, Transient)
class AFeedbackVisual : public AActor
{
	GENERATED_BODY()

public:
	AFeedbackVisual();
	static AFeedbackVisual* Play(const UObject* Context, EFeedbackVisual Style, const FTransform& Transform,
		USceneComponent* Anchor = nullptr, FName Socket = NAME_None);
	virtual void Tick(float DeltaSeconds) override;

private:
	void Initialize(EFeedbackVisual InStyle);
	void UpdateVisual();

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Outer;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> Core;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> GlowMaterial;

	UPROPERTY()
	TObjectPtr<UPointLightComponent> Flash;

	EFeedbackVisual Style = EFeedbackVisual::None;
	TArray<FTransform> OuterTransforms;
	TArray<FTransform> CoreTransforms;
	float Seconds = 0.f;
	float Duration = 0.f;
	float Radius = 0.f;
	float Lift = 0.f;
	float FlashIntensity = 0.f;
	uint8 Motion = 0;
};
