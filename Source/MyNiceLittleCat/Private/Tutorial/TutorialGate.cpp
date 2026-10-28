#include "Tutorial/TutorialGate.h"

ATutorialGate::ATutorialGate()
{
 	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 문이 움직을 떄만 Tick이 돌아요

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(Root);

	// (문짝은 경첩(피벗)이 문틀 양 끝에 오도록 배치)
	LeftDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftDoorMesh"));
	LeftDoorMesh->SetupAttachment(Root);
	LeftDoorMesh->SetRelativeLocation(FVector(-170.f, 0.f, 0.f));

	RightDoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightDoorMesh"));
	RightDoorMesh->SetupAttachment(Root);
	RightDoorMesh->SetRelativeLocation(FVector(170.f, 0.f, 0.f));

	// 문짝은 내비 메시 생성에서 제외
	LeftDoorMesh->SetCanEverAffectNavigation(false);
	RightDoorMesh->SetCanEverAffectNavigation(false);
}

void ATutorialGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 레벨에 놓았을 때 bStartOpen 상태가 바로 보이도록
	bWantOpen = bStartOpen;
	OpenAlpha = (true == bStartOpen) ? 1.f : 0.f; 
	ApplyDoorRotation();
}

void ATutorialGate::BeginPlay()
{
	Super::BeginPlay();
	
	// 시작 상태로 즉시 맞춤 (애니메이션 없이)
	bWantOpen = bStartOpen;
	OpenAlpha = (true == bStartOpen) ? 1.f : 0.f;
	ApplyDoorRotation();
}

void ATutorialGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 목표(0 또는 1)를 향해 일정한 속도로 문을 이동시킴
	const float GoalAlpha = (true == bWantOpen) ? 1.f : 0.f;
	OpenAlpha = FMath::FInterpConstantTo(OpenAlpha, GoalAlpha, DeltaTime, 1.f / OpenDuration);
	ApplyDoorRotation();

	// 목표까지 다 움직였다면 다시 Tick을 꺼요
	if (FMath::IsNearlyEqual(OpenAlpha, GoalAlpha))
		SetActorTickEnabled(false);
}


void ATutorialGate::Open()
{
	if (true == bWantOpen) return; // 이미 열려있거나 열리는 중
	
	bWantOpen = true;
	SetActorTickEnabled(true); // 여는 건 틱에서
}

void ATutorialGate::Close()
{
	if (false == bWantOpen) return; // 이미 닫혀있거나 닫히는 중
	
	bWantOpen = false;
	SetActorTickEnabled(true); // 닫는 건 틱에서
}

void ATutorialGate::ApplyDoorRotation()
{
	// 처음과 끝을 부드럽게..
	const float EasedAlpha = FMath::InterpEaseInOut(0.f, 1.f, OpenAlpha, 2.f);
	const float Sign = bOpenInverted ? -1.f : 1.f;
	const float Yaw = OpenAngle * EasedAlpha * Sign;

	// 양쪽 문짝은 서로 반대 방향으로 회전해야 같은 쪽으로 열려요
	LeftDoorMesh->SetRelativeRotation(FRotator(0.f, -Yaw, 0.f));
	RightDoorMesh->SetRelativeRotation(FRotator(0.f, Yaw, 0.f));
}

