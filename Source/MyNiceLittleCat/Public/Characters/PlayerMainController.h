// 작성자: 우진
// 작성일자: 2026-10-16
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Cinematics/CinematicTypes.h"
#include "PlayerMainController.generated.h"

/**
 * 
 */
UCLASS()
class MYNICELITTLECAT_API APlayerMainController : public APlayerController
{
	GENERATED_BODY()
	
public:
	APlayerMainController();

public:
	// 플레이어가 현재 누르고 있는 모든 키의 입력을 초기화하는 함수
	virtual void FlushPressedKeys() override;

	// 게임 화면 밖일 때 현재 게임 입력 처리를 초기화 할 지 정해 반환하는 함수
	virtual bool ShouldFlushKeysWhenViewportFocusChanges() const override;

	// PlayerController의 Tick 함수
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// ========================================================================================

public:
	void SetPartnerPet(class APetCharacter* _pet);

	// 함께 다니는 펫 (아직 연결 전이면 nullptr) - HUD 모드 배지 등에서 사용
	class APetCharacter* GetPartnerPet() const { return PartnerPet.Get(); }

	bool TryGetPetCursorHit(FHitResult& _outHit) const;

	// 펫 입력(모드 다이얼 / 모드 사용 / 특수 행동) 켜고 끄기
	// 튜토리얼처럼 펫 조작을 막아야 하는 곳에서 사용
	void SetPetInputEnabled(bool _bEnable);

	// 일시정지 메뉴 열기/닫기 (Esc / 패드 Start)
	void TogglePauseMenu();

	bool IsCinematicSkipPromptVisible() const;
	float GetCinematicSkipProgress() const { return CinematicSkipProgress; }

private:
	// 펫 입력 관련 콜백 함수들
	void HandleDialStarted();
	void HandleDialTriggered();
	void HandleDialCompleted();
	void HandleDialCanceled();
	void HandleModeUseStarted();
	void HandleModeUseReleased();
	void HandlePetSpecialAction();
	void HandleModeUseCanceled();

	// 컷신 스킵 입력: 재생 중인 컷신이 스킵 가능하면 마지막 장면으로 넘김
	void HandleSkipCinematic();
	void UpdateCinematicSkip(float DeltaTime);
	void ResetCinematicSkip();

	// 컷신 시작/종료 시 입력 매핑 교체
	// - 시작: 게임플레이 IMC를 빼고 컷신 IMC(스킵만)를 넣음 → 허용 목록 방식이라 액션이 늘어도 자동 차단
	// - 종료: 시작 시점에 적용돼 있던 IMC만 원래 우선순위로 복구 (튜토리얼 등으로 꺼둔 IMC는 그대로)
	void HandleCutsceneStateChanged(bool _bCutscenePlaying);

	// 마우스 위치를 다이얼 UI와 같은 좌표 단위로 가져오는 함수 
	bool TryGetDialPointerPosition(FVector2D& _outPosition) const;

	// 다이얼 UI가 게임 화면 바깥으로 벗어나려고 하는지 체크하는 함수
	bool TryClampDialCenterToViewport(FVector2D& _outPosition) const;

	bool TryGetPetAimDirection(FVector& _outDirection) const;
	// ========================================================================================
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet|UI")
	TObjectPtr<class UPetModeDialComponent> PetModeDialComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputMappingContext> IMC_Pet;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetModeDial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetModeUse;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pet|Input")
	TObjectPtr<class UInputAction> IA_PetSpecialAction;

	// 기존 에셋 참조 유지. E 홀드와 진행도는 컨트롤러에서 함께 처리해 IA의 Hold 설정과 중복되지 않음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TObjectPtr<class UInputAction> IA_SkipCinematic;

	// 컷신 중에만 적용하는 IMC: IA_SkipCinematic 매핑만 둠
	// - 비어 있으면 스킵 입력까지 막히지 않도록 컷신 입력 차단을 하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TObjectPtr<class UInputMappingContext> IMC_Cinematic;

	// 컷신 IMC 우선순위: 게임플레이 IMC보다 높게
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	int32 CinematicContextPriority = 100;

	// 컷신 동안 빼둘 게임플레이 IMC (IMC_Player, IMC_Pet 등)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cinematic|Input")
	TArray<TObjectPtr<class UInputMappingContext>> CutsceneBlockedContexts;

	// 컷신 시작 시 빼둔 IMC와 원래 우선순위: 종료 시 그대로 복구
	UPROPERTY(Transient)
	TMap<TObjectPtr<class UInputMappingContext>, int32> SuspendedCutsceneContexts;

	// 컷신 입력 차단 중인지
	bool bCutsceneInputBlocked = false;

	FDelegateHandle CutsceneStateChangedHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Cinematic|UI", meta = (ClampMin = "0.2", Units = "s"))
	float CinematicSkipHoldSeconds = 1.f;

	UPROPERTY(Transient)
	TObjectPtr<class UCinematicSkipWidget> CinematicSkipWidget;

	FCinematicHandle SkipPromptHandle;
	float CinematicSkipProgress = 0.f;
	bool bSkipNeedsRelease = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<class APetCharacter> PartnerPet;

	// 일시정지 메뉴 위젯 (WBP_PauseMenu)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Pause")
	TSubclassOf<class UPauseMenuWidget> PauseMenuClass;

	UPROPERTY(Transient)
	TObjectPtr<class UPauseMenuWidget> PauseMenu;
};
