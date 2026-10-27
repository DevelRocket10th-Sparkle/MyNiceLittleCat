/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 바닥 광역 레이저 패턴 구현

#include "Gimmicks/Boss/LaserGridPattern.h"
#include "Gimmicks/Boss/LaserGridSequenceData.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Warning/AttackWarningDecal.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	// 아직 쓰지 않은 줄 중에서 무작위 (전부 쓰였으면 아무 줄)
	int32 PickRandomUnusedLane(int32 Count, const TSet<int32>& UsedLanes)
	{
		TArray<int32> Candidates;
		for (int32 Lane = 0; Lane < Count; ++Lane)
		{
			if (!UsedLanes.Contains(Lane))
			{
				Candidates.Add(Lane);
			}
		}

		return Candidates.IsEmpty() ? FMath::RandRange(0, Count - 1) : Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	}
}

ALaserGridPattern::ALaserGridPattern()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	ArenaBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ArenaBox"));
	ArenaBox->SetupAttachment(Root);
	ArenaBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArenaBox->SetHiddenInGame(true);
	ArenaBox->ShapeColor = FColor::Red;

	// 기본 위치: 대기 지점은 경기장(절반 1000) 바깥 -X 쪽, 단일 레이저 지점은 안쪽. 둘 다 +X(경기장 중앙)를 바라봄
	BossStandPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("BossStandPoint"));
	BossStandPoint->SetupAttachment(Root);
	BossStandPoint->SetRelativeLocation(FVector(-1300.f, 0.f, 0.f));
	BossStandPoint->ArrowColor = FColor::Orange;
	BossStandPoint->ArrowSize = 2.f;

	BossBeamPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("BossBeamPoint"));
	BossBeamPoint->SetupAttachment(Root);
	BossBeamPoint->SetRelativeLocation(FVector(-600.f, 0.f, 0.f));
	BossBeamPoint->ArrowColor = FColor::Cyan;
	BossBeamPoint->ArrowSize = 2.f;
}

FTransform ALaserGridPattern::GetBossStandTransform() const
{
	return BossStandPoint->GetComponentTransform();
}

FTransform ALaserGridPattern::GetBossBeamTransform() const
{
	return BossBeamPoint->GetComponentTransform();
}

void ALaserGridPattern::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 경기장 크기를 바로 볼 수 있게 (높이는 표시용)
	ArenaBox->SetBoxExtent(FVector(ArenaHalfSize.X, ArenaHalfSize.Y, 50.f));
	ArenaBox->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
}

void ALaserGridPattern::BeginPlay()
{
	Super::BeginPlay();

	// 동시 공격 수만큼 레이저를 미리 만들어 돌려 씀
	Slots.SetNum(MaxConcurrentStrikes);
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		ULaserBeamComponent* Laser = NewObject<ULaserBeamComponent>(this, *FString::Printf(TEXT("GridLaser_%d"), Index));
		Laser->SetupAttachment(Root);
		Laser->SetVisualSettings(LaserVisualSettings);
		Laser->SetDrawDebugBeam(bDrawDebugLasers);
		Laser->RegisterComponent();
		Slots[Index].Laser = Laser;
	}

	if (!Slots.IsEmpty() && IsValid(Slots[0].Laser))
	{
		DefaultDamagePerHit = Slots[0].Laser->GetDamagePerHit();
	}

	if (bAutoStart)
	{
		StartPattern();
	}
}

void ALaserGridPattern::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopPattern();

	Super::EndPlay(EndPlayReason);
}

void ALaserGridPattern::StartPattern()
{
	if (bPatternActive)
	{
		return;
	}

	bPatternActive = true;
	SweepAxis = bAllowAxisX ? ELaserGridAxis::X : ELaserGridAxis::Y;
	SweepLane = 0;

	// Sequence 모드는 단계마다 DelayToNext로 진행 (StrikeInterval 사용 안 함)
	if (TargetMode == ELaserGridTargetMode::Sequence)
	{
		SequenceIndex = INDEX_NONE;
		StartNextSequence();
		return;
	}

	if (bStrikeOnStart)
	{
		HandleStrikeTick();
	}

	GetWorldTimerManager().SetTimer(StrikeTimerHandle, this, &ALaserGridPattern::HandleStrikeTick, StrikeInterval, true);
}

bool ALaserGridPattern::PlaySequenceOnce(ULaserGridSequenceData* Sequence, float DamagePerHitOverride)
{
	if (bPatternActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaserGrid] %s: 이미 실행 중이라 1회 실행을 건너뜁니다."), *GetName());
		return false;
	}

	if (!Sequence)
	{
		Sequence = PickNextSequence();
	}
	if (!Sequence || Sequence->Steps.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaserGrid] %s: 1회 실행할 시퀀스가 없습니다."), *GetName());
		return false;
	}

	bPatternActive = true;
	bPlayingOnce = true;
	bSequenceStepsDone = false;
	CurrentSequence = Sequence;
	StepIndex = 0;
	ActiveDamageOverride = DamagePerHitOverride;

	UE_LOG(LogTemp, Log, TEXT("[LaserGrid] 1회 실행 시작: %s (%d단계)"), *GetNameSafe(CurrentSequence), CurrentSequence->Steps.Num());

	RunSequenceSteps();
	return true;
}

void ALaserGridPattern::StopPattern()
{
	// 먼저 끄면 아래 ReleaseSlot에서 완료 통지가 나가지 않음
	bPatternActive = false;
	bPlayingOnce = false;
	bSequenceStepsDone = false;
	CurrentSequence = nullptr;
	ActiveDamageOverride = -1.f;

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(StrikeTimerHandle);
	}

	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(Slots[Index].TimerHandle);
		}
		ReleaseSlot(Index);
	}
}

void ALaserGridPattern::TogglePattern()
{
	if (bPatternActive)
	{
		StopPattern();
	}
	else
	{
		StartPattern();
	}
}

void ALaserGridPattern::HandleStrikeTick()
{
	TArray<ELaserGridAxis> AllowedAxes;
	if (bAllowAxisX)
	{
		AllowedAxes.Add(ELaserGridAxis::X);
	}
	if (bAllowAxisY)
	{
		AllowedAxes.Add(ELaserGridAxis::Y);
	}
	if (AllowedAxes.IsEmpty())
	{
		return;
	}

	const ELaserGridAxis RandomAxis = AllowedAxes[FMath::RandRange(0, AllowedAxes.Num() - 1)];
	const APawn* Target = GetTargetPawn();

	// 플레이어를 노리는 모드인데 플레이어가 없으면 무작위로
	ELaserGridTargetMode Mode = TargetMode;
	if (!Target && (Mode == ELaserGridTargetMode::TargetPlayer || Mode == ELaserGridTargetMode::Cross))
	{
		Mode = ELaserGridTargetMode::Random;
	}

	switch (Mode)
	{
	case ELaserGridTargetMode::TargetPlayer:
	{
		StrikeLane(RandomAxis, GetLaneIndexAt(RandomAxis, Target->GetActorLocation()));

		const ELaserGridAxis Other = OtherAxis(RandomAxis);
		if (AllowedAxes.Contains(Other) && FMath::FRand() < CrossChance)
		{
			StrikeLane(Other, GetLaneIndexAt(Other, Target->GetActorLocation()));
		}
		break;
	}

	case ELaserGridTargetMode::Random:
		StrikeLane(RandomAxis, FMath::RandRange(0, GetLaneCount(RandomAxis) - 1));
		break;

	case ELaserGridTargetMode::Sweep:
		StrikeLane(SweepAxis, SweepLane);
		if (++SweepLane >= GetLaneCount(SweepAxis))
		{
			// 끝까지 훑으면 다른 축이 허용될 때 축을 바꿈
			SweepLane = 0;
			if (AllowedAxes.Contains(OtherAxis(SweepAxis)))
			{
				SweepAxis = OtherAxis(SweepAxis);
			}
		}
		break;

	case ELaserGridTargetMode::Cross:
		for (const ELaserGridAxis Axis : AllowedAxes)
		{
			StrikeLane(Axis, GetLaneIndexAt(Axis, Target->GetActorLocation()));
		}
		break;

	case ELaserGridTargetMode::Sequence:
		// 타이머 대신 RunSequenceSteps로 진행
		break;
	}
}

void ALaserGridPattern::StartNextSequence()
{
	StepIndex = 0;
	CurrentSequence = PickNextSequence();

	if (!CurrentSequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaserGrid] %s: Sequence 모드인데 실행할 시퀀스가 없습니다."), *GetName());
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[LaserGrid] 시퀀스 시작: %s (%d단계)"), *GetNameSafe(CurrentSequence), CurrentSequence->Steps.Num());

	RunSequenceSteps();
}

ULaserGridSequenceData* ALaserGridPattern::PickNextSequence()
{
	TArray<int32> ValidIndices;
	for (int32 Index = 0; Index < Sequences.Num(); ++Index)
	{
		if (Sequences[Index] && !Sequences[Index]->Steps.IsEmpty())
		{
			ValidIndices.Add(Index);
		}
	}

	if (ValidIndices.IsEmpty())
	{
		return nullptr;
	}

	if (bRandomSequenceOrder)
	{
		// 같은 시퀀스가 연속으로 나오지 않게 (2개 이상일 때)
		if (ValidIndices.Num() > 1)
		{
			ValidIndices.Remove(SequenceIndex);
		}
		SequenceIndex = ValidIndices[FMath::RandRange(0, ValidIndices.Num() - 1)];
	}
	else
	{
		const int32 Position = ValidIndices.IndexOfByKey(SequenceIndex);
		SequenceIndex = ValidIndices[(Position + 1) % ValidIndices.Num()];
	}

	return Sequences[SequenceIndex];
}

void ALaserGridPattern::RunSequenceSteps()
{
	if (!bPatternActive || !CurrentSequence)
	{
		return;
	}

	const TArray<FLaserGridStep>& Steps = CurrentSequence->Steps;

	// DelayToNext가 0인 단계는 다음 단계와 같은 순간에 실행: 이번에 실행할 단계 묶음 [GroupStart, GroupEnd)
	const int32 GroupStart = StepIndex;
	int32 GroupEnd = StepIndex;
	while (Steps.IsValidIndex(GroupEnd))
	{
		if (Steps[GroupEnd++].DelayToNext > 0.f)
		{
			break;
		}
	}

	// 줄 정하기: 지정 줄(Fixed·플레이어 기준)을 먼저 정하고, Random은 같은 순간 같은 축에서 이미 쓰는 줄을 피함
	TArray<int32> GroupLanes;
	GroupLanes.Init(INDEX_NONE, GroupEnd - GroupStart);
	TSet<int32> UsedLanes[2];

	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		if (Step.LaneMode != ELaserGridLaneMode::Random)
		{
			GroupLanes[Index - GroupStart] = ResolveStepLane(Step);
			if (GroupLanes[Index - GroupStart] != INDEX_NONE)
			{
				UsedLanes[static_cast<int32>(Step.Axis)].Add(GroupLanes[Index - GroupStart]);
			}
		}
	}

	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		if (Step.LaneMode == ELaserGridLaneMode::Random)
		{
			TSet<int32>& AxisUsedLanes = UsedLanes[static_cast<int32>(Step.Axis)];
			const int32 LaneIndex = PickRandomUnusedLane(GetLaneCount(Step.Axis), AxisUsedLanes);
			GroupLanes[Index - GroupStart] = LaneIndex;
			AxisUsedLanes.Add(LaneIndex);
		}
	}

	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		const int32 LaneIndex = GroupLanes[Index - GroupStart];
		if (LaneIndex != INDEX_NONE)
		{
			const bool bReversed = Step.FireSide == ELaserGridFireSide::Positive
				|| (Step.FireSide == ELaserGridFireSide::Random && FMath::RandBool());
			const float StepWarningTime = Step.WarningTimeOverride >= 0.f ? Step.WarningTimeOverride : WarningTime;
			const float StepFireDuration = Step.FireDurationOverride > 0.f ? Step.FireDurationOverride : FireDuration;

			StartStrike(Step.Axis, LaneIndex, bReversed, StepWarningTime, StepFireDuration);
		}
	}
	StepIndex = GroupEnd;

	// 다음 묶음이 남아 있으면 마지막 단계의 DelayToNext 뒤에 실행 (마지막 묶음이면 아래 종료 처리)
	if (GroupEnd > GroupStart && Steps.IsValidIndex(StepIndex))
	{
		GetWorldTimerManager().SetTimer(StrikeTimerHandle, this, &ALaserGridPattern::RunSequenceSteps, Steps[GroupEnd - 1].DelayToNext, false);
		return;
	}

	// 1회 실행: 다음 시퀀스로 가지 않고 남은 공격이 끝나기를 기다림
	if (bPlayingOnce)
	{
		bSequenceStepsDone = true;
		TryFinishSequenceOnce();
		return;
	}

	// 시퀀스 끝: 마지막 단계의 DelayToNext + RestAfter 뒤에 다음 시퀀스 (0초 타이머는 걸리지 않으므로 최소값)
	const float LastDelay = Steps.IsEmpty() ? 0.f : Steps.Last().DelayToNext;
	const float WaitTime = FMath::Max(LastDelay + CurrentSequence->RestAfter, 0.05f);
	GetWorldTimerManager().SetTimer(StrikeTimerHandle, this, &ALaserGridPattern::StartNextSequence, WaitTime, false);
}

int32 ALaserGridPattern::ResolveStepLane(const FLaserGridStep& Step) const
{
	const int32 Count = GetLaneCount(Step.Axis);
	const APawn* Target = GetTargetPawn();

	// 플레이어가 없으면 가운데 줄 기준
	const int32 PlayerLane = Target ? GetLaneIndexAt(Step.Axis, Target->GetActorLocation()) : Count / 2;

	int32 LaneIndex = INDEX_NONE;
	switch (Step.LaneMode)
	{
	case ELaserGridLaneMode::Fixed:
		LaneIndex = Step.Lane >= 0 ? Step.Lane : Count + Step.Lane;
		break;

	case ELaserGridLaneMode::PlayerLane:
		LaneIndex = PlayerLane;
		break;

	case ELaserGridLaneMode::PlayerOffset:
		LaneIndex = PlayerLane + Step.Lane;
		break;

	case ELaserGridLaneMode::Random:
		LaneIndex = FMath::RandRange(0, Count - 1);
		break;
	}

	return LaneIndex >= 0 && LaneIndex < Count ? LaneIndex : INDEX_NONE;
}

bool ALaserGridPattern::StrikeLane(ELaserGridAxis Axis, int32 LaneIndex)
{
	return StartStrike(Axis, LaneIndex, bRandomFireSide && FMath::RandBool(), WarningTime, FireDuration);
}

bool ALaserGridPattern::StartStrike(ELaserGridAxis Axis, int32 LaneIndex, bool bReversed, float InWarningTime, float InFireDuration)
{
	if (LaneIndex < 0 || LaneIndex >= GetLaneCount(Axis))
	{
		return false;
	}

	const int32 SlotIndex = FindFreeSlot();
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaserGrid] %s: 빈 레이저가 없어 공격을 건너뜁니다. MaxConcurrentStrikes를 늘리세요."), *GetName());
		return false;
	}

	FLaserGridSlot& Slot = Slots[SlotIndex];
	Slot.bBusy = true;
	Slot.Axis = Axis;
	Slot.LaneIndex = LaneIndex;
	Slot.bReversed = bReversed;
	Slot.FireDuration = InFireDuration;

	if (WarningDecalClass)
	{
		FVector Center;
		FVector Direction;
		GetLaneGeometry(Axis, LaneIndex, Slot.bReversed, Center, Direction);

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		Slot.Warning = GetWorld()->SpawnActor<AAttackWarningDecal>(WarningDecalClass, Center, FRotator::ZeroRotator, SpawnParams);
		if (Slot.Warning)
		{
			// 데칼 로컬 X(진행도 방향) = 레이저 진행 방향 → 레이저가 나오는 쪽부터 차오름
			const FVector2D HalfSize(GetAlongHalfSize(Axis), GetEffectiveBeamWidth() * 0.5f);
			Slot.Warning->ShowWarning(Center, HalfSize, Direction.Rotation().Yaw, InWarningTime, WarningColor);
		}
	}

	OnStrikeWarned.Broadcast(Axis, LaneIndex);

	if (InWarningTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(Slot.TimerHandle, FTimerDelegate::CreateUObject(this, &ALaserGridPattern::FireSlot, SlotIndex), InWarningTime, false);
	}
	else
	{
		FireSlot(SlotIndex);
	}

	return true;
}

void ALaserGridPattern::FireSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FLaserGridSlot& Slot = Slots[SlotIndex];
	if (IsValid(Slot.Warning))
	{
		Slot.Warning->HideWarning();
	}
	Slot.Warning = nullptr;

	FVector Center;
	FVector Direction;
	GetLaneGeometry(Slot.Axis, Slot.LaneIndex, Slot.bReversed, Center, Direction);

	const float AlongHalf = GetAlongHalfSize(Slot.Axis);
	const FVector Start = Center - Direction * AlongHalf + FVector::UpVector * BeamHeight;
	const FVector End = Start + Direction * AlongHalf * 2.f;

	if (IsValid(Slot.Laser))
	{
		// 빔 굵기 = BeamWidth(0이면 줄 폭), 사거리 = 줄 길이 (영역 밖으로 나가지 않게)
		Slot.Laser->SetWorldLocation(Start);
		Slot.Laser->SetBeamRadius(GetEffectiveBeamWidth() * 0.5f);
		Slot.Laser->SetMaxRange(AlongHalf * 2.f);
		Slot.Laser->SetDamagePerHit(ActiveDamageOverride >= 0.f ? ActiveDamageOverride : DefaultDamagePerHit);
		Slot.Laser->StartLaser(End, Slot.FireDuration);
	}

	OnStrikeFired.Broadcast(Slot.Axis, Slot.LaneIndex);

	GetWorldTimerManager().SetTimer(Slot.TimerHandle, FTimerDelegate::CreateUObject(this, &ALaserGridPattern::ReleaseSlot, SlotIndex), Slot.FireDuration, false);
}

void ALaserGridPattern::ReleaseSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FLaserGridSlot& Slot = Slots[SlotIndex];
	if (IsValid(Slot.Warning))
	{
		Slot.Warning->HideWarning();
	}
	Slot.Warning = nullptr;

	if (IsValid(Slot.Laser))
	{
		Slot.Laser->StopLaser();
	}

	Slot.bBusy = false;

	TryFinishSequenceOnce();
}

void ALaserGridPattern::TryFinishSequenceOnce()
{
	if (!bPlayingOnce || !bSequenceStepsDone)
	{
		return;
	}

	if (Slots.ContainsByPredicate([](const FLaserGridSlot& Slot) { return Slot.bBusy; }))
	{
		return;
	}

	ULaserGridSequenceData* FinishedSequence = CurrentSequence;

	bPatternActive = false;
	bPlayingOnce = false;
	bSequenceStepsDone = false;
	CurrentSequence = nullptr;
	ActiveDamageOverride = -1.f;
	GetWorldTimerManager().ClearTimer(StrikeTimerHandle);

	UE_LOG(LogTemp, Log, TEXT("[LaserGrid] 1회 실행 끝: %s"), *GetNameSafe(FinishedSequence));

	OnSequenceFinished.Broadcast(FinishedSequence);
}

int32 ALaserGridPattern::FindFreeSlot() const
{
	return Slots.IndexOfByPredicate([](const FLaserGridSlot& Slot) { return !Slot.bBusy && IsValid(Slot.Laser); });
}

int32 ALaserGridPattern::GetLaneCount(ELaserGridAxis Axis) const
{
	return FMath::Max(1, FMath::FloorToInt(GetCrossHalfSize(Axis) * 2.f / LaneWidth));
}

int32 ALaserGridPattern::GetLaneIndexAt(ELaserGridAxis Axis, const FVector& WorldLocation) const
{
	const FVector Local = GetActorTransform().InverseTransformPosition(WorldLocation);
	const float Cross = Axis == ELaserGridAxis::X ? Local.Y : Local.X;

	// 줄들은 영역 중앙 기준으로 정렬 (줄 개수 × 폭이 영역보다 작으면 양쪽 여백은 공격 안 함)
	const int32 Count = GetLaneCount(Axis);
	const int32 Index = FMath::FloorToInt((Cross + Count * LaneWidth * 0.5f) / LaneWidth);
	return FMath::Clamp(Index, 0, Count - 1);
}

float ALaserGridPattern::GetCrossHalfSize(ELaserGridAxis Axis) const
{
	return Axis == ELaserGridAxis::X ? ArenaHalfSize.Y : ArenaHalfSize.X;
}

float ALaserGridPattern::GetAlongHalfSize(ELaserGridAxis Axis) const
{
	return Axis == ELaserGridAxis::X ? ArenaHalfSize.X : ArenaHalfSize.Y;
}

float ALaserGridPattern::GetEffectiveBeamWidth() const
{
	return BeamWidth > 0.f ? FMath::Min(BeamWidth, LaneWidth) : LaneWidth;
}

void ALaserGridPattern::GetLaneGeometry(ELaserGridAxis Axis, int32 LaneIndex, bool bReversed, FVector& OutCenter, FVector& OutDirection) const
{
	const int32 Count = GetLaneCount(Axis);
	const float Cross = (LaneIndex + 0.5f) * LaneWidth - Count * LaneWidth * 0.5f;

	const FVector LocalCenter = Axis == ELaserGridAxis::X ? FVector(0.f, Cross, 0.f) : FVector(Cross, 0.f, 0.f);
	FVector LocalDirection = Axis == ELaserGridAxis::X ? FVector::ForwardVector : FVector::RightVector;
	if (bReversed)
	{
		LocalDirection = -LocalDirection;
	}

	OutCenter = GetActorTransform().TransformPosition(LocalCenter);
	OutDirection = GetActorTransform().TransformVectorNoScale(LocalDirection);
}

APawn* ALaserGridPattern::GetTargetPawn() const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}
