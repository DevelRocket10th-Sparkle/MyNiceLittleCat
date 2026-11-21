/*
* 작성자: 전선현
* 작성 일자: 2026.11.20. (UCameraOcclusionComponent에서 분리)
* 방 단위 컷어웨이 기준과 방별 숨김 후보 캐시
* - 방(ARoomMaskVolume)에 처음 들어갈 때 월드의 메시와 조명을 한 번 훑어 면별로 분류
* - 숨길 조각은 카메라 쪽 옆면 비트마스크로 고름 (방이나 마스크가 바뀔 때만 다시 모음)
*/
#pragma once

#include "CoreMinimal.h"
#include "RoomCutawayCache.generated.h"

class ARoomMaskVolume;
class UWorld;
class UPrimitiveComponent;
class ULocalLightComponent;

// 방 단위 컷어웨이 판정 기준
// 플레이어가 RoomMaskVolume 안에 있으면 태그 없이 위치만으로 숨길 메시를 고름
//  - 윗층: 방 박스 윗면보다 중심이 높고, 방 바닥 면적과 겹치는 메시 전부 (천장, 윗층 바닥 / 벽 / 소품, 지붕)
//  - 벽면: 카메라 쪽을 향한 박스 옆면에 붙은 얇은 띠 안의 메시 (벽, 문틀, 창, 커튼, 액자 등 벽에 붙은 소품)
// 숨기지 않을 액터는 해당 RoomMaskVolume의 CutawayIgnoreActors에 등록
USTRUCT(BlueprintType)
struct FRoomCutawaySettings
{
	GENERATED_BODY()

	// 방 단위 컷어웨이 사용 여부
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway")
	bool bEnabled = true;
	// 벽면에서 방 안쪽으로 이 거리 이내까지만 들어온 메시를 벽에 붙은 것으로 간주 (cm)
	// 커튼 / 액자 / 벽등은 잡히고, 소파 / 침대처럼 깊은 가구는 남도록 설정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float InnerDepth = 80.f;
	// 벽면에서 방 바깥쪽으로 이 거리 이내에 걸친 메시까지 숨김: 벽 두께 + 외벽에 붙은 장식 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OuterDepth = 150.f;
	// 옆면 법선과 (플레이어 → 카메라) 방향의 내적이 이 값보다 크면 카메라 쪽 벽으로 간주
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FaceDotThreshold = 0.3f;
	// 박스 윗면보다 이만큼 낮은 곳까지 중심이 있으면 천장 / 윗층으로 간주 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float TopTolerance = 30.f;
	// 윗층 판정 시 방 바닥 면적을 바깥으로 넓히는 거리: 방 경계 위 벽 / 외벽 윗부분 포함 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float TopFootprintMargin = 150.f;
	// 방 박스와 전혀 겹치지 않는 메시 (옆방 기둥 / 소품, 건물 밖)도 숨김
	// RoomMask가 방 밖을 어둡게 해도 경계 부드러움 (EdgeSoftness) 구간이나 반투명 메시는 비쳐 보이므로 아예 그리지 않음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway")
	bool bHideOutsideRoom = true;
	// 방 박스를 이만큼 줄여서 겹침 판정: 경계에 살짝 걸친 옆방 메시도 바깥으로 취급 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OutsideInset = 20.f;
	// 방 박스에서 이 거리 안에 있는 바깥 메시만 숨김 대상: 멀리 있는 메시는 RoomMask가 완전히 가림 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float OutsideRadius = 5000.f;
	// 벽면 (박스 옆면)을 이 거리 이상 뚫고 나간 메시는 벽에 박힌 것으로 보고 깊이와 관계없이 벽과 함께 숨김: 벽난로, 문틀, 창 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float WallEmbedDepth = 20.f;
	// 벽에 박힌 메시로 인정하는 방 안쪽 최대 깊이: 모서리에서 옆벽 조각까지 같이 숨는 것 방지 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway", meta = (ClampMin = "0.0"))
	float MaxEmbeddedReach = 250.f;
	// 방 안에 있을 때, 빛이 닿는 범위 (감쇠 반경 구)가 현재 방 박스와 전혀 겹치지 않는 조명의 그림자를 끔
	// 이런 조명은 현재 방을 비추지 못하고, 비추는 옆방 / 건물 밖 메시는 컷어웨이로 이미 화면에서 숨겨져 있으므로 화면은 그대로이고
	// 조명마다 매 프레임 다시 그리던 그림자 깊이 패스 (드로우콜, 렌더 스레드 비용)만 줄어듦
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway|Performance")
	bool bCullOutsideLightShadows = true;
	// 조명 범위와 방 박스 겹침 판정 시 방 박스를 넓히는 여유 거리: 경계에 걸친 조명은 그림자 유지 (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cutaway|Performance", meta = (ClampMin = "0.0"))
	float LightShadowMargin = 50.f;
};

// 방 하나의 컷어웨이 후보: 처음 들어갈 때 한 번만 계산
struct FRoomCutawayCache
{
	// 옆면 인덱스: 0 = -X, 1 = +X, 2 = -Y, 3 = +Y
	static constexpr int32 SideCount = 4;

	// 옆면별 숨길 조각: 박스가 여러 개면 같은 방향 옆면끼리 합침
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SidePieces[SideCount];
	// 천장 / 윗층 조각: 방 안이면 항상 숨김
	TArray<TWeakObjectPtr<UPrimitiveComponent>> TopPieces;
	// 방 박스 밖 조각 (옆방, 건물 밖): 방 안이면 항상 숨김 (bHideOutsideRoom)
	TArray<TWeakObjectPtr<UPrimitiveComponent>> OutsidePieces;
	// 빛이 닿는 범위가 방 박스와 겹치지 않는 그림자 조명: 방 안이면 그림자만 끔
	TArray<TWeakObjectPtr<ULocalLightComponent>> OutsideShadowLights;

	// 월드의 메시와 조명을 한 번 훑어 방 기준으로 분류 (걸린 시간은 로그로 남김)
	static FRoomCutawayCache Build(const UWorld& World, const ARoomMaskVolume& Room, const FRoomCutawaySettings& Settings);

	// 카메라 쪽 옆면 비트마스크 (비트 번호 = 옆면 인덱스)
	// - ToCamera2D: 플레이어 → 카메라 수평 방향 (정규화)
	static uint8 GetCameraSideMask(const FVector& ToCamera2D, float FaceDotThreshold);

	// 숨길 조각 모으기: 천장 / 윗층 + (방 밖) + 마스크의 옆면
	void CollectPieces(uint8 SideMask, bool bIncludeOutside, TSet<UPrimitiveComponent*>& OutPieces) const;
};
