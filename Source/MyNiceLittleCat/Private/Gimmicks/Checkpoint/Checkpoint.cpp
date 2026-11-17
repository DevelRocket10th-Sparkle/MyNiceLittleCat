/// 작성자: 최요환
/// 작성일자: 2026-10-19
///
/// 체크포인트 액터 구현

#include "Gimmicks/Checkpoint/Checkpoint.h"
#include "Frameworks/Save/GameProgressSubsystem.h"
#include "Frameworks/Transition/ScreenTransitionSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Components/LightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Particles/ParticleSystemComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "Characters/GAS/PlayerAttributeSet.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Widgets/Checkpoint/CheckpointMenuWidget.h"
#include "Kismet/GameplayStatics.h"

static APlayerController* GetPlayerControllerOf(AActor* InteractInstigator)
{
	const APawn* Pawn = Cast<APawn>(InteractInstigator);
	return Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
}

ACheckpoint::ACheckpoint()
{
	// 켜짐·꺼짐 전환 중에만 틱
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	RespawnPoint = CreateDefaultSubobject<UArrowComponent>(TEXT("RespawnPoint"));
	RespawnPoint->SetupAttachment(Mesh);
	RespawnPoint->SetRelativeLocation(FVector(150.f, 0.f, 0.f));
	RespawnPoint->ArrowColor = FColor::Green;

	// 빛 구슬·빛기둥: 보이기만 하고 판정·그림자 없음 (메시를 지정해야 보임)
	BeaconOrb = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconOrb"));
	BeaconOrb->SetupAttachment(Mesh);
	BeaconOrb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconOrb->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BeaconOrb->SetCastShadow(false);
	BeaconOrb->SetCanEverAffectNavigation(false);

	BeaconBeam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeaconBeam"));
	BeaconBeam->SetupAttachment(Mesh);
	BeaconBeam->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeaconBeam->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BeaconBeam->SetCastShadow(false);
	BeaconBeam->SetCanEverAffectNavigation(false);
}

void ACheckpoint::BeginPlay()
{
	Super::BeginPlay();

	CacheStateVisuals();
	OnActivationChanged.AddDynamic(this, &ACheckpoint::HandleStateVisualActivation);

	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->OnCheckpointChanged.AddDynamic(this, &ACheckpoint::HandleCheckpointChanged);

		// 로드된 세이브의 체크포인트면 시작부터 활성 표시
		SetActivated(Progress->IsCurrentCheckpoint(this, GetCheckpointId()), this);
	}

	if (BeaconOrb)
	{
		OrbBaseLocation = BeaconOrb->GetRelativeLocation();
		OrbBaseRotation = BeaconOrb->GetRelativeRotation();
		// 체크포인트마다 움직임이 겹치지 않게 시작 시점을 섞음
		FloatTime = FMath::FRandRange(0.f, FloatPeriod);
	}

	// 시작 상태는 페이드 없이 바로
	StateBlendTarget = IsActivated() ? 1.f : 0.f;
	StateBlend = StateBlendTarget;
	ApplyStateBlend();
	SetActorTickEnabled(NeedsTick());
}

void ACheckpoint::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const bool bBlending = !FMath::IsNearlyEqual(StateBlend, StateBlendTarget) || !FMath::IsNearlyEqual(FocusBlend, FocusBlendTarget);
	if (bBlending)
	{
		const float StateStep = StateBlendTime > 0.f ? DeltaSeconds / StateBlendTime : 1.f;
		StateBlend = FMath::FInterpConstantTo(StateBlend, StateBlendTarget, StateStep, 1.f);

		const float FocusStep = FocusBlendTime > 0.f ? DeltaSeconds / FocusBlendTime : 1.f;
		FocusBlend = FMath::FInterpConstantTo(FocusBlend, FocusBlendTarget, FocusStep, 1.f);

		ApplyStateBlend();
	}

	UpdateBeaconFloat(DeltaSeconds);

	if (!NeedsTick())
	{
		SetActorTickEnabled(false);
	}
}

bool ACheckpoint::HasFloatingOrb() const
{
	return BeaconOrb && BeaconOrb->GetStaticMesh() && (FloatAmplitude > 0.f || !FMath::IsNearlyZero(FloatSpinSpeed));
}

bool ACheckpoint::NeedsTick() const
{
	return HasFloatingOrb()
		|| !FMath::IsNearlyEqual(StateBlend, StateBlendTarget)
		|| !FMath::IsNearlyEqual(FocusBlend, FocusBlendTarget);
}

void ACheckpoint::UpdateBeaconFloat(float DeltaSeconds)
{
	if (!HasFloatingOrb())
	{
		return;
	}

	// 화면에 안 보이면 계산 생략 (시간만 흐름)
	FloatTime += DeltaSeconds;
	if (!BeaconOrb->WasRecentlyRendered(0.5f))
	{
		return;
	}

	const float Period = FMath::Max(FloatPeriod, 0.1f);
	const float Offset = FloatAmplitude * FMath::Sin(FloatTime * UE_TWO_PI / Period);
	FRotator Rotation = OrbBaseRotation;
	Rotation.Yaw = FRotator::NormalizeAxis(OrbBaseRotation.Yaw + FloatSpinSpeed * FloatTime);

	BeaconOrb->SetRelativeLocationAndRotation(OrbBaseLocation + FVector(0.f, 0.f, Offset), Rotation);
}

void ACheckpoint::OnFocusBegin_Implementation(AActor* InteractInstigator)
{
	Super::OnFocusBegin_Implementation(InteractInstigator);

	FocusBlendTarget = 1.f;
	SetActorTickEnabled(true);
}

void ACheckpoint::OnFocusEnd_Implementation(AActor* InteractInstigator)
{
	Super::OnFocusEnd_Implementation(InteractInstigator);

	FocusBlendTarget = 0.f;
	SetActorTickEnabled(true);
}

void ACheckpoint::HandleStateVisualActivation(AGimmickActorBase* Gimmick, bool bNewActivated, AActor* ChangeInstigator)
{
	StateBlendTarget = bNewActivated ? 1.f : 0.f;
	if (!HasActorBegunPlay() || StateBlendTime <= 0.f)
	{
		StateBlend = StateBlendTarget;
		ApplyStateBlend();
		return;
	}
	SetActorTickEnabled(true);
}

void ACheckpoint::CacheStateVisuals()
{
	if (bStateVisualsCached)
	{
		return;
	}
	bStateVisualsCached = true;

	TInlineComponentArray<USceneComponent*> SceneComponents(this);
	for (USceneComponent* Component : SceneComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}

		const bool bOn = !StateOnTag.IsNone() && Component->ComponentHasTag(StateOnTag);
		if (bOn)
		{
			StateOnComponents.Add(Component);
			if (ULightComponent* Light = Cast<ULightComponent>(Component))
			{
				StateLights.Add(Light);
				StateLightBaseIntensities.Add(Light->Intensity);
			}
		}
		if (!StateOffTag.IsNone() && Component->ComponentHasTag(StateOffTag))
		{
			StateOffComponents.Add(Component);
		}

		// 빛 구슬·빛기둥은 태그 없이도 발광 값을 받음
		const bool bBeaconMesh = (Component == BeaconOrb || Component == BeaconBeam);
		if (!StateGlowParameterName.IsNone() && (bBeaconMesh || (!StateGlowTag.IsNone() && Component->ComponentHasTag(StateGlowTag))))
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
			{
				for (int32 Index = 0; Index < Primitive->GetNumMaterials(); ++Index)
				{
					if (UMaterialInstanceDynamic* MID = Primitive->CreateDynamicMaterialInstance(Index))
					{
						StateGlowMaterials.Add(MID);
					}
				}
			}
		}
	}
}

void ACheckpoint::ApplyStateBlend()
{
	const bool bShowOn = StateBlend > KINDA_SMALL_NUMBER || StateBlendTarget > 0.f;
	const bool bShowOff = StateBlend < 1.f - KINDA_SMALL_NUMBER && StateBlendTarget < 1.f;

	for (USceneComponent* Component : StateOnComponents)
	{
		if (!IsValid(Component))
		{
			continue;
		}
		// 꺼짐 상태 밝기 비율이 있으면 라이트는 계속 보임
		const bool bAlwaysVisibleLight = Component->IsA<ULightComponent>() && StateOffLightScale > 0.f;
		Component->SetVisibility(bShowOn || bAlwaysVisibleLight);

		// 이펙트(나이아가라 등)는 켜짐 목표일 때만 재생
		if (UFXSystemComponent* Effect = Cast<UFXSystemComponent>(Component))
		{
			const bool bShouldPlay = StateBlendTarget > 0.f;
			if (bShouldPlay && !Effect->IsActive())
			{
				Effect->Activate(true);
			}
			else if (!bShouldPlay && Effect->IsActive())
			{
				Effect->Deactivate();
			}
		}
	}

	for (USceneComponent* Component : StateOffComponents)
	{
		if (IsValid(Component))
		{
			Component->SetVisibility(bShowOff);
		}
	}

	// 꺼짐 상태 최소값 → 켜짐 1, 포커스 중이면 추가로 밝게
	const float LightScale = FMath::Lerp(StateOffLightScale, 1.f, StateBlend) * (1.f + FocusLightBoost * FocusBlend);
	for (int32 Index = 0; Index < StateLights.Num(); ++Index)
	{
		if (IsValid(StateLights[Index]))
		{
			StateLights[Index]->SetIntensity(StateLightBaseIntensities[Index] * LightScale);
		}
	}

	const float Glow = FMath::Lerp(StateOffGlow, 1.f, StateBlend) + FocusGlowBoost * FocusBlend;
	for (UMaterialInstanceDynamic* MID : StateGlowMaterials)
	{
		if (IsValid(MID))
		{
			MID->SetScalarParameterValue(StateGlowParameterName, Glow);
		}
	}
}

void ACheckpoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UGameProgressSubsystem* Progress = GameInstance->GetSubsystem<UGameProgressSubsystem>())
		{
			Progress->OnCheckpointChanged.RemoveDynamic(this, &ACheckpoint::HandleCheckpointChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ACheckpoint::Interact_Implementation(AActor* InteractInstigator)
{
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!Progress)
	{
		return;
	}

	// 처음 발견하면 빠른 이동 목록에 추가하고 바로 저장
	if (Progress->DiscoverCheckpoint(this, GetCheckpointId(), GetCheckpointDisplayName(), GetRespawnTransform()))
	{
		Progress->SaveProgress();
	}
	K2_OnCheckpointInteracted(InteractInstigator);

	// 메뉴가 있으면 메뉴에서 휴식 / 이동 선택
	APlayerController* PlayerController = GetPlayerControllerOf(InteractInstigator);
	if (MenuWidgetClass && PlayerController)
	{
		if (UCheckpointMenuWidget* Menu = CreateWidget<UCheckpointMenuWidget>(PlayerController, MenuWidgetClass))
		{
			Menu->Open(this, InteractInstigator);
			return;
		}
	}

	Rest(InteractInstigator);
}

void ACheckpoint::Rest(AActor* InteractInstigator)
{
	UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	if (!Progress)
	{
		return;
	}

	// 등록하면 OnCheckpointChanged로 이 체크포인트는 켜지고 이전 체크포인트는 꺼짐
	Progress->SetCheckpoint(this, GetCheckpointId(), GetRespawnTransform(), bSaveOnInteract);

	// 연출이 있으면 화면이 어두울 때 회복, 없으면 바로 회복
	if (!bPlayRestTransition || !PlayRestTransition(InteractInstigator))
	{
		RestorePlayer(InteractInstigator);
	}
}

void ACheckpoint::RestorePlayer(AActor* InteractInstigator)
{
	const IAbilitySystemInterface* ASI = Cast<IAbilitySystemInterface>(InteractInstigator);
	if (UAbilitySystemComponent* ASC = ASI ? ASI->GetAbilitySystemComponent() : nullptr)
	{
		const float MaxHealth = ASC->GetNumericAttribute(UPlayerAttributeSet::GetMaxHealthAttribute());
		ASC->SetNumericAttributeBase(UPlayerAttributeSet::GetHealthAttribute(), MaxHealth);
	}

	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		Progress->OnPlayerRested.Broadcast();
	}
}

void ACheckpoint::TravelTo(AActor* InteractInstigator, const FCheckpointSaveData& Destination)
{
	UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>();
	APlayerController* PlayerController = GetPlayerControllerOf(InteractInstigator);
	if (!Progress || !PlayerController || !Destination.bValid)
	{
		return;
	}

	// 화면이 어두울 때 목적지를 마지막 체크포인트로 지정하고 리스폰 / 맵 이동
	TWeakObjectPtr<UGameProgressSubsystem> WeakProgress(Progress);
	TWeakObjectPtr<APlayerController> WeakController(PlayerController);
	auto DoTravel = [WeakProgress, WeakController, Destination]()
	{
		UGameProgressSubsystem* ProgressPtr = WeakProgress.Get();
		APlayerController* Controller = WeakController.Get();
		if (!ProgressPtr || !Controller)
		{
			return;
		}

		ProgressPtr->SetCheckpointData(Destination, true);

		if (Destination.LevelName == UGameplayStatics::GetCurrentLevelName(Controller, true))
		{
			if (AMainGameMode* GameMode = Controller->GetWorld()->GetAuthGameMode<AMainGameMode>())
			{
				GameMode->RespawnPlayer(Controller);
			}
		}
		else
		{
			// 새 맵에서 AMainGameMode가 저장된 체크포인트 위치에 스폰 (bSpawnAtCheckpointOnStart)
			UGameplayStatics::OpenLevel(Controller, FName(*Destination.LevelName));
		}
	};

	UScreenTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UScreenTransitionSubsystem>();
	if (!Transition || !Transition->PlayTransition(PlayerController, TravelTransition, DoTravel))
	{
		DoTravel();
	}
}

bool ACheckpoint::PlayRestTransition(AActor* InteractInstigator)
{
	APlayerController* PlayerController = GetPlayerControllerOf(InteractInstigator);
	UScreenTransitionSubsystem* Transition = GetWorld()->GetSubsystem<UScreenTransitionSubsystem>();
	if (!PlayerController || !Transition)
	{
		return false;
	}

	// 연출 도중 체크포인트나 플레이어가 사라질 수 있어 약한 참조로 넘김
	TWeakObjectPtr<ACheckpoint> WeakThis(this);
	TWeakObjectPtr<AActor> WeakInstigator(InteractInstigator);

	return Transition->PlayTransition(PlayerController, RestTransition,
		[WeakThis, WeakInstigator]()
		{
			if (ACheckpoint* Checkpoint = WeakThis.Get())
			{
				Checkpoint->RestorePlayer(WeakInstigator.Get());
				Checkpoint->K2_OnRestScreenDark(WeakInstigator.Get());
			}
		},
		[WeakThis, WeakInstigator]()
		{
			if (ACheckpoint* Checkpoint = WeakThis.Get())
			{
				Checkpoint->K2_OnRestFinished(WeakInstigator.Get());
			}
		});
}

bool ACheckpoint::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 활성 상태와 관계없이 다시 저장할 수 있게 토글 규칙은 쓰지 않음
	if (!IsInteractionEnabled())
	{
		return false;
	}

	// 휴식 연출 등 화면 전환 중에는 다시 상호작용하지 않음
	const UScreenTransitionSubsystem* Transition = GetWorld() ? GetWorld()->GetSubsystem<UScreenTransitionSubsystem>() : nullptr;
	return !Transition || !Transition->IsTransitionPlaying();
}

void ACheckpoint::HandleCheckpointChanged(FName NewCheckpointId)
{
	if (UGameProgressSubsystem* Progress = GetGameInstance()->GetSubsystem<UGameProgressSubsystem>())
	{
		SetActivated(Progress->IsCurrentCheckpoint(this, GetCheckpointId()), this);
	}
}

FName ACheckpoint::GetCheckpointId() const
{
	return CheckpointId.IsNone() ? GetFName() : CheckpointId;
}

FText ACheckpoint::GetCheckpointDisplayName() const
{
	return DisplayName.IsEmpty() ? FText::FromName(GetCheckpointId()) : DisplayName;
}

FTransform ACheckpoint::GetRespawnTransform() const
{
	return FTransform(FRotator(0.f, RespawnPoint->GetComponentRotation().Yaw, 0.f), RespawnPoint->GetComponentLocation());
}
