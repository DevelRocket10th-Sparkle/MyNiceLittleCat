// 작성자: 우진
// 작성일자: 2026-11-18

// BGM 재생 게임 인스턴스 서브시스템
// 우선순위 3단: 레벨 기본곡 < Zone(구역 볼륨) < 이벤트(보스 등)
// - 레벨 기본곡: 맵의 액터 초기화 시점에 UBGMSettings::LevelBGMs에서 자동 조회
// - Zone: ABGMZoneVolume 진입/이탈 시 EnterZone/ExitZone. 겹치면 Priority 높은 쪽, 같으면 나중 진입 쪽
// - 이벤트: PushEventBGM/PopEventBGM. 스택 맨 위가 최우선
// 의도: OpenLevel로 레벨이 바뀌어도 같은 곡이면 끊기지 않고, 다른 곡이면 크로스페이드.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BGMSubSystem.generated.h"

UCLASS()
class MYNICELITTLECAT_API UBGMSubSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ====================================================================================
public:
	// ===== Zone (ABGMZoneVolume에서 호출) =====
	void EnterZone(UObject* Zone, USoundBase* Sound, int32 Priority, float FadeTime = -1.f);
	void ExitZone(UObject* Zone, float FadeTime = -1.f);

	// ===== 이벤트 (보스 등) =====
	// Owner: 누가 Push했는지 식별용. 같은 Owner로 다시 Push하면 곡 교체(페이즈 전환 등)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PushEventBGM(UObject* Owner, USoundBase* Sound, float FadeTime = -1.f);

	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PopEventBGM(UObject* Owner, float FadeTime = -1.f);

	// 컷신 등에서 BGM을 잠깐 완전히 끄고 싶을 때 (bStop=false로 다시 재개)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void SetMuted(bool bMute, float FadeTime = -1.f);

private:
	// 새 맵의 액터 초기화 완료 시 → 상태 초기화 + 레벨 기본곡 설정
	// (PostLoadMap은 PIE 첫 진입 때 안 불려서 이 델리게이트 사용)
	void HandleWorldActorsInitialized(const FActorsInitializedParams& Params);

	// 설정에서 해당 월드의 기본곡 조회 (PIE 접두사 제거 후 비교)
	USoundBase* FindLevelDefaultSound(const UWorld* World) const;

	// 3단 우선순위로 지금 틀어야 할 곡 결정
	USoundBase* ResolveDesiredSound() const;

	// 결정된 곡이 현재 곡과 다를 때만 크로스페이드
	void RefreshPlayback(float FadeTime);

	float GetFadeTime(float Requested) const;
	// ====================================================================================

private:
	struct FActiveZone
	{
		TWeakObjectPtr<UObject> Zone;
		TWeakObjectPtr<USoundBase> Sound;
		int32 Priority = 0;
		uint32 EnterOrder = 0;
	};

	struct FEventEntry
	{
		TWeakObjectPtr<UObject> Owner;
		TWeakObjectPtr<USoundBase> Sound;
	};

	// 1순위(가장 낮음): 레벨 기본곡
	UPROPERTY()
	TObjectPtr<USoundBase> LevelDefaultSound;

	// 2순위: 현재 들어가 있는 Zone들
	TArray<FActiveZone> ActiveZones;
	uint32 ZoneEnterCounter = 0;

	// 3순위: 이벤트 스택 (Last가 최우선)
	TArray<FEventEntry> EventStack;

	// 현재 재생 중인 컴포넌트 (레벨 전환에도 유지됨)
	UPROPERTY()
	TObjectPtr<UAudioComponent> CurrentComp;

	bool bMuted = false;

	FDelegateHandle ActorsInitializedHandle;
};
