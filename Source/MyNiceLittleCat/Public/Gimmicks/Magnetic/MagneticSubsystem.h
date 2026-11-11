/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자력원 목록을 관리하는 월드 서브시스템입니다.
/// 자력원(펫 자석 능력 등)은 능력을 켤 때 RegisterSource, 쓰는 동안 매 틱 UpdateSource, 끌 때 UnregisterSource를 호출합니다.
/// UMagneticComponent는 매 틱 ForEachSource로 자력원을 조회해 범위·가림 판정 후 힘을 받습니다.
/// 자력원은 받는 쪽 액터를 직접 찾지 않아도 되고, 받는 쪽은 자력원 종류를 몰라도 됩니다.
/// 콘솔 변수 Magnetic.DrawDebug 1 이면 등록·갱신 시 자기장 범위를 그립니다.
/// 성능: 자력원이 생기거나 없어질 때 OnSourcesChanged로 알려서, 자성 컴포넌트가 자력원이 있을 때만 틱을 켭니다.
/// 맵 시작 때 프로젝트 설정(Magnetic)의 기본 극성 이펙트·막 머티리얼을 미리 불러 둡니다.
/// (보스 코인처럼 게임 중 처음 생기는 자성 액터가 그 자리에서 동기 로딩해 화면이 멈칫하는 것을 막음)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticSubsystem.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

DECLARE_MULTICAST_DELEGATE(FOnMagneticSourcesChanged);

// TODO(합의): 펫 담당과 자력원 등록 방식 확정 필요
UCLASS()
class MYNICELITTLECAT_API UMagneticSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 유효한 자력원이 하나라도 있는지
	bool HasAnySource() const;

	// 자력원이 새로 생기거나 없어질 때 (정보 갱신 때는 안 부름)
	FOnMagneticSourcesChanged OnSourcesChanged;

	// 자력원 등록 (능력 on). 이미 등록돼 있으면 정보만 갱신
	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	void RegisterSource(AActor* SourceActor, const FMagneticSourceInfo& Info);

	// 자력원 정보 갱신 (능력 사용 중 매 틱: 위치·조준 방향·극성). 등록되지 않은 자력원은 무시
	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	void UpdateSource(AActor* SourceActor, const FMagneticSourceInfo& Info);

	// 자력원 해제 (능력 off)
	UFUNCTION(BlueprintCallable, Category = "Magnetic")
	void UnregisterSource(AActor* SourceActor);

	UFUNCTION(BlueprintPure, Category = "Magnetic")
	bool IsSourceRegistered(const AActor* SourceActor) const;

	// 유효한 자력원마다 호출
	void ForEachSource(TFunctionRef<void(AActor* SourceActor, const FMagneticSourceInfo& Info)> Func) const;

	// 자기장 범위 디버그 표시 (N 빨강, S 파랑)
	static void DrawDebugSource(const UWorld* World, const FMagneticSourceInfo& Info, float Duration = 0.f);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TMap<TWeakObjectPtr<AActor>, FMagneticSourceInfo> Sources;

	// 미리 불러 둔 기본 이펙트·머티리얼 (월드가 살아 있는 동안 메모리에 유지)
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> PreloadedPolarityEffect;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PreloadedPolarityOverlay;
};
