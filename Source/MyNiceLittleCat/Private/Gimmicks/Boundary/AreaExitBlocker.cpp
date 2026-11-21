#include "Gimmicks/Boundary/AreaExitBlocker.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Cinematics/CinematicSubsystem.h"
#include "Frameworks/PlayerLookup.h"
#include "Frameworks/Save/ProgressFlagGateComponent.h"
#include "Interfaces/TargetableInterface.h"
#include "Pet/PetCharacter.h"
#include "Pet/SpeechBubbleComponent.h"

namespace
{
	// 직선 보간 + 위로 볼록한 포물선 (Alpha 0.5에서 ArcHeight만큼 최고점)
	FVector EvaluateArc(const FVector& Start, const FVector& End, float ArcHeight, float Alpha)
	{
		FVector Location = FMath::Lerp(Start, End, Alpha);
		Location.Z += 4.f * ArcHeight * Alpha * (1.f - Alpha);
		return Location;
	}
}

AAreaExitBlocker::AAreaExitBlocker()
{
	// 비행 중에만 Tick
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 보이지 않는 벽: BlockedChannels만 막음 (OnConstruction·BeginPlay에서 적용)
	BlockingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockingBox"));
	BlockingBox->SetupAttachment(SceneRoot);
	BlockingBox->SetBoxExtent(FVector(50.f, 400.f, 400.f));
	BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BlockingBox->SetCollisionObjectType(ECC_WorldStatic);
	BlockingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BlockingBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	BlockingBox->SetCanEverAffectNavigation(false);
	BlockingBox->SetHiddenInGame(true);

	// 감지 영역: 기본은 벽 바로 앞(-X, 마을 쪽). 레벨에서 위치·크기 조절
	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);
	TriggerBox->SetRelativeLocation(FVector(-200.f, 0.f, 0.f));
	TriggerBox->SetBoxExtent(FVector(120.f, 400.f, 400.f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
	TriggerBox->SetCanEverAffectNavigation(false);
	TriggerBox->SetHiddenInGame(true);

	PetViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PetViewCamera"));
	PetViewCamera->SetupAttachment(SceneRoot);

	ProgressGate = CreateDefaultSubobject<UProgressFlagGateComponent>(TEXT("ProgressGate"));

	PetLines = {
		{ NSLOCTEXT("AreaExitBlocker", "Line0", "어딜 가려는 거야?"), 2.f },
		{ NSLOCTEXT("AreaExitBlocker", "Line1", "아직 마을에서 할 일이 남아있지 않아?"), 2.5f },
	};
}

void AAreaExitBlocker::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyBlockedChannels();
}

void AAreaExitBlocker::BeginPlay()
{
	Super::BeginPlay();
	// 패키지 빌드에서는 배치된 액터의 OnConstruction이 다시 돌지 않으므로 여기서도 적용
	ApplyBlockedChannels();
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &AAreaExitBlocker::HandleTriggerBeginOverlap);
}

void AAreaExitBlocker::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &AAreaExitBlocker::HandleTriggerBeginOverlap);
	GetWorldTimerManager().ClearTimer(StepTimerHandle);
	// 연출 도중 제거되거나 레벨이 언로드되면 잠금 상태를 남기지 않음 (월드 종료 시에는 서브시스템이 함께 정리됨)
	if (bBusy && (EndPlayReason == EEndPlayReason::Destroyed || EndPlayReason == EEndPlayReason::RemovedFromWorld))
	{
		FinishSequence();
	}
	Super::EndPlay(EndPlayReason);
}

void AAreaExitBlocker::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bBusy)
		return;

	ACharacter* Player = Cast<ACharacter>(OtherActor);
	if (!Player || !Player->IsPlayerControlled() || !IsAliveCharacter(Player))
		return;

	BeginSequence(Player);
}

// 1) 입력 잠금, 멈춤 → 펫 카메라
void AAreaExitBlocker::BeginSequence(ACharacter* Player)
{
	bBusy = true;
	ActivePlayer = Player;
	ActiveController = Cast<APlayerController>(Player->GetController());
	ActivePet = ResolvePet();

	SetPlayerInputLocked(true);
	Player->GetCharacterMovement()->StopMovementImmediately();

	if (!ActivePet.IsValid() || PetLines.IsEmpty())
	{
		// 펫이나 대사가 없으면 바로 복귀
		BeginFlight();
		return;
	}

	FocusPetCamera();
	LineIndex = INDEX_NONE;
	// 카메라 블렌드가 끝난 뒤 첫 대사
	GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AAreaExitBlocker::PlayNextLine,
		FMath::Max(CameraBlendTime, KINDA_SMALL_NUMBER), false);
}

// 2) 펫이 플레이어를 보게 돌리고, 펫 정면에서 펫을 바라보는 카메라로 블렌드
void AAreaExitBlocker::FocusPetCamera()
{
	APetCharacter* Pet = ActivePet.Get();
	ACharacter* Player = ActivePlayer.Get();
	if (!Pet || !Player)
		return;

	SetPetLogicPaused(true);
	SetBubbleHeightOverridden(true);
	Pet->GetCharacterMovement()->StopMovementImmediately();

	const FVector ToPlayer = (Player->GetActorLocation() - Pet->GetActorLocation()).GetSafeNormal2D();
	if (!ToPlayer.IsNearlyZero())
	{
		Pet->SetActorRotation(ToPlayer.Rotation());
	}

	const FVector PetLocation = Pet->GetActorLocation();
	const FVector Forward = Pet->GetActorForwardVector().GetSafeNormal2D();
	const FVector CameraLocation = PetLocation + Forward * CameraDistance + FVector::UpVector * CameraHeight;
	const FVector LookAt = PetLocation + FVector::UpVector * CameraLookAtHeight;
	PetViewCamera->SetWorldLocationAndRotation(CameraLocation, (LookAt - CameraLocation).Rotation());

	if (APlayerController* Controller = ActiveController.Get())
	{
		Controller->SetViewTargetWithBlend(this, CameraBlendTime, VTBlend_Cubic);
	}
}

// 3) 대사를 차례로 출력, 끝나면 복귀
void AAreaExitBlocker::PlayNextLine()
{
	APetCharacter* Pet = ActivePet.Get();
	ACharacter* Player = ActivePlayer.Get();
	if (!IsAliveCharacter(Player))
	{
		FinishSequence();
		return;
	}

	++LineIndex;
	USpeechBubbleComponent* Bubble = Pet ? Pet->GetSpeechBubble() : nullptr;
	if (!Bubble || !PetLines.IsValidIndex(LineIndex))
	{
		if (Bubble)
		{
			Bubble->HideBubble();
		}
		BeginFlight();
		return;
	}

	const FAreaExitBlockerLine& Line = PetLines[LineIndex];
	Bubble->Say(Line.Text, Line.Duration);
	GetWorldTimerManager().SetTimer(StepTimerHandle, this, &AAreaExitBlocker::PlayNextLine, Line.Duration, false);
}

// 4) 카메라를 플레이어로 되돌리며 복귀 지점까지 포물선 비행
void AAreaExitBlocker::BeginFlight()
{
	ACharacter* Player = ActivePlayer.Get();
	FRotator ReturnRotation;
	if (!IsAliveCharacter(Player) || !ResolveReturnLocation(FlightEnd, ReturnRotation))
	{
		FinishSequence();
		return;
	}

	if (APlayerController* Controller = ActiveController.Get())
	{
		Controller->SetViewTargetWithBlend(Player, CameraBlendTime, VTBlend_Cubic);
	}

	FlightStart = Player->GetActorLocation();
	FlightEndRotation = ReturnRotation;
	FlightElapsed = 0.f;
	bFlying = true;

	// 중력·지면 판정 없이 위치를 직접 옮김 (건물에 걸리지 않게 스윕 없음)
	UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
	Movement->StopMovementImmediately();
	Movement->SetMovementMode(MOVE_Flying);

	const FVector Direction = (FlightEnd - FlightStart).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		Player->SetActorRotation(Direction.Rotation());
	}

	SetActorTickEnabled(true);
}

void AAreaExitBlocker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFlying)
	{
		SetActorTickEnabled(false);
		return;
	}

	ACharacter* Player = ActivePlayer.Get();
	if (!Player)
	{
		FinishSequence();
		return;
	}

	FlightElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(FlightElapsed / FlightDuration, 0.f, 1.f);

	Player->SetActorLocation(EvaluateArc(FlightStart, FlightEnd, FlightArcHeight, Alpha), false, nullptr, ETeleportType::TeleportPhysics);

	if (Alpha >= 1.f)
	{
		bFlying = false;
		SetActorTickEnabled(false);
		Player->SetActorRotation(FRotator(0.f, FlightEndRotation.Yaw, 0.f));
		if (APlayerController* Controller = ActiveController.Get())
		{
			Controller->SetControlRotation(FRotator(Controller->GetControlRotation().Pitch, FlightEndRotation.Yaw, 0.f));
		}
		FinishSequence();
	}
}

// 5) 착지·정리: 이동 모드 복구, 펫 복귀, 입력 해제
void AAreaExitBlocker::FinishSequence()
{
	GetWorldTimerManager().ClearTimer(StepTimerHandle);
	bFlying = false;
	SetActorTickEnabled(false);

	ACharacter* Player = ActivePlayer.Get();
	if (Player)
	{
		UCharacterMovementComponent* Movement = Player->GetCharacterMovement();
		if (Movement->MovementMode == MOVE_Flying)
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Falling);
		}

		APlayerController* Controller = ActiveController.Get();
		if (Controller && Controller->GetViewTarget() == this)
		{
			Controller->SetViewTargetWithBlend(Player, CameraBlendTime, VTBlend_Cubic);
		}
	}

	if (APetCharacter* Pet = ActivePet.Get())
	{
		if (USpeechBubbleComponent* Bubble = Pet->GetSpeechBubble())
		{
			Bubble->HideBubble();
		}
		SetBubbleHeightOverridden(false);
		SetPetLogicPaused(false);
		if (bTeleportPetAfterLanding && IsAliveCharacter(Player))
		{
			Pet->TeleportToPartner();
		}
	}

	SetPlayerInputLocked(false);

	ActivePlayer.Reset();
	ActiveController.Reset();
	ActivePet.Reset();
	LineIndex = INDEX_NONE;
	bBusy = false;
}

void AAreaExitBlocker::SetPlayerInputLocked(bool bLocked)
{
	if (bInputLocked == bLocked)
	{
		return;
	}
	bInputLocked = bLocked;

	// 컷신과 같은 경로로 차단: 컨트롤러가 입력 매핑을 바꾸고(누르고 있던 키는 뗄 때까지 무시) 가림 처리도 멈춤
	// - DisableInput/EnableInput은 횟수를 세지 않아 화면 전환, 튜토리얼 등 다른 잠금을 풀어 버리므로 쓰지 않음
	UCinematicSubsystem* Cinematics = GetWorld() ? GetWorld()->GetSubsystem<UCinematicSubsystem>() : nullptr;
	if (!Cinematics)
	{
		return;
	}

	if (bLocked)
	{
		Cinematics->BeginExternalCutscene();
	}
	else
	{
		Cinematics->EndExternalCutscene();
	}
}

bool AAreaExitBlocker::ResolveReturnLocation(FVector& OutLocation, FRotator& OutRotation) const
{
	const AActor* Target = ReturnPoint.Get();
	if (!IsValid(Target))
	{
		AGameModeBase* GameMode = UGameplayStatics::GetGameMode(this);
		AController* Controller = ActiveController.Get();
		Target = (GameMode && Controller) ? GameMode->FindPlayerStart(Controller) : nullptr;
	}
	if (!IsValid(Target))
	{
		UE_LOG(LogTemp, Warning, TEXT("[AreaExitBlocker] %s has no ReturnPoint / PlayerStart"), *GetName());
		return false;
	}

	OutLocation = Target->GetActorLocation();
	OutRotation = Target->GetActorRotation();
	return true;
}

APetCharacter* AAreaExitBlocker::ResolvePet() const
{
	if (APetCharacter* Pet = PlayerLookup::GetPartnerPet(this))
	{
		return Pet;
	}
	return Cast<APetCharacter>(UGameplayStatics::GetActorOfClass(this, APetCharacter::StaticClass()));
}

void AAreaExitBlocker::ApplyBlockedChannels()
{
	BlockingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	for (const TEnumAsByte<ECollisionChannel>& Channel : BlockedChannels)
	{
		BlockingBox->SetCollisionResponseToChannel(Channel, ECR_Block);
	}
}

bool AAreaExitBlocker::IsAliveCharacter(const ACharacter* Character)
{
	return IsValid(Character)
		&& (!Character->Implements<UTargetableInterface>() || ITargetableInterface::Execute_IsTargetAlive(Character));
}

// 대사 중 펫 AI가 움직이거나 돌아서지 않게 BT 일시정지
void AAreaExitBlocker::SetPetLogicPaused(bool bPaused)
{
	if (bPetLogicPaused == bPaused)
		return;

	APetCharacter* Pet = ActivePet.Get();
	AAIController* PetController = Pet ? Cast<AAIController>(Pet->GetController()) : nullptr;
	UBrainComponent* Brain = PetController ? PetController->GetBrainComponent() : nullptr;
	if (!Brain)
	{
		bPetLogicPaused = false;
		return;
	}

	bPetLogicPaused = bPaused;
	if (bPaused)
	{
		PetController->StopMovement();
		Brain->PauseLogic(TEXT("AreaExitBlocker"));
	}
	else
	{
		Brain->ResumeLogic(TEXT("AreaExitBlocker"));
	}
}

// 펫 정면 카메라는 가까워서 평소 높이의 말풍선이 떠 보이므로 연출 중에만 낮춤
void AAreaExitBlocker::SetBubbleHeightOverridden(bool bOverride)
{
	if (bBubbleHeightOverridden == bOverride)
		return;

	APetCharacter* Pet = ActivePet.Get();
	USpeechBubbleComponent* Bubble = Pet ? Pet->GetSpeechBubble() : nullptr;
	if (!Bubble)
	{
		bBubbleHeightOverridden = false;
		return;
	}

	if (bOverride)
	{
		if (!bOverrideBubbleHeight)
			return;
		SavedBubbleRelativeLocation = Bubble->GetRelativeLocation();
		FVector Location = SavedBubbleRelativeLocation;
		Location.Z = BubbleHeightDuringFocus;
		Bubble->SetRelativeLocation(Location);
	}
	else
	{
		Bubble->SetRelativeLocation(SavedBubbleRelativeLocation);
	}
	bBubbleHeightOverridden = bOverride;
}
