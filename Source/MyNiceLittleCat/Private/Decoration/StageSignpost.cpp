#include "Decoration/StageSignpost.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

namespace SignpostPrivate
{
	// 다시 만들 때 지울 컴포넌트를 구분하는 태그
	static const FName GeneratedTag(TEXT("SignpostGenerated"));
	// 엔진 기본 도형은 한 변(지름)이 100 단위
	static constexpr float BasicShapeSize = 100.f;

	// 판 크기(두께 X, 길이 Y, 높이 Z)를 기본 큐브 스케일로 변환
	FVector CubeScale(float SizeX, float SizeY, float SizeZ)
	{
		return FVector(SizeX, SizeY, SizeZ) / BasicShapeSize;
	}
}

AStageSignpost::AStageSignpost()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PostMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PostMesh"));
	PostMesh->SetupAttachment(Root);

	CapMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CapMesh"));
	CapMesh->SetupAttachment(Root);
	CapMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapMesh->SetCanEverAffectNavigation(false);

	// 엔진 기본 에셋 (아이콘 머티리얼·텍스처는 BP에서 지정)
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	BoardMeshAsset = CubeFinder.Object;
	PostMeshAsset = CylinderFinder.Object;
	IconMeshAsset = PlaneFinder.Object;
	ColorMaterial = MaterialFinder.Object;
}

void AStageSignpost::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildSignpost();
}

void AStageSignpost::BeginPlay()
{
	Super::BeginPlay();
	RebuildSignpost();
}

void AStageSignpost::RebuildSignpost()
{
	// 1) 이전에 만든 팻말 제거 (태그로 찾아서 지움: PIE 복제본까지 정리)
	TArray<USceneComponent*> OldComponents;
	GetComponents<USceneComponent>(OldComponents);
	for (USceneComponent* Component : OldComponents)
	{
		if (Component && Component->ComponentHasTag(SignpostPrivate::GeneratedTag))
		{
			Component->DestroyComponent();
		}
	}

	// 2) 기둥: 실린더는 중심이 피벗이라 높이의 절반만큼 올림
	const float PostDiameterScale = (PostRadius * 2.f) / SignpostPrivate::BasicShapeSize;
	if (PostMeshAsset)
	{
		PostMesh->SetStaticMesh(PostMeshAsset);
		PostMesh->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, PostHeight * 0.5f),
			FVector(PostDiameterScale, PostDiameterScale, PostHeight / SignpostPrivate::BasicShapeSize)));
	}

	// 기둥 꼭대기 블록
	if (BoardMeshAsset)
	{
		const float CapWidth = PostRadius * 2.f + 16.f;
		CapMesh->SetStaticMesh(BoardMeshAsset);
		CapMesh->SetRelativeTransform(FTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, PostHeight + 6.f),
			SignpostPrivate::CubeScale(CapWidth, CapWidth, 12.f)));
	}

	// 기둥·꼭대기 색 (같은 머티리얼에 Color만 다르게)
	if (ColorMaterial)
	{
		for (UStaticMeshComponent* Mesh : { PostMesh.Get(), CapMesh.Get() })
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0, ColorMaterial))
			{
				MID->SetVectorParameterValue(TEXT("Color"), PostColor);
			}
		}
	}

	// 3) 팻말: 기둥 꼭대기 조금 아래부터 위→아래로 쌓음
	const float OuterHeight = BoardSize.Z + BorderWidth * 2.f;
	float CenterZ = PostHeight - 30.f - OuterHeight * 0.5f;
	for (int32 Index = 0; Index < Arms.Num(); ++Index)
	{
		const FSignpostArm& Arm = Arms[Index];
		// 위쪽 화살표는 끝이 팻말 위로 솟으므로, 첫 팻말이 아니면 간격을 더 줌
		if (Arm.Direction == ESignpostArrowDirection::Up && Index > 0)
		{
			CenterZ -= OuterHeight * 0.5f;
		}
		BuildArm(Arm, CenterZ);
		CenterZ -= (OuterHeight + BoardGap) * Arm.Scale;
	}
}

UStaticMeshComponent* AStageSignpost::AddColoredMesh(USceneComponent* Parent, UStaticMesh* Mesh, const FTransform& RelativeTransform, const FLinearColor& Color)
{
	if (!Mesh || !Parent)
	{
		return nullptr;
	}

	// Transient: 레벨에 저장하지 않고 매번 Arms 값으로 다시 만듦
	UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	Component->ComponentTags.Add(SignpostPrivate::GeneratedTag);
	Component->SetStaticMesh(Mesh);
	Component->SetupAttachment(Parent);
	Component->SetRelativeTransform(RelativeTransform);
	// 팻말은 장식이라 충돌·내비 영향 없음
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->RegisterComponent();

	if (ColorMaterial)
	{
		if (UMaterialInstanceDynamic* MID = Component->CreateDynamicMaterialInstance(0, ColorMaterial))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
	return Component;
}

void AStageSignpost::BuildArm(const FSignpostArm& Arm, float CenterZ)
{
	const float T = BoardSize.X;       // 두께
	const float L = BoardSize.Y;       // 면 길이
	const float H = BoardSize.Z;       // 면 높이
	const float B = BorderWidth;       // 테두리
	const FRotator DiamondRotation(0.f, 0.f, 45.f);

	// 팻말 기준점: 기둥 앞면(-X)의 쌓인 높이 + 사용자 Offset, 사용자 Rotation/Scale 적용
	USceneComponent* ArmRoot = NewObject<USceneComponent>(this, NAME_None, RF_Transient);
	ArmRoot->ComponentTags.Add(SignpostPrivate::GeneratedTag);
	ArmRoot->SetupAttachment(Root);
	ArmRoot->SetRelativeTransform(FTransform(Arm.Rotation, FVector(-PostRadius, 0.f, CenterZ) + Arm.Offset, FVector(Arm.Scale)));
	ArmRoot->RegisterComponent();

	// 앞뒤 순서(카메라 쪽이 -X): 테두리판(뒤) < 색 화살표 끝 < 나무 면 < 아이콘(맨 앞)
	const float BorderX = -T * 0.5f;
	const float FaceX = -T - 1.f;
	const float TipX = -T - 0.6f;
	const float IconX = -T - 2.6f;

	// 방향별 테두리판 / 면 / 화살표 끝 위치 계산
	FVector BorderCenter, BorderSize, FaceCenter, TipCenter;
	switch (Arm.Direction)
	{
	case ESignpostArrowDirection::Right:
		// 기둥에서 +Y로 뻗고, 면 끝(+Y)에 화살표
		BorderCenter = FVector(BorderX, (B + L) * 0.5f, 0.f);
		BorderSize = FVector(T, B + L, H + B * 2.f);
		FaceCenter = FVector(FaceX, B + L * 0.5f, 0.f);
		TipCenter = FVector(0.f, B + L, 0.f);
		break;
	case ESignpostArrowDirection::Left:
		BorderCenter = FVector(BorderX, -(B + L) * 0.5f, 0.f);
		BorderSize = FVector(T, B + L, H + B * 2.f);
		FaceCenter = FVector(FaceX, -(B + L * 0.5f), 0.f);
		TipCenter = FVector(0.f, -(B + L), 0.f);
		break;
	case ESignpostArrowDirection::Up:
	default:
		// 기둥 앞 가운데, 위쪽(+Z)에 화살표
		BorderCenter = FVector(BorderX, 0.f, -B * 0.5f);
		BorderSize = FVector(T, L + B * 2.f, H + B);
		FaceCenter = FVector(FaceX, 0.f, 0.f);
		TipCenter = FVector(0.f, 0.f, H * 0.5f);
		break;
	}

	// 테두리판 (면보다 B만큼 큼)
	AddColoredMesh(ArmRoot, BoardMeshAsset, FTransform(FRotator::ZeroRotator, BorderCenter, SignpostPrivate::CubeScale(BorderSize.X, BorderSize.Y, BorderSize.Z)), BorderColor);

	// 화살표 테두리: 45도 돌린 정사각형(마름모). 테두리판보다 살짝 얇게 해서 안쪽 절반은 판 속에 숨음
	const float TipBorderSide = (H + B * 2.f) / UE_SQRT_2;
	AddColoredMesh(ArmRoot, BoardMeshAsset, FTransform(DiamondRotation, FVector(BorderX, TipCenter.Y, TipCenter.Z),
		SignpostPrivate::CubeScale(T * 0.9f, TipBorderSide, TipBorderSide)), BorderColor);

	// 색 화살표 끝: 테두리보다 앞, 나무 면보다 뒤 → 면 밖으로 나온 삼각형만 색으로 보임
	const float TipSide = H / UE_SQRT_2;
	AddColoredMesh(ArmRoot, BoardMeshAsset, FTransform(DiamondRotation, FVector(TipX, TipCenter.Y, TipCenter.Z),
		SignpostPrivate::CubeScale(1.5f, TipSide, TipSide)), Arm.TipColor);

	// 나무 면
	AddColoredMesh(ArmRoot, BoardMeshAsset, FTransform(FRotator::ZeroRotator, FaceCenter, SignpostPrivate::CubeScale(2.f, L, H)), BoardColor);

	// 아이콘: 면 가운데에 평면을 세우고 텍스처를 입힘
	if (Arm.Icon && IconMeshAsset && IconMaterial)
	{
		UStaticMeshComponent* IconComp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		IconComp->ComponentTags.Add(SignpostPrivate::GeneratedTag);
		IconComp->SetStaticMesh(IconMeshAsset);
		IconComp->SetupAttachment(ArmRoot);
		IconComp->SetRelativeTransform(FTransform(IconPlaneRotation, FVector(IconX, FaceCenter.Y, FaceCenter.Z),
			FVector(IconSize / SignpostPrivate::BasicShapeSize)));
		IconComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		IconComp->SetCanEverAffectNavigation(false);
		IconComp->SetCastShadow(false);
		IconComp->RegisterComponent();

		if (UMaterialInstanceDynamic* MID = IconComp->CreateDynamicMaterialInstance(0, IconMaterial))
		{
			MID->SetTextureParameterValue(TEXT("Icon"), Arm.Icon);
		}
	}
}
