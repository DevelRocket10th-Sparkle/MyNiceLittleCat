// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.27.
* 수정 일자: 2026.11.17. (주석 정리, 비행/충돌 연출 슬롯 추가)
* 수정 일자: 2026.11.20. (발사한 보스가 움직일 때 자기 탄환에 막히지 않게 함)
* MoneyGun이 쏘는 지폐 탄환
* - 충돌은 ProjectileMovement 직선 궤도, 메시만 Roll 흔들림 (Tick)
* - 첫 유효 충돌에서 1회만 피해를 주고 제거
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Frameworks/Feedback/FeedbackCue.h"
#include "MoneyProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS(Blueprintable)
class MYNICELITTLECAT_API AMoneyProjectile : public AActor
{
	GENERATED_BODY()

public :

	AMoneyProjectile();

	// 발사: 방향, 속도, 1회 피해량, 수명 설정 후 이동 시작
	void Launch(const FVector& Direction, float Speed, float Damage, float LifeSeconds);

	// Getter: 지폐 메시 (MoneyGun이 장전 연출 메시의 원본으로 사용)
	UStaticMeshComponent* GetMoneyMesh() const { return Mesh; }

	// 메시 흔들림 갱신
	virtual void Tick(float DeltaSeconds) override;

	// 발사한 보스의 이동 무시 목록에서 이 탄환 제거
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected :

	/* 컴포넌트 */

	// 충돌 판정 (루트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<USphereComponent> Collision;

	// 지폐 메시: 흔들림은 이 메시에만 적용
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="MoneyProjectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	/* 흔들림: 시각 효과만 회전하며 충돌은 직선 궤도를 따름 (재질의 정점 변위로 휘어짐 추가 가능) */

	// 최대 Roll 각도
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Flutter", meta=(ClampMin="0"))
	float FlutterDegrees = 12.f;

	// 초당 흔들림 횟수
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Flutter", meta=(ClampMin="0"))
	float FlutterFrequency = 6.f;

	/* 연출 */

	// 발사 후 날아가는 동안 메시에 붙어 반복 재생 (반짝이 꼬리, 펄럭이는 소리): 탄환이 사라지면 함께 제거
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Feedback")
	FFeedbackCue TrailCue;

	// 무언가에 맞아 사라지는 순간 충돌 지점에서 재생 (플레이어 피격 여부와 무관)
	UPROPERTY(EditDefaultsOnly, Category="MoneyProjectile|Feedback")
	FFeedbackCue ImpactCue;

	// 충돌: 발사자는 무시하고, 첫 유효 충돌에만 피해를 준 뒤 제거
	UFUNCTION()
	void HandleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

private :

	// 1회 피해량
	float HitDamage = 0.f;

	// 발사 후 경과 시간과 탄환별 흔들림 시작 위상 (탄환끼리 같은 박자로 흔들리지 않게)
	float Age = 0.f;
	float FlutterPhase = 0.f;

	// 흔들림 기준 회전
	FRotator InitialMeshRotation;

	// 발사됐는지, 이미 피해를 줬는지 (중복 충돌 방지)
	bool bLaunched = false;
	bool bConsumed = false;
};
