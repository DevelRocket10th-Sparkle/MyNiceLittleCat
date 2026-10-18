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

public:
	// 플레이어가 현재 누르고 있는 모든 키의 입력을 초기화하는 함수
	virtual void FlushPressedKeys() override;

	// 게임 화면 밖일 때 현재 게임 입력 처리를 초기화 할 지 정해 반환하는 함수
	virtual bool ShouldFlushKeysWhenViewportFocusChanges() const override;

	// PlayerController의 Tick 함수
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// ========================================================================================

public:
	void SetPartnerPet(class APetCharacter* _pet);

private:
	// 펫 입력 관련 콜백 함수들
	void HandleDialStarted();
	void HandleDialTriggered();
	void HandleDialCompleted();
	void HandleDialCanceled();
	void HandleModeUseStarted();
	void HandleModeUseReleased();
	void HandlePetSpecialAction();

	// 마우스 위치를 다이얼 UI와 같은 좌표 단위로 가져오는 함수 
	bool TryGetDialPointerPosition(FVector2D& _outPosition) const;

	// 다이얼 UI가 게임 화면 바깥으로 벗어나려고 하는지 체크하는 함수
	bool TryClampDialCenterToViewport(FVector2D& _outPosition) const;

	bool TryGetPetAimDirection(FVector& _outDirection) const;
	// ========================================================================================
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|UI")
	TObjectPtr<class UPetModeDialComponent> PetModeDialComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputMappingContext> IMC_Pet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetModeDial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetModeUse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetSpecialAction;

	UPROPERTY(Transient)
	TWeakObjectPtr<class APetCharacter> PartnerPet;
};
