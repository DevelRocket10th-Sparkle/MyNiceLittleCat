#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/PetAIController.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayAbilitySpec.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameplayTagContainer.h"

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
}

void APetCharacter::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	GrantModeAbilities();
}

void APetCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APetCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

UAbilitySystemComponent* APetCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent.Get();
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
	if ((EPetBehavior::MoveTo == NewBehavior)) return false;

	if((EPetBehavior::Follow == NewBehavior) && (false == IsValid(GetPartnerPlayer())))
		return false;


		RequestedBehavior = NewBehavior;
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

	MoveDestination = _destination;
	RequestedBehavior = EPetBehavior::MoveTo;

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
