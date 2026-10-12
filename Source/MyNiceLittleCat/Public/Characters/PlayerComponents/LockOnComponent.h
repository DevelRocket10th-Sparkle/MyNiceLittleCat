// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockOnComponent.generated.h"

class AActor;
class UUserWidget;
class UInterface;
class USpringArmComponent;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYNICELITTLECAT_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	ULockOnComponent();

	// 락온 토글
	void ToggleTargetLock();

	// 마우스 휠로 타겟 변경: 양수 = 다음, 음수 = 이전
	void SwitchTarget(float Direction);

	__forceinline AActor* GetCurrentCandidate() const { return CurrentCandidate.Get(); }
	__forceinline AActor* GetLockedTarget() const { return LockedTarget.Get(); }

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 후보 탐색 간격
	UPROPERTY(EditDefaultsOnly, Category = "Targeting", meta = (ClampMin = "0.03"))
	float ScanInterval = 0.1f;

private:
	void RefreshCandidates();

	bool GetTargetScreenPosition(
		AActor* Target,
		FVector2D& OutScreenPosition
	) const;

	bool IsValidCandidate(AActor* Target) const;
	void UpdateLockOnCamera();
	void UpdateReticle();

protected:
	TArray<TWeakObjectPtr<AActor>> Candidates;

	TWeakObjectPtr<AActor> CurrentCandidate;
	TWeakObjectPtr<AActor> LockedTarget;

	FTimerHandle ScanTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Camera", meta = (ClampMin = "0.1"))
	float CameraFocusInterpSpeed = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Camera", meta = (ClampMin = "0.0"))
	float MaxExtraArmLength = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Camera", meta = (ClampMin = "0.0"))
	float ZoomDistanceThreshold = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Targeting|Camera", meta = (ClampMin = "0.0"))
	float ZoomDistanceScale = 0.35f;

	FVector BaseCameraTargetOffset = FVector::ZeroVector;
	float BaseCameraArmLength = 0.0f;

	FTimerHandle CameraFocusTimerHandle;

	
		
};
