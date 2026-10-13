#include "Pet/PetAIController.h"
#include "Components/StateTreeAIComponent.h"
#include "Pet/PetCharacter.h"

APetAIController::APetAIController()
{
	StateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("StateTree"));
	BrainComponent = StateTreeComponent;

	StateTreeComponent->SetStartLogicAutomatically(false); // StateTree 자동 실행 해제
	bStartAILogicOnPossess = false;
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

void APetAIController::TryStartPetLogic()
{
	APetCharacter* Pet = Cast<APetCharacter>(GetPawn());

	if (false == IsValid(Pet)) return;
	if (false == IsValid(Pet->GetFollowTarget())) return;

	if (false == StateTreeComponent->IsRunning())
		StateTreeComponent->StartLogic();
}
