/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 진행 플래그(UGameProgressSubsystem)에 따라 소유 액터를 없애거나 끄는 컴포넌트
* - 레벨 시작 시(세이브 로드 포함)와 게임 중 플래그가 바뀐 순간 모두 반영
* - 예: 최종 보스 처치 플래그가 켜지면 보스 트리거·마을 입구 차단 액터 제거
* - 진행 상태는 서브시스템 한 곳에만 두고, 각 액터는 자기 반응만 정함 (액터끼리 서로 참조하지 않음)
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProgressFlagGateComponent.generated.h"

UENUM(BlueprintType)
enum class EProgressFlagGateAction : uint8
{
	// 소유 액터 제거
	DestroyOwner,
	// 숨김 + 충돌 끔 (액터는 남김)
	DisableOwner,
};

UCLASS(ClassGroup = (Progress), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UProgressFlagGateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProgressFlagGateComponent();

protected:
	// 현재 플래그 상태 반영 후 변경 이벤트 구독
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleProgressFlagChanged(FName ChangedFlag, bool bEnabled);

	// 플래그 상태가 조건에 맞으면 Action 실행
	void Evaluate();
	void ApplyAction();

protected:
	// 감시할 진행 플래그 (None이면 아무것도 하지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress")
	FName Flag;

	// true: 플래그가 켜지면 실행 / false: 플래그가 꺼져 있으면 실행
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress")
	bool bActWhenFlagSet = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progress")
	EProgressFlagGateAction Action = EProgressFlagGateAction::DestroyOwner;

private:
	bool bApplied = false;
};
