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

	// 컷신 등 외부 연출이 펫을 직접 움직이는 동안 StateTree와 이동 요청 중지 (다시 켤 때 StartPetLogic)
	UFUNCTION(BlueprintCallable, Category = "Pet|AI")
	void StopPetLogic();

private:
	void TryStartPetLogic();
	// =============================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pet|AI")
	TObjectPtr<class UStateTreeAIComponent> StateTreeComponent;
};
