#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "MoneyGunPatternSettings.generated.h"

class AMoneyGun;
class AMoneyProjectile;
class USoundBase;

// 패턴별 실행 설정이다. 메시와 재질은 총과 탄환 블루프린트에서 지정한다.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class MYNICELITTLECAT_API UMoneyGunPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()
public:
	UMoneyGunPatternSettings();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TSubclassOf<AMoneyGun> GunClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TSubclassOf<AMoneyProjectile> ProjectileClass;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment")
	FName LeftAnchorName = TEXT("MoneyGunAnchor_L");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attachment")
	FName RightAnchorName = TEXT("MoneyGunAnchor_R");
	// 총 하나에 장전하고 발사할 탄환 수이다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="1", ClampMax="256"))
	int32 ProjectileCount = 12;
	// 등장과 마지막 발사 후 퇴장에 같은 시간을 사용하여 서로 반대인 스케일 연출을 만든다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="0", Units="s"))
	float AppearSeconds = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="0.05", Units="s"))
	float LoadSeconds = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="1", Units="cm"))
	float LoadRadius = 140.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loading", meta=(ClampMin="0", Units="s"))
	float PreFireSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> LoadedSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sound")
	TObjectPtr<USoundBase> ShotSound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0.02", Units="s"))
	float ShotInterval = 0.1f;
	// 초속 12미터이면 8미터 도달에 약 0.67초가 걸린다. 전투 공간의 실제 거리에 맞춰 조정한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firing", meta=(ClampMin="1", Units="cm/s"))
	float ProjectileSpeed = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Firing", meta=(ClampMin="0.1", Units="s"))
	float ProjectileLifeSeconds = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming", meta=(ClampMin="1", Units="deg/s"))
	float AimDegreesPerSecond = 100.f;
	// 높은 부착 지점에서도 아래쪽 플레이어를 맞힐 수 있도록 높이 차이를 포함해 조준한다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Aiming")
	FVector TargetOffset = FVector::ZeroVector;
	// 이전 에셋의 값을 패턴 공통 이동 정책으로 이전할 때만 읽는다.
	UPROPERTY(BlueprintReadOnly)
	bool bChaseTarget = true;
	UPROPERTY(BlueprintReadOnly)
	float ChaseAcceptanceRadius = 650.f;

	bool IsValidConfiguration() const;
	float GetExecutionSeconds() const;
};
