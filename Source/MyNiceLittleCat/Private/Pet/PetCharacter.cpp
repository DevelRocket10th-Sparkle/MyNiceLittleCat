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

void APetCharacter::SetFollowTarget(AActor* _target)
{
	if (this == _target) return; // 펫 본인은 추종 대상에서 제외해요

	FollowTarget = (true == IsValid(_target)) ? _target : nullptr;
}

AActor* APetCharacter::GetFollowTarget() const
{
	return (true == IsValid(FollowTarget.Get())) ? FollowTarget.Get() : nullptr;
}
