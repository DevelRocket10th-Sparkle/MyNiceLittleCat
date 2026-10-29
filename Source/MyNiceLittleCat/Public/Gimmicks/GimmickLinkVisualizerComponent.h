/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 기믹 사이의 신호 연결(LinkedActors)을 에디터 뷰포트에 선으로 그리는 에디터 전용 컴포넌트입니다.
/// AGimmickActorBase가 기본으로 가지고 있으며, 쿠킹된 빌드에서는 빠지고 게임 월드(PIE 포함)에서는 그리지 않습니다.
/// 액터를 선택하면 보내는 연결(하늘색: 이 액터 → 받는 쪽)과 받는 연결(노랑: 신호기 → 이 액터)을 화살표로 그립니다.
/// 선은 메시에 가려지지 않도록 맨 앞에 그립니다.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GimmickLinkVisualizerComponent.generated.h"

UCLASS(ClassGroup = (Gimmick), HideCategories = (Activation, Cooking, AssetUserData, Collision, Tags, Replication, ComponentReplication))
class MYNICELITTLECAT_API UGimmickLinkVisualizerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGimmickLinkVisualizerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// 선택하지 않아도 항상 표시 (레벨 전체 연결을 한눈에 볼 때)
	UPROPERTY(EditAnywhere, Category = "Gimmick|Editor")
	bool bAlwaysShowLinks = false;

	// 받는 연결(이 액터에 신호를 보내는 기믹)도 표시
	UPROPERTY(EditAnywhere, Category = "Gimmick|Editor")
	bool bShowIncomingLinks = true;

	// 선 시작·끝 높이 (액터 위치 기준, 바닥에 묻히지 않게)
	UPROPERTY(EditAnywhere, Category = "Gimmick|Editor")
	float LineHeightOffset = 60.f;

protected:
	virtual void BeginPlay() override;

private:
	void DrawLink(const AActor* From, const AActor* To, const FColor& Color) const;
};
