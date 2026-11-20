#include "Frameworks/PlayerLookup.h"

#include "Kismet/GameplayStatics.h"

#include "Characters/PlayerCharacter.h"
#include "Characters/PlayerMainController.h"
#include "Pet/PetCharacter.h"

APlayerCharacter* PlayerLookup::GetAlivePlayer(const UObject* WorldContext)
{
	APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(WorldContext, 0));
	return Player && !Player->GetbIsDead() ? Player : nullptr;
}

APetCharacter* PlayerLookup::GetPartnerPet(const UObject* WorldContext)
{
	const APlayerMainController* Controller = Cast<APlayerMainController>(UGameplayStatics::GetPlayerController(WorldContext, 0));
	return Controller ? Controller->GetPartnerPet() : nullptr;
}
