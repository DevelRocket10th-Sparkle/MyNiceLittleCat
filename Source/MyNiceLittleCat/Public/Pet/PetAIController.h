#pragma once
// 작성자: 우진
// 작성일자: 2026-10-13

#include "CoreMinimal.h"
#include "AIController.h"
#include "PetAIController.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API APetAIController : public AAIController
{
	GENERATED_BODY()
	
public:
	APetAIController();
	
protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	// =============================================================================
public:
	UFUNCTION(BlueprintCallable, Category = "Pet|AI")
	void StartPetLogic();

private:
	void TryStartPetLogic();
	// =============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|AI")
	TObjectPtr<class UStateTreeAIComponent> StateTreeComponent;
};
