#pragma once
#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "GA_EnemyPatternAttack.generated.h"

class AEnemyCharacter;
class UEnemyPatternComponent;

// 패턴 데이터의 몽타주를 사용하는 기본 근접 공격: 원거리 및 아레나 공격은 별도 GA로 확장
UCLASS()
class MYNICELITTLECAT_API UGA_EnemyPatternAttack : public UGameplayAbility
{
	GENERATED_BODY()
public :
	// 서버 실행 및 Enemy별 인스턴스 설정
	UGA_EnemyPatternAttack();
	// 실행 컨텍스트 복사 후 이동과 회전을 제한하고 몽타주 실행
	// 요청 컨텍스트를 복사하고 판정 이벤트 및 몽타주 실행
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	// 정상 완료 및 취소 모두 이동과 회전 제한을 정리
	// 정상 완료와 취소 모두 이 능력이 획득한 제한 및 참조 정리
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
protected :
	// 몽타주가 끝났을 때 정상 종료
	UFUNCTION()
	void HandleMontageCompleted();
	// 몽타주 중단 또는 취소 시 취소 결과로 종료
	UFUNCTION()
	void HandleMontageCancelled();
	// 판정 이벤트를 현재 실행에 한해 처리하고 조건에 맞는 타깃에 피해 적용
	// 현재 요청의 판정 시점에 실제 타깃 위치를 검사하고 피해 GE 적용
	UFUNCTION()
	void HandleHitEvent(FGameplayEventData Payload);
	// 현재 실행용으로 복사한 불변 패턴 설정
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	FEnemyPatternDefinition ExecutionPattern;
	// 실행 시작 시 타깃: Blackboard 변경과 독립적으로 유지
	UPROPERTY(BlueprintReadOnly, Category = "Pattern")
	TObjectPtr<AActor> ExecutionTarget;
private :
	// 공격 주체: 사망 또는 제거 여부 검사에 사용
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	// 현재 요청의 유효성을 확인할 컴포넌트
	TWeakObjectPtr<UEnemyPatternComponent> PatternComponent;
	// 이번 GA가 실행하는 요청 번호
	int64 RequestId = 0;
	// 이 GA가 획득한 이동 제한만 해제하기 위한 상태
	bool bOwnsMovementLock = false;
	// 이 GA가 획득한 회전 제한만 해제하기 위한 상태
	bool bOwnsRotationLock = false;
	// 같은 실행에서 기본 타깃에 중복 피해를 주지 않기 위한 상태
	bool bHitApplied = false;
};
