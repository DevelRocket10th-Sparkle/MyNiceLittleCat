// 작성자: 우진
// 작성일자: 2026-11-16
//
// 마을 중앙에 세우는 스테이지 안내 표지판 액터입니다.
// 나무 기둥 하나에 팻말(Arms)을 위에서부터 차례로 붙이고, 팻말마다 스테이지 아이콘을 그립니다(글자 없음).
// 팻말은 어두운 테두리 + 밝은 나무 면 + 색이 있는 화살표 끝으로 이루어집니다.
// 각 팻말은 Offset(뷰포트에서 다이아몬드 위젯으로 끌어서 이동)과 Rotation(각도)으로 따로 조절할 수 있습니다.
// 회전 기준점은 팻말이 기둥에 붙는 지점이라, Yaw를 돌리면 기둥을 축으로 팻말이 돌아갑니다.
// 게임 카메라는 항상 +X를 바라보므로, 기본 상태(Rotation 0)의 팻말 면은 카메라(-X) 쪽을 향합니다.
// 판·아이콘 컴포넌트는 OnConstruction에서 Arms 배열로 다시 만들어지므로, BP나 레벨 디테일에서 값만 바꾸면 바로 반영됩니다.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StageSignpost.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture2D;

// 화살표 끝이 붙는 쪽 (팻말 기준. Rotation 0일 때 화면 기준과 같음)
UENUM(BlueprintType)
enum class ESignpostArrowDirection : uint8
{
	// 오른쪽 (+Y)
	Right,
	// 왼쪽 (-Y)
	Left,
	// 위쪽 = 앞으로 쭉 (+X 방향을 화면 위로 표현)
	Up
};

// 팻말 하나의 설정
USTRUCT(BlueprintType)
struct FSignpostArm
{
	GENERATED_BODY()

	// 팻말에 그릴 스테이지 아이콘 (알파 있는 텍스처)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost")
	TObjectPtr<UTexture2D> Icon = nullptr;

	// 화살표 끝이 붙는 쪽
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost")
	ESignpostArrowDirection Direction = ESignpostArrowDirection::Right;

	// 화살표 끝 색 (가리키는 방향 강조)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost")
	FLinearColor TipColor = FLinearColor(0.33f, 0.15f, 0.055f);

	// 자동으로 쌓인 위치에서 더 옮길 양 (액터 기준). 뷰포트에서 위젯으로 끌어도 됨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost", meta = (MakeEditWidget))
	FVector Offset = FVector::ZeroVector;

	// 팻말 각도 (기둥에 붙는 지점 기준). Yaw = 가리키는 방향 돌리기, Roll = 기울이기
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost")
	FRotator Rotation = FRotator::ZeroRotator;

	// 팻말 크기 배율
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost", meta = (ClampMin = "0.1"))
	float Scale = 1.f;
};

UCLASS()
class MYNICELITTLECAT_API AStageSignpost : public AActor
{
	GENERATED_BODY()

public:
	AStageSignpost();

	// 에디터에서 값이 바뀔 때마다 팻말을 다시 만듦
	virtual void OnConstruction(const FTransform& Transform) override;

	// 팻말 다시 만들기 (기존 생성분은 태그로 찾아 지움)
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Signpost")
	void RebuildSignpost();

protected:
	// 팻말은 저장하지 않는(Transient) 컴포넌트라 게임 시작 시 한 번 더 만듦
	virtual void BeginPlay() override;

	// ---------- 구성 요소 ----------
	UPROPERTY(VisibleAnywhere, Category = "Signpost")
	TObjectPtr<USceneComponent> Root;

	// 나무 기둥 (플레이어가 통과하지 못하도록 충돌 있음)
	UPROPERTY(VisibleAnywhere, Category = "Signpost")
	TObjectPtr<UStaticMeshComponent> PostMesh;

	// 기둥 꼭대기 장식 블록
	UPROPERTY(VisibleAnywhere, Category = "Signpost")
	TObjectPtr<UStaticMeshComponent> CapMesh;

	// ---------- 내용 ----------
	// 위에서부터 차례로 붙는 팻말 목록
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Signpost")
	TArray<FSignpostArm> Arms;

	// ---------- 에셋 (BP에서 지정) ----------
	// 판·기둥 메시 (기본: 엔진 기본 큐브/실린더, 100 단위)
	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	TObjectPtr<UStaticMesh> BoardMeshAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	TObjectPtr<UStaticMesh> PostMeshAsset;

	// 아이콘을 붙일 평면 메시 (기본: 엔진 Plane, 100x100, +Z가 앞면)
	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	TObjectPtr<UStaticMesh> IconMeshAsset;

	// "Color" 벡터 파라미터를 가진 머티리얼 (기본: 엔진 BasicShapeMaterial)
	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	TObjectPtr<UMaterialInterface> ColorMaterial;

	// "Icon" 텍스처 파라미터를 가진 Masked 머티리얼 (M_SignIcon)
	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	TObjectPtr<UMaterialInterface> IconMaterial;

	// 아이콘 평면을 팻말 면(-X)에 똑바로 세우는 회전 (메시 UV 방향에 맞춘 보정값)
	UPROPERTY(EditDefaultsOnly, Category = "Signpost|Asset")
	FRotator IconPlaneRotation = FRotator(0.f, 90.f, 90.f);

	// ---------- 모양 ----------
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	FLinearColor PostColor = FLinearColor(0.12f, 0.065f, 0.028f);

	// 팻말 면(나무) 색
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	FLinearColor BoardColor = FLinearColor(0.42f, 0.24f, 0.10f);

	// 팻말 테두리 색
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	FLinearColor BorderColor = FLinearColor(0.06f, 0.03f, 0.012f);

	// 기둥 높이 / 반지름
	UPROPERTY(EditAnywhere, Category = "Signpost|Look", meta = (ClampMin = "100"))
	float PostHeight = 380.f;

	UPROPERTY(EditAnywhere, Category = "Signpost|Look", meta = (ClampMin = "5"))
	float PostRadius = 14.f;

	// 팻말 크기: 두께(X) / 길이(Y) / 높이(Z) - 테두리 제외한 면 크기
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	FVector BoardSize = FVector(10.f, 170.f, 78.f);

	// 테두리 두께
	UPROPERTY(EditAnywhere, Category = "Signpost|Look", meta = (ClampMin = "0"))
	float BorderWidth = 7.f;

	// 팻말 사이 간격
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	float BoardGap = 16.f;

	// 아이콘 크기 (한 변)
	UPROPERTY(EditAnywhere, Category = "Signpost|Look")
	float IconSize = 70.f;

private:
	// 색을 입힌 메시 컴포넌트 하나 생성 (Parent 아래에 붙임)
	UStaticMeshComponent* AddColoredMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& RelativeTransform, const FLinearColor& Color);

	// 팻말 하나(테두리 + 면 + 화살표 끝 + 아이콘) 생성
	void BuildArm(const FSignpostArm& Arm, float CenterZ);
};
