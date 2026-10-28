/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 보스 패턴의 자성 코인입니다.
/// 물리 메시 + UMagneticComponent로 구성되어 펫 자석 능력에 끌리거나 밀립니다.
/// 상태: Warning(예고) → Falling(낙하) → Landed(착지) ⇄ Held(잡힘) → Thrown(던져짐) → Landed
/// - Warning: StartFall 시 WarningTime > 0이면 착지 지점에 경고 데칼(WarningDecalClass)을 표시하고 그 자리에서 대기합니다.
///            데칼 진행도는 예고 시작부터 착지까지 0 → 1로 차오르고, 착지(또는 코인 소멸) 시 사라집니다.
/// - Falling: 물리·자력 없이 착지 지점까지 직선으로 떨어지고, 극성 색은 표시합니다.
///            떨어지는 코인에 플레이어(FallDamageTargetClasses)가 닿으면 낙하 피해를 한 번 줍니다.
///            플레이어는 ApplyPlayerHit 직접 호출(피해·경직·무적), 그 외 액터는 ApplyDamage(UDamageType_MagneticCoinFall).
/// - Landed:  물리·자력을 켭니다. 자력을 받은 직후(MagneticForceGraceTime 이내) MinImpactSpeed 이상으로
///            DamageTargetClasses(기본 ABarrierGenerator)에 부딪히면 UDamageType_MagneticCoin 피해를 줍니다.
/// - Held:    착지한 코인의 물리를 외부(펫 자석 능력의 잡기)가 끄면 잡힌 것으로 봅니다.
/// - Thrown:  잡힌 코인의 물리가 다시 켜지면(놓거나 던짐) ThrowGraceTime 동안 던져진 상태로,
///            이 동안 MinImpactSpeed 이상으로 부딪히면 자력 조건 없이 피해를 줍니다. 시간이 지나면 Landed로 돌아갑니다.
///            펫 코드를 호출하지 않고 물리 상태 변화만으로 판단하므로 펫 능력 쪽 변경이 필요 없습니다.
/// 레벨에 직접 배치하면 bFallOnBeginPlay가 false일 때 바로 Landed로 시작합니다.
/// TODO(합의): 보스 패턴이 스폰 후 StartFall(극성)을 호출하는 방식은 보스 담당과 확정 필요
/// TODO: 소멸 상태는 다음 단계에서 추가

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticCoin.generated.h"

class UStaticMeshComponent;
class UMagneticComponent;
class UDamageType;
class AMagneticCoin;
class AAttackWarningDecal;

UENUM(BlueprintType)
enum class EMagneticCoinState : uint8
{
	// 하늘에서 떨어지는 중 (자력 무시, 플레이어 피격)
	Falling	UMETA(DisplayName = "Falling"),
	// 바닥에 착지 (물리·자력 활성)
	Landed	UMETA(DisplayName = "Landed"),
	// 펫 자석 능력 등에 잡혀 물리가 꺼진 상태
	Held	UMETA(DisplayName = "Held"),
	// 놓이거나 던져진 직후 (ThrowGraceTime 동안 충돌 피해 유효)
	Thrown	UMETA(DisplayName = "Thrown"),
	// 떨어지기 전 착지 지점에 경고를 표시하며 대기
	Warning	UMETA(DisplayName = "Warning"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagneticCoinDamaged, AMagneticCoin*, Coin, AActor*, DamagedActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMagneticCoinStateChanged, AMagneticCoin*, Coin, EMagneticCoinState, NewState);

UCLASS()
class MYNICELITTLECAT_API AMagneticCoin : public AActor
{
	GENERATED_BODY()

public:
	AMagneticCoin();

	virtual void Tick(float DeltaTime) override;

	// 낙하 시작: 극성 지정(None이면 유지) 후 현재 위치에서 아래로 트레이스한 바닥까지 떨어짐
	UFUNCTION(BlueprintCallable, Category = "MagneticCoin")
	void StartFall(EMagneticPolarity NewPolarity);

	// 낙하 시작: 지정한 바닥 지점(LandLocation)까지 떨어짐
	UFUNCTION(BlueprintCallable, Category = "MagneticCoin")
	void StartFallTo(EMagneticPolarity NewPolarity, const FVector& LandLocation);

	UFUNCTION(BlueprintCallable, Category = "MagneticCoin")
	void SetPolarity(EMagneticPolarity NewPolarity);

	UFUNCTION(BlueprintPure, Category = "MagneticCoin")
	EMagneticPolarity GetPolarity() const;

	UFUNCTION(BlueprintPure, Category = "MagneticCoin")
	EMagneticCoinState GetCoinState() const { return CoinState; }

	// 아직 착지 전인지 (예고 대기 또는 낙하 중)
	UFUNCTION(BlueprintPure, Category = "MagneticCoin")
	bool IsAirborne() const { return CoinState == EMagneticCoinState::Warning || CoinState == EMagneticCoinState::Falling; }

	// 낙하 진행도 (0 시작 ~ 1 착지, 예고 대기 중이면 0, 착지 후면 1)
	UFUNCTION(BlueprintPure, Category = "MagneticCoin")
	float GetFallProgress() const;

public:
	// 생성장치 등 대상이 실제로 피해를 받았을 때 브로드캐스트 (bDestroyOnDamage면 직후 소멸)
	UPROPERTY(BlueprintAssignable, Category = "MagneticCoin")
	FOnMagneticCoinDamaged OnCoinDamaged;

	// 낙하 중 플레이어에게 낙하 피해를 줬을 때 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "MagneticCoin")
	FOnMagneticCoinDamaged OnCoinFallHit;

	UPROPERTY(BlueprintAssignable, Category = "MagneticCoin")
	FOnMagneticCoinStateChanged OnCoinStateChanged;

protected:
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 예고 대기가 끝나면 실제 낙하 시작
	void BeginFalling();

	void ShowLandingWarning(const FVector& LandLocation);

	void HideLandingWarning();

	void TickFalling(float DeltaTime);

	// 착지 후 물리 상태 변화로 잡힘·던져짐 판단
	void UpdateHeldState();

	void Land();

	void SetCoinState(EMagneticCoinState NewState);

	UFUNCTION()
	void HandleMeshHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void HandleMeshBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleMagneticForce(UMagneticComponent* InMagneticComponent, FVector Force);

	// 착지 상태에서 이 대상에 충돌 피해를 줄 수 있는지 (대상 클래스·속도·자력 조건·쿨다운)
	bool CanDamageTarget(const AActor* OtherActor) const;

	// Classes가 비어 있으면 true, 아니면 그중 하나의 하위 클래스인지
	static bool IsActorOfClasses(const AActor* Actor, const TArray<TSubclassOf<AActor>>& Classes);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UMagneticComponent> MagneticComponent;

	// true면 BeginPlay에서 현재 극성으로 StartFall (레벨 배치 테스트용), false면 바로 Landed
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall")
	bool bFallOnBeginPlay = false;

	// 낙하 중 콜리전 프로파일 (Pawn과 겹침만, 나머지 무시). 착지하면 원래 프로파일로 복구
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall")
	FName FallingCollisionProfile = TEXT("OverlapOnlyPawn");

	// 낙하 속도 (cm/s)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall", meta = (ClampMin = "1.0"))
	float FallSpeed = 1200.f;

	// StartFall에서 바닥을 찾는 최대 트레이스 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall", meta = (ClampMin = "0.0"))
	float GroundTraceDistance = 5000.f;

	// 바닥으로 인식할 오브젝트 타입
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall")
	TArray<TEnumAsByte<EObjectTypeQuery>> GroundObjectTypes;

	// 낙하 중 닿은 대상에게 주는 피해량 (플레이어 기본 체력 3 기준 1)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall", meta = (ClampMin = "0.0"))
	float FallDamage = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall")
	TSubclassOf<UDamageType> FallDamageTypeClass;

	// 낙하 피해를 받을 클래스 (기본 APlayerCharacter, 비어 있으면 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Fall")
	TArray<TSubclassOf<AActor>> FallDamageTargetClasses;

	// 떨어지기 전 착지 지점 경고를 표시하며 대기하는 시간 (초, 0이면 바로 낙하)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning", meta = (ClampMin = "0.0"))
	float WarningTime = 1.f;

	// 착지 지점에 표시할 경고 데칼 (비어 있으면 표시 없이 대기만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning")
	TSubclassOf<AAttackWarningDecal> WarningDecalClass;

	// 경고 반경 = 코인 메시 바닥 반경 × WarningRadiusScale + WarningRadiusPadding
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning", meta = (ClampMin = "0.1"))
	float WarningRadiusScale = 1.f;

	// 경고 반경에 더하는 여유 (cm). 플레이어 캡슐 반경만큼 더하면 플레이어 중심이 원 안에 있을 때 맞는 범위가 됨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning", meta = (ClampMin = "0.0"))
	float WarningRadiusPadding = 35.f;

	// true면 경고 색을 코인 극성 색으로, false면 WarningColor
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning")
	bool bUsePolarityColorForWarning = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning", meta = (EditCondition = "!bUsePolarityColorForWarning"))
	FLinearColor WarningColor = FLinearColor(1.f, 0.2f, 0.f);

	// 예고 대기 중 코인을 숨김 (false면 공중에 떠 있는 모습이 보임)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Warning")
	bool bHideCoinDuringWarning = false;

	// 한 번 부딪힐 때 주는 피해량
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float Damage = 1.f;

	// 피해 타입 (생성장치 AllowedDamageTypes 판별용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	// 이 클래스(하위 포함) 액터에만 피해 (비어 있으면 모든 액터)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	TArray<TSubclassOf<AActor>> DamageTargetClasses;

	// 부딪힐 때 이 속도 이상이어야 피해 (굴러가다 살짝 닿는 경우 제외)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float MinImpactSpeed = 300.f;

	// true면 자력을 받은 직후 또는 던져진 직후(Thrown)에만 피해 (자석 능력으로 날린 코인만 유효)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	bool bRequireMagneticForce = true;

	// 마지막으로 자력을 받은 뒤 피해를 인정하는 시간 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0", EditCondition = "bRequireMagneticForce"))
	float MagneticForceGraceTime = 0.5f;

	// 잡혔다가 놓인 뒤 충돌 피해를 인정하는 시간 (초). 던져서 날아가는 시간보다 길게
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float ThrowGraceTime = 1.5f;

	// 피해 후 다시 피해를 줄 수 있을 때까지의 시간 (한 번 부딪힐 때 히트 이벤트가 여러 번 오는 것 방지)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage", meta = (ClampMin = "0.0"))
	float DamageCooldown = 0.5f;

	// 피해를 주면(대상이 실제로 피해를 받으면) 코인 소멸
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MagneticCoin|Damage")
	bool bDestroyOnDamage = true;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "MagneticCoin")
	EMagneticCoinState CoinState = EMagneticCoinState::Landed;

	// 착지 시 복구할 콜리전 프로파일
	FName LandedCollisionProfile;

	FVector FallStartLocation = FVector::ZeroVector;

	// 착지 시 코인 중심 위치
	FVector FallTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	TObjectPtr<AAttackWarningDecal> LandingWarning;

	// 예고 대기 시작 시각
	float WarningStartTime = -1.f;

	// 이번 낙하에서 이미 피해를 준 대상 (한 번만 피해)
	TArray<TWeakObjectPtr<AActor>> FallHitActors;

	// 히트 이벤트 시점엔 충돌로 감속돼 있을 수 있어 이전 틱 속도도 함께 사용
	float PreviousSpeed = 0.f;

	float LastMagneticForceTime = -1.f;

	// Thrown 상태가 된 시각
	float ThrownTime = -1.f;

	float LastDamageTime = -1.f;
};
