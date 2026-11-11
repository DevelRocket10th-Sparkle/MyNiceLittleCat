#include "Enemy/Boss/Stage3BossCinematics.h"

#include "Characters/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Enemy/Components/EnemyPhaseComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemData.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "LevelUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MovieScene.h"
#include "MovieSceneBindingOverrides.h"
#include "MovieSceneObjectBindingID.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"

#include "Systems/CameraOcclusionComponent.h"

namespace
{
	const FName ArrivalSeen(TEXT("Stage3.MansionArrivalSeen"));
	constexpr float ArrivalTriggerRadius = 1200.f;
	const FName PhaseOne(TEXT("CombatPhase1"));
	const FName Barrier70(TEXT("Barrier70"));
	const FName Barrier30(TEXT("Barrier30"));
	const FName PhaseThree(TEXT("CombatPhase3"));

	// 핏자국 간격(경로 길이 기준, 0.7~1.3배로 불규칙)과 크기 범위: 찔끔찔끔 떨어지는 작은 방울
	constexpr float BloodSpacing = 650.f;
	constexpr float BloodMinSize = 22.f;
	constexpr float BloodMaxSize = 38.f;
	constexpr float BloodFadeIn = 0.15f;
	// 사이렌 연출 길이(초)
	constexpr float SirenSeconds = 3.f;

	FTransform GetStageLevelTransform(const AActor& Stage)
	{
		const ULevelStreaming* StreamingLevel = FLevelUtils::FindStreamingLevel(Stage.GetLevel());
		return StreamingLevel ? StreamingLevel->LevelTransform : FTransform::Identity;
	}

	// 시퀀스 마커(MarkedFrame) 하나: 재생 시작 기준 초 + 라벨
	struct FStage3Mark
	{
		double Seconds = 0.0;
		FString Label;
	};

	// 시퀀스의 마커를 시간순으로 반환. 연출 이벤트(문, 핏자국, 계단 줄) 타이밍은 시퀀서에서 마커를 옮겨 조절
	TArray<FStage3Mark> GetSequenceMarks(const UMovieSceneSequence* Sequence)
	{
		TArray<FStage3Mark> Marks;
		const UMovieScene* Scene = Sequence ? Sequence->GetMovieScene() : nullptr;
		if (!Scene)
			return Marks;
		const FFrameRate TickRate = Scene->GetTickResolution();
		const double StartSeconds = TickRate.AsSeconds(Scene->GetPlaybackRange().GetLowerBoundValue());
		for (const FMovieSceneMarkedFrame& Mark : Scene->GetMarkedFrames())
			Marks.Add({ TickRate.AsSeconds(Mark.FrameNumber) - StartSeconds, Mark.Label });
		Marks.StableSort([](const FStage3Mark& A, const FStage3Mark& B) { return A.Seconds < B.Seconds; });
		return Marks;
	}

	// "Door:<인덱스>:Open|Close[:초]" 형식 파싱
	bool ParseDoorMark(const FString& Label, int32& OutDoor, bool& bOutOpen, float& OutDuration)
	{
		TArray<FString> Parts;
		Label.ParseIntoArray(Parts, TEXT(":"));
		if (Parts.Num() < 3 || !Parts[0].Equals(TEXT("Door"), ESearchCase::IgnoreCase) || !Parts[1].IsNumeric())
			return false;
		OutDoor = FCString::Atoi(*Parts[1]);
		bOutOpen = Parts[2].Equals(TEXT("Open"), ESearchCase::IgnoreCase);
		OutDuration = Parts.Num() > 3 ? FMath::Max(0.f, FCString::Atof(*Parts[3])) : (bOutOpen ? 0.85f : 0.18f);
		return true;
	}

	bool HasDoorMark(const TArray<FStage3Mark>& Marks, int32 Door)
	{
		int32 MarkDoor = INDEX_NONE;
		bool bOpen = false;
		float Duration = 0.f;
		return Marks.ContainsByPredicate([&](const FStage3Mark& Mark)
			{ return ParseDoorMark(Mark.Label, MarkDoor, bOpen, Duration) && MarkDoor == Door; });
	}

	const FStage3Mark* FindMark(const TArray<FStage3Mark>& Marks, const TCHAR* Label)
	{
		return Marks.FindByPredicate([Label](const FStage3Mark& Mark) { return Mark.Label.Equals(Label, ESearchCase::IgnoreCase); });
	}

	// 보스 경로 위 한 점(시퀀스 로컬 좌표, 캡슐 중심)과 보스가 그 점을 지나는 시각
	struct FStage3TrailPoint
	{
		FVector Local = FVector::ZeroVector;
		double Seconds = 0.0;
	};

	// "Boss" 태그 바인딩의 트랜스폼 키를 샘플링해 Blood:Start ~ Blood:End 구간을 일정 간격으로 나눔
	TArray<FStage3TrailPoint> SampleBossTrail(const UMovieSceneSequence* Sequence, const TArray<FStage3Mark>& Marks)
	{
		TArray<FStage3TrailPoint> Points;
		const UMovieScene* Scene = Sequence ? Sequence->GetMovieScene() : nullptr;
		if (!Scene)
			return Points;

		// 트랜스폼 트랙의 위치 채널(X, Y, Z) 찾기; MovieSceneTracks 모듈 의존 없이 클래스 이름으로 판별
		TArrayView<FMovieSceneDoubleChannel* const> Location;
		// "Boss" 태그 바인딩 우선, 없으면 이름이 "Boss (runtime)"인 바인딩
		TArray<FGuid> Candidates;
		for (const FMovieSceneObjectBindingID& Id : Sequence->FindBindingsByTag(TEXT("Boss")))
			Candidates.Add(Id.GetGuid());
		UMovieScene* MutableScene = const_cast<UMovieScene*>(Scene);	// GetPossessable에 const 오버로드가 없음 (읽기만 함)
		for (int32 Index = 0; Index < MutableScene->GetPossessableCount(); ++Index)
		{
			const FMovieScenePossessable& Possessable = MutableScene->GetPossessable(Index);
			if (Possessable.GetName() == TEXT("Boss (runtime)"))
				Candidates.AddUnique(Possessable.GetGuid());
		}
		for (const FGuid& Guid : Candidates)
		{
			const FMovieSceneBinding* Binding = Scene->FindBinding(Guid);
			if (!Binding)
				continue;
			for (UMovieSceneTrack* Track : Binding->GetTracks())
			{
				if (!Track || Track->GetClass()->GetFName() != TEXT("MovieScene3DTransformTrack") || Track->GetAllSections().IsEmpty())
					continue;
				UMovieSceneSection* Section = Track->GetAllSections()[0];
				TArrayView<FMovieSceneDoubleChannel* const> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
				if (Channels.Num() >= 3)
					Location = Channels;
				break;
			}
			if (Location.Num() >= 3)
				break;
		}
		if (Location.Num() < 3)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Stage3Blood] %s: Boss transform track not found (candidates %d)"),
				*Sequence->GetName(), Candidates.Num());
			return Points;
		}

		const FFrameRate TickRate = Scene->GetTickResolution();
		const FFrameRate DisplayRate = Scene->GetDisplayRate();
		const TRange<FFrameNumber> Range = Scene->GetPlaybackRange();
		const double RangeStart = TickRate.AsSeconds(Range.GetLowerBoundValue());
		const double RangeEnd = TickRate.AsSeconds(Range.GetUpperBoundValue());
		const FStage3Mark* StartMark = FindMark(Marks, TEXT("Blood:Start"));
		const FStage3Mark* EndMark = FindMark(Marks, TEXT("Blood:End"));
		const double From = StartMark ? StartMark->Seconds : 0.0;
		const double To = EndMark ? EndMark->Seconds : RangeEnd - RangeStart;

		auto Evaluate = [&](double Seconds)
			{
				const FFrameTime Time = (RangeStart + Seconds) * TickRate;
				double X = 0.0, Y = 0.0, Z = 0.0;
				Location[0]->Evaluate(Time, X);
				Location[1]->Evaluate(Time, Y);
				Location[2]->Evaluate(Time, Z);
				return FVector(X, Y, Z);
			};

		const double Step = DisplayRate.AsInterval();
		FVector Last = Evaluate(From);
		float Travelled = BloodSpacing * 0.6f;
		float NextGap = BloodSpacing;
		for (double Seconds = From + Step; Seconds <= To + KINDA_SMALL_NUMBER; Seconds += Step)
		{
			const FVector Current = Evaluate(Seconds);
			Travelled += FVector::Dist2D(Last, Current);
			Last = Current;
			if (Travelled >= NextGap)
			{
				Travelled = 0.f;
				Points.Add({ Current, Seconds });
				// 일정한 발자국처럼 보이지 않도록 다음 간격을 불규칙하게
				NextGap = BloodSpacing * (0.7f + 0.6f * FMath::Frac(Points.Num() * 0.618f));
			}
		}
		return Points;
	}
}

AStage3BossCinematics::AStage3BossCinematics()
{
	PrimaryActorTick.bCanEverTick = true;
	InteractionBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBounds"));
	SetRootComponent(InteractionBounds);
	InteractionBounds->SetBoxExtent(FVector(6900.f, 5400.f, 2200.f));
	InteractionBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBounds->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionBounds->SetGenerateOverlapEvents(false);
}

void AStage3BossCinematics::BeginPlay()
{
	Super::BeginPlay();

	DoorMotion.SetNum(Doors.Num());
	DoorIsOpen.Init(false, Doors.Num());
	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		const FStage3CinematicDoor& Door = Doors[Index];
		if (!IsValid(Door.Actor))
			continue;
		TInlineComponentArray<UStaticMeshComponent*> Components(Door.Actor);
		for (FName Name : Door.Leaves)
		{
			for (UStaticMeshComponent* Component : Components)
			{
				if (Component->GetFName() == Name)
				{
					FDoorMotion& Motion = DoorMotion[Index].AddDefaulted_GetRef();
					Motion.Leaf = Component;
					Motion.Closed = Component->GetRelativeRotation();
					// These mansion leaves are authored in their open poses in the level.
					Motion.Closed.Yaw = 0.f;
					Component->SetRelativeRotation(Motion.Closed);
					break;
				}
			}
		}
	}
	if (StairBlocker)
	{
		StairBlocker->SetActorHiddenInGame(false);
		StairBlocker->SetActorEnableCollision(true);
	}
}

void AStage3BossCinematics::EndPlay(const EEndPlayReason::Type Reason)
{
	StopOpeningCut();
	Super::EndPlay(Reason);
}

APlayerCharacter* AStage3BossCinematics::GetPlayer() const
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return Player && !Player->GetbIsDead() ? Player : nullptr;
}

void AStage3BossCinematics::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!HasAuthority() || !IsValid(Encounter))
		return;

	if (WarningTime > 0.f)
	{
		WarningTime = FMath::Max(0.f, WarningTime - DeltaSeconds);
		const float Pulse = 0.35f + 0.65f * FMath::Abs(FMath::Sin(WarningTime * 13.f));
		SetEyeGlow(Encounter->GetCurrentBoss(), FLinearColor(13.f * Pulse, 0.05f, 0.02f));
	}

	for (TArray<FDoorMotion>& Motions : DoorMotion)
	{
		for (FDoorMotion& Motion : Motions)
		{
			if (Motion.Time >= Motion.Duration || !Motion.Leaf.IsValid())
				continue;
			Motion.Time = FMath::Min(Motion.Time + DeltaSeconds, Motion.Duration);
			const float Alpha = Motion.Time / Motion.Duration;
			Motion.Leaf->SetRelativeRotation(FMath::Lerp(Motion.From, Motion.To, Alpha * Alpha));
		}
	}

	// 등장 연출(1페이즈 시퀀스) 전에는 미리 스폰된 보스를 숨김; 등장 연출 시작 시 StartPhaseOneCut에서 다시 보임
	if (Opening == EOpening::Initial || Opening == EOpening::Locked
		|| Opening == EOpening::Unlocking || Opening == EOpening::Courtyard)
	{
		ABossCharacter* WaitingBoss = Encounter->GetCurrentBoss();
		if (IsValid(WaitingBoss) && !WaitingBoss->IsHidden())
			WaitingBoss->SetActorHiddenInGame(true);
	}

	APlayerCharacter* Player = GetPlayer();
	if (!Player)
		return;
	if (Encounter->IsWaitingForRetry())
		Encounter->ResetEncounter();

	if (Opening == EOpening::Initial && !OpeningPlayer)
	{
		UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
		const AActor* ArrivalAnchor = IsValid(GateLock) ? GateLock.Get()
			: (Doors.IsValidIndex(0) ? Doors[0].Actor.Get() : nullptr);
		if (!ArrivalSequence || (Progress && Progress->HasProgressFlag(ArrivalSeen)))
			FinishOpeningCut();
		else if (IsValid(ArrivalAnchor) && FVector::DistSquared(Player->GetActorLocation(), ArrivalAnchor->GetActorLocation())
			<= FMath::Square(ArrivalTriggerRadius))
			Play(ArrivalSequence);
	}

	// 재생 중인 시퀀스의 마커를 현재 시간 기준으로 적용: 마지막으로 지난 Door 마커가 그 문의 상태를 결정
	auto ApplySequenceMarks = [this](const UMovieSceneSequence* Sequence, double Seconds)
		{
			const TArray<FStage3Mark> Marks = GetSequenceMarks(Sequence);
			TMap<int32, TPair<bool, float>> DoorStates;
			for (const FStage3Mark& Mark : Marks)
			{
				if (Mark.Seconds > Seconds)
					break;
				int32 Door = INDEX_NONE;
				bool bOpen = false;
				float Duration = 0.f;
				if (ParseDoorMark(Mark.Label, Door, bOpen, Duration))
					DoorStates.Add(Door, { bOpen, Duration });
				else if (Mark.Label.Equals(TEXT("Stair:Break"), ESearchCase::IgnoreCase) && StairBlocker && !StairBlocker->IsHidden())
				{
					StairBlocker->SetActorHiddenInGame(true);
					StairBlocker->SetActorEnableCollision(false);
				}
			}
			for (const TPair<int32, TPair<bool, float>>& State : DoorStates)
			{
				if (DoorIsOpen.IsValidIndex(State.Key) && DoorIsOpen[State.Key] != State.Value.Key)
					SetDoor(State.Key, State.Value.Key, State.Value.Value);
			}
		};

	// 1페이즈 연출 시작은 ABossEncounterTrigger -> ABossEncounterManager::RequestEncounterStart 경로로만 진입
	if (OpeningPlayer && OpeningPlayer->IsPlaying() && DoorIsOpen.IsValidIndex(0))
	{
		const double Seconds = OpeningPlayer->GetCurrentTime().AsSeconds();
		const UMovieSceneSequence* Sequence = OpeningPlayer->GetSequence();
		if (HasDoorMark(GetSequenceMarks(Sequence), 0))
			ApplySequenceMarks(Sequence, Seconds);
		else
		{
			// 마커가 없는 시퀀스용 기존 타이밍
			if (Opening == EOpening::Unlocking && Seconds >= 1.2 && !DoorIsOpen[0])
				SetDoor(0, true, 2.4f);
			if (Opening == EOpening::PhaseOne && Seconds >= 2.35 && DoorIsOpen[0])
				SetDoor(0, false, 0.16f);
		}
	}

	// 페이즈 전환 시퀀스는 UEnemyPhaseComponent가 재생하므로 재생 중인 시퀀스 액터에서 시간을 읽음
	for (TActorIterator<ALevelSequenceActor> It(GetWorld()); It; ++It)
	{
		ULevelSequencePlayer* SequencePlayer = It->GetSequencePlayer();
		const UMovieSceneSequence* Sequence = SequencePlayer ? SequencePlayer->GetSequence() : nullptr;
		if (!Sequence || *It == OpeningActor || !SequencePlayer->IsPlaying())
			continue;
		for (const TPair<FName, FStage3CinematicPhase>& Phase : Phases)
		{
			if (Phase.Value.Sequence == Sequence)
			{
				ApplySequenceMarks(Sequence, SequencePlayer->GetCurrentTime().AsSeconds());
				break;
			}
		}
	}

	if (Opening == EOpening::Combat && !bEntranceClosed && DoorIsOpen.IsValidIndex(1) && DoorIsOpen[1]
		&& Doors[1].Actor)
	{
		const FVector EntranceDelta = Player->GetActorLocation() - Doors[1].Actor->GetActorLocation();
		if (GetStageLevelTransform(*this).InverseTransformVector(EntranceDelta).X > 260.f)
		{
			bEntranceClosed = true;
			SetDoor(1, false, 0.18f);
		}
	}
}

int32 AStage3BossCinematics::FindNearestDoor(const AActor* Player) const
{
	int32 Nearest = INDEX_NONE;
	float Best = FMath::Square(320.f);
	for (int32 Index = 0; Index < Doors.Num(); ++Index)
	{
		const AActor* Door = Doors[Index].Actor;
		if (!Door || !DoorIsOpen.IsValidIndex(Index) || DoorIsOpen[Index])
			continue;
		const FVector Delta = GetDoorInteractionLocation(Index) - Player->GetActorLocation();
		if (FMath::Abs(Delta.Z) < 350.f && Delta.SizeSquared2D() < Best)
		{
			Nearest = Index;
			Best = Delta.SizeSquared2D();
		}
	}
	return Nearest;
}

FVector AStage3BossCinematics::GetDoorInteractionLocation(int32 DoorIndex) const
{
	if (DoorIndex == 0 && GateLock)
		return GateLock->GetActorLocation() + FVector(0.f, 0.f, 100.f);
	return Doors[DoorIndex].Actor->GetActorLocation() + FVector(0.f, 0.f, 150.f);
}

bool AStage3BossCinematics::CanInteract_Implementation(AActor* InstigatorActor) const
{
	return InstigatorActor && InstigatorActor == GetPlayer() && !OpeningPlayer
		&& FindNearestDoor(InstigatorActor) != INDEX_NONE;
}

FVector AStage3BossCinematics::GetInteractFocusLocation_Implementation(AActor* InstigatorActor) const
{
	if (!InstigatorActor)
		return GetActorLocation();
	const int32 Door = FindNearestDoor(InstigatorActor);
	if (!Doors.IsValidIndex(Door))
		return InstigatorActor->GetActorLocation() + InstigatorActor->GetActorForwardVector() * 30.f;
	const FVector Center = GetDoorInteractionLocation(Door);
	const FVector FromPlayer = (Center - InstigatorActor->GetActorLocation()).GetSafeNormal2D();
	return Center - FromPlayer * 85.f;
}

FInteractPromptInfo AStage3BossCinematics::GetInteractPrompt_Implementation(AActor* InstigatorActor) const
{
	FInteractPromptInfo Prompt;
	Prompt.WorldLocation = GetInteractFocusLocation_Implementation(InstigatorActor);
	const int32 Door = FindNearestDoor(InstigatorActor);
	if (Door == 0 && Opening == EOpening::Locked)
		Prompt.ActionText = NSLOCTEXT("Stage3", "UseKey", "대문 열쇠 사용");
	else
		Prompt.ActionText = NSLOCTEXT("Stage3", "LockedDoor", "잠긴 문 확인");
	return Prompt;
}

void AStage3BossCinematics::Interact_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || !CanInteract_Implementation(InstigatorActor))
		return;

	const int32 Door = FindNearestDoor(InstigatorActor);
	const UInventorySubsystem* Inventory = GetGameInstance()->GetSubsystem<UInventorySubsystem>();
	if (Door == 0 && Opening == EOpening::Locked && (bGateUnlocked || (GateKey && Inventory && Inventory->HasItem(GateKey))))
	{
		bGateUnlocked = true;
		Opening = EOpening::Unlocking;
		Play(UnlockSequence);
		return;
	}
	if (LockedRattle && Doors.IsValidIndex(Door))
		UGameplayStatics::PlaySoundAtLocation(this, LockedRattle, Doors[Door].Actor->GetActorLocation());
}

bool AStage3BossCinematics::StartPhaseOneCut(APlayerCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Encounter) || !Player || Player != GetPlayer()
		|| Opening != EOpening::Courtyard || OpeningPlayer)
		return false;

	const FStage3CinematicPhase* Setup = FindPhase(PhaseOne);
	if (!Setup)
		return false;

	Opening = EOpening::PhaseOne;
	// 숨겨 두었던 보스를 등장 연출부터 보이게 함
	if (ABossCharacter* Boss = Encounter->GetCurrentBoss(); IsValid(Boss))
		Boss->SetActorHiddenInGame(false);
	Play(Setup->Sequence);
	return true;
}

bool AStage3BossCinematics::TrySkipActiveCut(AActor* InstigatorActor)
{
	if (!HasAuthority() || !IsValid(Encounter) || !InstigatorActor || InstigatorActor != GetPlayer())
		return false;

	if (IsValid(OpeningPlayer) && (OpeningPlayer->IsPlaying() || OpeningPlayer->IsPaused()))
	{
		// 마지막 프레임을 평가한 뒤 정지해야 KeepState 트랙(1페이즈 진입 후 플레이어 위치)이 최종 상태로 남음
		OpeningPlayer->OnFinished.RemoveDynamic(this, &AStage3BossCinematics::FinishOpeningCut);
		OpeningPlayer->OnStop.RemoveDynamic(this, &AStage3BossCinematics::FinishOpeningCut);
		OpeningPlayer->GoToEndAndStop();
		FinishOpeningCut();
		return true;
	}

	ABossCharacter* Boss = Encounter->GetCurrentBoss();
	return IsValid(Boss) && Boss->GetPhaseComponent()->SkipActiveTransitionSequence();
}

void AStage3BossCinematics::Play(ULevelSequence* Sequence)
{
	APlayerCharacter* Player = GetPlayer();
	if (!Sequence || !Player)
	{
		FinishOpeningCut();
		return;
	}
	FMovieSceneSequencePlaybackSettings Settings;
	Settings.bDisableMovementInput = true;
	Settings.bDisableLookAtInput = true;
	Settings.bHideHud = true;
	// 섹션별 완료 모드 사용: 1페이즈 진입 연출의 플레이어 위치만 KeepState, 나머지는 RestoreState
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::None;
	ALevelSequenceActor* Actor = nullptr;
	OpeningPlayer = ULevelSequencePlayer::CreateLevelSequencePlayer(this, Sequence, Settings, Actor);
	OpeningActor = Actor;
	if (!OpeningPlayer || !OpeningActor)
	{
		FinishOpeningCut();
		return;
	}
	BindSequence(Sequence, Actor, Encounter->GetCurrentBoss());
	OpeningPlayer->OnFinished.AddDynamic(this, &AStage3BossCinematics::FinishOpeningCut);
	OpeningPlayer->OnStop.AddDynamic(this, &AStage3BossCinematics::FinishOpeningCut);

	if (UCameraOcclusionComponent* Occlusion = Player->FindComponentByClass<UCameraOcclusionComponent>())
	{
		OpeningOcclusion = Occlusion;
		Occlusion->SetOcclusionSuspended(true);
	}
	OpeningPlayer->Play();

	if (OpeningPlayer && !OpeningPlayer->IsPlaying())
		FinishOpeningCut();
}

void AStage3BossCinematics::StopOpeningCut()
{
	if (OpeningPlayer)
	{
		OpeningPlayer->OnFinished.RemoveDynamic(this, &AStage3BossCinematics::FinishOpeningCut);
		OpeningPlayer->OnStop.RemoveDynamic(this, &AStage3BossCinematics::FinishOpeningCut);
		if (OpeningPlayer->IsPlaying() || OpeningPlayer->IsPaused())
			OpeningPlayer->Stop();
		OpeningPlayer = nullptr;
	}
	if (OpeningActor)
	{
		const TWeakObjectPtr<ALevelSequenceActor> Actor = OpeningActor.Get();
		OpeningActor = nullptr;
		GetWorldTimerManager().SetTimerForNextTick([Actor]
			{
				if (Actor.IsValid())
					Actor->Destroy();
			});
	}

	// 플레이어 참조가 이미 비어 있어도 가림을 복구하기 위해 'if (OpeningPlayer)' 밖에서 실행
	if (UCameraOcclusionComponent* Occlusion = OpeningOcclusion.Get())
	{
		Occlusion->SetOcclusionSuspended(false);
	}
	OpeningOcclusion.Reset();
}

void AStage3BossCinematics::FinishOpeningCut()
{
	StopOpeningCut();
	switch (Opening)
	{
	case EOpening::Initial:
		if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
			Progress->SetProgressFlag(ArrivalSeen);
		Opening = bGateUnlocked ? EOpening::Courtyard : EOpening::Locked;
		break;
	case EOpening::Unlocking:
		if (!DoorIsOpen.IsValidIndex(0) || !DoorIsOpen[0])
			SetDoor(0, true, 0.f);
		Opening = EOpening::Courtyard;
		break;
	case EOpening::PhaseOne:
		if (APlayerCharacter* Player = GetPlayer())
		{
			SetDoor(0, false, 0.16f);
			bStartingCombat = true;
			const bool bStarted = Encounter->StartEncounter(Player);
			bStartingCombat = false;
			if (bStarted)
				Opening = EOpening::Combat;
			else
				ResetStaging();
		}
		break;
	default:
		break;
	}
}

void AStage3BossCinematics::OpenExit()
{
	for (int32 Index = 0; Index < DoorMotion.Num(); ++Index)
		SetDoor(Index, true, 0.8f);
}

void AStage3BossCinematics::ResetStaging()
{
	StopOpeningCut();
	for (int32 Index = 0; Index < DoorMotion.Num(); ++Index)
		SetDoor(Index, Index == 0 && bGateUnlocked, 0.f);
	for (const auto& Decal : TrailDecals)
	{
		if (Decal.IsValid())
			Decal->DestroyComponent();
	}
	TrailDecals.Reset();
	bEntranceClosed = false;
	WarningTime = 0.f;
	if (StairBlocker)
	{
		StairBlocker->SetActorHiddenInGame(false);
		StairBlocker->SetActorEnableCollision(true);
	}
	if (Opening != EOpening::Initial)
		Opening = bGateUnlocked ? EOpening::Courtyard : EOpening::Locked;
}

void AStage3BossCinematics::SetDoor(int32 DoorIndex, bool bOpen, float Duration)
{
	if (!DoorMotion.IsValidIndex(DoorIndex))
		return;
	const FStage3CinematicDoor& Door = Doors[DoorIndex];
	const bool bWasOpen = DoorIsOpen[DoorIndex];
	DoorIsOpen[DoorIndex] = bOpen;
	for (int32 Index = 0; Index < DoorMotion[DoorIndex].Num(); ++Index)
	{
		FDoorMotion& Motion = DoorMotion[DoorIndex][Index];
		if (UStaticMeshComponent* Leaf = Motion.Leaf.Get())
		{
			Motion.From = Leaf->GetRelativeRotation();
			Motion.To = Motion.Closed;
			if (bOpen && Door.OpenYaw.IsValidIndex(Index))
				Motion.To.Yaw += Door.OpenYaw[Index];
			Motion.Time = 0.f;
			Motion.Duration = Duration;
			if (Duration <= 0.f)
				Leaf->SetRelativeRotation(Motion.To);
		}
	}
	if (bWasOpen && !bOpen && Duration > 0.f && DoorSlam && Door.Actor)
		UGameplayStatics::PlaySoundAtLocation(this, DoorSlam, Door.Actor->GetActorLocation());
}

void AStage3BossCinematics::BindSequence(ULevelSequence* Sequence, ALevelSequenceActor* Actor, ABossCharacter* Boss) const
{
	// The camera and boss transform keys were authored in the original stage's coordinates.
	if (UDefaultLevelSequenceInstanceData* InstanceData = Cast<UDefaultLevelSequenceInstanceData>(Actor->DefaultInstanceData))
	{
		Actor->bOverrideInstanceData = true;
		InstanceData->TransformOrigin = GetStageLevelTransform(*this);
	}

	if (Boss && !Sequence->FindBindingsByTag(TEXT("Boss")).IsEmpty())
		Actor->SetBindingByTag(TEXT("Boss"), {Boss}, false);
	if (APlayerCharacter* Player = GetPlayer(); Player && !Sequence->FindBindingsByTag(TEXT("Player")).IsEmpty())
		Actor->SetBindingByTag(TEXT("Player"), {Player}, false);

	if (Sequence == UnlockSequence && IsValid(GateLock) && Actor->BindingOverrides)
	{
		// The authored lock component binding points into the original level asset.
		UStaticMeshComponent* LockMechanism = FindObjectFast<UStaticMeshComponent>(GateLock.Get(), TEXT("Main"));
		UMovieScene* Scene = Sequence->GetMovieScene();
		if (LockMechanism && Scene)
		{
			for (int32 Index = 0; Index < Scene->GetPossessableCount(); ++Index)
			{
				const FMovieScenePossessable& Binding = Scene->GetPossessable(Index);
				if (Binding.GetName() != TEXT("Key lock mechanism"))
					continue;

				const FGuid Parent = Binding.GetParent();
				if (Parent.IsValid())
					Actor->SetBinding(UE::MovieScene::FFixedObjectBindingID(Parent, MovieSceneSequenceID::Root), {GateLock.Get()});
				Actor->BindingOverrides->SetBinding(
					UE::MovieScene::FFixedObjectBindingID(Binding.GetGuid(), MovieSceneSequenceID::Root), {LockMechanism});
				break;
			}
		}
	}
}

void AStage3BossCinematics::BeginPhaseCut(FName Phase, ABossCharacter* Boss)
{
	const FStage3CinematicPhase* Setup = FindPhase(Phase);
	const TArray<FStage3Mark> Marks = GetSequenceMarks(Setup ? Setup->Sequence.Get() : nullptr);
	UE_LOG(LogTemp, Log, TEXT("[Stage3Blood] BeginPhaseCut %s (setup %d, marks %d)"), *Phase.ToString(), Setup != nullptr, Marks.Num());
	if (Setup)
	{
		// 시퀀스에 Door 마커가 있는 문은 마커 시점에 열고, 없는 문만 즉시 엶
		for (int32 Door : Setup->OpenDoors)
		{
			if (!HasDoorMark(Marks, Door))
				SetDoor(Door, true, 0.85f);
		}
		// 핏자국은 지금 모두 깔고, 보스가 그 지점을 지나는 시각에 페이드인
		AddTrail(*Setup, Boss);
	}
	if (Phase == Barrier30)
	{
		WarningTime = SirenSeconds;
		if (StairBlocker && !FindMark(Marks, TEXT("Stair:Break")))
		{
			StairBlocker->SetActorHiddenInGame(true);
			StairBlocker->SetActorEnableCollision(false);
		}
	}
	if (Phase == PhaseThree && Boss && RoofEscapeEffect)
		UNiagaraFunctionLibrary::SpawnSystemAttached(RoofEscapeEffect, Boss->GetMesh(), NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
}

void AStage3BossCinematics::FinishPhaseCut(FName Phase, ABossCharacter* Boss)
{
	if (Phase == Barrier30)
		WarningTime = 0.f;

	// 정상 종료/스킵 모두 아직 안 보인 핏자국을 즉시 표시 (스킵 시 늦게 생기던 문제 방지)
	for (const TWeakObjectPtr<UDecalComponent>& Decal : TrailDecals)
	{
		if (Decal.IsValid())
			Decal->SetFadeIn(0.f, 0.f);
	}

	if (const FStage3CinematicPhase* Setup = FindPhase(Phase))
	{
		// 스킵으로 마커 시점을 건너뛰어도 플레이어가 지나갈 문은 열려 있어야 함
		for (int32 Door : Setup->OpenDoors)
		{
			if (DoorIsOpen.IsValidIndex(Door) && !DoorIsOpen[Door])
				SetDoor(Door, true, 0.35f);
		}
		if (Phase == Barrier30 && StairBlocker)
		{
			StairBlocker->SetActorHiddenInGame(true);
			StairBlocker->SetActorEnableCollision(false);
		}
		if (Boss)
		{
			if (Setup->BossStart)
				Boss->SetActorLocationAndRotation(Setup->BossStart->GetActorLocation(), Setup->BossStart->GetActorRotation(),
					false, nullptr, ETeleportType::TeleportPhysics);
			SetEyeGlow(Boss, Setup->EyeGlow);
		}
	}
}

bool AStage3BossCinematics::HasEnteredArena(FName Phase) const
{
	const FStage3CinematicPhase* Setup = FindPhase(Phase);
	const APlayerCharacter* Player = GetPlayer();
	if (!Setup || !Player)
		return false;
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector StagePosition = GetStageLevelTransform(*this).InverseTransformPosition(Player->GetActorLocation());
	const FVector Delta = (StagePosition - Setup->EntryCenter).GetAbs();
	return Delta.X + Radius < Setup->EntryExtent.X
		&& Delta.Y + Radius < Setup->EntryExtent.Y && Delta.Z < Setup->EntryExtent.Z;
}

void AStage3BossCinematics::ActivatePhase(FName Phase, ABossCharacter* Boss)
{
	if (const FStage3CinematicPhase* Setup = FindPhase(Phase))
	{
		SetDoor(Setup->DoorToClose, false, 0.18f);
		SetEyeGlow(Boss, Setup->EyeGlow);
	}
}

void AStage3BossCinematics::AddTrail(const FStage3CinematicPhase& Phase, ABossCharacter* Boss)
{
	if (!BloodMaterial || !Phase.Sequence)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Stage3Blood] Skipped: material %d, sequence %d"), BloodMaterial != nullptr, Phase.Sequence != nullptr);
		return;
	}

	// 경로는 시퀀스의 보스 트랜스폼 키에서 직접 계산 -> 시퀀스 경로를 고치면 핏자국도 따라감
	const TArray<FStage3Mark> Marks = GetSequenceMarks(Phase.Sequence);
	TArray<FStage3TrailPoint> Points = SampleBossTrail(Phase.Sequence, Marks);
	// 수동 지정 지점(BloodTrail)은 가장 가까운 경로 지점의 시각에 나타남
	for (const FVector& Manual : Phase.BloodTrail)
	{
		double Seconds = 0.0;
		float Best = TNumericLimits<float>::Max();
		for (const FStage3TrailPoint& Point : Points)
		{
			const float Distance = FVector::DistSquared2D(Point.Local, Manual);
			if (Distance < Best)
			{
				Best = Distance;
				Seconds = Point.Seconds;
			}
		}
		Points.Add({ Manual + FVector(0.f, 0.f, 160.f), Seconds });
	}

	const FTransform StageTransform = GetStageLevelTransform(*this);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(Stage3BloodFloor), false, Boss);
	if (APlayerCharacter* Player = GetPlayer())
		Query.AddIgnoredActor(Player);
	const FCollisionObjectQueryParams StaticOnly(ECC_WorldStatic);
	int32 Spawned = 0;
	for (int32 Index = 0; Index < Points.Num(); ++Index)
	{
		// 캡슐 중심에서 아래로 바닥 탐색 (공중에 떠 있는 구간은 바닥이 멀어 자동 제외)
		const FVector Point = StageTransform.TransformPosition(Points[Index].Local);
		const FVector Side = FVector(FMath::Sin(Index * 2.1f), FMath::Cos(Index * 1.7f), 0.f) * 45.f;
		const FVector Start = Point + Side;
		const FVector End = Start - FVector(0.f, 0.f, 420.f);
		FHitResult Floor;
		const bool bHit = GetWorld()->LineTraceSingleByObjectType(Floor, Start, End, StaticOnly, Query)
			|| GetWorld()->LineTraceSingleByChannel(Floor, Start, End, ECC_Visibility, Query);
		if (!bHit || Floor.bStartPenetrating || Floor.ImpactNormal.Z < 0.65f)
			continue;

		const float Alpha = FMath::Frac(Index * 0.618f);
		const float Size = FMath::Lerp(BloodMinSize, BloodMaxSize, Alpha);
		const FRotator Rotation = (-Floor.ImpactNormal).Rotation() + FRotator(0.f, 0.f, Index * 71.f);
		UDecalComponent* Decal = UGameplayStatics::SpawnDecalAtLocation(this, BloodMaterial, FVector(30.f, Size, Size),
			Floor.ImpactPoint, Rotation);
		if (!Decal)
			continue;
		// 화면 크기에 따른 페이드 아웃을 끄고(멀리서도 보이게), 보스가 지나가는 시각에 맞춰 페이드인
		Decal->SetFadeScreenSize(0.f);
		Decal->SetFadeIn(static_cast<float>(Points[Index].Seconds), BloodFadeIn);
		TrailDecals.Add(Decal);
		++Spawned;
	}
	UE_LOG(LogTemp, Log, TEXT("[Stage3Blood] %s: path points %d, decals %d"), *Phase.Sequence->GetName(), Points.Num(), Spawned);
}

void AStage3BossCinematics::SetEyeGlow(ABossCharacter* Boss, FLinearColor Color)
{
	if (!Boss || !EyeMaterial || !EyeMesh)
		return;
	if (Color.GetMax() <= 0.f)
	{
		if (EyeInstance && EyeOwner.Get() == Boss)
			EyeInstance->SetVectorParameterValue(TEXT("EyeGlow"), Color);
		return;
	}
	if (EyeOwner.Get() != Boss)
	{
		EyeOwner = Boss;
		EyeInstance = UMaterialInstanceDynamic::Create(EyeMaterial, Boss);
		// EvilMage 눈: EyeRoot_L/R 뼈에 붙이고 위/아래 눈꺼풀 뼈 중간(눈 표면)에 배치
		USkeletalMeshComponent* Mesh = Boss->GetMesh();
		for (const TCHAR* Side : { TEXT("L"), TEXT("R") })
		{
			const FName Root(*FString::Printf(TEXT("EyeRoot_%s"), Side));
			const FName Upper(*FString::Printf(TEXT("UppderEye_%s"), Side));
			const FName Lower(*FString::Printf(TEXT("BottomEye_%s"), Side));
			if (!Mesh->DoesSocketExist(Root))
				continue;
			const FVector Surface = Mesh->DoesSocketExist(Upper) && Mesh->DoesSocketExist(Lower)
				? (Mesh->GetSocketLocation(Upper) + Mesh->GetSocketLocation(Lower)) * 0.5f
				: Mesh->GetSocketLocation(Root);
			UStaticMeshComponent* Eye = NewObject<UStaticMeshComponent>(Boss);
			Boss->AddInstanceComponent(Eye);
			Eye->SetStaticMesh(EyeMesh);
			Eye->SetMaterial(0, EyeInstance);
			Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Eye->SetCastShadow(false);
			Eye->SetupAttachment(Mesh, Root);
			Eye->RegisterComponent();
			Eye->SetWorldLocation(Surface + Boss->GetActorForwardVector() * 2.f);
			Eye->SetWorldScale3D(FVector(0.12f));
		}
	}
	EyeInstance->SetVectorParameterValue(TEXT("EyeGlow"), Color);
}
