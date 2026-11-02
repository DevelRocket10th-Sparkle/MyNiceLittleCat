#include "Pet/PetAIController.h"
#include "Components/StateTreeAIComponent.h"
#include "Pet/PetCharacter.h"
#include "Navigation/PathFollowingComponent.h"

APetAIController::APetAIController()
{
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTree"));
	BrainComponent = StateTreeComponent;

	StateTreeComponent->SetStartLogicAutomatically(false); // StateTree 자동 실행 해제
	bStartAILogicOnPossess = false;

	// 원인 확인용: 도착 높이 허용치를 캡슐 절반 높이의 2배로
	if (UPathFollowingComponent* PathFollowing = GetPathFollowingComponent())
		PathFollowing->SetPreciseReachThreshold(1.1f, 2.f);
}

void APetAIController::BeginPlay()
{
	Super::BeginPlay();

	TryStartPetLogic();
}

void APetAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (true == HasActorBegunPlay())
		TryStartPetLogic();
}

void APetAIController::OnUnPossess()
{
	StateTreeComponent->StopLogic(TEXT("Pet unpossessed"));
	StopMovement();

	Super::OnUnPossess();
}

void APetAIController::StartPetLogic()
{
	if (true == HasActorBegunPlay())
		TryStartPetLogic();
}

void APetAIController::TryStartPetLogic()
{
	APetCharacter* Pet = Cast<APetCharacter>(GetPawn());

	if (false == IsValid(Pet)) return;

	// 펫 초기 세팅 진행 확인
	if (false == Pet->IsPetInitialized()) return;

	if (false == StateTreeComponent->IsRunning())
		StateTreeComponent->StartLogic();
}
