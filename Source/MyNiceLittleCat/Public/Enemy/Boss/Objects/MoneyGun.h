#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MoneyGun.generated.h"

class UMoneyGunPatternSettings;
class UStaticMeshComponent;
class USceneComponent;
class AMoneyProjectile;

UCLASS(Blueprintable)
class MYNICELITTLECAT_API AMoneyGun : public AActor
{
	GENERATED_BODY()
public:
	AMoneyGun();
	bool StartPattern(UMoneyGunPatternSettings* InSettings, AActor* InTarget, float InDamage);
	bool HasFinished() const { return bFinished; }
	bool HasFailed() const { return bFailed; }
	void ClearProjectiles();
	virtual void Tick(float DeltaSeconds) override;
protected:
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<UStaticMeshComponent> Mesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> MuzzlePoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> LoadPoint;
	UFUNCTION(BlueprintImplementableEvent, Category="MoneyGun")
	void OnShot();
private:
	// 매 프레임 조준, 장전 연출, 발사 순서로 진행한다.
	void UpdateAim(float DeltaSeconds);
	bool UpdateLoading(float DeltaSeconds);
	void UpdateFiring();
	void UpdateDisappearance();

	bool CreateLoadingVisuals();
	void ClearLoadingVisuals();
	bool Fire();
	UPROPERTY(Transient)
	TObjectPtr<UMoneyGunPatternSettings> Settings;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LoadingVisuals;
	TArray<FVector> LoadingStarts;
	TArray<FVector> LoadingScales;
	TArray<TWeakObjectPtr<AMoneyProjectile>> Projectiles;
	TWeakObjectPtr<AActor> Target;
	FVector GunScale = FVector::OneVector;
	float Damage = 0.f;
	float Elapsed = 0.f;
	int32 ShotsFired = 0;
	bool bLoadingCreated = false;
	bool bLoadFinished = false;
	bool bFinished = false;
	bool bFailed = false;
};
