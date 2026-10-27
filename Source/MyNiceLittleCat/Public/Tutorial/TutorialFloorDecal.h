// 작성자: 우진
// 작성일자: 2026-10-27
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ActivatableInterface.h"
#include "TutorialFloorDecal.generated.h"

UCLASS()
class MYNICELITTLECAT_API ATutorialFloorDecal : public AActor, public IActivatableInterface
{
	GENERATED_BODY()
	
public:	
	ATutorialFloorDecal();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// =======================================================================
public:
	// 서서히 나타나기 / 사라지기
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Decal")
	void Show();

	UFUNCTION(BlueprintCallable, Category = "Tutorial|Decal")
	void Hide();

	// IActivatableInterface
	virtual void Activate_Implementation(AActor* Activator) override { Show(); }
	virtual void Deactivate_Implementation(AActor* Activator) override { Hide(); }

protected:
	// 현재 투명도(FadeAlpha)를 머티리얼과 표시 여부에 반영
	void ApplyFade();

	// =======================================================================
protected:
	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Decal")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Tutorial|Decal")
	TObjectPtr<class UDecalComponent> Decal; // 바닥에 투영되는 데칼 (머티리얼은 BP에서 지정)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal")
	bool bStartVisible = false; // 게임 시작 시 보일지 (보통 매니저가 켜므로 false)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal", meta = (ClampMin = "0.01"))
	float FadeDuration = 0.4f; // 나타나거나 사라지는 데 걸리는 시간(초)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal")
	FName OpacityParamName = TEXT("Opacity"); // 데칼 머티리얼의 투명도 스칼라 파라미터 이름

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal|Wobble")
	bool bWobble = true; // 보이는 동안 좌우로 살짝 흔들릴지

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal|Wobble", meta = (EditCondition = "bWobble"))
	float WobbleAngle = 6.f; // 흔들리는 최대 각도(도)

	UPROPERTY(EditAnywhere, Category = "Tutorial|Decal|Wobble", meta = (EditCondition = "bWobble"))
	float WobbleSpeed = 1.5f; // 1초에 몇 번 왕복할지

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DecalMID; // 투명도를 바꾸기 위한 동적 머티리얼
private:
	bool bWantVisible = false; // 목표 상태 (보이기 요청 = true)
	float FadeAlpha = 0.f;     // 현재 보이는 정도. 0 = 안 보임, 1 = 다 보임
	float WobbleTime = 0.f;    // 흔들림 계산용 누적 시간
};
