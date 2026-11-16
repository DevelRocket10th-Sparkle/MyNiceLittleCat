// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.10.22.
* 수정 일자: 2026.11.17. (주석 정리)
* 근접 공격 GA(GA_Enemy_MeleeAttack)의 패턴별 실행 설정
* - 공격 몽타주와 시작 섹션
* - 판정 구간(AnimNotifyState) 동안 Sphere Sweep할 소켓 목록 (판정 소스별)
*/

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

public :

	// 판정 소스: 소켓을 찾을 메시
	// - Body: Enemy 본체 스켈레탈 메시
	// - Weapon: 본체에 붙은 무기 메시 (아래 소켓을 모두 가진 자식 메시를 자동 탐색)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName SourceName = TEXT("Body");

#if WITH_EDITORONLY_DATA
	// 에디터 전용: SocketNames 드롭다운에 소켓 목록을 띄우기 위한 메시 에셋 (게임 실행에는 쓰지 않음)
	UPROPERTY(EditAnywhere, Category = "Melee", meta = (AllowedClasses = "/Script/Engine.SkeletalMesh,/Script/Engine.StaticMesh"))
	TSoftObjectPtr<UObject> SocketSourceMesh;
#endif

	// 판정할 소켓: 각 소켓의 이전 위치 → 현재 위치로 Sweep
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (GetOptions = "GetSocketOptions"))
	TArray<FName> SocketNames;

	// Sweep 구 반지름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee", meta = (ClampMin = "0.1", Units = "cm"))
	float Radius = 45.f;

#if WITH_EDITOR
	// 에디터 전용: SocketSourceMesh의 소켓 이름 목록
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

public :

	// 캐릭터 및 패턴별 공격 몽타주
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	TObjectPtr<UAnimMontage> Montage;

	// 공격 몽타주 시작 섹션: None이면 기본 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Melee")
	FName MontageSection;

	// NotifyState 구간 동안 각 소켓의 이전 위치에서 현재 위치까지 Sphere Sweep
	UPROPERTY(EditAnywhere, Instanced, NoClear, BlueprintReadOnly, Category = "Melee", meta = (DisplayName = "Melee Traces"))
	TArray<TObjectPtr<UEnemyMeleeTraceSettings>> MeleeTraceSettings;

	// 배열에 항목을 추가하면 판정 설정 객체를 생성
#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif
};
