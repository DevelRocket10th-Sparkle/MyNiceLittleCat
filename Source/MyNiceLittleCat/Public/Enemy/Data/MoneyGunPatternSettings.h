/*
* 작성자: 전선현
* 수정 일자: 2026.11.20. (장전 완료·발사의 소리와 이펙트를 LoadedCue, ShotCue로 통일)
* 수정 일자: 2026.11.20. (값 범위·실행 시간 검사를 부모 가상 함수로)
* GA_Enemy_MoneyGun 전용 실행 설정 (패턴 데이터 에셋 안에서 인라인 생성)
* - 메시와 재질은 총/탄환 BP에서 지정
*/

#pragma once

#include "CoreMinimal.h"
#include "Frameworks/Feedback/FeedbackCue.h"
#include "Enemy/Data/EnemyPatternExecutionSettings.h"
#include "MoneyGunPatternSettings.generated.h"

class AMoneyGun;
class AMoneyProjectile;
class UNiagaraSystem;
class USoundBase;

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Money Gun Pattern Settings"))
class MYNICELITTLECAT_API UMoneyGunPatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()

public:

	// 모든 값이 실행 가능한 범위인지 검사
	virtual bool IsValidConfiguration() const override;

	// 전체 시간 + 0.1초가 패턴 MaxExecutionSeconds보다 짧은지: 총 퇴장이 시간 초과보다 먼저 끝나야 함
	virtual bool FitsExecutionTime(float MaxExecutionSeconds) const override;

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

	/* 연출 (소리 + 이펙트) */

	// 총이 나타나기 시작할 때 총 위치에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FFeedbackCue AppearCue;

	// 지폐가 모여들기 시작할 때 LoadPoint에서 재생 (지폐 펄럭임 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FFeedbackCue LoadStartCue;

	// 장전 완료 순간 LoadPoint에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FFeedbackCue LoadedCue;

	// 발사 순간 총구(MuzzlePoint) 위치에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FFeedbackCue ShotCue;

	// 마지막 발사 후 총이 사라지기 시작할 때 총 위치에서 재생
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Feedback")
	FFeedbackCue DisappearCue;

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
