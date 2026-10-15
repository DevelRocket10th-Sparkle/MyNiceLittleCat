#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/PetAIController.h"

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
}

void APetCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void APetCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void APetCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
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
