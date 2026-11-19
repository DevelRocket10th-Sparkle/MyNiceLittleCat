// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Combat/EnemyTargetUtils.h"

#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

#include "Enemy/EnemyCharacter.h"
#include "Interfaces/TargetableInterface.h"

namespace
{
	// Enemy 편인지: Enemy 본인이거나 Enemy가 발사한 액터(Instigator가 Enemy)
	bool IsEnemySide(const AActor* Actor)
	{
		return IsValid(Actor) && (Actor->IsA<AEnemyCharacter>() || Cast<AEnemyCharacter>(Actor->GetInstigator()) != nullptr);
	}
}

bool EnemyTargetUtils::IsAttackableTarget(const AActor* Target)
{
	// 플레이어와 적 모두 ITargetableInterface로 생존 여부를 알려줌
	return IsValid(Target) && Target->Implements<UTargetableInterface>()
		&& ITargetableInterface::Execute_IsTargetAlive(Target);
}

bool EnemyTargetUtils::IsHostileTarget(const AActor* Attacker, const AActor* Target)
{
	// Enemy끼리는 공격하지 않음: 근접·돌진 판정이 옆의 Enemy 캡슐에 걸려도 무시
	return IsAttackableTarget(Target) && !(IsEnemySide(Attacker) && IsEnemySide(Target));
}

bool EnemyTargetUtils::IsHitReceiverComponent(const AActor* Target, const UPrimitiveComponent* HitComponent)
{
	// 캐릭터는 캡슐로만 피격 판정 (엔진 ACharacter 기준이라 플레이어 타입을 몰라도 됨)
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		return HitComponent == Character->GetCapsuleComponent();
	}
	
	return IsValid(HitComponent);
}

bool EnemyTargetUtils::ApplyAttackHit(AActor* Target, float Damage, const FVector& HitDirection, AActor* DamageCauser)
{
	if (!IsHostileTarget(DamageCauser, Target) || !FMath::IsFinite(Damage) || Damage <= 0.f)
		return false;

	// 가해자 컨트롤러: 적 본인이면 그 컨트롤러, 탄환이면 발사한 적(Instigator)의 컨트롤러
	const APawn* InstigatorPawn = DamageCauser ? DamageCauser->GetInstigator() : nullptr;
	if (!InstigatorPawn)
	{
		InstigatorPawn = Cast<APawn>(DamageCauser);
	}
	AController* InstigatorController = InstigatorPawn ? InstigatorPawn->GetController() : nullptr;

	// 엔진 표준 피해 경로: 받는 쪽의 TakeDamage가 무적/사망/피격 반응을 처리
	FHitResult HitResult;
	UGameplayStatics::ApplyPointDamage(Target, Damage, HitDirection, HitResult, InstigatorController, DamageCauser, nullptr);
	
	return true;	// 가해자 특정, 가해자 컨트롤러 특정, ApplyPointDamage() 수행을 마치면 공격 적용 완료
}
