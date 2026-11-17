// 작성자: 우진
// 작성일자: 2026-11-16

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TutorialSideWallHider.generated.h"

// 옆면 하나에 속한 벽 액터 목록
USTRUCT(BlueprintType)
struct FSideWallSet
{
	GENERATED_BODY()

	// 이 면에 붙은 벽 / 기둥 / 장식 액터: 레벨에서 직접 지정
	UPROPERTY(EditInstanceOnly, Category = "SideWall")
	TArray<TObjectPtr<AActor>> WallActors;
};

// 카메라가 방 박스 옆면 평면 바깥에 있으면 그 면의 벽을 숨기는 튜토리얼 전용 액터
UCLASS()
class MYNICELITTLECAT_API ATutorialSideWallHider : public AActor
{
	GENERATED_BODY()

public:
	ATutorialSideWallHider();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	// 면 하나의 숨김 상태 적용: 바뀔 때만 실제 변경
	void ApplySide(int32 Side, bool bHide);

private:
	// 방 영역: 액터를 회전시켜도 로컬 축 기준으로 판정
	UPROPERTY(VisibleAnywhere, Category = "SideWall")
	TObjectPtr<class UBoxComponent> RoomBox;

	// 옆면별 벽 목록. 인덱스 0 = 로컬 -X, 1 = +X, 2 = -Y, 3 = +Y
	UPROPERTY(EditInstanceOnly, Category = "SideWall")
	FSideWallSet Sides[4];

	// 평면 경계에서 깜빡이지 않도록 카메라가 이만큼 더 나가야 숨김 (cm)
	UPROPERTY(EditAnywhere, Category = "SideWall", meta = (ClampMin = "0.0"))
	float Margin = 30.f;

	// 면별 현재 숨김 상태
	bool bSideHidden[4] = {};
};