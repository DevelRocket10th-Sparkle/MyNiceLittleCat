/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자성 컴포넌트 테스트용 자력원 액터입니다.
/// 펫 자석 능력이 준비되기 전에 UMagneticSubsystem 자력원 역할을 대신합니다.
/// bFollowPlayerPawn이면 플레이어 위치에서, bAimAtCursor면 마우스 커서 방향으로 자기장을 냅니다.
/// 테스트 키 (기본): Z 능력 on/off, X 극성 N/S 전환, C 범위 모양 Line/Cone 전환.
/// 디테일 패널(PIE 중 F8로 빠져나와 선택)의 버튼으로도 같은 조작을 할 수 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "Gimmicks/Magnetic/MagneticTypes.h"
#include "Test_MagneticSource.generated.h"

class UArrowComponent;

UCLASS()
class MYNICELITTLECAT_API ATest_MagneticSource : public AActor
{
	GENERATED_BODY()

public:
	ATest_MagneticSource();

	virtual void Tick(float DeltaTime) override;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Magnetic|Test")
	void ToggleActive();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Magnetic|Test")
	void TogglePolarity();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Magnetic|Test")
	void ToggleShape();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 위치·조준 방향 갱신
	void UpdateSourceTransform();

	// bSourceActive에 맞춰 서브시스템에 등록·갱신·해제
	void SyncSource();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UArrowComponent> Arrow;

	// 능력 on/off
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test")
	bool bSourceActive = true;

	// 자력원 정보 (Location·AimDirection은 매 틱 덮어씀)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test")
	FMagneticSourceInfo SourceInfo;

	// true면 플레이어 폰 위치를 따라감 (펫이 플레이어 곁에 있는 상황 흉내)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test")
	bool bFollowPlayerPawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test", meta = (EditCondition = "bFollowPlayerPawn"))
	FVector FollowOffset = FVector::ZeroVector;

	// true면 마우스 커서 방향으로 조준 (커서 표시를 켬), false면 액터 정면
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test")
	bool bAimAtCursor = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test|Input")
	bool bBindTestKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey ToggleActiveKey = EKeys::Z;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey TogglePolarityKey = EKeys::X;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magnetic|Test|Input", meta = (EditCondition = "bBindTestKeys"))
	FKey ToggleShapeKey = EKeys::C;
};
