// 작성자: 우진
// 작성일자: 2026-10-12

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PetCharacter.generated.h"

// 괴냥이 클래스
UCLASS()
class MYNICELITTLECAT_API APetCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APetCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Pet|Follow")
	void SetFollowTarget(AActor* _target); //! 추후 플레이어 캐릭터로 변경 예정

	UFUNCTION(BlueprintPure, Category = "Pet|Follow")
	AActor* GetFollowTarget() const; //! 추후 플레이어 캐릭터로 변경 예정

	// =============================================================================
private:
	UPROPERTY(EditInstanceOnly, Category = "Pet|Follow")
	TObjectPtr<AActor> FollowTarget; //! 추후 플레이어 캐릭터로 변경 예정
};
