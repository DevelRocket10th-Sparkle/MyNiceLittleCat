#include "Enemy/Boss/Stage3BossCinematics.h"

#include "Characters/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemy/Boss/BossCharacter.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemData.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"

namespace
{
	const FName ArrivalSeen(TEXT("Stage3.MansionArrivalSeen"));
	const FName PhaseOne(TEXT("CombatPhase1"));
	const FName Barrier70(TEXT("Barrier70"));
	const FName Barrier30(TEXT("Barrier30"));
	const FName PhaseThree(TEXT("CombatPhase3"));
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

	APlayerCharacter* Player = GetPlayer();
	if (!Player)
		return;
	if (Encounter->IsWaitingForRetry())
		Encounter->ResetEncounter();

	if (Opening == EOpening::Initial && !OpeningPlayer)
	{
		UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
		if (Progress && Progress->HasProgressFlag(ArrivalSeen))
			FinishOpeningCut();
		else
			Play(ArrivalSequence);
	}

	if (Opening == EOpening::Courtyard && !bRequirePhaseOneConfirmation && HasEnteredArena(PhaseOne))
	{
		Opening = EOpening::PhaseOne;
		Play(FindPhase(PhaseOne)->Sequence);
	}
	if (OpeningPlayer && DoorIsOpen.IsValidIndex(0))
	{
		const double Seconds = OpeningPlayer->GetCurrentTime().AsSeconds();
		if (Opening == EOpening::Unlocking && Seconds >= 1.2 && !DoorIsOpen[0])
			SetDoor(0, true, 2.4f);
		if (Opening == EOpening::PhaseOne && Seconds >= 2.35 && DoorIsOpen[0])
			SetDoor(0, false, 0.16f);
	}

	if (Opening == EOpening::Combat && !bEntranceClosed && DoorIsOpen.IsValidIndex(1) && DoorIsOpen[1]
		&& Doors[1].Actor && Player->GetActorLocation().X > Doors[1].Actor->GetActorLocation().X + 260.f)
	{
		bEntranceClosed = true;
		SetDoor(1, false, 0.18f);
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
		&& ((Opening == EOpening::Courtyard && bRequirePhaseOneConfirmation && HasEnteredArena(PhaseOne))
			|| FindNearestDoor(InstigatorActor) != INDEX_NONE);
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
	if (Opening == EOpening::Courtyard && bRequirePhaseOneConfirmation && HasEnteredArena(PhaseOne))
		Prompt.ActionText = NSLOCTEXT("Stage3", "StartFight", "전투 시작");
	else if (Door == 0 && Opening == EOpening::Locked)
		Prompt.ActionText = NSLOCTEXT("Stage3", "UseKey", "대문 열쇠 사용");
	else
		Prompt.ActionText = NSLOCTEXT("Stage3", "LockedDoor", "잠긴 문 확인");
	return Prompt;
}

void AStage3BossCinematics::Interact_Implementation(AActor* InstigatorActor)
{
	if (!HasAuthority() || !CanInteract_Implementation(InstigatorActor))
		return;
	if (Opening == EOpening::Courtyard && bRequirePhaseOneConfirmation && HasEnteredArena(PhaseOne))
	{
		Opening = EOpening::PhaseOne;
		Play(FindPhase(PhaseOne)->Sequence);
		return;
	}

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
	Settings.FinishCompletionStateOverride = EMovieSceneCompletionModeOverride::ForceRestoreState;
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
	if (Boss && !Sequence->FindBindingsByTag(TEXT("Boss")).IsEmpty())
		Actor->SetBindingByTag(TEXT("Boss"), {Boss}, false);
	if (APlayerCharacter* Player = GetPlayer(); Player && !Sequence->FindBindingsByTag(TEXT("Player")).IsEmpty())
		Actor->SetBindingByTag(TEXT("Player"), {Player}, false);
}

void AStage3BossCinematics::BeginPhaseCut(FName Phase, ABossCharacter* Boss)
{
	if (const FStage3CinematicPhase* Setup = FindPhase(Phase))
	{
		for (int32 Door : Setup->OpenDoors)
			SetDoor(Door, true, 0.85f);
	}
	if (Phase == Barrier30)
	{
		WarningTime = 1.35f;
		if (StairBlocker)
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
	if (const FStage3CinematicPhase* Setup = FindPhase(Phase); Setup && Boss)
	{
		if (Setup->BossStart)
			Boss->SetActorLocationAndRotation(Setup->BossStart->GetActorLocation(), Setup->BossStart->GetActorRotation(),
				false, nullptr, ETeleportType::TeleportPhysics);
		AddTrail(*Setup, Boss);
		SetEyeGlow(Boss, Setup->EyeGlow);
	}
}

bool AStage3BossCinematics::HasEnteredArena(FName Phase) const
{
	const FStage3CinematicPhase* Setup = FindPhase(Phase);
	const APlayerCharacter* Player = GetPlayer();
	if (!Setup || !Player)
		return false;
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const FVector Delta = (Player->GetActorLocation() - Setup->EntryCenter).GetAbs();
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
	if (!BloodMaterial)
		return;
	for (int32 Index = 0; Index < Phase.BloodTrail.Num(); ++Index)
	{
		const FVector Point = Phase.BloodTrail[Index];
		FHitResult Floor;
		FCollisionQueryParams Query(SCENE_QUERY_STAT(Stage3BloodFloor), false, Boss);
		if (GetWorld()->LineTraceSingleByChannel(Floor, Point + FVector(0, 0, 70), Point - FVector(0, 0, 220),
			ECC_WorldStatic, Query) && Floor.ImpactNormal.Z > 0.65f)
		{
			const float Size = 24.f + (Index % 3) * 11.f;
			const FRotator Rotation = (-Floor.ImpactNormal).Rotation() + FRotator(0.f, 0.f, Index * 71.f);
			TrailDecals.Add(UGameplayStatics::SpawnDecalAtLocation(this, BloodMaterial, FVector(14.f, Size, Size),
				Floor.ImpactPoint + Floor.ImpactNormal, Rotation));
		}
	}
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
		for (const FName Bone : { FName(TEXT("Eye02_L")), FName(TEXT("Eye02_R")) })
		{
			if (!Boss->GetMesh()->DoesSocketExist(Bone))
				continue;
			UStaticMeshComponent* Eye = NewObject<UStaticMeshComponent>(Boss);
			Boss->AddInstanceComponent(Eye);
			Eye->SetStaticMesh(EyeMesh);
			Eye->SetMaterial(0, EyeInstance);
			Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Eye->SetCastShadow(false);
			Eye->SetupAttachment(Boss->GetMesh(), Bone);
			Eye->SetRelativeScale3D(FVector(0.075f));
			Eye->RegisterComponent();
		}
	}
	EyeInstance->SetVectorParameterValue(TEXT("EyeGlow"), Color);
}
