// Fill out your copyright notice in the Description page of Project Settings.
// 히트스톱: 맞은 순간 월드 전체를 잠깐 멈춤 (글로벌 타임 딜레이션)
// 공격에 관여하지 않은 주변 적·투사체도 같이 멈추므로 플레이어만 손해 보지 않음
// 멈춘 동안 캐릭터 스켈레탈 메시는 틱을 꺼서 포즈를 그대로 고정 (꼬리 물리 등이 아주 작은 DeltaTime에 튀는 것 방지)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/Ticker.h"
#include "HitStopSubsystem.generated.h"

UCLASS()
class MYNICELITTLECAT_API UHitStopSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 월드 전체를 Duration초(실제 시간) 동안 멈춤. 이미 멈춰 있으면 남은 시간을 더 긴 쪽으로 연장
	void ApplyHitStop(float Duration);

	// 즉시 해제 (레벨 이동·사망 연출 등)
	void CancelHitStop();

	bool IsHitStopActive() const { return bActive; }

	virtual void Deinitialize() override;

	// 멈춰 있는 동안의 시간 배율 (0이면 타이머·물리가 꼬일 수 있어 아주 작은 값 사용)
	float FrozenTimeDilation = 0.02f;

private:
	bool TickHitStop(float DeltaTime);
	void FreezeMeshes();
	void UnfreezeMeshes();

	FTSTicker::FDelegateHandle TickHandle;
	float RemainingSeconds = 0.f;
	bool bActive = false;

	// 멈출 때 틱을 끈 메시 (해제 때 다시 켬)
	TArray<TWeakObjectPtr<class USkeletalMeshComponent>> FrozenMeshes;
	float SavedTimeDilation = 1.f;
};
