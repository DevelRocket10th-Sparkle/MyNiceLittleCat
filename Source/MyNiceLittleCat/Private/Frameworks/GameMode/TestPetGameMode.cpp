#include "Frameworks/GameMode/TestPetGameMode.h"
#include "Pet/PetCharacter.h"
#include "Pet/PetAIController.h"
#include "Characters/PlayerMainController.h"

void ATestPetGameMode::RestartPlayer(AController* NewPlayer)
{
	Super::RestartPlayer(NewPlayer);

	if (false == IsValid(NewPlayer)) return;

    APawn* PlayerPawn = NewPlayer->GetPawn();
    if (false == IsValid(PlayerPawn)) return;

	if (false == IsValid(Pet.Get()))
	{
		if (nullptr == PetClass) return;

		// 펫 스폰하기
        const FVector SpawnLocation = PlayerPawn->GetActorLocation() + PlayerPawn->GetActorRightVector() * 300.f;

        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

        Pet = GetWorld()->SpawnActor<APetCharacter>(PetClass, SpawnLocation, PlayerPawn->GetActorRotation(), SpawnParams);
	}

	if (false == IsValid(Pet.Get())) return;

	if (nullptr == Pet->GetController())
		Pet->SpawnDefaultController();

	// GameMode가 들고 있는 펫 초기화 데이터를 이용해 펫을 초기화해요
	FPetInitializationData InitData;
	InitData.PartnerPlayer = PlayerPawn;
	InitData.InitialBehavior = PetInitialBehavior;

	Pet->InitializePet(InitData);

	if (APlayerMainController* PlayerController = Cast<APlayerMainController>(NewPlayer))
		PlayerController->SetPartnerPet(Pet);
}
