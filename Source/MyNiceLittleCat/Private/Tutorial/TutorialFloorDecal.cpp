#include "Tutorial/TutorialFloorDecal.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

ATutorialFloorDecal::ATutorialFloorDecal()
{
 	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 문이 움직을 떄만 Tick이 돌아요

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Decal = CreateDefaultSubobject<UDecalComponent>(TEXT("Decal"));
	Decal->SetupAttachment(Root);
	Decal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f)); // 데칼은 X축 방향으로 투영됨 -> 아래(-Z)를 보도록 Pitch -90
	Decal->DecalSize = FVector(64.f, 240.f, 240.f); //. X = 투영 깊이, Y/Z = 바닥에 보이는 크기
}

void ATutorialFloorDecal::BeginPlay()
{
	Super::BeginPlay();
	
	// 동적 머티리얼 사용
	if (Decal->GetDecalMaterial())
		DecalMID = Decal->CreateDynamicMaterialInstance();

	bWantVisible = bStartVisible;
	FadeAlpha = (true == bStartVisible) ? 1.f : 0.f;
	ApplyFade();
	SetActorTickEnabled((true == bStartVisible) && (true == bWobble));
}

void ATutorialFloorDecal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 페이드
	const float GoalAlpha = (true == bWantVisible) ? 1.f : 0.f;
	FadeAlpha = FMath::FInterpConstantTo(FadeAlpha, GoalAlpha, DeltaTime, 1.f / FadeDuration);
	ApplyFade();

	// 흔들림 (사인 그래프 이용 Roll = 투영 방향 기준 회전 = 바닥에서 보면 제자리 회전)
	if (true == bWobble)
	{
		WobbleTime += DeltaTime;
		const float Angle = FMath::Sin(WobbleTime * WobbleSpeed * UE_TWO_PI) * WobbleAngle;
		Decal->SetRelativeRotation(FRotator(-90.f, 0.f, Angle));
	}

	// 다 사라졌으면 Tick 끝내기
	const bool bFadeDone = FMath::IsNearlyEqual(FadeAlpha, GoalAlpha);
	if (true == bFadeDone && (false == bWantVisible || false == bWobble))
		SetActorTickEnabled(false);
}

void ATutorialFloorDecal::Show()
{
	if (true == bWantVisible) return; // 이미 보이거나 나타나는 중

	bWantVisible = true;
	SetActorTickEnabled(true);
}

void ATutorialFloorDecal::Hide()
{
	if (false == bWantVisible) return; // 이미 안 보이거나 사라지는 중
	bWantVisible = false;
	SetActorTickEnabled(true);
}

void ATutorialFloorDecal::ApplyFade()
{
	// 머터리얼에 투명도를 전달(파라미터가 없는 파라미터라면 아무 일도 일어나지 않음)
	if (nullptr != DecalMID)
		DecalMID->SetScalarParameterValue(OpacityParamName, FadeAlpha);

	// 완전히 투명할 때는 아예 그리지 않아요
	Decal->SetVisibility(FadeAlpha > KINDA_SMALL_NUMBER);
}

