// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 캐릭터의 눈 표면 재질 또는 소켓에 붙인 발광 메시의 색을 제어하는 컴포넌트
* - 메시, 재질, 소켓(뼈) 이름, 색 파라미터 이름을 모두 캐릭터 BP에서 설정
* - 처음 발광할 때 눈 메시를 만들고, 이후에는 재질 파라미터만 변경
* - 경고 깜빡임(PlayWarningPulse) 동안만 Tick
*/

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyEyeGlowComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

// 눈 하나의 부착 위치
USTRUCT(BlueprintType)
struct FEnemyEyeSocket
{
	GENERATED_BODY()

	// 눈 메시를 붙일 뼈/소켓 이름 (필수)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eye")
	FName RootSocket;

	// 위/아래 눈꺼풀 뼈: 둘 다 있으면 그 중간(눈 표면)에 배치, 없으면 RootSocket 위치
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eye")
	FName UpperLidSocket;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Eye")
	FName LowerLidSocket;
};

UCLASS(ClassGroup = (Enemy), meta = (BlueprintSpawnableComponent))
class MYNICELITTLECAT_API UEnemyEyeGlowComponent : public UActorComponent
{
	GENERATED_BODY()

public :

	// 경고 깜빡임 중에만 Tick
	UEnemyEyeGlowComponent();

	// 눈 발광 색 설정: 검은색이면 끔 (처음 발광할 때 눈 메시 생성)
	UFUNCTION(BlueprintCallable, Category = "Enemy|Eye Glow")
	void SetGlowColor(FLinearColor Color);

	// 경고 깜빡임 시작: Duration 동안 PeakColor 밝기로 진동, 끝나면 마지막 색 유지
	UFUNCTION(BlueprintCallable, Category = "Enemy|Eye Glow")
	void PlayWarningPulse(FLinearColor PeakColor, float Duration);

	// 경고 깜빡임 중지
	UFUNCTION(BlueprintCallable, Category = "Enemy|Eye Glow")
	void StopWarningPulse();

	// 경고 깜빡임 진행
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected :

	// 생성한 눈 메시 정리 후 부모 호출
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private :

	// 소켓마다 눈 메시 생성: 성공하면 true
	bool CreateEyeMeshes();

	// 재질 파라미터에 색 적용
	void ApplyColor(const FLinearColor& Color);

protected :

	/* 외형 */

	// 실제 눈 표면의 재질 파라미터로 발광. 이 모드에서는 별도 눈 메시를 생성하지 않음.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow")
	bool bUseMeshMaterial = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow", meta = (EditCondition = "bUseMeshMaterial", ClampMin = "0"))
	int32 EyeMaterialSlot = 0;

	// 눈 위치에 붙일 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow")
	TObjectPtr<UStaticMesh> EyeMesh;

	// 발광 재질: ColorParameterName 벡터 파라미터를 가져야 함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow")
	TObjectPtr<UMaterialInterface> EyeMaterial;

	// 발광 색 벡터 파라미터 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow")
	FName ColorParameterName = TEXT("EyeGlow");

	// 눈 부착 위치 목록: 캐릭터 스켈레톤에 맞게 BP에서 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow")
	TArray<FEnemyEyeSocket> EyeSockets;

	// 눈 메시 크기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow", meta = (ClampMin = "0.001"))
	float EyeScale = 0.12f;

	// 눈 표면에서 캐릭터 정면 방향으로 띄우는 거리: 눈꺼풀 메시에 묻히지 않게 함
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow", meta = (Units = "cm"))
	float SurfaceForwardOffset = 2.f;

	/* 경고 깜빡임 */

	// 깜빡임 속도: |sin(남은 시간 × 값)| 형태로 밝기 진동
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow|Pulse", meta = (ClampMin = "0.1"))
	float PulseAngularSpeed = 13.f;

	// 가장 어두울 때의 밝기 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Eye Glow|Pulse", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PulseMinIntensity = 0.35f;

private :

	// 눈 메시들이 공유하는 동적 재질
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EyeMaterialInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMeshComponent> SurfaceMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalSurfaceMaterial;

	// 생성한 눈 메시
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> EyeComponents;

	// 경고 깜빡임 최대 색과 남은 시간
	FLinearColor PulsePeakColor = FLinearColor::Black;
	float PulseRemaining = 0.f;
};
