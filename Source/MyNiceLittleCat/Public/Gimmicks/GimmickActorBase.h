/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 켜짐/꺼짐 상태를 가지고 다른 기믹과 신호를 주고받는 기믹의 공통 베이스 액터입니다.
/// 상태가 바뀌면 델리게이트를 브로드캐스트하고, LinkedActors 중 IActivatableInterface 구현체에 Activate/Deactivate를 보냅니다.
/// 다른 기믹의 신호도 받을 수 있습니다 (SignalRule: 하나라도 켜지면 / 모두 켜지면, bInvertSignal로 반전).
/// 같은 액터의 UGimmickMoverComponent(bFollowActivation)는 상태에 맞춰 열림/닫힘 위치로 움직입니다.
/// bSaveState를 켜면 세이브(체크포인트 상호작용 등) 시점의 상태를 저장하고, 맵을 다시 열면 그 상태로 시작합니다.
/// E 상호작용 기믹은 AInteractableBase(레버·버튼), 상호작용 없는 기믹은 이 클래스를 직접 상속합니다(문·감압 발판).
/// 에디터에서 선택하면 보내는/받는 신호 연결이 뷰포트에 선으로 표시됩니다 (UGimmickLinkVisualizerComponent).
/// SignalLinkEffect를 지정하면 게임에서 신호를 받을 때 보낸 기믹 → 이 기믹으로 빛 입자가 흘러가고, 도착하는 시점(SignalLinkTravelTime)에 신호가 적용됩니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "GimmickActorBase.generated.h"

class AGimmickActorBase;
class UGameProgressSaveGame;
class UGimmickLinkVisualizerComponent;
class UNiagaraSystem;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnGimmickActivationChanged, AGimmickActorBase*, Gimmick, bool, bActivated, AActor*, ChangeInstigator);

// 받은 신호로 켜질 조건
UENUM(BlueprintType)
enum class EGimmickSignalRule : uint8
{
	// 신호를 보낸 기믹 중 하나라도 켜져 있으면 켜짐
	Any,
	// 필요한 수(RequiredSignalCount, 0이면 이 액터를 LinkedActors에 넣은 기믹 수)만큼 켜져 있으면 켜짐
	All
};

UCLASS(Abstract)
class MYNICELITTLECAT_API AGimmickActorBase : public AActor, public IActivatableInterface
{
	GENERATED_BODY()

public:
	AGimmickActorBase();

	// IActivatableInterface (다른 기믹의 신호 수신)
	virtual void Activate_Implementation(AActor* Activator) override;
	virtual void Deactivate_Implementation(AActor* Activator) override;

	// 활성 상태 변경 (같은 상태면 무시). 델리게이트 브로드캐스트 + 연결 대상에 신호 전달
	UFUNCTION(BlueprintCallable, Category = "Gimmick")
	void SetActivated(bool bNewActivated, AActor* ChangeInstigator);

	UFUNCTION(BlueprintPure, Category = "Gimmick")
	bool IsActivated() const { return bActivated; }

	// 시작 상태를 적용하는 중인지 (세이브 복원, 시작 신호). 연출(소리·애니메이션)을 건너뛸 때 확인
	UFUNCTION(BlueprintPure, Category = "Gimmick")
	bool IsApplyingInitialState() const { return bApplyingInitialState; }

	// 세이브에 쓰는 이름 (SaveId, 비면 액터 이름)
	UFUNCTION(BlueprintPure, Category = "Gimmick|Save")
	FName GetSaveId() const;

	// 신호를 보낼 대상
	const TArray<TObjectPtr<AActor>>& GetLinkedActors() const { return LinkedActors; }

	// 이 액터를 LinkedActors에 넣은 기믹 (월드를 검색하므로 매 프레임 호출은 에디터 표시용으로만)
	void GetLinkedSenders(TArray<AGimmickActorBase*>& OutSenders) const;

	// 읽기 전용 신호 정보(Linked Senders / Required Signals) 갱신
	void RefreshSignalInfo();

	// 켜지는 데 필요한 신호 수 (Any = 1, All = RequiredSignalCount 또는 자동 개수)
	int32 GetRequiredSignalCount() const;

public:
	// 활성 상태가 바뀔 때 브로드캐스트
	UPROPERTY(BlueprintAssignable, Category = "Gimmick")
	FOnGimmickActivationChanged OnActivationChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	// 상태 변경 시 자식 클래스용 처리 (C++)
	virtual void HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator) {}

	// 상태 변경 시 BP 연출용 이벤트 (레버 애니메이션, 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "Gimmick", meta = (DisplayName = "On Activation Changed"))
	void K2_OnActivationChanged(bool bNewActivated, AActor* ChangeInstigator);

	// 받은 신호가 바뀌었을 때. 기본 = SignalRule에 맞춰 SetActivated
	virtual void HandleSignalChanged(AActor* Source, bool bSignalOn);

	// 받은 신호 기준으로 켜져야 하는지
	bool EvaluateSignals() const;

	// 이동 컴포넌트를 현재 상태에 맞춤. 자식 클래스가 열림 조건을 바꿀 때 override (예: 문이 막혀 있으면 열린 채 유지)
	virtual void UpdateMovers(bool bInstant);

	// LinkedActors에 현재 상태 신호 전달
	void NotifyLinkedActors();

	// 이 액터를 LinkedActors에 넣은 기믹 수
	int32 CountLinkedSenders() const;

	// 세이브 직전 현재 상태 기록
	void WriteSaveState(UGameProgressSaveGame* SaveGame);

	// 받은 신호를 바로 또는 연결 이펙트 도착 뒤 적용
	void QueueSignal(AActor* Source, bool bSignalOn);
	void ApplySignal(TWeakObjectPtr<AActor> Source, bool bSignalOn);

	// 보낸 기믹 → 이 기믹으로 연결 이펙트 재생
	void PlaySignalLinkEffect(AActor* Source);

	// 기믹 효과음 재생 (비어 있으면 무시). bPlaySoundAs2D면 일시정지 중에도 들리는 2D UI 소리, 아니면 액터 위치
	void PlayGimmickSound(USoundBase* Sound) const;

	// 연결 이펙트가 나가고 도착하는 지점 (SignalLinkPointTag 컴포넌트 위치, 없으면 보이는 메시들 윗면 가운데)
	static FVector GetSignalLinkPoint(const AActor* Actor);

protected:
	// 시작 시 활성 상태 (BeginPlay에서 연결 대상에도 신호 전달). 저장된 상태가 있으면 그쪽이 우선
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bStartActivated = false;

	// 상태 변경 시 신호를 받을 액터 (IActivatableInterface 구현체만 신호 수신)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Interaction")
	TArray<TObjectPtr<AActor>> LinkedActors;

	// 다른 기믹의 신호로 상태를 바꿀지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal")
	bool bReceiveSignals = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal", meta = (EditCondition = "bReceiveSignals"))
	EGimmickSignalRule SignalRule = EGimmickSignalRule::Any;

	// All일 때 필요한 켜진 신호 수 (0이면 자동 = 이 액터를 LinkedActors에 넣은 기믹 수, 연결이 없으면 켜지지 않음)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal", meta = (ClampMin = "0", DisplayName = "Required Signal Count (0 = Auto)", EditCondition = "bReceiveSignals && SignalRule == EGimmickSignalRule::All"))
	int32 RequiredSignalCount = 0;

	// [읽기 전용] 이 액터를 LinkedActors에 넣은 기믹 수 (에디터에서 선택하면 갱신)
	UPROPERTY(VisibleInstanceOnly, Transient, DuplicateTransient, Category = "Gimmick|Signal", meta = (DisplayName = "Linked Senders"))
	int32 LinkedSenderCount = 0;

	// [읽기 전용] 켜지는 데 실제로 필요한 신호 수 (Any = 1, All = 위 값 또는 자동 개수, 0이면 신호로는 켜지지 않음)
	UPROPERTY(VisibleInstanceOnly, Transient, DuplicateTransient, Category = "Gimmick|Signal", meta = (DisplayName = "Required Signals (Effective)"))
	int32 EffectiveRequiredSignals = 0;

	// 신호 반전 (조건을 만족하면 꺼짐. 예: 발판을 밟으면 닫히는 문)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal", meta = (EditCondition = "bReceiveSignals"))
	bool bInvertSignal = false;

	// 신호 조건이 한 번 맞아 켜지면 그 뒤로 신호가 꺼져도 계속 켜짐 (예: 두 Hold 수신기를 동시에 켜야 열리고, 빛을 치워도 열린 채인 문).
	// 체크포인트 뒤에도 유지하려면 bSaveState와 함께 사용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal", meta = (EditCondition = "bReceiveSignals"))
	bool bLatchWhenSignaled = false;

	// 신호를 받을 때 보낸 기믹 → 이 기믹으로 흘러가는 이펙트 (비면 연출 없이 바로 적용).
	// User 파라미터: 끝점(SignalLinkEndParam, 월드 위치)·색(SignalLinkColorParam)·호 높이(SignalLinkArcHeightParam). 예: NS_SignalLink
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (EditCondition = "bReceiveSignals"))
	TObjectPtr<UNiagaraSystem> SignalLinkEffect;

	// 이펙트 색 (보낸 기믹·받는 기믹의 표식 색과 맞추면 연결이 잘 보임)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (EditCondition = "bReceiveSignals"))
	FLinearColor SignalLinkColor = FLinearColor(3.f, 0.25f, 0.15f);

	// 신호가 실제로 적용되기까지 시간 (= 이펙트 입자가 도착하는 시간, 이펙트 입자 수명과 맞춤). 켜짐·꺼짐 모두 같은 시간만큼 늦어져 순서는 유지
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (ClampMin = "0.0", Units = "s", EditCondition = "bReceiveSignals"))
	float SignalLinkTravelTime = 0.8f;

	// 입자를 내보내는 시간 (이후 이펙트를 끄고, 남은 입자는 끝까지 날아감)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (ClampMin = "0.05", Units = "s", EditCondition = "bReceiveSignals"))
	float SignalLinkSpawnDuration = 0.35f;

	// 호 높이 (두 지점 사이 가운데에서 위로 솟는 정도)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (Units = "cm", EditCondition = "bReceiveSignals"))
	float SignalLinkArcHeight = 150.f;

	// 꺼짐 신호에도 이펙트 재생 (기본은 켜짐 신호만)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (EditCondition = "bReceiveSignals"))
	bool bSignalLinkOnDeactivate = false;

	// 도착 지점 보정 (월드 기준 cm)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", meta = (EditCondition = "bReceiveSignals"))
	FVector SignalLinkEndOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", AdvancedDisplay)
	FName SignalLinkEndParam = TEXT("TetherEnd");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", AdvancedDisplay)
	FName SignalLinkColorParam = TEXT("Color");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Signal|Link Effect", AdvancedDisplay)
	FName SignalLinkArcHeightParam = TEXT("ArcHeight");

	// 이 태그를 단 컴포넌트가 있으면 그 위치에서 이펙트가 나가고 도착함
	static const FName SignalLinkPointTag;

	// 켜질 때 효과음 (문 열림, 가로등 점등, 수신기 켜짐, 상자 열림 등). 세이브 복원·시작 상태 적용 때는 재생 안 함
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound")
	TObjectPtr<USoundBase> ActivateSound;

	// 꺼질 때 효과음 (문 닫힘 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound")
	TObjectPtr<USoundBase> DeactivateSound;

	// 이 기믹 효과음 전체 볼륨 배율
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound", meta = (ClampMin = "0.0"))
	float SoundVolumeMultiplier = 1.f;

	// 효과음을 위치 없는 2D UI 소리로 재생 (게임 일시정지 중에도 들림. 예: 열면 일시정지되는 보물 상자)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Sound")
	bool bPlaySoundAs2D = false;

	// 세이브 시 상태 저장 (한 번 열면 계속 열려 있어야 하는 레버·발판 등)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gimmick|Save")
	bool bSaveState = false;

	// 세이브 이름 (같은 맵 안에서 겹치지 않게). 비면 액터 이름
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Gimmick|Save", meta = (EditCondition = "bSaveState"))
	FName SaveId;

#if WITH_EDITORONLY_DATA
	// 선택하면 신호 연결을 뷰포트에 선으로 표시 (에디터 전용)
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UGimmickLinkVisualizerComponent> LinkVisualizer;
#endif

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Interaction")
	bool bActivated = false;

	// 지금 켜진 신호를 보낸 액터
	TSet<TWeakObjectPtr<AActor>> ActiveSignalSources;

	// 시작 상태 적용 중 (BeginPlay 전 신호 포함, BeginPlay 다음 틱까지)
	bool bApplyingInitialState = true;

	FDelegateHandle CollectSaveHandle;
};
