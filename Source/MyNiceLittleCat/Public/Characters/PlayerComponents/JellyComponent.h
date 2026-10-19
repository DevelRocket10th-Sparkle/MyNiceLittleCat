// 작성자: 강진구
// 작성일자: 2026-10-19
// 
// 캐릭터용 역경직 젤리 컴포넌트 (맞으면 찌그러짐)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttributeSet.h"
#include "JellyComponent.generated.h"

class USkeletalMeshComponent;
struct FOnAttributeChangeData;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class MYNICELITTLECAT_API UJellyComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UJellyComponent();

	UFUNCTION(BlueprintCallable, Category = "Jelly")
	void StartJelly();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnHealthChanged(const FOnAttributeChangeData& Data);
	void StopJelly();

protected:
	// 이 attribute가 감소하면 출렁임 시작
	UPROPERTY(EditAnywhere, Category = "Jelly")
	FGameplayAttribute HealthAttribute;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Amount = 0.65f;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Frequency = 25.f;

	UPROPERTY(EditAnywhere, Category = "Jelly")
	float Damping = 7.f;


	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> Mesh;

	FVector BaseScale = FVector::OneVector;
	float Elapsed = 0.f;
		
};
