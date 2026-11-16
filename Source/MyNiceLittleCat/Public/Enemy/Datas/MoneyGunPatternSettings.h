/*
* 작성자: 전선현
* 수정 일자: 2026.11.13. (이전 형식 이전용 필드 제거, 앵커를 컴포넌트 태그로 변경)
* GA_Enemy_MoneyGun 전용 실행 설정 (패턴 데이터 에셋 안에서 인라인 생성)
* - 메시와 재질은 총/탄환 BP에서 지정
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "MoneyGunPatternSettings.generated.h"

class AMoneyGun;
class AMoneyProjectile;
class USoundBase;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Money Gun Pattern Settings"))
class MYNICELITTLECAT_API UMoneyGunPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()

public:

	// 모든 값이 실행 가능한 범위인지 검사
	bool IsValidConfiguration() const;

	// 등장 + 장전 + 발사 준비 + 연사 + 퇴장 전체 시간: 패턴 MaxExecutionSeconds보다 짧아야 함
	float GetExecutionSeconds() const;

public :
	
	/* 사용 클래스 */

	// 소환할 총과 탄환 BP
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MoneyGun")
	TSubclassOf<AMoneyGun> GunClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "MoneyGun")
	TSubclassOf<AMoneyProjectile> ProjectileClass;

	/* 총을 붙일 앵커: 보스 BP의 SceneComponent > Component Tags에 같은 태그 추가 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FName LeftAnchorTag = TEXT("MoneyGunAnchor_L");
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attachment")
	FName RightAnchorTag = TEXT("MoneyGunAnchor_R");

	/* 장전 */

	// 총 하나에 장전하고 발사할 탄환 수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "1", ClampMax = "256"))
	int32 ProjectileCount = 12;

	// 등장과 마지막 발사 후 퇴장에 같은 시간을 사용하여 서로 반대인 스케일 연출
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0", Units = "s"))
	float AppearSeconds = 0.25f;

	// 탄환이 주변에서 총으로 모이는 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0.05", Units = "s"))
	float LoadSeconds = 0.9f;
	
	// 장전 연출 시작 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "1", Units = "cm"))
	float LoadRadius = 140.f;
	
	// 장전 완료 후 첫 발사까지 대기
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0", Units = "s"))
	float PreFireSeconds = 0.2f;

	/* 소리 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> LoadedSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> ShotSound;

	/* 발사 */

	// 연사 간격
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0.02", Units = "s"))
	float ShotInterval = 0.1f;

	// 탄속
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1", Units = "cm/s"))
	float ProjectileSpeed = 1200.f;

	// 탄환 수명
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0.1", Units = "s"))
	float ProjectileLifeSeconds = 4.f;

	/* 조준 */

	// 총구 회전 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aiming", meta = (ClampMin = "1", Units = "deg/s"))
	float AimDegreesPerSecond = 100.f;

	// 조준점 보정: 높은 부착 지점에서도 아래쪽 플레이어를 맞힐 수 있도록 높이 차이 포함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aiming")
	FVector TargetOffset = FVector::ZeroVector;
	
};
