/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 수정 일자: 2026.11.19. (입력 잠금을 컷신 처리로 통일, 레벨 언로드 시에도 정리)
* 수정 일자: 2026.11.21. (플레이어 판정을 대상 인터페이스로, 막는 채널을 BlockedChannels로 지정)
* 아직 나가면 안 되는 출구(마을 입구 등)를 막는 액터
* - BlockingBox: 보이지 않는 벽 (Pawn만 막음). 트리거를 피해 가도 못 나가게 하는 안전장치
* - TriggerBox: 벽 앞의 감지 영역. 플레이어가 들어오면
*   1) 입력 잠금 → 2) 카메라를 펫 정면으로 블렌드 → 3) 펫 말풍선 대사를 차례로 출력
*   → 4) 카메라를 플레이어로 되돌리며 플레이어를 포물선으로 날려 복귀 지점(비우면 PlayerStart)에 착지 → 5) 펫을 플레이어 옆으로 순간이동
* - 시퀀서를 쓰지 않음: 대상(펫 위치·방향)이 매번 달라 고정 카메라 트랙보다 런타임 카메라 계산이 맞음
* - 해제는 ProgressGate(UProgressFlagGateComponent)에 진행 플래그를 지정 (예: 최종 보스 처치 시 액터 제거)
*/

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AreaExitBlocker.generated.h"

class UBoxComponent;
class UCameraComponent;
class UPrimitiveComponent;
class UProgressFlagGateComponent;
class ACharacter;
class APlayerController;
class APetCharacter;

USTRUCT(BlueprintType)
struct FAreaExitBlockerLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (MultiLine = "true"))
	FText Text;

	// 말풍선 유지 시간 (다음 대사까지 간격)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.1", Units = "s"))
	float Duration = 2.f;
};

UCLASS(Blueprintable)
class MYNICELITTLECAT_API AAreaExitBlocker : public AActor
{
	GENERATED_BODY()

public:
	AAreaExitBlocker();

	virtual void Tick(float DeltaSeconds) override;

	// 에디터 배치·값 변경 시 막는 채널 적용
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 단계
	void BeginSequence(ACharacter* Player);
	void FocusPetCamera();
	void PlayNextLine();
	void BeginFlight();
	void FinishSequence();

	// 입력 잠금/해제 (짝 맞춰 호출): 시네마틱 서브시스템의 외부 컷신으로 처리 (참조 횟수, 다른 잠금과 겹쳐도 안전)
	void SetPlayerInputLocked(bool bLocked);
	// 복귀 지점: ReturnPoint → GameMode의 PlayerStart
	bool ResolveReturnLocation(FVector& OutLocation, FRotator& OutRotation) const;
	APetCharacter* ResolvePet() const;
	void SetPetLogicPaused(bool bPaused);
	// 연출 중 말풍선 높이 적용/복구 (짝 맞춰 호출)
	void SetBubbleHeightOverridden(bool bOverride);

	// 벽이 BlockedChannels만 막고 나머지는 통과 (가시 경계 벽과 같은 방식)
	void ApplyBlockedChannels();

	// 살아 있는 캐릭터인지: 대상 인터페이스를 구현하면 그 판정을 따름 (플레이어 클래스에 묶이지 않음)
	static bool IsAliveCharacter(const ACharacter* Character);

protected:
	/* 컴포넌트 */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TObjectPtr<USceneComponent> SceneRoot;

	// 보이지 않는 벽 (BlockedChannels만 막음)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TObjectPtr<UBoxComponent> BlockingBox;

	// 벽이 막는 채널: 기본 Pawn (투사체·카메라는 통과)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TArray<TEnumAsByte<ECollisionChannel>> BlockedChannels = { ECC_Pawn };

	// 감지 영역: 벽보다 안쪽(마을 쪽)에 두어 벽에 닿기 전에 발동
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TObjectPtr<UBoxComponent> TriggerBox;

	// 펫 정면 카메라 (발동 시 위치를 계산해서 옮김)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TObjectPtr<UCameraComponent> PetViewCamera;

	// 진행 플래그로 이 액터를 제거 (Flag에 보스 처치 플래그 지정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exit Blocker")
	TObjectPtr<UProgressFlagGateComponent> ProgressGate;

	/* 대사 */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Lines")
	TArray<FAreaExitBlockerLine> PetLines;

	// 연출 중 말풍선 높이를 바꿀지 (끝나면 원래 높이로 복구)
	// - 말풍선은 화면 고정 크기라 카메라가 가까우면 같은 월드 높이도 화면에서 더 떠 보임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Lines")
	bool bOverrideBubbleHeight = true;

	// 연출 중 말풍선 높이 (펫 캡슐 중심 기준 Z)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Lines", meta = (EditCondition = "bOverrideBubbleHeight", Units = "cm"))
	float BubbleHeightDuringFocus = 70.f;

	/* 카메라 */

	// 펫 정면 카메라 거리 / 높이 / 바라볼 높이 (펫 위치 기준)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Camera", meta = (Units = "cm"))
	float CameraDistance = 260.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Camera", meta = (Units = "cm"))
	float CameraHeight = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Camera", meta = (Units = "cm"))
	float CameraLookAtHeight = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Camera", meta = (ClampMin = "0.0", Units = "s"))
	float CameraBlendTime = 0.5f;

	/* 복귀 */

	// 착지 지점 (비우면 GameMode의 PlayerStart)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Exit Blocker|Return")
	TObjectPtr<AActor> ReturnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Return", meta = (ClampMin = "0.1", Units = "s"))
	float FlightDuration = 1.4f;

	// 포물선 최고 높이 (출발·도착 중 높은 쪽 기준 추가 높이)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Return", meta = (ClampMin = "0.0", Units = "cm"))
	float FlightArcHeight = 600.f;

	// 착지 후 펫을 플레이어 옆으로 순간이동
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Exit Blocker|Return")
	bool bTeleportPetAfterLanding = true;

private:
	TWeakObjectPtr<ACharacter> ActivePlayer;
	TWeakObjectPtr<APlayerController> ActiveController;
	TWeakObjectPtr<APetCharacter> ActivePet;

	FTimerHandle StepTimerHandle;
	int32 LineIndex = INDEX_NONE;
	bool bBusy = false;
	bool bInputLocked = false;
	bool bPetLogicPaused = false;

	// 말풍선 높이 복구용
	bool bBubbleHeightOverridden = false;
	FVector SavedBubbleRelativeLocation = FVector::ZeroVector;

	// 비행
	bool bFlying = false;
	float FlightElapsed = 0.f;
	FVector FlightStart = FVector::ZeroVector;
	FVector FlightEnd = FVector::ZeroVector;
	FRotator FlightEndRotation = FRotator::ZeroRotator;
};
