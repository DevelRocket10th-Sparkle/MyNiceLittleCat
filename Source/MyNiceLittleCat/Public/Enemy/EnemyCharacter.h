// Fill out your copyright notice in the Description page of Project Settings.
/*
* �ۼ���: ������
* �ۼ� ����: 2026.10.14.
* ����/���� ĳ������ �⺻ Ŭ����
* - GAS ����
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "AbilitySystemInterface.h"
#include "EnemyTypes.h"
#include "Interfaces/TargetableInterface.h"

#include "EnemyCharacter.generated.h"

class UEnemyAttributeSet;
class UEnemyConfigData;

UCLASS()
class MYNICELITTLECAT_API AEnemyCharacter : public ACharacter, public IAbilitySystemInterface, public ITargetableInterface
{
	GENERATED_BODY()

public:

	// Sets default values for this character's properties
	AEnemyCharacter();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent()const override;

	// ��� ��ó�� - ��� ���� ���� ������ ����
	virtual void FinishDeath();

	// Getter: AttributeSet
	inline UEnemyAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// Getter: bWantsToMvoe
	bool GetWantsToMove() const { return bWantsToMove; }
	// Setter: bWantsToMove
	void SetWantsToMove(bool bNewWantsToMove)	{ bWantsToMove = bNewWantsToMove; }

	// ITargetable ��ȿ�� Ÿ������ üũ
	virtual bool IsTargetAlive_Implementation() const override;

protected:

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Enemy �ɷ�ġ ����
	UFUNCTION(CallInEditor)
	virtual void ApplyEnemyConfig();

	// ��� ��ó�� - ��� ���� ���� ����
	virtual void BeginDeath();	// �ʿ��� ��� EnemyTypes�� ����� ����ü FEnemyDeathContext �ޱ�

	/* ��������Ʈ ���� �Լ� */

	void HandleHealthChanged(float NewHealth, float MaxHealth);

	void HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted);

protected :

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> ASC;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UEnemyAttributeSet> AttributeSet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TObjectPtr<UEnemyConfigData> EnemyConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Life")
	EEnemyLifeState LifeState = EEnemyLifeState::Alive;

	// AnimInstance���� �÷��̾� �̵� �ִϸ��̼� ��� ���θ� �����ϱ� ���� ���
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Movement")
	bool bWantsToMove = false;

	/* AnimMontage */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation")
	TObjectPtr<UAnimMontage> DeathMontage;

};
