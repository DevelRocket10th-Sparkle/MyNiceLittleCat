#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/InteractableInterface.h"
#include "Stage3BossCinematics.generated.h"

class ABossCharacter;
class ABossEncounterManager;
class APlayerCharacter;
class UBoxComponent;
class UDecalComponent;
class UItemData;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class UStaticMesh;
class UStaticMeshComponent;
class USoundBase;
class UCameraOcclusionComponent;

USTRUCT()
struct FStage3CinematicDoor
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly)
	TObjectPtr<AActor> Actor;

	UPROPERTY(EditInstanceOnly)
	TArray<FName> Leaves;

	UPROPERTY(EditInstanceOnly)
	TArray<float> OpenYaw;
};

USTRUCT()
struct FStage3CinematicPhase
{
	GENERATED_BODY()

	UPROPERTY(EditInstanceOnly)
	TObjectPtr<ULevelSequence> Sequence;

	UPROPERTY(EditInstanceOnly)
	TObjectPtr<AActor> BossStart;

	UPROPERTY(EditInstanceOnly)
	FVector EntryCenter = FVector::ZeroVector;

	UPROPERTY(EditInstanceOnly)
	FVector EntryExtent = FVector(200.f);

	UPROPERTY(EditInstanceOnly)
	TArray<int32> OpenDoors;

	UPROPERTY(EditInstanceOnly)
	int32 DoorToClose = INDEX_NONE;

	UPROPERTY(EditInstanceOnly)
	TArray<FVector> BloodTrail;

	UPROPERTY(EditInstanceOnly)
	FLinearColor EyeGlow = FLinearColor::Black;
};

UCLASS()
class MYNICELITTLECAT_API AStage3BossCinematics : public AActor, public IInteractableInterface
{
	GENERATED_BODY()

public:
	AStage3BossCinematics();

	UFUNCTION(BlueprintCallable, Category = "Stage 3")
	bool StartPhaseOneCut(APlayerCharacter* Player);

	bool TrySkipActiveCut(AActor* InstigatorActor);
	bool CanStartEncounter() const { return bStartingCombat; }
	void ResetStaging();
	void OpenExit();
	const FStage3CinematicPhase* FindPhase(FName Phase) const { return Phases.Find(Phase); }
	void BeginPhaseCut(FName Phase, ABossCharacter* Boss);
	void FinishPhaseCut(FName Phase, ABossCharacter* Boss);
	bool HasEnteredArena(FName Phase) const;
	void ActivatePhase(FName Phase, ABossCharacter* Boss);
	void BindSequence(ULevelSequence* Sequence, ALevelSequenceActor* Actor, ABossCharacter* Boss) const;

private:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Interact_Implementation(AActor* InstigatorActor) override;
	virtual bool CanInteract_Implementation(AActor* InstigatorActor) const override;
	virtual FVector GetInteractFocusLocation_Implementation(AActor* InstigatorActor) const override;
	virtual FInteractPromptInfo GetInteractPrompt_Implementation(AActor* InstigatorActor) const override;

	void Play(ULevelSequence* Sequence);
	UFUNCTION()
	void FinishOpeningCut();
	void StopOpeningCut();
	void SetDoor(int32 DoorIndex, bool bOpen, float Duration);
	void AddTrail(const FStage3CinematicPhase& Phase, ABossCharacter* Boss);
	void SetEyeGlow(ABossCharacter* Boss, FLinearColor Color);
	int32 FindNearestDoor(const AActor* Player) const;
	FVector GetDoorInteractionLocation(int32 DoorIndex) const;
	APlayerCharacter* GetPlayer() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> InteractionBounds;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<ABossEncounterManager> Encounter;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<UItemData> GateKey;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<AActor> GateLock;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<ULevelSequence> ArrivalSequence;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<ULevelSequence> UnlockSequence;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TArray<FStage3CinematicDoor> Doors;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TMap<FName, FStage3CinematicPhase> Phases;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<AActor> StairBlocker;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<UMaterialInterface> BloodMaterial;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<UMaterialInterface> EyeMaterial;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<UStaticMesh> EyeMesh;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<USoundBase> DoorSlam;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<USoundBase> LockedRattle;

	UPROPERTY(EditInstanceOnly, Category = "Stage 3")
	TObjectPtr<UNiagaraSystem> RoofEscapeEffect;

	struct FDoorMotion
	{
		TWeakObjectPtr<UStaticMeshComponent> Leaf;
		FRotator Closed = FRotator::ZeroRotator;
		FRotator From = FRotator::ZeroRotator;
		FRotator To = FRotator::ZeroRotator;
		float Time = 0.f;
		float Duration = 0.f;
	};
	TArray<TArray<FDoorMotion>> DoorMotion;
	TArray<bool> DoorIsOpen;
	TArray<TWeakObjectPtr<UDecalComponent>> TrailDecals;
	TWeakObjectPtr<ABossCharacter> EyeOwner;
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EyeInstance;
	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> OpeningActor;
	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> OpeningPlayer;

	TWeakObjectPtr<UCameraOcclusionComponent> OpeningOcclusion;

	enum class EOpening : uint8 { Initial, Locked, Unlocking, Courtyard, PhaseOne, Combat };
	EOpening Opening = EOpening::Initial;
	bool bStartingCombat = false;
	bool bGateUnlocked = false;
	bool bEntranceClosed = false;
	float WarningTime = 0.f;
};
