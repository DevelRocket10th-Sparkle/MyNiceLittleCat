#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MansionLightingSubsystem.generated.h"

// Stage3의 시간대는 현재 월드에 유지하고, 체크포인트에는 복귀할 시간대를 따로 저장한다.
UCLASS()
class MYNICELITTLECAT_API UMansionLightingSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Stage3|Lighting")
	void InitializeForStage();

	UFUNCTION(BlueprintCallable, Category = "Stage3|Lighting")
	void SetEvening(bool bNewEvening);

	UFUNCTION(BlueprintPure, Category = "Stage3|Lighting")
	bool IsEvening() const { return bEvening; }

	UFUNCTION(BlueprintCallable, Category = "Stage3|Lighting")
	void RestoreCheckpointLighting();

private:
	bool bConfigured = false;
	bool bEvening = false;
};
