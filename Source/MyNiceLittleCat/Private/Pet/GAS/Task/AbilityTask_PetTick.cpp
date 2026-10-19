#include "Pet/GAS/Task/AbilityTask_PetTick.h"

UAbilityTask_PetTick::UAbilityTask_PetTick()
{
	bTickingTask = true;
}

void UAbilityTask_PetTick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (ShouldBroadcastAbilityTaskDelegates())
		OnTick.Broadcast(DeltaTime);
}

void UAbilityTask_PetTick::OnDestroy(bool bInOwnerFinished)
{
	OnTick.Clear();

	Super::OnDestroy(bInOwnerFinished);
}

UAbilityTask_PetTick* UAbilityTask_PetTick::CreatePetTick(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<UAbilityTask_PetTick>(OwningAbility);
}
