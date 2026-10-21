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
	// ========================================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<class UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> ReflectorMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflector", meta = (ClampMin = "0.1", Units = "s"))
	float ReflectorLifeSpan = 10.f;
};
