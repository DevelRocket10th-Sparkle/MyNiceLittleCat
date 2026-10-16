// 작성자: 우진
// 작성일자: 2026-10-16
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerMainController.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API APlayerMainController : public APlayerController
{
	GENERATED_BODY()
	
public:
	APlayerMainController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// ========================================================================================
private:
	// 펫 입력 관련 콜백 함수들
	void HandleDialStarted();
	void HandleDialTriggered();
	void HandleDialCompleted();
	void HandleDialCanceled();

	// 마우스 위치를 다이얼 UI와 같은 좌표 단위로 가져오는 함수 
	bool TryGetDialPointerPosition(FVector2D& _outPosition) const;
	// ========================================================================================
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|UI")
	TObjectPtr<class UPetModeDialComponent> PetModeDialComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputMappingContext> IMC_Pet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetModeDial;
};
