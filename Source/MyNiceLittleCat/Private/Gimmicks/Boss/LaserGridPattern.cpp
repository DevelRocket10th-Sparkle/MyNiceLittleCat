/// 작성자: 최요환
/// 작성일자: 2026-10-21
///
/// 바닥 광역 레이저 패턴 구현

#include "Gimmicks/Boss/LaserGridPattern.h"
#include "Gimmicks/Boss/LaserGridSequenceData.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Warning/AttackWarningDecal.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

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

	// 프리뷰 기본: 실제 레이저보다 가늘고 흐린 빨강 (불꽃 없는 리본 빔)
	// 기본 이펙트를 생성자에서 지정 → 이미 배치된 격자 패턴(보스 맵 등)도 맵 수정 없이 프리뷰가 나옴
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> PreviewEffectFinder(
		TEXT("/Game/Game/VFX/Niagara/Laser/NS_LaserGrid_Preview_v1.NS_LaserGrid_Preview_v1"));
	if (PreviewEffectFinder.Succeeded())
	{
		PreviewVisualSettings.BeamEffect = PreviewEffectFinder.Object;
	}
	// 연한 분홍빛 흰색 (실제 레이저의 진한 빨강과 구분)
	PreviewVisualSettings.BeamColor = FLinearColor(1.f, 0.45f, 0.4f, 1.f);
	PreviewVisualSettings.BeamWidthScale = 0.35f;
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

	// 동시 공격 수만큼 레이저를 미리 만들어 돌려 씀 (모자라면 FindOrAddFreeSlot이 늘림)
	Slots.SetNum(MaxConcurrentStrikes);
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		CreateSlot(Index);
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

void ALaserGridPattern::CreateSlot(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	ULaserBeamComponent* Laser = NewObject<ULaserBeamComponent>(this, *FString::Printf(TEXT("GridLaser_%d"), SlotIndex));
	Laser->SetupAttachment(Root);
	Laser->SetVisualSettings(LaserVisualSettings);
	Laser->SetDrawDebugBeam(bDrawDebugLasers);
	Laser->SetBreakReflectors(bBreakReflectors);
	if (bHitPlayerOnly)
	{
		Laser->SetPierceNonTargets(true);
		Laser->SetDamageTargetClasses({ APlayerCharacter::StaticClass() });
	}
	Laser->RegisterComponent();
	Slots[SlotIndex].Laser = Laser;

	// 예고 프리뷰: 레이저 컴포넌트를 쓰지 않음 (트레이스·피해·반사판 파괴 없이 모양만)
	if (bShowBeamPreview && PreviewVisualSettings.BeamEffect)
	{
		Slots[SlotIndex].Preview = UNiagaraFunctionLibrary::SpawnSystemAttached(PreviewVisualSettings.BeamEffect, Root, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset,
			/*bAutoDestroy*/ false, /*bAutoActivate*/ false, ENCPoolMethod::None, /*bPreCullCheck*/ false);
	}
}

int32 ALaserGridPattern::FindOrAddFreeSlot()
{
	const int32 FreeIndex = FindFreeSlot();
	if (FreeIndex != INDEX_NONE || Slots.Num() >= MaxSlotCount)
	{
		return FreeIndex;
	}

	const int32 NewIndex = Slots.AddDefaulted();
	CreateSlot(NewIndex);
	UE_LOG(LogTemp, Log, TEXT("[LaserGrid] %s: 레이저가 모자라 %d개로 늘림"), *GetName(), Slots.Num());
	return IsValid(Slots[NewIndex].Laser) ? NewIndex : INDEX_NONE;
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
	LastBaseLane[0] = LastBaseLane[1] = INDEX_NONE;
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
	LastBaseLane[0] = LastBaseLane[1] = INDEX_NONE;
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

	// 기준 줄 정하기: 지정 줄(Fixed·플레이어 기준·앞 단계 기준)을 먼저 정하고,
	// 줄 하나짜리 Random은 같은 순간 같은 축에서 이미 쓰는 줄을 피함
	TArray<int32> GroupBaseLanes;
	GroupBaseLanes.Init(INDEX_NONE, GroupEnd - GroupStart);
	TSet<int32> UsedLanes[2];

	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		const bool bAvoidUsed = Step.LaneMode == ELaserGridLaneMode::Random && Step.LaneSet == ELaserGridLaneSet::Single;
		if (!bAvoidUsed)
		{
			GroupBaseLanes[Index - GroupStart] = ResolveStepLane(Step);
			if (GroupBaseLanes[Index - GroupStart] != INDEX_NONE && Step.LaneSet == ELaserGridLaneSet::Single)
			{
				UsedLanes[static_cast<int32>(Step.Axis)].Add(GroupBaseLanes[Index - GroupStart]);
			}
		}
	}

	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		if (Step.LaneMode == ELaserGridLaneMode::Random && Step.LaneSet == ELaserGridLaneSet::Single)
		{
			TSet<int32>& AxisUsedLanes = UsedLanes[static_cast<int32>(Step.Axis)];
			const int32 LaneIndex = PickRandomUnusedLane(GetLaneCount(Step.Axis), AxisUsedLanes);
			GroupBaseLanes[Index - GroupStart] = LaneIndex;
			AxisUsedLanes.Add(LaneIndex);
		}
	}

	TArray<int32> StepLanes;
	for (int32 Index = GroupStart; Index < GroupEnd; ++Index)
	{
		const FLaserGridStep& Step = Steps[Index];
		const int32 BaseLane = GroupBaseLanes[Index - GroupStart];
		if (BaseLane == INDEX_NONE)
		{
			continue;
		}

		// 다음 단계의 PreviousOffset 기준
		LastBaseLane[static_cast<int32>(Step.Axis)] = BaseLane;

		// 한 단계의 여러 줄은 같은 방향에서 발사 (Random이면 단계마다 한 번만 고름)
		const bool bReversed = Step.FireSide == ELaserGridFireSide::Positive
			|| (Step.FireSide == ELaserGridFireSide::Random && FMath::RandBool());
		const float StepWarningTime = Step.WarningTimeOverride >= 0.f ? Step.WarningTimeOverride : WarningTime;
		const float StepFireDuration = Step.FireDurationOverride > 0.f ? Step.FireDurationOverride : FireDuration;

		ExpandLaneSet(Step, BaseLane, StepLanes);
		for (const int32 LaneIndex : StepLanes)
		{
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

	case ELaserGridLaneMode::PreviousOffset:
	{
		// 앞 단계가 없으면 플레이어 줄 기준, 영역 끝에서는 멈춤 (구멍이 밖으로 사라지지 않게)
		const int32 PreviousLane = LastBaseLane[static_cast<int32>(Step.Axis)];
		const int32 From = PreviousLane != INDEX_NONE ? PreviousLane : PlayerLane;
		return FMath::Clamp(From + Step.Lane, 0, Count - 1);
	}
	}

	// 여러 줄 묶음은 기준 줄이 밖이면 끝 줄로 (줄 하나짜리 PlayerOffset은 기존대로 건너뜀)
	if (Step.LaneSet != ELaserGridLaneSet::Single)
	{
		return FMath::Clamp(LaneIndex, 0, Count - 1);
	}

	return LaneIndex >= 0 && LaneIndex < Count ? LaneIndex : INDEX_NONE;
}

void ALaserGridPattern::ExpandLaneSet(const FLaserGridStep& Step, int32 BaseLane, TArray<int32>& OutLanes) const
{
	OutLanes.Reset();
	const int32 Count = GetLaneCount(Step.Axis);
	if (BaseLane < 0 || BaseLane >= Count)
	{
		return;
	}

	// 기준 줄을 가운데로 Width개 (짝수면 + 쪽이 하나 더): [First, First + Width)
	const int32 Width = FMath::Max(1, Step.SetWidth);
	const int32 First = BaseLane - (Width - 1) / 2;

	switch (Step.LaneSet)
	{
	case ELaserGridLaneSet::Single:
		OutLanes.Add(BaseLane);
		break;

	case ELaserGridLaneSet::Range:
		for (int32 Lane = FMath::Max(First, 0); Lane < FMath::Min(First + Width, Count); ++Lane)
		{
			OutLanes.Add(Lane);
		}
		break;

	case ELaserGridLaneSet::Stride:
	{
		const int32 StrideValue = FMath::Max(2, Step.Stride);
		for (int32 Lane = 0; Lane < Count; ++Lane)
		{
			if (((Lane - BaseLane) % StrideValue + StrideValue) % StrideValue == 0)
			{
				OutLanes.Add(Lane);
			}
		}
		break;
	}

	case ELaserGridLaneSet::AllExceptGap:
	{
		// 구멍이 영역 끝에 걸려도 폭이 줄지 않게 안쪽으로 밀어 넣음
		const int32 GapFirst = FMath::Clamp(First, 0, FMath::Max(0, Count - Width));
		for (int32 Lane = 0; Lane < Count; ++Lane)
		{
			if (Lane < GapFirst || Lane >= GapFirst + Width)
			{
				OutLanes.Add(Lane);
			}
		}
		break;
	}
	}
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

	const int32 SlotIndex = FindOrAddFreeSlot();
	if (SlotIndex == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[LaserGrid] %s: 빈 레이저가 없어 공격을 건너뜁니다. MaxSlotCount를 늘리세요."), *GetName());
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

	if (InWarningTime > 0.f)
	{
		ShowPreview(SlotIndex);
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
	HidePreview(SlotIndex);

	FVector Start;
	FVector End;
	GetBeamPoints(Slot.Axis, Slot.LaneIndex, Slot.bReversed, Start, End);
	const float AlongHalf = GetAlongHalfSize(Slot.Axis);

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
	HidePreview(SlotIndex);

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

void ALaserGridPattern::GetBeamPoints(ELaserGridAxis Axis, int32 LaneIndex, bool bReversed, FVector& OutStart, FVector& OutEnd) const
{
	FVector Center;
	FVector Direction;
	GetLaneGeometry(Axis, LaneIndex, bReversed, Center, Direction);

	const float AlongHalf = GetAlongHalfSize(Axis);
	OutStart = Center - Direction * AlongHalf + FVector::UpVector * BeamHeight;
	OutEnd = OutStart + Direction * AlongHalf * 2.f;
}

float ALaserGridPattern::GetVisibleBeamWidth() const
{
	return FMath::Max(GetEffectiveBeamWidth() * LaserVisualSettings.BeamWidthScale, LaserVisualSettings.MinBeamWidth);
}

float ALaserGridPattern::GetPreviewWidth() const
{
	if (PreviewBeamWidth > 0.f)
	{
		return PreviewBeamWidth;
	}
	return FMath::Max(GetVisibleBeamWidth() * PreviewVisualSettings.BeamWidthScale, PreviewVisualSettings.MinBeamWidth);
}

void ALaserGridPattern::ShowPreview(int32 SlotIndex)
{
	if (!bShowBeamPreview || !Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	FLaserGridSlot& Slot = Slots[SlotIndex];
	if (!IsValid(Slot.Preview))
	{
		return;
	}

	// 경로·굵기·색은 예고 동안 바뀌지 않으므로 켤 때 한 번만 넣음
	FVector Start;
	FVector End;
	GetBeamPoints(Slot.Axis, Slot.LaneIndex, Slot.bReversed, Start, End);

	// 불투명도는 색 알파로 (이펙트 머티리얼 Opacity가 입자 색 알파를 곱함)
	FLinearColor Color = PreviewVisualSettings.BeamColor;
	Color.A = PreviewOpacity;

	ULaserBeamComponent::ApplyBeamEffectParameters(Slot.Preview, PreviewVisualSettings, Start, End, GetPreviewWidth(), Color);
	Slot.Preview->Activate(true);
}

void ALaserGridPattern::HidePreview(int32 SlotIndex)
{
	if (!Slots.IsValidIndex(SlotIndex))
	{
		return;
	}

	// 입자가 남지 않게 즉시 끔 (발사 레이저가 바로 그 자리를 덮음)
	UNiagaraComponent* Preview = Slots[SlotIndex].Preview;
	if (IsValid(Preview) && Preview->IsActive())
	{
		Preview->DeactivateImmediate();
	}
}

APawn* ALaserGridPattern::GetTargetPawn() const
{
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	return PlayerController ? PlayerController->GetPawn() : nullptr;
}
