#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MoneyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS(Blueprintable)
class MYNICELITTLECAT_API AMoneyProjectile : public AActor
{
	GENERATED_BODY()
public:
	AMoneyProjectile();
	void Launch(const FVector& Direction, float Speed, float Damage, float LifeSeconds);
	UStaticMeshComponent* GetMoneyMesh() const { return Mesh; }
	virtual void Tick(float DeltaSeconds) override;
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<USphereComponent> Collision;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<UProjectileMovementComponent> Movement;
	// 시각 효과만 회전하며 충돌은 직선 궤도를 따른다. 재질의 정점 변위로 휘어짐을 추가할 수 있다.
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Flutter", meta=(ClampMin="0"))
	float FlutterDegrees = 12.f;
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Flutter", meta=(ClampMin="0"))
	float FlutterFrequency = 6.f;
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
private:
	float HitDamage = 0.f;
	float Age = 0.f;
	float FlutterPhase = 0.f;
	FRotator InitialMeshRotation;
	bool bLaunched = false;
	bool bConsumed = false;
};
