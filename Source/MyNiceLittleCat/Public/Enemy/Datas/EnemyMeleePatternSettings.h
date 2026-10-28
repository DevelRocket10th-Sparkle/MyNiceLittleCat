#pragma once

#include "CoreMinimal.h"
#include "Enemy/Datas/EnemyPatternExecutionSettings.h"
#include "EnemyMeleePatternSettings.generated.h"

class UAnimMontage;

// 데이터 에셋 내부에서 생성하는 소켓별 판정 설정. 별도 에셋 생성 불필요.
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Melee Trace Settings"))
class MYNICELITTLECAT_API UEnemyMeleeTraceSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName SourceName = TEXT("Body");

#if WITH_EDITORONLY_DATA
	// 목록 조회용 메시 에셋. 실제 판정은 SourceName에 등록된 메시 컴포넌트 사용.
	UPROPERTY(EditAnywhere, Category = "Melee", meta = (AllowedClasses = "/Script/Engine.SkeletalMesh,/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> SocketSourceMesh;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (GetOptions = "GetSocketOptions"))
	TArray<FName> SocketNames;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.1", Units = "cm"))
	float Radius = 45.f;

#if WITH_EDITOR
	UFUNCTION()
	TArray<FName> GetSocketOptions() const;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

// 근접 GA 전용 실행 설정; 몽타주를 사용하지 않는 다른 GA에는 지정할 필요 없음
UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced, meta = (DisplayName = "Melee Pattern Settings"))
class MYNICELITTLECAT_API UEnemyMeleePatternSettings : public UEnemyPatternExecutionSettings
{
	GENERATED_BODY()
public:
	// 캐릭터 및 패턴별 공격 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UAnimMontage> Montage;
	// 공격 몽타주 시작 섹션; None이면 기본 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName MontageSection;
	// NotifyState 구간 동안 각 소켓의 이전 위치에서 현재 위치까지 Sphere Sweep
	UPROPERTY(EditAnywhere, Instanced, NoClear, BlueprintReadOnly, Category = "Melee", meta = (DisplayName = "Melee Traces"))
	TArray<TObjectPtr<UEnemyMeleeTraceSettings>> MeleeTraceSettings;
	// 플레이어에게 전달할 공격력 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;
	// 배열 추가 시 판정 설정 객체를 생성
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};
