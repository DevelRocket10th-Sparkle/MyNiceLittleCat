#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/PetAIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Pet/SpeechBubbleComponent.h"
#include "Pet/GAS/Ability/GameplayAbility_PetMagnet.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Engine/World.h"
#include "Engine/HitResult.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
APetCharacter::APetCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	// 컨트롤러의 회전을 그대로 따르지 않고
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// 이동하는 방향을 바라봐요
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f); //! 임시 기본 회전 속도

	// 펫 캐릭터에 컨트롤러 세팅
	AIControllerClass = APetAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	// AI 경로 이동에도 가속을 사용해요
	GetCharacterMovement()->GetNavMovementProperties()->bUseAccelerationForPaths = true;

	// 이동 감각의 주요 설정값
	GetCharacterMovement()->MaxAcceleration = 1200.f; // 얼마나 빠르게 속도를 올릴지
	GetCharacterMovement()->BrakingDecelerationWalking = 1600.f; // 얼마나 빠르게 속도를 줄일지

	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));

	CarryPoint = CreateDefaultSubobject<USceneComponent>(TEXT("CarryPoint"));
	CarryPoint->SetupAttachment(GetMesh(), TEXT("CarrySocket"));

	// 말풍선
	SpeechBubble = CreateDefaultSubobject<USpeechBubbleComponent>(TEXT("SpeechBubble"));
	SpeechBubble->SetupAttachment(GetRootComponent()); // 캡슐 기준이라 메시 애니메이션에 흔들리지 않음

	// Collision 관련
	// 지형 충돌은 캡슐에서 담당
	GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pet"));
	// 외형 메시는 충돌 판정에서 제외
	GetMesh()->SetCollisionProfileName(TEXT("NoCollision"));
}

void APetCharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	GrantModeAbilities();

	//UE_LOG(LogTemp, Warning,
	//	TEXT("CarryPoint Parent=%s / Socket=%s / SocketExists=%d"),
	//	*GetNameSafe(CarryPoint->GetAttachParent()),
	//	*CarryPoint->GetAttachSocketName().ToString(),
	//	GetMesh()->DoesSocketExist(TEXT("CarrySocket")) ? 1 : 0);

	//const APetCharacter* NativeDefault = GetDefault<APetCharacter>();
	//const APetCharacter* BPDefault = GetClass()->GetDefaultObject<APetCharacter>();

	//UE_LOG(LogTemp, Warning,
	//	TEXT("CarrySocket Native=%s / BP=%s / Instance=%s"),
	//	*NativeDefault->GetCarryPoint()->GetAttachSocketName().ToString(),
	//	*BPDefault->GetCarryPoint()->GetAttachSocketName().ToString(),
	//	*CarryPoint->GetAttachSocketName().ToString());
}

void APetCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APetCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void APetCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(NavigationRecoveryTimer);
	bNavigationRecoveryPending = false;

	Super::EndPlay(EndPlayReason);
}

UAbilitySystemComponent* APetCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
}

void APetCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	CarryPoint->AttachToComponent(GetMesh(), 
		FAttachmentTransformRules::KeepRelativeTransform, TEXT("CarrySocket"));
}

void APetCharacter::InitializePet(const FPetInitializationData& _data)
{
	// 펫한테 필요한 초기 세팅을 해요
	InitializationData = _data;
	RequestedBehavior = _data.InitialBehavior;
	bInitialized = true;

	// 초기 세팅이 끝나면 컨트롤러에게 알려줘요
	if (APetAIController* PetController = Cast<APetAIController>(GetController()))
		PetController->StartPetLogic();
	// 컨트롤러가 아직 없다면 ? 이후 기존 OnPossess에서 진행
}

APawn* APetCharacter::GetPartnerPlayer() const
{
	APawn* Player = InitializationData.PartnerPlayer.Get();
	return (true == IsValid(Player)) ? Player : nullptr;
}

bool APetCharacter::RequestBehavior(EPetBehavior NewBehavior)
{
	if (false == bInitialized) return false;

	//! 테스트용
	//! MoveTo는 RequestMoveTo(_destination)로 호출 
	if ((NewBehavior == EPetBehavior::MoveTo) || (NewBehavior == EPetBehavior::AbilityMoveTo)) return false;

	if((EPetBehavior::Follow == NewBehavior) && (false == IsValid(GetPartnerPlayer())))
		return false;

	CancelAbilityMove(ActiveAbilityMoveRequestId);
	RequestedBehavior = NewBehavior;
	AnimationMovementStyle = EPetMovementStyle::Normal;
	return true;
}

void APetCharacter::DebugRequestIdle()
{
	RequestBehavior(EPetBehavior::Idle);
}

void APetCharacter::DebugRequestFollow()
{
	RequestBehavior(EPetBehavior::Follow);
}

void APetCharacter::DebugRequestMoveTo()
{
	RequestMoveTo(DebugMoveDestination);
}

bool APetCharacter::RequestMoveTo(const FVector& _destination)
{
	if (false == bInitialized) return false;
	if (true == _destination.ContainsNaN()) return false;

	CancelAbilityMove(ActiveAbilityMoveRequestId);

	MoveDestination = _destination;
	RequestedBehavior = EPetBehavior::MoveTo;
	AnimationMovementStyle = EPetMovementStyle::Normal;

	return true; // MoveTo 명령 접수 완료
}

bool APetCharacter::RequestMode(EPetMode _newMode)
{
	if(false == bInitialized)
		return false;

	// 올바른 모드가 요청된건지 확인해요
	switch (_newMode)
	{
	case EPetMode::Basic:
	case EPetMode::Magnet:
	case EPetMode::Reflect:
		break;
	default: return false;
	}

	if (_newMode == CurrentMode) return true;

	// 모드 갈아끼우고 델리게이트 보내요
	const EPetMode PrevMode = CurrentMode;
	CurrentMode = _newMode;

	OnPetModeChanged.Broadcast(PrevMode, CurrentMode);

	return true;
}

void APetCharacter::DebugRequestBasicMode()
{
	RequestMode(EPetMode::Basic);
}

void APetCharacter::DebugRequestMagnetMode()
{
	RequestMode(EPetMode::Magnet);
}

void APetCharacter::DebugRequestReflectMode()
{
	RequestMode(EPetMode::Reflect);
}

void APetCharacter::SetAimDirection(const FVector& _direction)
{
	if (_direction.ContainsNaN()) return;

	// 현재는 수평 방향만 사용
	FVector HorizontalDirection(_direction.X, _direction.Y, 0.f);

	// 플레이어 바로 위를 가리키는 경우에는 기존 방향을 유지
	if (false == HorizontalDirection.Normalize())
		return;

	AimDirection = HorizontalDirection;
}

bool APetCharacter::TryGetModeAbilityHandle(EPetMode _mode, FGameplayAbilitySpecHandle& _outHandle) const
{
	_outHandle = FGameplayAbilitySpecHandle();

	const FGameplayAbilitySpecHandle* Handle = ModeAbilityHandles.Find(_mode);
	if (nullptr == Handle || false == Handle->IsValid())
		return false;

	_outHandle = *Handle;
	return true;
}

void APetCharacter::RequestModeUseInput(EPetModeUseInputPhase Phase)
{
	if (false == bInitialized) return;

	// 누르는중 ? + 누름/놓음/취소 전달
	switch (Phase)
	{
	case EPetModeUseInputPhase::Pressed:
		if (true == bModeAbilityInputHeld) return;

		bModeAbilityInputHeld = true;
		break;

	case EPetModeUseInputPhase::Released:
	case EPetModeUseInputPhase::Canceled:
		if (false == bModeAbilityInputHeld) return;

		bModeAbilityInputHeld = false;
		break;

	default:
		return;
	}

	OnPetModeUseInput.Broadcast(Phase);
}

void APetCharacter::RequestSpecialAction()
{
	if (false == bInitialized) return;

	const FGameplayTag SpecialActionTag = FGameplayTag::RequestGameplayTag(TEXT("Pet.SpecialAction"));

	FGameplayEventData Payload;
	Payload.EventTag = SpecialActionTag;
	Payload.Instigator = GetPartnerPlayer();
	Payload.Target = this;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(this, SpecialActionTag, Payload);
}

FGuid APetCharacter::RequestAbilityMoveTo(const FVector& Destination)
{
	if (false == bInitialized || true == Destination.ContainsNaN() || true == ActiveAbilityMoveRequestId.IsValid())
		return FGuid();

	ActiveAbilityMoveRequestId = FGuid::NewGuid();

	// 기존 이동 Task에서 읽는 목적지 재사용
	MoveDestination = Destination;
	RequestedBehavior = EPetBehavior::AbilityMoveTo;
	AnimationMovementStyle = EPetMovementStyle::Normal;

	return ActiveAbilityMoveRequestId;
}

void APetCharacter::CompleteAbilityMove(const FGuid& RequestId, bool bSucceeded)
{
	if (false == ActiveAbilityMoveRequestId.IsValid() ||  ActiveAbilityMoveRequestId != RequestId)
		return;

	const FGuid FinishedRequestId = ActiveAbilityMoveRequestId;
	ActiveAbilityMoveRequestId.Invalidate();

	// 다른 행동으로 이미 바뀌었다면 그 행동을 유지
	if (RequestedBehavior == EPetBehavior::AbilityMoveTo)
		RequestedBehavior = (true == IsValid(GetPartnerPlayer())) ? EPetBehavior::Follow : EPetBehavior::Idle;

	OnPetAbilityMoveFinished.Broadcast(FinishedRequestId, bSucceeded);
}

void APetCharacter::CancelAbilityMove(const FGuid& RequestId)
{
	CompleteAbilityMove(RequestId, false);
}

bool APetCharacter::IsModeAbilityActive(EPetMode _mode) const
{
	if (false == IsValid(AbilitySystemComponent.Get()))
		return false;

	FGameplayAbilitySpecHandle Handle;
	if (false == TryGetModeAbilityHandle(_mode, Handle))
		return false;

	const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	return (nullptr != Spec && true == Spec->IsActive());
}

AActor* APetCharacter::GetCarriedActor() const
{
	if (false == IsValid(AbilitySystemComponent.Get())) return nullptr;

	FGameplayAbilitySpecHandle Handle;
	if (false == TryGetModeAbilityHandle(EPetMode::Magnet, Handle)) return nullptr;

	const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
	if (nullptr == Spec || false == Spec->IsActive()) return nullptr;

	const UGameplayAbility_PetMagnet* MagnetAbility = Cast<UGameplayAbility_PetMagnet>(Spec->GetPrimaryInstance());

	return (true == IsValid(MagnetAbility)) ? MagnetAbility->GetCarriedActor() : nullptr;
}

bool APetCharacter::CanResumeFollowingAt(const FVector& CapsuleLocation)
{
	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	APawn* Player = GetPartnerPlayer();

	if (false == IsValid(World) || false == IsValid(Capsule) || false == IsValid(Movement)
		|| false == IsValid(Player)|| CapsuleLocation.ContainsNaN())
		return false;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (false == IsValid(NavSystem))
		return false;

	const float Radius = Capsule->GetScaledCapsuleRadius();
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FVector FeetLocation = CapsuleLocation - FVector::UpVector * HalfHeight;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetRecoveryCheck), false, this);

	// 실제 펫의 충돌 응답을 검사에 사용
	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	const bool bBlocked = World->OverlapBlockingTestByChannel(
		CapsuleLocation, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Radius, HalfHeight), QueryParams, ResponseParams);

	if (true == bBlocked)
		return false;

	FHitResult GroundHit;

	const bool bFoundGround = World->LineTraceSingleByChannel(
		GroundHit, FeetLocation + FVector::UpVector * 10.f, FeetLocation - FVector::UpVector * (Movement->MaxStepHeight + 5.f), 
		Capsule->GetCollisionObjectType(), QueryParams, ResponseParams);

	if (false == bFoundGround || false == Movement->IsWalkable(GroundHit))
		return false;

	FNavLocation NavLocation;
	if (false == NavSystem->ProjectPointToNavigation(
		FeetLocation, NavLocation, FVector(20.f, 20.f, 100.f), &GetNavAgentPropertiesRef()))
		return false;

	constexpr float MaxHorizontalProjection = 5.f;

	if (FVector::DistSquared2D(FeetLocation, NavLocation.Location) > FMath::Square(MaxHorizontalProjection))
		return false;

	FVector PlayerFeetLocation = Player->GetActorLocation();
	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(Player))
	{
		if (const UCharacterMovementComponent* PlayerMovement = PlayerCharacter->GetCharacterMovement())
			PlayerFeetLocation = PlayerMovement->GetActorFeetLocation();
	}

	UNavigationPath* Path = UNavigationSystemV1::FindPathToLocationSynchronously(
			World, FeetLocation, PlayerFeetLocation, this);

	return IsValid(Path) && Path->IsValid() && false == Path->IsPartial();
}

bool APetCharacter::TryFindSafeRecoveryLocation(const FVector& PreferredLocation, FVector& OutLocation)
{
	OutLocation = FVector::ZeroVector;

	if (true == PreferredLocation.ContainsNaN()) return false;

	UWorld* World = GetWorld();
	APawn* Player = GetPartnerPlayer();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (false == IsValid(World) || false == IsValid(Player) || false == IsValid(Capsule) || false == IsValid(Movement))
		return false;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (false == IsValid(NavSystem))
		return false;

	FVector PlayerFeetLocation = Player->GetActorLocation();
	if (const ACharacter* PlayerCharacter = Cast<ACharacter>(Player))
	{
		if (const UCharacterMovementComponent* PlayerMovement = PlayerCharacter->GetCharacterMovement())
			PlayerFeetLocation = PlayerMovement->GetActorFeetLocation();
	}

	// 플레이어 주변 방향부터 검사
	FVector BaseDirection = (PreferredLocation - PlayerFeetLocation).GetSafeNormal2D();

	if (BaseDirection.IsNearlyZero()) BaseDirection = Player->GetActorForwardVector().GetSafeNormal2D();
	if (BaseDirection.IsNearlyZero()) BaseDirection = FVector::ForwardVector;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetRecoverySearch), false, this);

	QueryParams.AddIgnoredActor(Player);

	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());
	const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();

	constexpr int32 RingCount = 3;
	constexpr int32 DirectionsPerRing = 24;

	TArray<FVector> Candidates;
	Candidates.Reserve(RingCount * DirectionsPerRing);

	// 다른 높이의 지면으로 복귀하는 것을 제한
	const float GroundSearchHeight = Movement->MaxStepHeight + 10.f;

	for (int32 RingIndex = 0; RingIndex < RingCount; ++RingIndex)
	{
		const float SearchRadius = RecoveryStartRadius + RecoveryRadiusStep * RingIndex;

		for (int32 DirectionIndex = 0; DirectionIndex < DirectionsPerRing; ++DirectionIndex)
		{
			const float Angle = 360.f * DirectionIndex / DirectionsPerRing;

			const FVector Direction = BaseDirection.RotateAngleAxis(Angle, FVector::UpVector);

			const FVector SearchPoint = PlayerFeetLocation + Direction * SearchRadius;

			// 후보 주변에 NavMesh가 있는지 먼저 확인
			FNavLocation NavLocation;
			if (false == NavSystem->ProjectPointToNavigation(
				SearchPoint, NavLocation, FVector(30.f, 30.f, 100.f), &GetNavAgentPropertiesRef()))
				continue;

			// NavMesh 높이를 그대로 쓰지 않고 실제 지면을 찾음
			FVector GroundProbe = NavLocation.Location;
			GroundProbe.Z = PlayerFeetLocation.Z;

			FHitResult GroundHit;

			const bool bFoundGround = World->LineTraceSingleByChannel(
				GroundHit,
				GroundProbe + FVector::UpVector * GroundSearchHeight, GroundProbe - FVector::UpVector * GroundSearchHeight,
				Capsule->GetCollisionObjectType(), QueryParams, ResponseParams);

			if (false == bFoundGround || false == Movement->IsWalkable(GroundHit))
				continue;
	
			// 바닥에서 살짝 띄워 캡슐 중심 위치로 변환
			const FVector CandidateLocation = GroundHit.ImpactPoint + FVector::UpVector * (HalfHeight + 2.f);

			//// 충돌·지면·NavMesh·플레이어까지의 경로를 최종 검사
			//if (true == CanResumeFollowingAt(CandidateLocation))
			//{
			//	OutLocation = CandidateLocation;
			//	return true;
			//}
			Candidates.Add(CandidateLocation);
		}
	}

	// 선호 위치와 가까운 후보부터 검사
	Candidates.Sort([&PreferredLocation](const FVector& A, const FVector& B)
		{return FVector::DistSquared(A, PreferredLocation) < FVector::DistSquared(B, PreferredLocation);});

	for (const FVector& CandidateLocation : Candidates)
	{
		if (true == CanResumeFollowingAt(CandidateLocation))
		{
			OutLocation = CandidateLocation;
			return true;
		}
	}

	return false;
}
void APetCharacter::ResumeNavigationSafely(EMovementMode RestoreMode, uint8 RestoreCustomMode)
{
	CancelNavigationRecovery();

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (false == IsValid(Movement) || false == HasActorBegunPlay()|| IsActorBeingDestroyed())
		return;

	RecoveryMovementMode = RestoreMode;
	RecoveryCustomMovementMode = RestoreCustomMode;
	bNavigationRecoveryPending = true;

	Movement->StopMovementImmediately();
	Movement->DisableMovement();

	// 정상 위치라면 여기서 바로 이동을 복원
	TryCompleteNavigationRecovery();

	// 안전 위치가 아직 없을 때만 재시도
	if (true == bNavigationRecoveryPending)
		GetWorldTimerManager().SetTimer(NavigationRecoveryTimer, this, &APetCharacter::TryCompleteNavigationRecovery,0.5f, true);
}
void APetCharacter::CancelNavigationRecovery()
{
	if (UWorld* World = GetWorld())
		World->GetTimerManager().ClearTimer(NavigationRecoveryTimer);

	if (false == bNavigationRecoveryPending) return;
	bNavigationRecoveryPending = false;

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->SetMovementMode(RecoveryMovementMode, RecoveryCustomMovementMode);
	}
}

void APetCharacter::GrantModeAbilities()
{
	if (false == IsValid(AbilitySystemComponent.Get()))
		return;

	for (const auto& Entry : ModeAbilities)
	{
		const EPetMode Mode = Entry.Key;
		const TSubclassOf<UGameplayAbility> AbilityClass = Entry.Value;

		if (nullptr == AbilityClass || true == ModeAbilityHandles.Contains(Mode))
			continue;

		const FGameplayAbilitySpec Spec(AbilityClass,1, INDEX_NONE, this);
		const FGameplayAbilitySpecHandle Handle = AbilitySystemComponent->GiveAbility(Spec);

		if (true == Handle.IsValid())
			ModeAbilityHandles.Add(Mode, Handle);
	}
}

void APetCharacter::TryCompleteNavigationRecovery()
{
	if (false == bNavigationRecoveryPending) return;

	if (false == IsValid(GetCharacterMovement())) return;

	if (false == CanResumeFollowingAt(GetActorLocation()))
	{
		FVector RecoveryLocation;

		// 후보가 없으면 이동하지 않고 다음 재시도까지 대기
		if (false == TryFindSafeRecoveryLocation(GetActorLocation(), RecoveryLocation)) return;

		// 기둥 밖으로 복귀하는 것이므로 이동 경로는 Sweep하지 않음.
		// 목적지의 충돌·지면·경로는 위 함수에서 검사한 상태.
		const bool bMoved = SetActorLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);
		if (false == bMoved) return;
	}

	// 현재 위치 또는 복귀 위치가 유효하면 이동 복원
	CancelNavigationRecovery();
}

bool APetCharacter::CanUseMagnetPosition(const FVector& CapsuleLocation)
{
	UWorld* World = GetWorld();
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	UCharacterMovementComponent* Movement = GetCharacterMovement();

	if (false == IsValid(World) || false == IsValid(Capsule) || false == IsValid(Movement) || CapsuleLocation.ContainsNaN())
		return false;

	UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);

	if (false == IsValid(NavSystem)) return false;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetMagnetPositionCheck), false, this);
	const FCollisionResponseParams ResponseParams(Capsule->GetCollisionResponseToChannels());

	// 1. 목표 위치에서 캡슐이 장애물과 겹치는지
	const bool bBlocked = World->OverlapBlockingTestByChannel(
		CapsuleLocation, Capsule->GetComponentQuat(), Capsule->GetCollisionObjectType(),
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()),
		QueryParams, ResponseParams);

	if (true == bBlocked) return false;

	// 자성 위치는 지면에서 조금 떠 있을 수 있으므로
	// 일반 추종 검사보다 아래쪽 지면을 넓게 탐색
	const FVector FeetLocation = CapsuleLocation - FVector::UpVector * Capsule->GetScaledCapsuleHalfHeight();

	FHitResult GroundHit;

	const bool bFoundGround = World->LineTraceSingleByChannel(
		GroundHit, FeetLocation + FVector::UpVector * 10.f, FeetLocation - FVector::UpVector * 200.f,
		Capsule->GetCollisionObjectType(), QueryParams, ResponseParams);

	if (false == bFoundGround || false == Movement->IsWalkable(GroundHit))
		return false;

	// 2. 해당 지면 바로 근처에 NavMesh가 있는지
	FNavLocation NavLocation;
	if (false == NavSystem->ProjectPointToNavigation(GroundHit.ImpactPoint, NavLocation, FVector(20.f, 20.f, 100.f), &GetNavAgentPropertiesRef()))
		return false;

	return FVector::DistSquared2D(GroundHit.ImpactPoint, NavLocation.Location) <= FMath::Square(5.f);
}