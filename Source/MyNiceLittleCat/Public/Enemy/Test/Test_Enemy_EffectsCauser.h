// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.15.
* Enemy 구현 중 테스트용 이펙트 발동 액터
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"

#include "Test_Enemy_EffectsCauser.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class AEnemyCharacter;

UCLASS()
class MYNICELITTLECAT_API ATest_Enemy_EffectsCauser : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ATest_Enemy_EffectsCauser();

protected:
	// 게임 시작 시 초기화한다. or when spawned
	virtual void BeginPlay() override;

	// 진행 중인 전환 시퀀스의 상태를 확인한다.
	virtual void Tick(float DeltaTime) override;

public :

	UFUNCTION(CallInEditor, Category = "GAS")
	void ApplyGameplayEffect();

	UFUNCTION(CallInEditor, Category = "GAS")
	void RemoveAllEffects();
	
	UFUNCTION(CallInEditor, Category = "GAS")
	void AddTag();

	UFUNCTION(CallInEditor, Category = "GAS")
	void RemoveAllTag();

	UFUNCTION(CallInEditor, Category = "GAS")
	void ResetTarget();

private:

	UAbilitySystemComponent* GetTestTargetASC() const;

protected :

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TSubclassOf<UGameplayEffect> GameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	float EffectLevel = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TObjectPtr<AEnemyCharacter> Target;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GAS")
	TObjectPtr<AEnemyCharacter> Source;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GAS")
	FGameplayTag TagToTarget;

private :

	// Target이 바뀌어도 대상별 Effects 적용 기록 유지
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, TArray<FActiveGameplayEffectHandle>> AppliedEffects;

	// Target이 바뀌어도 대상별 Tags 적용 기록 유지
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FGameplayTagContainer> AddedTags;
};
