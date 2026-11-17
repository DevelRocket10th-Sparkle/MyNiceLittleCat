// 작성자: 우진
// 작성일자: 2026-11-16

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PetFacialComponent.generated.h"

// 펫 감정 종류 - 모프 타겟 이름 문자열을 직접 쓰지 않도록 enum으로 감쌈
UENUM(BlueprintType)
enum class EPetEmotion : uint8
{
	Neutral,   // 기본 (모든 감정 모프 0)
	Happy,
	Sad,
	Annoyed,
	Excited,
	Cry,
	Sleep,
	Dead,
};

UCLASS( ClassGroup=(Pet), meta=(BlueprintSpawnableComponent) )
class MYNICELITTLECAT_API UPetFacialComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UPetFacialComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//==========================================================================

public:
	UFUNCTION(BlueprintCallable, Category= "Pet|Facial")
	void SetEmotion(EPetEmotion NewEmotion);

private:
	// enum -> 모프 타겟 이름 변환(Neutral은 NAME_None)
	static FName GetMorphName(EPetEmotion Emotion);

	// 깜빡임 처리 (타이머 대신 Tick에서 직접 계산 - 보간과 같은 흐름으로 관리하려고)
	void UpdateBlink(float DeltaTime);

	//==========================================================================
protected:
	// 표정 전환 속도 - 클수록 빠르게 바뀜 (BP에서 값만 조정)
	UPROPERTY(EditAnywhere, Category = "Pet|Facial")
	float EmotionBlendSpeed = 8.f;

	// 깜빡임 간격 (최소~최대 사이 랜덤) - 일정하면 기계적으로 보여서 랜덤으로 줌
	UPROPERTY(EditAnywhere, Category = "Pet|Facial|Blink")
	FVector2D BlinkIntervalRange = FVector2D(2.f, 5.f);

	// 눈 한 번 감았다 뜨는 데 걸리는 시간
	UPROPERTY(EditAnywhere, Category = "Pet|Facial|Blink")
	float BlinkDuration = 0.15f;

private:
	UPROPERTY()
	TObjectPtr<class USkeletalMeshComponent> Mesh;

	EPetEmotion CurrentEmotion = EPetEmotion::Neutral;

	// 감정별 현재 가중치 - 이전 표정은 0으로, 새 표정은 1로 동시에 보간하기 위해 전부 들고 있음
	TMap<EPetEmotion, float> EmotionWeights;

	float BlinkTimer = 0.f;      // 다음 깜빡임까지 남은 시간
	float BlinkElapsed = -1.f;   // 깜빡이는 중이면 0 이상, 아니면 -1
};
