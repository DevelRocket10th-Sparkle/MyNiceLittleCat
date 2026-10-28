#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

UEnemyMeleeTraceComponent::UEnemyMeleeTraceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UEnemyMeleeTraceComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner()))
		RegisterHitSource(TEXT("Body"), Enemy->GetMesh());
}

void UEnemyMeleeTraceComponent::RegisterHitSource(FName SourceName, UMeshComponent* Mesh)
{
	if (SourceName.IsNone() || !IsValid(Mesh))
	{
		return;
	}
	// 장비 교체 시 이전 무기의 위치에서 새 무기까지 잘못 Sweep하지 않음
	if (ActiveRequestId != 0)
	{
		StopAttack();
	}
	Sources.Add(SourceName, Mesh);
}

void UEnemyMeleeTraceComponent::UnregisterHitSource(FName SourceName)
{
	if (ActiveRequestId != 0)
	{
		StopAttack();
	}
	Sources.Remove(SourceName);
}

UMeshComponent* UEnemyMeleeTraceComponent::GetHitSource(FName SourceName) const
{
	const TWeakObjectPtr<UMeshComponent>* Source = Sources.Find(SourceName);
	return Source ? Source->Get() : nullptr;
}

bool UEnemyMeleeTraceComponent::PrepareAttack(const TArray<TObjectPtr<UEnemyMeleeTraceSettings>>& Definitions, int64 RequestId)
{
	StopAttack();
	if (!GetOwner() || !GetOwner()->HasAuthority() || RequestId == 0 || Definitions.IsEmpty())
	{
		return false;
	}
	for (const UEnemyMeleeTraceSettings* Definition : Definitions)
	{
		if (!IsValid(Definition))
		{
			StopAttack();
			return false;
		}
		UMeshComponent* Mesh = GetHitSource(Definition->SourceName);
		if (!IsValid(Mesh) || Definition->SocketNames.IsEmpty()
			|| !FMath::IsFinite(Definition->Radius) || Definition->Radius <= 0.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%s] Invalid melee source/settings: %s"), *GetNameSafe(GetOwner()), *Definition->SourceName.ToString());
			StopAttack();
			return false;
		}
		for (FName Socket : Definition->SocketNames)
		{
			if (Socket.IsNone() || !Mesh->DoesSocketExist(Socket))
			{
				UE_LOG(LogTemp, Warning, TEXT("[%s] Melee socket missing: %s.%s"), *GetNameSafe(GetOwner()), *Definition->SourceName.ToString(), *Socket.ToString());
				StopAttack();
				return false;
			}
			FTracePoint& Point = Points.AddDefaulted_GetRef();
			Point.Mesh = Mesh;
			Point.SourceName = Definition->SourceName;
			Point.Socket = Socket;
			Point.Radius = Definition->Radius;
		}
		AddTickPrerequisiteComponent(Mesh);
		if (USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh))
		{
			if (!PoseSettings.ContainsByPredicate([SkeletalMesh](const FPoseSetting& Setting) { return Setting.Mesh == SkeletalMesh; }))
			{
				PoseSettings.Add({SkeletalMesh, static_cast<uint8>(SkeletalMesh->VisibilityBasedAnimTickOption)});
				// 공격 중에는 화면 밖/서버에서도 소켓의 최신 뼈 위치가 필요
				SkeletalMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			}
		}
	}
	ActiveRequestId = RequestId;
	return !Points.IsEmpty();
}

void UEnemyMeleeTraceComponent::BeginWindow(const UObject* NotifyKey)
{
	if (!NotifyKey || ActiveRequestId == 0 || ActiveNotify.IsValid())
	{
		return;
	}
	for (FTracePoint& Point : Points)
	{
		if (!Point.Mesh.IsValid())
		{
			StopAttack();
			return;
		}
		Point.PreviousPosition = Point.Mesh->GetSocketLocation(Point.Socket);
	}
	ActiveNotify = NotifyKey;
	// 판정 구간이 여러 개여도 요청 전체에서 중복 타격을 방지한다.
	++Generation;
	SetComponentTickEnabled(true);
	// 시작 시 이미 겹쳐 있는 캡슐도 검사
	SampleSockets();
}

void UEnemyMeleeTraceComponent::EndWindow(const UObject* NotifyKey)
{
	if (ActiveNotify.Get() != NotifyKey || !NotifyKey)
	{
		return;
	}
	const uint64 EndingGeneration = Generation;
	// 마지막 애니메이션 갱신부터 NotifyEnd까지 이동한 구간도 포함
	SampleSockets();
	if (Generation != EndingGeneration)
	{
		return;
	}
	ActiveNotify.Reset();
	SetComponentTickEnabled(false);
	++Generation;
}

void UEnemyMeleeTraceComponent::StopAttack(int64 ExpectedRequestId)
{
	if (ExpectedRequestId != 0 && ActiveRequestId != ExpectedRequestId)
	{
		return;
	}
	++Generation;
	ActiveNotify.Reset();
	ActiveRequestId = 0;
	SetComponentTickEnabled(false);
	for (const FTracePoint& Point : Points)
		if (Point.Mesh.IsValid())
		{
			RemoveTickPrerequisiteComponent(Point.Mesh.Get());
		}
	for (const FPoseSetting& Setting : PoseSettings)
		if (Setting.Mesh.IsValid())
		{
			Setting.Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(Setting.PreviousOption);
		}
	PoseSettings.Reset();
	Points.Reset();
	HitActors.Reset();
}

void UEnemyMeleeTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	SampleSockets();
}

void UEnemyMeleeTraceComponent::SampleSockets()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!ActiveNotify.IsValid() || !Enemy || !Enemy->HasAuthority() || !Enemy->IsAlive())
	{
		return;
	}
	const uint64 SampleGeneration = Generation;
	FCollisionObjectQueryParams Objects(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyMeleeSweep), false, Enemy);
	for (const FTracePoint& Point : Points)
		if (Point.Mesh.IsValid())
		{
			Params.AddIgnoredActor(Point.Mesh->GetOwner());
		}
	TArray<FHitResult> Candidates;
	for (FTracePoint& Point : Points)
	{
		if (!Point.Mesh.IsValid() || !Point.Mesh->IsRegistered())
		{
			StopAttack();
			return;
		}
		const FVector Current = Point.Mesh->GetSocketLocation(Point.Socket);
		if (Current.ContainsNaN() || Point.PreviousPosition.ContainsNaN())
		{
			StopAttack();
			return;
		}
		TArray<FHitResult> Hits;
		GetWorld()->SweepMultiByObjectType(Hits, Point.PreviousPosition, Current, FQuat::Identity,
			Objects, FCollisionShape::MakeSphere(Point.Radius), Params);
		Candidates.Append(Hits);
		if (bDrawDebugTraces)
		{
			DrawDebugSphere(GetWorld(), Current, Point.Radius, 16, FColor::Cyan, false, 0.f);
			const FVector SweepDelta = Current - Point.PreviousPosition;
			if (!SweepDelta.IsNearlyZero())
				DrawDebugCapsule(GetWorld(), (Point.PreviousPosition + Current) * 0.5f,
					Point.Radius + SweepDelta.Size() * 0.5f, Point.Radius,
					FQuat::FindBetweenNormals(FVector::UpVector, SweepDelta.GetSafeNormal()), FColor::Cyan, false, 0.f);
			DrawDebugLine(GetWorld(), Point.PreviousPosition, Current, FColor::Cyan, false, 0.f);
			DrawDebugString(GetWorld(), Current + FVector(0.f, 0.f, Point.Radius + 8.f),
				FString::Printf(TEXT("%s.%s (%.1f cm)"), *Point.SourceName.ToString(), *Point.Socket.ToString(), Point.Radius),
				nullptr, FColor::Cyan, 0.f, true);
		}
		Point.PreviousPosition = Current;
	}
	// 피해로 플레이어가 사망하면 GA 종료가 Points를 비울 수 있으므로 검사와 전달을 분리
	for (const FHitResult& Hit : Candidates)
	{
		if (Generation != SampleGeneration)
		{
			return;
		}
		APlayerCharacter* Player = Cast<APlayerCharacter>(Hit.GetActor());
		if (!IsValid(Player) || Player->GetbIsDead() || Hit.GetComponent() != Player->GetCapsuleComponent() || HitActors.Contains(Player))
		{
			continue;
		}
		HitActors.Add(Player);
		OnPlayerHit.Broadcast(Hit);
	}
}

void UEnemyMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttack();
	OnPlayerHit.Clear();
	Sources.Reset();
	Super::EndPlay(EndPlayReason);
}
