// 작성자: 우진
// 작성일자: 2026-10-16
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Pet/PetTypes.h"
#include "PetModeDialComponent.generated.h"


// 펫의 모드를 선택할 수 있는 Dial UI 관련 컴포넌트
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYNICELITTLECAT_API UPetModeDialComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPetModeDialComponent();
	// ========================================================================================
public:
	bool OpenDial(const FVector2D& _centerInViewport);
	void CloseDial();
	bool IsDialOpen() const { return bDialOpen; }

	void UpdatePointerPosition(const FVector2D& _pointerInViewport);
	bool TryGetSelectedMode(EPetMode& _outMode) const;

	// 다이얼 UI 크기 반환하는 함수
	float GetDialDiameter() const;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// ========================================================================================
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Dial")
	TSubclassOf<class UPetModeDialWidget> DialWidgetClass;

private:
	// 실제 생성된 다이얼 위젯
	UPROPERTY(Transient)
	TObjectPtr<UPetModeDialWidget> DialWidget;

	// 다이얼이 열렸나요?
	bool bDialOpen = false;

	// 다이얼 UI의 센터 위치
	FVector2D DialCenter = FVector2D::ZeroVector;
};
