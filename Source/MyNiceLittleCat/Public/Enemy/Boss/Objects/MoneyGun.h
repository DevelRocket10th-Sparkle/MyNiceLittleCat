// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.27.
* 수정 일자: 2026.11.17. (주석 정리, 등장/장전/발사/퇴장 연출 재생)
* 보스 MoneyGun 패턴이 소환하는 총 액터
* - 등장(크기 커짐) → 장전(지폐가 모여듦) → 발사(간격마다 탄환) → 사라짐 순서로 Tick에서 진행
* - 실행 GA는 HasFinished/HasFailed로 결과만 확인
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemy/Combat/EnemyFeedback.h"
#include "MoneyGun.generated.h"

class UMoneyGunPatternSettings;
class UStaticMeshComponent;
class USceneComponent;
class AMoneyProjectile;

UCLASS(Blueprintable)
class MYNICELITTLECAT_API AMoneyGun : public AActor
{
	GENERATED_BODY()

public :

	AMoneyGun();

	// 패턴 시작: 설정, 조준 대상, 탄환 1발 피해량 저장 후 대상 방향으로 회전
	// - 설정이 잘못됐거나 대상이 없거나 이미 시작했으면 false
	bool StartPattern(UMoneyGunPatternSettings* InSettings, AActor* InTarget, float InDamage);

	// Getter: 모든 탄환을 쏘고 사라지는 연출까지 끝났는지
	bool HasFinished() const { return bFinished; }

	// Getter: 대상 소실, 장전 연출 생성 실패, 탄환 생성 실패로 중단됐는지
	bool HasFailed() const { return bFailed; }

	// 이 총이 쏜 탄환 중 남아 있는 것을 모두 제거 (패턴 취소, 보스방 초기화)
	void ClearProjectiles();

	// 조준 → 등장/장전 → 발사 → 사라짐 순서로 진행
	virtual void Tick(float DeltaSeconds) override;

protected :

	// 장전 연출 메시 정리
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	/* 컴포넌트 */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> Root;

	// 총 메시: 등장/사라짐 때 크기가 변함
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<UStaticMeshComponent> Mesh;

	// 탄환 생성 위치와 발사 방향
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> MuzzlePoint;

	// 장전 연출에서 지폐가 모여드는 위치
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyGun")
	TObjectPtr<USceneComponent> LoadPoint;

	// 탄환 1발 발사 직후 호출: BP에서 반동, 이펙트 연결
	UFUNCTION(BlueprintImplementableEvent, Category="MoneyGun")
	void OnShot();

private :

	/* 단계별 갱신 (Tick에서 순서대로 호출) */

	// 대상 방향으로 회전
	void UpdateAim(float DeltaSeconds);

	// 등장 크기 보간과 장전 연출: 장전이 끝나면 true
	bool UpdateLoading(float DeltaSeconds);

	// 발사 간격마다 탄환 발사
	void UpdateFiring();

	// 마지막 발사 후 크기를 줄이고, 끝나면 bFinished
	void UpdateDisappearance();

	/* 장전 연출, 발사 */

	// 장전 연출용 지폐 메시를 주변 무작위 위치에 생성: 원본 메시가 없으면 false
	bool CreateLoadingVisuals();

	// 장전 연출 메시 제거
	void ClearLoadingVisuals();

	// 탄환 1발 생성 및 발사: 생성 실패 시 false
	bool Fire();

private :

	// 실행 중인 패턴 설정 (패턴 데이터의 MoneyGun 설정)
	UPROPERTY(Transient)
	TObjectPtr<UMoneyGunPatternSettings> Settings;

	// 장전 연출 메시와 각 메시의 시작 위치, 원래 크기 (같은 인덱스)
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> LoadingVisuals;
	TArray<FVector> LoadingStarts;
	TArray<FVector> LoadingScales;

	// 이 총이 쏜 탄환 (ClearProjectiles 대상)
	TArray<TWeakObjectPtr<AMoneyProjectile>> Projectiles;

	// 조준 대상
	TWeakObjectPtr<AActor> Target;

	// 배치된 총 메시 크기: 등장/사라짐 보간의 기준
	FVector GunScale = FVector::OneVector;

	// 탄환 1발 피해량
	float Damage = 0.f;

	// 패턴 시작 후 경과 시간: 단계 전환과 발사 시각 계산에 사용
	float Elapsed = 0.f;

	FEnemyFeedbackInstance LoadingFeedback;

	// 지금까지 쏜 탄환 수
	int32 ShotsFired = 0;

	/* 진행 상태 */

	bool bLoadingCreated = false;	// 장전 연출 메시를 만들었는지
	bool bLoadFinished = false;		// 장전이 끝났는지 (발사 단계 진입)
	bool bDisappearStarted = false;	// 퇴장 연출을 재생했는지
	bool bFinished = false;			// 정상 종료
	bool bFailed = false;			// 실패 종료
};
