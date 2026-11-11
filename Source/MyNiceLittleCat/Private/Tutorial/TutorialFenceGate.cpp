#include "Tutorial/TutorialFenceGate.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

ATutorialFenceGate::ATutorialFenceGate()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 움직일 때만 Tick

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	// 가림막은 보이기만 함 (막는 건 BlockVolume)
	BlockerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockerMesh"));
	BlockerMesh->SetupAttachment(Root);
	BlockerMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockerMesh->SetCanEverAffectNavigation(false); // 열고 닫을 때마다 내비가 바뀌지 않게

	// 입구 크기의 투명한 벽 (기본 폭 300 / 두께 40 / 높이 300, 레벨에서 입구에 맞게 조절)
	BlockVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BlockVolume"));
	BlockVolume->SetupAttachment(Root);
	BlockVolume->SetBoxExtent(FVector(150.f, 20.f, 150.f));
	BlockVolume->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	BlockVolume->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	BlockVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BlockVolume->SetCanEverAffectNavigation(false);
}

void ATutorialFenceGate::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서도 지금 설정(bStartClosed)대로 보이게
	bWantClosed = bStartClosed;
	CloseAlpha = (true == bStartClosed) ? 1.f : 0.f;
	ApplyAlpha();
}

void ATutorialFenceGate::BeginPlay()
{
	Super::BeginPlay();

	// 시작 상태로 즉시 맞춤 (연출 없이)
	bWantClosed = bStartClosed;
	CloseAlpha = (true == bStartClosed) ? 1.f : 0.f;
	ApplyAlpha();
}

void ATutorialFenceGate::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 목표(0 또는 1)를 향해 일정한 속도로 이동
	const float Goal = (true == bWantClosed) ? 1.f : 0.f;
	CloseAlpha = FMath::FInterpConstantTo(CloseAlpha, Goal, DeltaTime, 1.f / MoveDuration);
	ApplyAlpha();

	if (FMath::IsNearlyEqual(CloseAlpha, Goal))
		SetActorTickEnabled(false);
}

void ATutorialFenceGate::Close()
{
	if (true == bWantClosed) return; // 이미 닫혔거나 닫히는 중

	bWantClosed = true;
	SetActorTickEnabled(true);
	PlayFX(CloseSound);
}

void ATutorialFenceGate::Open()
{
	if (false == bWantClosed) return; // 이미 열렸거나 열리는 중

	bWantClosed = false;
	SetActorTickEnabled(true);
	PlayFX(OpenSound);
}

void ATutorialFenceGate::ApplyAlpha()
{
	// 처음과 끝을 부드럽게 (땅속 -SinkDepth -> 제자리 0)
	const float Eased = FMath::InterpEaseInOut(0.f, 1.f, CloseAlpha, 2.f);
	BlockerMesh->SetRelativeLocation(FVector(0.f, 0.f, -SinkDepth * (1.f - Eased)));
	BlockerMesh->SetVisibility(CloseAlpha > 0.f); // 땅속에 있을 땐 숨김 (바닥이 얇아도 비치지 않게)

	// 막는 충돌은 닫기 시작하자마자 켜고, 다 열렸을 때 끔
	// 의도: 솟아오르는 도중에 빠져나가는 틈이 없게
	BlockVolume->SetCollisionEnabled((true == bWantClosed || CloseAlpha > 0.f) ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void ATutorialFenceGate::PlayFX(USoundBase* _sound)
{
	const FVector Location = GetActorLocation(); // 입구 바닥 가운데

	if (nullptr != MoveEffect)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, MoveEffect, Location, GetActorRotation()); // 회전 같이 -> 먼지가 입구 폭을 따라 퍼짐

	if (nullptr != _sound)
		UGameplayStatics::PlaySoundAtLocation(this, _sound, Location);
}
