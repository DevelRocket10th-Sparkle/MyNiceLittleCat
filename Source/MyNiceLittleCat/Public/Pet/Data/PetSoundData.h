// 작성자: 우진
// 작성일자: 2026-11-12
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PetSoundData.generated.h"

class USoundBase;
class USoundAttenuation;

// 펫 효과음 전체 설정 데이터
// 의도: 펫·자성·반사 효과음을 한 에셋에 모아서 사운드 담당이 한 곳에서 채우게
//       비어 있는 슬롯은 재생하지 않으니까 에셋이 아직 없어도 게임은 그대로 동작해요
// (2D) = 플레이어 조작 피드백 / (3D) = 월드에서 일어난 사건
UCLASS()
class MYNICELITTLECAT_API UPetSoundData : public UDataAsset
{
    GENERATED_BODY()
public:
    // ===== 공통 =====
    // 3D 효과음에 공통으로 쓸 거리 감쇠
    // 의도: 펫은 항상 플레이어 근처라 기믹과 감쇠 거리가 달라서 따로 둬요
    //       비어 있으면 사운드 에셋 자체의 감쇠 설정을 따라요
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Common")
    TObjectPtr<USoundAttenuation> Attenuation = nullptr;

    // ===== 펫 =====
    // (3D) 날기 시작할 때
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
    TObjectPtr<USoundBase> FlySound = nullptr;

    // (3D) 땅에 착지할 때
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
    TObjectPtr<USoundBase> LandSound = nullptr;

    // (3D) 순간이동 복귀할 때 (뿅)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
    TObjectPtr<USoundBase> RecoveryTeleportSound = nullptr;

    // (2D) 말풍선 띄울 때 (5단계 말풍선 구조 변경 때 연결)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pet")
    TObjectPtr<USoundBase> SpeechBubbleSound = nullptr;

    // ===== 자성 =====
    // (3D) 자기장 루프: 우클릭 동안 재생 (루프 설정된 에셋을 넣어야 해요)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> MagnetFieldLoopSound = nullptr;

    // 자기장 루프가 커지는 시간
    // 의도: 우클릭을 짧게 톡 눌러도 소리가 들리도록 짧게 잡아요
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet", meta = (ClampMin = "0.0", Units = "s"))
    float MagnetFieldFadeInTime = 0.15f;

    // 자기장 루프가 작아지며 꺼지는 시간
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet", meta = (ClampMin = "0.0", Units = "s"))
    float MagnetFieldFadeOutTime = 0.25f;

    // 대상을 들고 있을 때 루프 피치 배율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet", meta = (ClampMin = "0.1"))
    float MagnetHoldingPitchMultiplier = 1.2f;

    // 대상을 들고 있을 때 루프 볼륨 배율
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet", meta = (ClampMin = "0.0"))
    float MagnetHoldingVolumeMultiplier = 1.3f;

    // (3D) 극변환 (Q) / 극 고정 구역에 들어가서 자동으로 바뀔 때도 재사용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> PolarityChangeSound = nullptr;

    // (2D) 극변환 불가 (극 고정 구역에서 Q)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> PolarityChangeBlockedSound = nullptr;

    // (3D) 대상이 끌려오기 시작할 때
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> AttractStartSound = nullptr;

    // (3D) 대상이 철컥 붙을 때
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> AttachSound = nullptr;

    // (3D) 들고 있는 대상 발사 (Q)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnet")
    TObjectPtr<USoundBase> LaunchSound = nullptr;

    // ===== 반사 =====
    // (2D) 드래그로 회전할 때 틱
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> RotateTickSound = nullptr;

    // 드래그 회전 틱이 울리는 각도 간격
    // 의도: 사운드 손맛 조절값이라 사운드와 같은 곳에서 바꾸게
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect", meta = (ClampMin = "1.0", ClampMax = "90.0", Units = "deg"))
    float RotateTickAngle = 15.f;

    // (2D) 설치 클릭 확정 시점
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> PlacementConfirmSound = nullptr;

    // (3D) 펫이 가서 설치를 완료했을 때 (반사판 위치에서 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> PlacementCompleteSound = nullptr;

    // (2D) 설치 불가 위치 클릭 / 이동 실패로 설치 취소 (둘이 같은 소리)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> PlacementBlockedSound = nullptr;

    // (2D) 최대 개수 초과 상태에서 설치 시도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> MaxCountReachedSound = nullptr;

    // (2D) 철거 대상에 마우스 호버
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> RemoveHoverSound = nullptr;

    // (3D) 반사판 철거 (Q) (반사판 위치에서 재생)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Reflect")
    TObjectPtr<USoundBase> RemoveSound = nullptr;
};