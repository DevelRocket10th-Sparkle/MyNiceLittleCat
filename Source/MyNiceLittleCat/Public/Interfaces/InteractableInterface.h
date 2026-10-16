/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 플레이어가 상호작용할 수 있는 액터가 구현하는 인터페이스입니다.
/// 감지(포커스 대상 선정, 입력 처리)는 플레이어 측 상호작용 컴포넌트가 담당하고,
/// 이 인터페이스는 감지된 액터가 어떻게 반응할지만 정의합니다.
/// GetInteractAlignment로 상호작용 전 캐릭터가 설 위치·바라볼 방향을 알려주면
/// 플레이어 측 상호작용 컴포넌트가 정렬을 마친 뒤 Interact를 호출합니다.
/// BP 구현체도 호출되도록 반드시 Implements<UInteractableInterface>() 확인 후
/// IInteractableInterface::Execute_XXX(Actor, ...) 형태로 호출해야 합니다.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// 상호작용 전 캐릭터 정렬 방식
UENUM(BlueprintType)
enum class EInteractAlignMode : uint8
{
	None		UMETA(DisplayName = "정렬 없음"),		// 그 자리에서 바로 실행
	FaceOnly	UMETA(DisplayName = "바라보기만"),		// 제자리에서 회전만
	SnapToPoint	UMETA(DisplayName = "지점으로 이동")	// 지정 위치로 이동 + 회전
};

// 상호작용 대상이 플레이어 측에 알려주는 정렬 목표
USTRUCT(BlueprintType)
struct FInteractAlignment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	EInteractAlignMode Mode = EInteractAlignMode::None;

	// 캐릭터가 설 위치 (XY만 사용, 높이는 캐릭터 현재 높이 유지). SnapToPoint에서만 사용
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FVector TargetLocation = FVector::ZeroVector;

	// 캐릭터가 바라볼 방향 (Yaw만 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FRotator TargetRotation = FRotator::ZeroRotator;
};

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

class MYNICELITTLECAT_API IInteractableInterface
{
	GENERATED_BODY()

public:
	// 상호작용 실행
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* InteractInstigator);

	// 현재 상호작용 가능 여부
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool CanInteract(AActor* InteractInstigator) const;

	// 상호작용 대상으로 포커스됨 (하이라이트, 안내 UI 등)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnFocusBegin(AActor* InteractInstigator);

	// 포커스 해제
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void OnFocusEnd(AActor* InteractInstigator);

	// 상호작용 실행 전 캐릭터 정렬 목표 (기본: 정렬 없음)
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FInteractAlignment GetInteractAlignment(AActor* InteractInstigator) const;
	virtual FInteractAlignment GetInteractAlignment_Implementation(AActor* InteractInstigator) const { return FInteractAlignment(); }
};
