// 작성자: 우진
// 작성일자: 2026-10-19

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/ReflectiveSurfaceInterface.h"
#include "PetReflector.generated.h"

UCLASS()
class MYNICELITTLECAT_API APetReflector : public AActor, public IReflectiveSurfaceInterface
{
	GENERATED_BODY()
	
public:	
	APetReflector();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	// ========================================================================================

public:
	float GetHalfWidth() const;
	FVector GetPlacementHalfExtents() const;
	virtual bool TryGetReflection_Implementation(const FVector& IncomingDirection, const FHitResult& Hit, FVector& OutDirection) const override;

	// 철거 대상으로 가리켜졌을 때 아웃라인(PP) 표시 on/off
	// 의도: Custom Stencil 값만 바꾸고 실제 테두리는 포스트프로세스 머티리얼이 그림
	void SetRemoveHighlighted(bool bHighlighted);

	// 마우스 광선이 이 반사판 박스에 닿는지 검사 (충돌 채널 설정과 무관하게 박스만)
	bool TraceCursorRay(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	// [추가: 최요환 2026-10-27] 반사판 파괴형 레이저(보스 격자 레이저)에 맞으면 즉시 파괴 (IReflectiveSurfaceInterface::BreakByLaser 참고)
	// 설치 목록(UGameplayAbility_PetReflect::PlacedReflectors)은 파괴된 반사판을 자동으로 정리하므로 따로 알릴 필요 없음
	virtual bool BreakByLaser_Implementation(AActor* LaserInstigator) override;
	// ========================================================================================
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<class UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reflector")
	TObjectPtr<UStaticMeshComponent> ReflectorMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Reflector", meta = (ClampMin = "0.1", Units = "s"))
	float ReflectorLifeSpan = 6.f;

	// 철거 하이라이트에 쓸 스텐실 값 (플레이어 실루엣이 1을 쓰므로 2)
	UPROPERTY(EditDefaultsOnly, Category = "Reflector|Remove", meta = (ClampMin = "1", ClampMax = "255"))
	int32 RemoveOutlineStencilValue = 2;
};
