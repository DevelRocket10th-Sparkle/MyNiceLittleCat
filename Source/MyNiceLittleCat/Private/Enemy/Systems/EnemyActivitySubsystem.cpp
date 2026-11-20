// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Systems/EnemyActivitySubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Enemy/Data/EnemyConfigData.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Systems/EnemyActivitySettings.h"

void UEnemyActivitySubsystem::RegisterEnemy(AEnemyCharacter* Enemy)
{
	if (IsValid(Enemy))
	{
		Enemies.AddUnique(Enemy);
	}
}

void UEnemyActivitySubsystem::UnregisterEnemy(AEnemyCharacter* Enemy)
{
	Enemies.Remove(Enemy);
}

bool UEnemyActivitySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UEnemyActivitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UEnemyActivitySettings* Settings = GetDefault<UEnemyActivitySettings>();
	if (!Settings->bEnableActivityTiers)
	{
		return;
	}

	InWorld.GetTimerManager().SetTimer(EvaluateTimerHandle, this, &UEnemyActivitySubsystem::EvaluateTiers,
		Settings->EvaluationInterval, true);
}

void UEnemyActivitySubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(EvaluateTimerHandle);
	}

	Enemies.Reset();

	Super::Deinitialize();
}

void UEnemyActivitySubsystem::EvaluateTiers()
{
	// 플레이어가 없으면(로딩, 리스폰 대기) 단계를 바꾸지 않음
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}

	const FVector PlayerLocation = Player->GetActorLocation();
	const UEnemyActivitySettings& Settings = *GetDefault<UEnemyActivitySettings>();

	// 뒤에서부터 돌며 사라진 적 정리
	for (int32 Index = Enemies.Num() - 1; Index >= 0; --Index)
	{
		AEnemyCharacter* Enemy = Enemies[Index].Get();
		if (!Enemy)
		{
			Enemies.RemoveAtSwap(Index);
			continue;
		}

		Enemy->ApplyActivityTier(ChooseTier(*Enemy, PlayerLocation, Settings), Settings.ReducedTickInterval);
	}
}

EEnemyActivityTier UEnemyActivitySubsystem::ChooseTier(const AEnemyCharacter& Enemy, const FVector& PlayerLocation,
	const UEnemyActivitySettings& Settings) const
{
	/* 항상 Active: 싸우는 중, 패턴 실행 중, 피해 직후, 화면에 보임 */

	if (Enemy.ShouldStayActive(Settings.DamageWakeSeconds))
	{
		return EEnemyActivityTier::Active;
	}

	const USkeletalMeshComponent* Mesh = Enemy.GetMesh();
	if (Mesh && GetWorld()->TimeSince(Mesh->GetLastRenderTimeOnScreen()) <= Settings.RecentlyRenderedSeconds)
	{
		return EEnemyActivityTier::Active;
	}

	/* 거리 */

	const EEnemyActivityTier CurrentTier = Enemy.GetActivityTier();
	const double DistanceSquared = FVector::DistSquared(Enemy.GetActorLocation(), PlayerLocation);

	// 바깥 단계로 넘어갈 때만 여유 거리를 더함: 이미 그 단계 이상이면 기본 반경으로 유지 판단
	auto IsBeyond = [&](float Radius, EEnemyActivityTier Tier)
	{
		const float Threshold = CurrentTier >= Tier ? Radius : Radius + Settings.TierHysteresis;
		return DistanceSquared > FMath::Square(Threshold);
	};

	// 인지 범위(시야 놓침 반경) 안에서는 항상 Active: 플레이어를 보고도 판단이 느려지는 일 방지
	const UEnemyConfigData* Config = Enemy.GetEnemyConfig();
	const float SenseRadius = Config ? FMath::Max(Config->SightRadius, Config->LoseSightRadius) : 0.f;
	const float ActiveRadius = FMath::Max(Settings.ActiveRadius, SenseRadius + Settings.TierHysteresis);

	if (!IsBeyond(ActiveRadius, EEnemyActivityTier::Reduced))
	{
		return EEnemyActivityTier::Active;
	}

	// Dormant는 쉬는 중일 때만: 이동 중이면 멈춘 자리에 설 때까지 Reduced
	const float DormantRadius = FMath::Max(Settings.DormantRadius, ActiveRadius);
	if (Enemy.IsResting() && IsBeyond(DormantRadius, EEnemyActivityTier::Dormant))
	{
		return EEnemyActivityTier::Dormant;
	}

	return EEnemyActivityTier::Reduced;
}
