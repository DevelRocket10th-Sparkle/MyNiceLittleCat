// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.17.
* BP에서 UCinematicSubsystem으로 시퀀스를 재생하는 비동기 노드 (Play Cinematic)
* - C++ 요청자(보스 전환, 스테이지 연출)와 같은 경로로 재생: 컷신 처리, 스킵, 제한 시간, 마커 전달이 똑같이 동작
* - BP 기믹이 시퀀스 플레이어를 직접 만들면 스킵 키가 닿지 않으므로 이 노드로 재생
* - 출력 핀
*   On Marker: 마커를 지날 때 (스킵·제한 시간으로 지나치지 못한 마커는 종료 직전에 bReachedByPlayback = false로 한꺼번에)
*   On Ended: 재생 종료 (Finished, Skipped, Interrupted, TimedOut)
*   On Failed: 재생 시작 실패 (시퀀스 없음 등) → 연출 없이 바로 다음 단계로 진행
* - Cancel로 재생을 취소하면 남은 마커와 종료 핀을 호출하지 않음 (서브시스템 Cancel과 같은 규칙)
*/

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Cinematics/CinematicTypes.h"
#include "PlayCinematicAsyncAction.generated.h"

class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayCinematicMarker, const FCinematicMarker&, Marker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayCinematicEnded, ECinematicEndReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPlayCinematicFailed);

UCLASS()
class MYNICELITTLECAT_API UPlayCinematicAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public :

	// 시퀀스 재생 요청: 반환된 노드 객체로 Cancel 가능
	UFUNCTION(BlueprintCallable, Category = "Cinematic",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Play Cinematic"))
	static UPlayCinematicAsyncAction* PlayCinematic(UObject* WorldContextObject, const FCinematicRequest& Request);

	// 재생 취소: 남은 마커와 종료 핀을 호출하지 않음
	UFUNCTION(BlueprintCallable, Category = "Cinematic")
	void Cancel();

	// 아직 재생 중인지 확인
	UFUNCTION(BlueprintPure, Category = "Cinematic")
	bool IsPlaying() const;

	// 마커를 지날 때 (또는 종료 직전에 지나치지 못한 마커 전달)
	UPROPERTY(BlueprintAssignable)
	FOnPlayCinematicMarker OnMarker;

	// 재생 종료
	UPROPERTY(BlueprintAssignable)
	FOnPlayCinematicEnded OnEnded;

	// 재생 시작 실패
	UPROPERTY(BlueprintAssignable)
	FOnPlayCinematicFailed OnFailed;

	/* UBlueprintAsyncActionBase */

	// 출력 핀 연결이 끝난 뒤 호출: 여기서 재생 시작
	virtual void Activate() override;

private :

	// 서브시스템 마커 콜백
	void HandleMarker(FCinematicHandle InHandle, const FCinematicMarker& Marker);

	// 서브시스템 종료 콜백
	void HandleEnded(FCinematicHandle InHandle, ECinematicEndReason Reason);

	// 월드가 정리될 때: 서브시스템은 콜백 없이 재생을 정리하므로 노드도 함께 해제
	void HandleWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);

	// 노드 해제: 월드 정리 구독 해제 후 GC 대상으로 표시
	void Finish();

	// 재생할 월드
	TWeakObjectPtr<UWorld> World;

	// 재생 요청 (Activate에서 사용)
	FCinematicRequest Request;

	// 진행 중인 재생
	FCinematicHandle Handle;

	// 월드 정리 구독 핸들
	FDelegateHandle WorldCleanupHandle;
};
