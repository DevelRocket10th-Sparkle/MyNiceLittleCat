/*
* 작성자: 전선현
* 수정 일자: 2026.11.16. (UEnemyPatternAbility 상속, 앵커를 컴포넌트 태그로 검색)
* 보스 양옆 앵커에 머니건 두 자루를 소환해 장전 연출 후 연사하는 원거리 패턴
* - 앵커: 보스 BP의 SceneComponent에 Component Tags로 LeftAnchorTag/RightAnchorTag를 붙여 지정
* - 총이 모두 끝나면 정상 종료, 하나라도 실패하거나 대상이 사라지면 취소
* 
*/

#pragma once

#include "CoreMinimal.h"
#include "Enemy/GAS/Abilities/EnemyPatternAbility.h"
#include "GA_Enemy_MoneyGun.generated.h"

class AMoneyGun;
class UMoneyGunPatternSettings;
class USceneComponent;

UCLASS()
class MYNICELITTLECAT_API UGA_Enemy_MoneyGun : public UEnemyPatternAbility
{
	GENERATED_BODY()

protected :

	/* UEnemyPatternAbility */

	// 설정 값, 앵커, 실행 제한 시간 검사
	virtual bool CanStartPattern() const override;

	// 총 소환, 회전 잠금, 진행 확인 타이머 시작
	virtual bool StartPattern() override;

	// 타이머 정리, 총 제거 (취소면 발사된 탄환도 제거)
	virtual void StopPattern(bool bWasCancelled) override;

private :

	// 태그로 앵커 컴포넌트 검색
	USceneComponent* FindAnchor(FName AnchorTag) const;

	// 앵커에 총 하나 소환 후 패턴 시작
	bool SpawnGun(USceneComponent* Anchor, float Damage);

	// 총 진행 상태 확인: 모두 끝나면 완료, 실패하면 취소
	void UpdatePattern();

private :

	// 이 GA만 해석하는 실행 설정
	UPROPERTY(Transient)
	TObjectPtr<UMoneyGunPatternSettings> Settings;

	// 소환한 총
	UPROPERTY(Transient)
	TArray<TObjectPtr<AMoneyGun>> Guns;

	// 진행 확인 타이머
	FTimerHandle UpdateTimer;

	// 진행 확인 간격
	UPROPERTY(EditDefaultsOnly, Category = "MoneyGun", meta = (ClampMin = "0.01", Units = "s"))
	float UpdateInterval = 0.05f;

};