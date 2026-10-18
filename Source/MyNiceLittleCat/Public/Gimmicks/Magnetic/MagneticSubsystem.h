/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자력원 목록을 관리하는 월드 서브시스템입니다.
/// 자력원(펫 자석 능력 등)은 능력을 켤 때 RegisterSource, 쓰는 동안 매 틱 UpdateSource, 끌 때 UnregisterSource를 호출합니다.
/// UMagneticComponent는 매 틱 ForEachSource로 자력원을 조회해 범위·가림 판정 후 힘을 받습니다.
/// 자력원은 받는 쪽 액터를 직접 찾지 않아도 되고, 받는 쪽은 자력원 종류를 몰라도 됩니다.
/// 콘솔 변수 Magnetic.DrawDebug 1 이면 등록·갱신 시 자기장 범위를 그립니다.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "MagneticSubsystem.generated.h"

// TODO(합의): 펫 담당과 자력원 등록 방식 확정 필요
UCLASS()
class MYNICELITTLECAT_API UMagneticSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
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
};
