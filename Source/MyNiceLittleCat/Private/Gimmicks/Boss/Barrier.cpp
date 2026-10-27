/// 작성자: 최요환
/// 작성일자: 2026-10-15
///
/// 보스전 배리어 구현

#include "Gimmicks/Boss/Barrier.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Gimmicks/Boss/BarrierGenerator.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

#if !UE_BUILD_SHIPPING
// 테스트용 콘솔 명령: Barrier.Debug <Activate|Deactivate> [이름 일부]
// 이름 일부를 생략하면 월드의 모든 배리어에 적용 (예: Barrier.Debug Activate 70)
static FAutoConsoleCommandWithWorldAndArgs GBarrierDebugCommand(
	TEXT("Barrier.Debug"),
	TEXT("Barrier.Debug <Activate|Deactivate> [NameFilter] : 테스트용 배리어 제어"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || Args.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("사용법: Barrier.Debug <Activate|Deactivate> [NameFilter]"));
			return;
		}

		const FString& Action = Args[0];
		const FString NameFilter = Args.IsValidIndex(1) ? Args[1] : FString();

		for (TActorIterator<ABarrier> It(World); It; ++It)
		{
			ABarrier* Barrier = *It;
			if (!NameFilter.IsEmpty() && !Barrier->GetActorNameOrLabel().Contains(NameFilter))
			{
				continue;
			}

			if (Action.Equals(TEXT("Activate"), ESearchCase::IgnoreCase))
			{
				Barrier->ActivateBarrier();
			}
			else if (Action.Equals(TEXT("Deactivate"), ESearchCase::IgnoreCase))
			{
				Barrier->DeactivateBarrier();
			}

			UE_LOG(LogTemp, Log, TEXT("Barrier.Debug %s -> %s (Active: %d)"), *Action, *Barrier->GetActorNameOrLabel(), Barrier->IsBarrierActive());
		}
	}));
#endif

ABarrier::ABarrier()
{
	// Dissolve 연출 중에만 틱
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	// 보스를 따라갈 수 있도록 이동 가능
	Mesh->SetMobility(EComponentMobility::Movable);

	// 피해 무효는 상태 플래그로 처리하므로 메시는 시각용
	Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	// 배리어 메시 스케일과 상관없이 실제 반경으로 보이도록 절대 스케일
	CollectRadiusPreview = CreateDefaultSubobject<USphereComponent>(TEXT("CollectRadiusPreview"));
	CollectRadiusPreview->SetupAttachment(Mesh);
	CollectRadiusPreview->SetAbsolute(false, false, true);
	CollectRadiusPreview->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CollectRadiusPreview->SetGenerateOverlapEvents(false);
	CollectRadiusPreview->SetHiddenInGame(true);
	CollectRadiusPreview->ShapeColor = FColor(80, 200, 255);
}

void ABarrier::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (CollectRadiusPreview)
	{
		CollectRadiusPreview->SetSphereRadius(GeneratorCollectRadius);
		// 직접 지정한 목록을 쓰거나 자동 수집이 꺼져 있으면 반경 표시도 숨김
		CollectRadiusPreview->SetVisibility(bAutoCollectGenerators && Generators.IsEmpty());
	}
}

void ABarrier::CollectNearbyGenerators()
{
	Modify();
	Generators.Reset();
	for (ABarrierGenerator* Generator : FindNearbyGenerators())
	{
		Generators.Add(Generator);
	}

	UE_LOG(LogTemp, Log, TEXT("%s: 생성장치 %d개 수집 (반경 %.0f)"), *GetName(), Generators.Num(), GeneratorCollectRadius);
}

TArray<ABarrierGenerator*> ABarrier::FindNearbyGenerators() const
{
	TArray<ABarrierGenerator*> Found;
	UWorld* World = GetWorld();
	if (!World)
	{
		return Found;
	}

	const FVector Center = GetActorLocation();
	for (TActorIterator<ABarrierGenerator> It(World); It; ++It)
	{
		ABarrierGenerator* Generator = *It;
		if (!IsValid(Generator)
			|| (!GeneratorCollectTag.IsNone() && !Generator->ActorHasTag(GeneratorCollectTag))
			|| FVector::Dist2D(Center, Generator->GetActorLocation()) > GeneratorCollectRadius)
		{
			continue;
		}

		Found.Add(Generator);
	}

	Found.Sort([&Center](const ABarrierGenerator& A, const ABarrierGenerator& B)
	{
		return FVector::DistSquared2D(Center, A.GetActorLocation()) < FVector::DistSquared2D(Center, B.GetActorLocation());
	});
	return Found;
}

void ABarrier::BeginPlay()
{
	Super::BeginPlay();

	HomeTransform = GetActorTransform();

	// 직접 지정한 목록이 우선, 비어 있을 때만 근처 생성장치 수집
	if (bAutoCollectGenerators && Generators.IsEmpty())
	{
		for (ABarrierGenerator* Generator : FindNearbyGenerators())
		{
			Generators.Add(Generator);
		}

		UE_LOG(LogTemp, Log, TEXT("%s: 근처 생성장치 %d개 자동 수집 (반경 %.0f)"), *GetName(), Generators.Num(), GeneratorCollectRadius);
	}

	for (ABarrierGenerator* Generator : Generators)
	{
		if (IsValid(Generator))
		{
			Generator->OnGeneratorDisabled.AddUniqueDynamic(this, &ABarrier::HandleGeneratorDisabled);
		}
	}

	// 머티리얼에 Dissolve 파라미터가 있을 때만 연출 사용
	if (Mesh && !DissolveParamName.IsNone())
	{
		float CurrentValue = 0.f;
		UMaterialInterface* Material = Mesh->GetMaterial(0);
		if (Material && Material->GetScalarParameterValue(FHashedMaterialParameterInfo(DissolveParamName), CurrentValue))
		{
			MeshMID = Mesh->CreateDynamicMaterialInstance(0);
		}
	}

	ApplyVisualState();

	if (bActivateOnBeginPlay)
	{
		ActivateBarrier();
	}
}

void ABarrier::Activate_Implementation(AActor* Activator)
{
	ActivateBarrier();
}

void ABarrier::Deactivate_Implementation(AActor* Activator)
{
	DeactivateBarrier();
}

void ABarrier::ActivateBarrier()
{
	if (bBarrierActive)
	{
		return;
	}

	if (Generators.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: 생성장치가 지정되지 않아 배리어가 붕괴하지 않습니다."), *GetName());
	}

	for (ABarrierGenerator* Generator : Generators)
	{
		if (IsValid(Generator))
		{
			Generator->ActivateGenerator();
		}
	}

	bBarrierActive = true;
	ApplyVisualState(AppearDuration);
	K2_OnBarrierStateChanged(true, false);
	OnBarrierActivated.Broadcast(this);
}

void ABarrier::DeactivateBarrier()
{
	if (!bBarrierActive)
	{
		return;
	}

	bBarrierActive = false;
	DeactivateGenerators();
	ApplyVisualState(DeactivateDissolveDuration);
	K2_OnBarrierStateChanged(false, false);
}

void ABarrier::BreakBarrier()
{
	if (!bBarrierActive)
	{
		return;
	}

	bBarrierActive = false;
	DeactivateGenerators();
	ApplyVisualState(BreakDissolveDuration);
	K2_OnBarrierStateChanged(false, true);
	OnBarrierBroken.Broadcast(this);
}

void ABarrier::HandleGeneratorDisabled(ABarrierGenerator* Generator)
{
	if (bBarrierActive && AreAllGeneratorsDisabled())
	{
		BreakBarrier();
	}
}

void ABarrier::DeactivateGenerators()
{
	for (ABarrierGenerator* Generator : Generators)
	{
		if (IsValid(Generator))
		{
			Generator->DeactivateGenerator();
		}
	}
}

bool ABarrier::AreAllGeneratorsDisabled() const
{
	bool bHasValidGenerator = false;

	for (const ABarrierGenerator* Generator : Generators)
	{
		if (!IsValid(Generator))
		{
			continue;
		}

		bHasValidGenerator = true;

		if (!Generator->IsDisabled())
		{
			return false;
		}
	}

	return bHasValidGenerator;
}

void ABarrier::ApplyVisualState(float DissolveDuration)
{
	if (!Mesh)
	{
		return;
	}

	// 켜질 때는 먼저 보이게 한 뒤 1 → 0, 꺼질 때는 0 → 1 후 숨김
	if (bBarrierActive)
	{
		Mesh->SetVisibility(true, true);
	}

	StartDissolve(bBarrierActive ? 0.f : 1.f, MeshMID ? DissolveDuration : 0.f);
}

void ABarrier::StartDissolve(float Target, float Duration)
{
	// 연출 도중에 반대로 바뀌면 지금 값에서 이어감 (BeginPlay에서 꺼진 상태 = 1로 시작)
	DissolveFrom = DissolveValue;
	DissolveTarget = Target;
	DissolveDurationCurrent = Duration;
	DissolveElapsed = 0.f;

	if (Duration <= 0.f || FMath::IsNearlyEqual(DissolveFrom, DissolveTarget))
	{
		SetDissolveValue(DissolveTarget);
		FinishDissolve();
		return;
	}

	SetActorTickEnabled(true);
}

void ABarrier::FinishDissolve()
{
	SetActorTickEnabled(false);
	if (DissolveTarget >= 1.f)
	{
		if (Mesh)
		{
			Mesh->SetVisibility(false, true);
		}
		// 다 사라진 뒤에 떼야 사라지는 동안에도 보스를 따라감
		ReturnToHome();
	}
}

void ABarrier::AttachToFollowTarget(AActor* Target)
{
	if (!IsValid(Target) || Target == this)
	{
		return;
	}

	if (AActor* PrevTarget = FollowTarget.Get(); PrevTarget && PrevTarget != Target)
	{
		PrevTarget->OnEndPlay.RemoveDynamic(this, &ABarrier::HandleFollowTargetEndPlay);
	}

	FollowTarget = Target;
	// 보스가 먼저 사라지면(리셋 등) 붙은 채로 남지 않게 바로 복귀
	Target->OnEndPlay.AddUniqueDynamic(this, &ABarrier::HandleFollowTargetEndPlay);

	// 위치만 대상을 따라가고 회전·크기는 월드 값 고정 (회전까지 따라가면 보스가 돌 때 무늬가 같이 돔)
	// 절대 회전·크기는 붙이기 전에 켜야 KeepWorld가 지금 월드 값을 그대로 유지함
	if (USceneComponent* Root = GetRootComponent())
	{
		Root->SetUsingAbsoluteRotation(true);
		Root->SetUsingAbsoluteScale(true);
	}
	const FAttachmentTransformRules Rules(EAttachmentRule::SnapToTarget, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false);
	AttachToActor(Target, Rules);
	SetActorRelativeLocation(FollowOffset);
}

void ABarrier::ReturnToHome()
{
	if (AActor* Target = FollowTarget.Get())
	{
		Target->OnEndPlay.RemoveDynamic(this, &ABarrier::HandleFollowTargetEndPlay);
	}
	FollowTarget.Reset();

	if (GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (USceneComponent* Root = GetRootComponent())
		{
			Root->SetUsingAbsoluteRotation(false);
			Root->SetUsingAbsoluteScale(false);
		}
		SetActorTransform(HomeTransform);
	}
}

void ABarrier::HandleFollowTargetEndPlay(AActor* Actor, EEndPlayReason::Type EndPlayReason)
{
	ReturnToHome();
}

void ABarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DissolveElapsed += DeltaSeconds;
	const float Alpha = FMath::Clamp(DissolveElapsed / DissolveDurationCurrent, 0.f, 1.f);
	SetDissolveValue(FMath::Lerp(DissolveFrom, DissolveTarget, Alpha));

	if (Alpha >= 1.f)
	{
		FinishDissolve();
	}
}

void ABarrier::SetDissolveValue(float Value)
{
	DissolveValue = Value;
	if (MeshMID)
	{
		MeshMID->SetScalarParameterValue(DissolveParamName, Value);
	}
}
