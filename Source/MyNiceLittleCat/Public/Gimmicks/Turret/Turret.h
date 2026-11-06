/// 자성 포탑입니다.
/// UMagneticComponent가 붙은 물리 물체라서 다른 자성 물체처럼 펫 자석(우클릭)으로 끌어와 펫의 CarryPoint에 붙습니다.
/// 잡히기 전엔 적대 상태: Head가 플레이어를 쫓고 자동 사격(탄은 적을 통과).
/// 펫이 한 번 잡으면 아군으로 바뀌고, 들고 있는 동안(APetCharacter::GetCarriedActor() == 이 포탑) 포구가 펫이 향하는 방향을 따라가고,
/// 플레이어가 FireKey(기본 E)를 누르면 Muzzle 방향으로 탄(ATurretProjectile)을 발사합니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Turret.generated.h"

class UStaticMeshComponent;
class UMagneticComponent;
class USceneComponent;
class ATurretProjectile;
class APetCharacter;
class USoundBase;
class UGameplayEffect;
class UNiagaraSystem;

UCLASS()
class MYNICELITTLECAT_API ATurret : public AActor
{
	GENERATED_BODY()

public:
	ATurret();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(BlueprintPure, Category = "Turret")
	bool IsCarried() const { return bCarried; }

	bool IsBeingPulled() const { return !bCarried && !GetActorEnableCollision(); }
protected:
	virtual void BeginPlay() override;

	void SetCarried(bool bNewCarried);
	void Fire();
	void TickHostile(float DeltaTime);
	void SpawnProjectile(bool bHostileShot);

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Land();
protected:
	UPROPERTY(EditAnywhere, Category = "Turret")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 루트 (물리 + 펫이 들고 다니는 몸체). 메시 정면(+X)이 포구 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMagneticComponent> Magnetic;

	// 회전 축(피벗). 이 컴포넌트가 Yaw로 돌고, HeadMesh·Muzzle은 여기에 붙음
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Head;

	// 포신 메시. BP에서 위치·회전을 조절해 Head 피벗 중심에 오고 정면이 +X(빨간 화살표)를 보게 맞춤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	// 탄 발사 위치·방향 (포구 끝에 배치)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret")
	TSubclassOf<ATurretProjectile> ProjectileClass;

	// 펫이 들고 있을 때 발사 키
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret")
	FKey FireKey = EKeys::E;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret", meta = (ClampMin = "0.05"))
	float FireCooldown = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Turret")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditAnywhere, Category = "Turret|FX")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditAnywhere, Category = "Turret|FX")
	FVector MuzzleFlashScale = FVector(1.f);

	// 적대 상태: 플레이어를 조준·자동 사격. 플레이어가 한 번 잡으면 아군으로 바뀜
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret|Hostile")
	bool bHostile = true;

	UPROPERTY(EditAnywhere, Category = "Turret|Hostile")
	float DetectRange = 1500.f;

	UPROPERTY(EditAnywhere, Category = "Turret|Hostile")
	float HeadTurnSpeed = 5.f;

	UPROPERTY(EditAnywhere, Category = "Turret|Hostile")
	float HostileFireInterval = 1.5f;

	// 적대 상태 발사 간격 랜덤 범위
	UPROPERTY(EditAnywhere, Category = "Turret|Hostile", meta = (ClampMin = "0.05", Units = "s"))
	float HostileFireIntervalMin = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Turret|Hostile", meta = (ClampMin = "0.05", Units = "s"))
	float HostileFireIntervalMax = 1.4f;

	// 이 각도 안으로 조준됐을 때만 발사
	UPROPERTY(EditAnywhere, Category = "Turret|Hostile")
	float AimToleranceDeg = 10.f;

	UPROPERTY(EditAnywhere, Category = "Turret", meta = (ClampMin = "0.0"))
	float PlayerSpreadDeg = 6.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Turret", meta = (ClampMin = "1"))
	int32 MaxAmmo = 30;

	UPROPERTY(EditAnywhere, Category = "Turret|Hostile", meta = (ClampMin = "0.0"))
	float ArmDelay = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Turret|Jelly")
	float JellyAmount = 0.35f;

	UPROPERTY(EditAnywhere, Category = "Turret|Jelly")
	float JellyFrequency = 30.f;

	UPROPERTY(EditAnywhere, Category = "Turret|Jelly")
	float JellyDamping = 9.f;

private:
	bool bCarried = false;
	bool bFireKeyBound = false;
	bool bLanded = false;
	float LandedTime = -1.f;
	float LastFireTime = -1000.f;
	float NextHostileFireTime = 0.f;
	float JellyElapsed = -1.f;
	int32 CurrentAmmo = 0;
	FVector HeadBaseScale = FVector::OneVector;
	TWeakObjectPtr<APetCharacter> CachedPet;
};
