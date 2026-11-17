// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.13.
* Enemy 공격의 대상 판정과 피해 전달 규칙을 한 곳에 모은 도우미
* - 플레이어 타입을 모름: 대상 판정은 ITargetableInterface, 피해는 엔진 표준 TakeDamage 경로
* - 플레이어 외 대상(펫, 소환물 등)도 인터페이스와 TakeDamage만 구현하면 같은 공격에 맞음
*/

#pragma once

#include "CoreMinimal.h"

class AActor;
class UPrimitiveComponent;

namespace EnemyTargetUtils
{
	// 공격 대상으로 유효한지: ITargetableInterface를 구현했고, 살아있는 액터
	MYNICELITTLECAT_API bool IsAttackableTarget(const AActor* Target);

	// 판정에 맞은 컴포넌트가 대상의 피격 판정 컴포넌트인지
	// - 캐릭터는 캡슐만 인정 (무기, 장식 메시에 맞은 것 제외)
	MYNICELITTLECAT_API bool IsHitReceiverComponent(const AActor* Target, const UPrimitiveComponent* HitComponent);

	// 대상에게 피해 전달 (UGameplayStatics::ApplyPointDamage): 전달했으면 true
	// - DamageCauser: 피해를 준 액터 (적 본인, 탄환 등)
	//   가해자 컨트롤러는 Causer의 Instigator에서 구함
	MYNICELITTLECAT_API bool ApplyAttackHit(AActor* Target, float Damage, const FVector& HitDirection, AActor* DamageCauser);

}
