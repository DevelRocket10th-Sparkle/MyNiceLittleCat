// 작성자: 우진
// 작성일자: 2026-10-14
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Pet/PetTypes.h"
#include "TestPetGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API ATestPetGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
    virtual void RestartPlayer(AController* NewPlayer) override;

    // =============================================================================
protected:
    UPROPERTY(EditDefaultsOnly, Category = "Pet")
    TSubclassOf<class APetCharacter> PetClass;

    // 펫 초기 행동(FPetInitializationData 세팅값)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet")
    EPetBehavior PetInitialBehavior = EPetBehavior::Idle;
private:
    UPROPERTY(Transient)
    TObjectPtr<class APetCharacter> Pet;
};
