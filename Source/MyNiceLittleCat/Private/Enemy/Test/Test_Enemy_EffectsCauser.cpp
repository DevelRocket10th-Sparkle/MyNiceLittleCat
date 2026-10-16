// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Test/Test_Enemy_EffectsCauser.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/GAS/EnemyAttributeSet.h"

// Sets default values
ATest_Enemy_EffectsCauser::ATest_Enemy_EffectsCauser()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

}

// Called when the game starts or when spawned
void ATest_Enemy_EffectsCauser::BeginPlay()
{
	Super::BeginPlay();

}

// Called every frame
void ATest_Enemy_EffectsCauser::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ATest_Enemy_EffectsCauser::ApplyGameplayEffect()
{
	if (!Source || !Target)
		return;
	if (!GameplayEffectClass)
		return;

	UAbilitySystemComponent* SourceASC = Source->GetAbilitySystemComponent();
	if (!SourceASC)
		return;
	UAbilitySystemComponent* TargetASC = GetTestTargetASC();
	if (!TargetASC)
		return;

	// 컨택스트 설정 (이펙트의 정보들을 설정)
	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(Source);			// 주로 이펙트의 발생 원인 에셋/데이터 (추가정보. 이벤트 발생 주체는 AdDInstigator()로 등록한 개체)
	EffectContext.AddInstigator(Source, Source);	// 이펙트를 발생한 주체와 매체

	// 이펙트 스펙 설정
	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(GameplayEffectClass, EffectLevel, EffectContext);
	if (!SpecHandle.IsValid())
		return;

	// ASC가 대상에게 스펙 적용
	FActiveGameplayEffectHandle ActiveEffectHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

	// 이펙트 적용 기록 (Instant 효과는 활성 상태로 남지 않으므로 기록하지 않음)
	if (ActiveEffectHandle.IsValid() && TargetASC->GetActiveGameplayEffect(ActiveEffectHandle))
	{
		const TWeakObjectPtr<UAbilitySystemComponent> KeyASC(TargetASC);
		AppliedEffects.FindOrAdd(KeyASC).AddUnique(ActiveEffectHandle);
	}

}

void ATest_Enemy_EffectsCauser::RemoveAllEffects()
{
	UAbilitySystemComponent* TargetASC = GetTestTargetASC();
	if (!TargetASC)
		return;

	const TWeakObjectPtr<UAbilitySystemComponent> Key(TargetASC);

	// 기록에서 Handle 꺼내서 꺼내서 EffectHandles에 복사하고 원본 지우기
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	if (!AppliedEffects.RemoveAndCopyValue(Key, EffectHandles))
	{
		return;
	}

	for (const FActiveGameplayEffectHandle& EffectHandle : EffectHandles)
	{
		// 모든 스택 제거 (이미 만료된 효과는 제거하지 않아도 됨)
		TargetASC->RemoveActiveGameplayEffect(EffectHandle, -1);
	}
}

void ATest_Enemy_EffectsCauser::AddTag()
{
	UAbilitySystemComponent* TargetASC = GetTestTargetASC();
	if (!TargetASC || !TagToTarget.IsValid())
		return;

	const TWeakObjectPtr<UAbilitySystemComponent> Key(TargetASC);
	FGameplayTagContainer& TargetAddedTags = AddedTags.FindOrAdd(Key);

	// 이미 추가한 태그면 중복 추가하지 않음
	if (TargetAddedTags.HasTagExact(TagToTarget))
		return;

	TargetASC->AddLooseGameplayTag(TagToTarget);
	TargetAddedTags.AddTag(TagToTarget);
}

void ATest_Enemy_EffectsCauser::RemoveAllTag()
{
	UAbilitySystemComponent* TargetASC = GetTestTargetASC();
	if (!TargetASC)
		return;

	const TWeakObjectPtr<UAbilitySystemComponent> Key(TargetASC);

	// 기록에서 Tags 꺼내서 꺼내서 Tags에 복사하고 원본 지우기
	FGameplayTagContainer TargetAddedTags;
	if (!AddedTags.RemoveAndCopyValue(Key, TargetAddedTags))
		return;
}

void ATest_Enemy_EffectsCauser::ResetTarget()
{
	UAbilitySystemComponent* TargetASC = GetTestTargetASC();
	if (!Target || !TagToTarget.IsValid())
		return;

	const UEnemyAttributeSet* Attributes = Target->GetAttributeSet();
	if (!IsValid(Attributes))
		return;

	const UEnemyAttributeSet* DefaultAttributes = Attributes->GetClass()->GetDefaultObject<UEnemyAttributeSet>();
	
	// 이펙트, 태그 정리
	RemoveAllEffects();
	RemoveAllTag();
	
	TargetASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetMaxHealthAttribute(),
		DefaultAttributes->GetMaxHealth()
	);
	
	TargetASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetHealthAttribute(),
		DefaultAttributes->GetHealth()
	);
	
	TargetASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetAttackPowerAttribute(),
		DefaultAttributes->GetAttackPower()
	);

	TargetASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetDamageAttribute(),
		0.f
	);
}

UAbilitySystemComponent* ATest_Enemy_EffectsCauser::GetTestTargetASC() const
{
	return IsValid(Target) ? Target->GetAbilitySystemComponent() : nullptr;
}
