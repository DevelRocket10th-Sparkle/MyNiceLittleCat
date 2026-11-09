#include "Tutorial/TutorialPortal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraShakeBase.h"

ATutorialPortal::ATutorialPortal()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false; // 변신 연출 중에만 Tick

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(Root);

	// 데칼은 자기 X 방향으로 투영 -> Yaw -90 으로 방 쪽(+Y)에서 벽(-Y)을 향해 쏨
	RuneDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("RuneDecal"));
	RuneDecal->SetupAttachment(Root);
	RuneDecal->SetRelativeLocation(FVector(0.f, 60.f, 300.f));
	RuneDecal->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
	RuneDecal->DecalSize = FVector(60.f, 260.f, 260.f);
	RuneDecal->SetVisibility(false);

	// 엔진 기본 Plane(100x100, 바닥에 눕힌 면)을 세워서 아치 크기(400 x 550)로 키움
	SwirlPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwirlPlane"));
	SwirlPlane->SetupAttachment(Root);
	SwirlPlane->SetRelativeLocation(FVector(0.f, 0.f, 275.f));
	SwirlPlane->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
	SwirlPlane->SetRelativeScale3D(FVector(4.f, 5.5f, 1.f));
	SwirlPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SwirlPlane->SetCastShadow(false);
	SwirlPlane->SetVisibility(false);

	LoopFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LoopFX"));
	LoopFX->SetupAttachment(Root);
	LoopFX->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	LoopFX->bAutoActivate = false; // 다 열린 다음에 켬

	// 아치 구멍 크기의 범위 (열리기 전엔 충돌 꺼둠 -> 벽 앞에 서 있어도 반응 없음)
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 280.f));
	Trigger->SetBoxExtent(FVector(200.f, 80.f, 280.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetGenerateOverlapEvents(true);

	// 방 쪽으로 조금 나와 있는 보라빛 조명 (처음엔 꺼짐)
	PortalLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PortalLight"));
	PortalLight->SetupAttachment(Root);
	PortalLight->SetRelativeLocation(FVector(0.f, 200.f, 300.f));
	PortalLight->SetLightColor(FLinearColor(0.7f, 0.45f, 1.f));
	PortalLight->SetAttenuationRadius(1500.f);
	PortalLight->SetIntensity(0.f);

	LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
	LoopAudio->SetupAttachment(Root);
	LoopAudio->SetRelativeLocation(FVector(0.f, 0.f, 300.f));
	LoopAudio->bAutoActivate = false;
}

void ATutorialPortal::BeginPlay()
{
	Super::BeginPlay();
	
	// 파라미터를 실시간으로 바꿀 머티리얼 (BP 에 꽂힌 머티리얼을 기반으로 생성)
	WallMID = WallMesh->CreateDynamicMaterialInstance(0);
	SwirlMID = SwirlPlane->CreateDynamicMaterialInstance(0);
	RuneMID = RuneDecal->CreateDynamicMaterialInstance();

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &ATutorialPortal::HandleTriggerBeginOverlap);

	ChangeState(ETutorialPortalState::Closed);
}

void ATutorialPortal::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	StateElapsed += DeltaTime;
	ApplyVisuals();

	// 시간이 다 되면 다음 단계로
	if (ETutorialPortalState::Charging == State && StateElapsed >= ChargeTime)
		ChangeState(ETutorialPortalState::Dissolving);
	else if (ETutorialPortalState::Dissolving == State && StateElapsed >= DissolveTime)
		ChangeState(ETutorialPortalState::Open);
}

void ATutorialPortal::OpenPortal()
{
	if (ETutorialPortalState::Closed != State) return; // 이미 열리는 중 / 열림

	ChangeState(ETutorialPortalState::Charging);
}

void ATutorialPortal::ChangeState(ETutorialPortalState NewState)
{
	State = NewState;
	StateElapsed = 0.f;

	const FVector Center = GetActorTransform().TransformPosition(FVector(0.f, 0.f, 300.f)); // 구멍 가운데

	switch (State)
	{
	case ETutorialPortalState::Closed:
		SetActorTickEnabled(false);
		break;

	case ETutorialPortalState::Charging:
		// 마법진이 그려지기 시작 + 우르르
		RuneDecal->SetVisibility(true);
		if (nullptr != ChargeSound) UGameplayStatics::PlaySoundAtLocation(this, ChargeSound, Center);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			PC->ClientStartCameraShake(OpenShake, OpenShakeScale * 0.5f); // 충전은 약하게
		SetActorTickEnabled(true);
		break;

	case ETutorialPortalState::Dissolving:
		// 펑! -> 벽이 녹기 시작
		if (nullptr != OpenBurstEffect)
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, OpenBurstEffect, Center, GetActorRotation()); // 회전 같이 -> 불꽃이 벽면을 따라 퍼짐
		if (nullptr != OpenSound) UGameplayStatics::PlaySoundAtLocation(this, OpenSound, Center);
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
			PC->ClientStartCameraShake(OpenShake, OpenShakeScale);
		break;

	case ETutorialPortalState::Open:
		// 다 열림: 반복 효과 + 소리 + 들어갈 수 있게
		RuneDecal->SetVisibility(false);
		LoopFX->Activate(true);
		LoopAudio->Play();
		Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		SetActorTickEnabled(false); // 이후엔 머티리얼이 알아서 움직임 (소용돌이는 Time 노드)
		UE_LOG(LogTemp, Log, TEXT("[TutorialPortal] 관문 열림"));
		break;
	}

	ApplyVisuals();
}

void ATutorialPortal::ApplyVisuals()
{
	// 단계별 진행도 (지난 단계는 1, 아직 안 온 단계는 0)
	const float ChargeAlpha = (ETutorialPortalState::Charging == State) ? FMath::Clamp(StateElapsed / ChargeTime, 0.f, 1.f)
		: (State > ETutorialPortalState::Charging ? 1.f : 0.f);
	const float DissolveAlpha = (ETutorialPortalState::Dissolving == State) ? FMath::Clamp(StateElapsed / DissolveTime, 0.f, 1.f)
		: (ETutorialPortalState::Open == State ? 1.f : 0.f);
	const float SwirlAlpha = FMath::Clamp((DissolveAlpha - SwirlStart) / (1.f - SwirlStart), 0.f, 1.f);

	if (nullptr != WallMID)
	{
		// 녹기 직전까지 점점 빛나다가, 녹기 시작하면 빛이 테두리로 옮겨감
		WallMID->SetScalarParameterValue(TEXT("Glow"), ChargeGlow * ChargeAlpha * (1.f - DissolveAlpha));
		WallMID->SetScalarParameterValue(TEXT("Dissolve"), FMath::InterpEaseOut(0.f, 1.f, DissolveAlpha, 2.f)); // 처음엔 빠르게 뚫리고 끝은 천천히
	}

	if (nullptr != RuneMID)
	{
		RuneMID->SetScalarParameterValue(TEXT("Reveal"), ChargeAlpha);            // 원을 따라 그려짐
		RuneMID->SetScalarParameterValue(TEXT("Opacity"), 1.f - DissolveAlpha);   // 벽이 녹으며 같이 사라짐
	}

	SwirlPlane->SetVisibility(SwirlAlpha > 0.f);
	if (nullptr != SwirlMID)
		SwirlMID->SetScalarParameterValue(TEXT("Open"), FMath::InterpEaseOut(0.f, 1.f, SwirlAlpha, 2.f)); // 가운데부터 펼쳐짐

	// 충전 중엔 은은하게, 소용돌이가 펼쳐지며 최대로
	PortalLight->SetIntensity(LightIntensity * (0.25f * ChargeAlpha + 0.75f * SwirlAlpha));
}

void ATutorialPortal::HandlePlayerEnter()
{
	if (true == bTraveling) return; // 이미 이동 중

	if (true == TargetLevel.IsNull())
	{
		UE_LOG(LogTemp, Warning, TEXT("[TutorialPortal] 이동할 레벨(TargetLevel)이 비어 있음 -> 이동 안 함"));
		return;
	}

	bTraveling = true;
	UE_LOG(LogTemp, Log, TEXT("[TutorialPortal] 관문 진입 -> %s 로 이동"), *TargetLevel.ToSoftObjectPath().ToString());

	// 화면이 완전히 어두워진 순간 레벨을 엶 (팀 체크포인트의 맵 이동과 같은 방식)
	// 람다 안에서는 이 액터가 이미 사라졌을 수 있어서 약한 포인터로 확인
	TWeakObjectPtr<ATutorialPortal> WeakThis(this);
	const TSoftObjectPtr<UWorld> Level = TargetLevel;
	auto DoTravel = [WeakThis, Level]()
		{
			if (ATutorialPortal* Portal = WeakThis.Get())
				UGameplayStatics::OpenLevelBySoftObjectPtr(Portal, Level);
		};

	// 화면 전환을 못 쓰면(서브시스템 없음 / 이미 재생 중) 바로 이동
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	UScreenTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UScreenTransitionSubsystem>();
	if (nullptr == Transition || false == Transition->PlayTransition(PC, TravelTransition, DoTravel))
		DoTravel();
}

void ATutorialPortal::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (false == IsOpen()) return;

	// 펫이나 적은 무시, 플레이어만
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (nullptr == Pawn || false == Pawn->IsPlayerControlled()) return;

	HandlePlayerEnter();
}

