// 발소리 컴포넌트입니다. (동구 요청으로 추가)
// 애니메이션에 노티파이를 박지 않고, 땅 위에서 이동한 거리만큼 걸음 소리를 냅니다.
// - 속도에 따라 보폭이 바뀜 (걷기: 길고 조용히, 달리기: 짧고 크게)
// - 같은 소리가 연달아 나지 않게 랜덤 선택 + 볼륨·피치 랜덤
// - 발밑 피직스 머티리얼의 Surface Type별로 다른 소리 (없으면 DefaultSounds)
// - 착지할 때 한 번 더 크게
// 캐릭터 BP에 컴포넌트로 추가하고 DefaultSounds에 사운드를 넣으면 동작합니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Chaos/ChaosEngineInterface.h"
#include "FootstepComponent.generated.h"

class USoundBase;
class USoundAttenuation;
class ACharacter;

USTRUCT(BlueprintType)
struct FFootstepSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep")
	TArray<TObjectPtr<USoundBase>> Sounds;

	// 이 바닥에서의 볼륨 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep", meta = (ClampMin = "0"))
	float VolumeScale = 1.f;
};

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UFootstepComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFootstepComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 강제로 발소리 한 번 (애니메이션 노티파이 등에서 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "Footstep")
	void PlayFootstep(float VolumeMultiplier = 1.f);

protected:
	virtual void BeginPlay() override;

	const FFootstepSoundSet& FindSoundSet(FVector& OutLocation) const;

protected:
	// 바닥 종류를 모를 때 쓰는 소리들 (여러 개 넣을수록 자연스러움)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound")
	FFootstepSoundSet DefaultSounds;

	// 피직스 머티리얼의 Surface Type별 소리 (선택)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound")
	TMap<TEnumAsByte<EPhysicalSurface>, FFootstepSoundSet> SurfaceSounds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound")
	TObjectPtr<USoundAttenuation> Attenuation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "0"))
	FVector2D VolumeRange = FVector2D(0.45f, 0.6f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Sound", meta = (ClampMin = "0.1"))
	FVector2D PitchRange = FVector2D(1.05f, 1.25f);

	/* 보폭 */

	// 이 속도보다 느리면 소리 없음
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (ClampMin = "0", Units = "cm/s"))
	float MinSpeed = 40.f;

	// 걷기 속도와 그때 보폭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (Units = "cm/s"))
	float WalkSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (ClampMin = "10", Units = "cm"))
	float WalkStride = 55.f;

	// 전력 질주 속도와 그때 보폭
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (Units = "cm/s"))
	float SprintSpeed = 700.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (ClampMin = "10", Units = "cm"))
	float SprintStride = 110.f;

	// 빠를수록 소리를 키우는 배율 (걷기 → 전력 질주)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Stride", meta = (ClampMin = "0"))
	FVector2D SpeedVolumeScale = FVector2D(0.8f, 1.2f);

	/* 착지 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Landing")
	bool bPlayOnLanding = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Landing", meta = (ClampMin = "0"))
	float LandingVolumeScale = 1.6f;

	// 이 시간 이상 떠 있다가 내려와야 착지 소리 (계단 턱 등 무시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Footstep|Landing", meta = (ClampMin = "0", Units = "s"))
	float MinAirTime = 0.2f;

private:
	TWeakObjectPtr<ACharacter> Character;
	float DistanceSinceStep = 0.f;
	float AirTime = 0.f;
	bool bWasFalling = false;
	int32 LastIndex = INDEX_NONE;
};
