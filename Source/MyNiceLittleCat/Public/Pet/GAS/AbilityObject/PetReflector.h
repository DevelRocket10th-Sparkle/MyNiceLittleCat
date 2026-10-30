// 작성자: 우진
// 작성일자: 2026-10-19

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ReflectiveSurfaceInterface.h"
#include "PetReflector.generated.h"

UCLASS()
class MYNICELITTLECAT_API APetReflector : public AActor, public IReflectiveSurfaceInterface
{
	GENERATED_BODY()
	
public:	
	APetReflector();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// ========================================================================================

public:
	float GetHalfWidth() const;
	FVector GetPlacementHalfExtents() const;
	virtual bool TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const override;

	// [추가: 최요환 2026-10-27] 반사판 파괴형 레이저(보스 격자 레이저)에 맞으면 즉시 파괴 (IReflectiveSurfaceInterface::BreakByLaser 참고)
	// 설치 목록(UGameplayAbility_PetReflect::PlacedReflectors)은 파괴된 반사판을 자동으로 정리하므로 따로 알릴 필요 없음
	virtual bool BreakByLaser_Implementation(AActor* LaserInstigator) override;
	// ========================================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<class UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> ReflectorMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflector", meta = (ClampMin = "0.1", Units = "s"))
	float ReflectorLifeSpan = 6.f;
};
