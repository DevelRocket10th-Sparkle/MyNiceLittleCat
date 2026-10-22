#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Enemy/Datas/EnemyMeleePatternSettings.h"
#include "EnemyMeleeTraceComponent.generated.h"

class UMeshComponent;
class USkeletalMeshComponent;
DECLARE_MULTICAST_DELEGATE_OneParam(FEnemyMeleePlayerHit, const FHitResult&);

// 메시 검색과 피해 적용을 분리한 캐릭터별 근접 판정 실행 상태
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyMeleeTraceComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UEnemyMeleeTraceComponent();
	// Body는 캐릭터 메시로 자동 등록. 장비 장착 후 Weapon 등의 이름으로 등록
	UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
	void RegisterHitSource(FName SourceName, UMeshComponent* Mesh);
	UFUNCTION(BlueprintCallable, Category = "Enemy|Melee")
	void UnregisterHitSource(FName SourceName);
	UFUNCTION(BlueprintPure, Category = "Enemy|Melee")
	UMeshComponent* GetHitSource(FName SourceName) const;

	bool PrepareAttack(const TArray<TObjectPtr<UEnemyMeleeTraceSettings>>& Definitions, int64 RequestId);
	void BeginWindow(const UObject* NotifyKey);
	void EndWindow(const UObject* NotifyKey);
	void StopAttack(int64 ExpectedRequestId = 0);
	FEnemyMeleePlayerHit OnPlayerHit;

	// 판정 구간 동안 소켓별 반지름과 이름을 표시. 캐릭터 BP에서 끌 수 있음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Melee")
	bool bDrawDebugTraces = true;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
	struct FTracePoint
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		FName SourceName;
		FName Socket;
		FVector PreviousPosition = FVector::ZeroVector;
		float Radius = 0.f;
	};
	struct FPoseSetting
	{
		TWeakObjectPtr<USkeletalMeshComponent> Mesh;
		uint8 PreviousOption = 0;
	};
	void SampleSockets();
	TMap<FName, TWeakObjectPtr<UMeshComponent>> Sources;
	TArray<FTracePoint> Points;
	TArray<FPoseSetting> PoseSettings;
	TSet<TWeakObjectPtr<AActor>> HitActors;
	TWeakObjectPtr<const UObject> ActiveNotify;
	int64 ActiveRequestId = 0;
	uint64 Generation = 0;
};
