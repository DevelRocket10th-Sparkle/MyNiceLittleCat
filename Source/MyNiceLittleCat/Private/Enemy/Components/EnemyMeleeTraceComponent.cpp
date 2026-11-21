#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/EnemyLog.h"

#include "Algo/AllOf.h"
#include "Components/MeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"

#include "Enemy/Combat/EnemyTargetUtils.h"
#include "Enemy/EnemyCharacter.h"

namespace
{
	// 등록된 Weapon 소스가 없을 때, 필요한 소켓을 모두 가진 부착 메시를 찾음 (둘 이상이면 모호하므로 실패)
	UMeshComponent* FindAttachedWeapon(
		USkeletalMeshComponent* BodyMesh,
		const TArray<TObjectPtr<UEnemyMeleeTraceSettings>>& Definitions)
	{
		if (!IsValid(BodyMesh))
		{
			return nullptr;
		}

		// Weapon 소스가 요구하는 소켓 목록
		TSet<FName> RequiredSockets;
		for (const UEnemyMeleeTraceSettings* Definition : Definitions)
		{
			if (IsValid(Definition) && Definition->SourceName == EnemyMeleeSources::Weapon)
			{
				RequiredSockets.Append(Definition->SocketNames);
			}
		}


		TArray<USceneComponent*> Attachments;
		BodyMesh->GetChildrenComponents(true, Attachments);

		UMeshComponent* Weapon = nullptr;
		for (USceneComponent* Attachment : Attachments)
		{
			UMeshComponent* Mesh = Cast<UMeshComponent>(Attachment);
			const bool bHasAllSockets =
				IsValid(Mesh) && Mesh->IsRegistered()
				&& Algo::AllOf(
					RequiredSockets,
					[Mesh](FName Socket)
					{
						return !Socket.IsNone() && Mesh->DoesSocketExist(Socket);
					}
				);
			if (!bHasAllSockets)
			{
				continue;
			}

			if (Weapon)
			{
				UE_LOG(LogEnemy, Warning, TEXT("[%s] Ambiguous melee source Weapon: %s and %s both match. Register the weapon explicitly."),
					*GetNameSafe(BodyMesh->GetOwner()), *GetNameSafe(Weapon), *GetNameSafe(Mesh));
				return nullptr;
			}

			Weapon = Mesh;
		}

		return Weapon;
	}
}

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
	{
		RegisterHitSource(EnemyMeleeSources::Body, Enemy->GetMesh());
	}
}

void UEnemyMeleeTraceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAttack();
	OnTargetHit.Clear();
	Sources.Reset();

	Super::EndPlay(EndPlayReason);
}

/* 판정 소스 등록 */

void UEnemyMeleeTraceComponent::RegisterHitSource(FName SourceName, UMeshComponent* Mesh)
{
	if (SourceName.IsNone() || !IsValid(Mesh))
	{
		return;
	}

	// 장비 교체 시 이전 무기 위치에서 새 무기까지 잘못 Sweep하지 않도록 진행 중인 판정 중단
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

/* 공격 실행 */

bool UEnemyMeleeTraceComponent::PrepareAttack(const TArray<TObjectPtr<UEnemyMeleeTraceSettings>>& Definitions, int64 RequestId)
{
	StopAttack();

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!Enemy || !Enemy->HasAuthority() || RequestId == 0 || Definitions.IsEmpty())
	{
		return false;
	}

	// 공격마다 현재 장착 상태 확인: 무기 교체 후 메시를 재사용하지 않음
	UMeshComponent* AutoWeapon = nullptr;

	for (const UEnemyMeleeTraceSettings* Definition : Definitions)
	{
		/* 설정 검사 */

		if (!IsValid(Definition) || Definition->SocketNames.IsEmpty()
			|| !FMath::IsFinite(Definition->Radius) || Definition->Radius <= 0.f)
		{
			UE_LOG(LogEnemy, Warning, TEXT("[%s] Invalid melee trace settings"), *GetNameSafe(GetOwner()));
			StopAttack();
			return false;
		}

		/* 소스 메시 찾기 */

		UMeshComponent* Mesh = GetHitSource(Definition->SourceName);
		if (!IsValid(Mesh) && Definition->SourceName == EnemyMeleeSources::Weapon)
		{
			if (!AutoWeapon)
			{
				AutoWeapon = FindAttachedWeapon(Enemy->GetMesh(), Definitions);
			}
			Mesh = AutoWeapon;
		}

		if (!IsValid(Mesh))
		{
			UE_LOG(LogEnemy, Warning, TEXT("[%s] No melee mesh for source %s"), *GetNameSafe(Enemy), *Definition->SourceName.ToString());
			StopAttack();
			return false;
		}

		/* 소켓마다 판정 지점 추가 */

		for (const FName Socket : Definition->SocketNames)
		{
			if (Socket.IsNone() || !Mesh->DoesSocketExist(Socket))
			{
				UE_LOG(LogEnemy, Warning, TEXT("[%s] Melee socket missing: %s.%s"),
					*GetNameSafe(GetOwner()), *Definition->SourceName.ToString(), *Socket.ToString());
				StopAttack();
				return false;
			}

			FTracePoint& Point = Points.AddDefaulted_GetRef();
			Point.Mesh = Mesh;
			Point.SourceName = Definition->SourceName;
			Point.Socket = Socket;
			Point.Radius = Definition->Radius;
			Point.bRequireBodyAttachment = Mesh == AutoWeapon;
		}

		/* 최신 포즈 보장: 메시 Tick 이후 판정, 화면 밖에서도 뼈 갱신 */

		AddTickPrerequisiteComponent(Mesh);

		for (USceneComponent* Component = Mesh; Component; Component = Component->GetAttachParent())
		{
			USkeletalMeshComponent* SkeletalMesh = Cast<USkeletalMeshComponent>(Component);
			const bool bAlreadySaved =
				SkeletalMesh
				&& PoseSettings.ContainsByPredicate(
					[SkeletalMesh](const FPoseSetting& Setting)
					{
						return Setting.Mesh == SkeletalMesh;
					}
				);

			if (SkeletalMesh && !bAlreadySaved)
			{
				// 스태틱 메시 무기도 부착된 스켈레탈 메시의 최신 포즈가 필요
				PoseSettings.Add({ SkeletalMesh, static_cast<uint8>(SkeletalMesh->VisibilityBasedAnimTickOption) });
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
		return;

	// 구간 시작 위치 기록
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
	++Generation;
	SetComponentTickEnabled(true);

	// 시작 시 이미 겹쳐 있는 캡슐도 검사
	SampleSockets();
}

void UEnemyMeleeTraceComponent::EndWindow(const UObject* NotifyKey)
{
	if (!NotifyKey || ActiveNotify.Get() != NotifyKey)
		return;

	// 마지막 애니메이션 갱신부터 NotifyEnd까지 이동한 구간도 포함
	const uint64 EndingGeneration = Generation;
	SampleSockets();

	// 알림 콜백에서 판정이 이미 중단/재시작됐으면 여기서 더 정리하지 않음
	if (Generation != EndingGeneration)
		return;

	ActiveNotify.Reset();
	SetComponentTickEnabled(false);
	++Generation;
}

void UEnemyMeleeTraceComponent::StopAttack(int64 ExpectedRequestId)
{
	if (ExpectedRequestId != 0 && ActiveRequestId != ExpectedRequestId)
		return;

	++Generation;
	ActiveNotify.Reset();
	ActiveRequestId = 0;
	SetComponentTickEnabled(false);

	for (const FTracePoint& Point : Points)
	{
		if (Point.Mesh.IsValid())
		{
			RemoveTickPrerequisiteComponent(Point.Mesh.Get());
		}
	}

	//  판정 중 바꾼 메시 갱신 설정 복원
	for (const FPoseSetting& Setting : PoseSettings)
	{
		if (Setting.Mesh.IsValid())
		{
			Setting.Mesh->VisibilityBasedAnimTickOption = static_cast<EVisibilityBasedAnimTickOption>(Setting.PreviousOption);
		}
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

/* 판정 */

void UEnemyMeleeTraceComponent::SampleSockets()
{
	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!ActiveNotify.IsValid() || !Enemy || !Enemy->HasAuthority() || !Enemy->IsAlive())
		return;

	const uint64 SampleGeneration = Generation;

	/* 자기 자신과 무기 소유 액터는 판정에서 제외 */

	const FCollisionObjectQueryParams Objects(ECC_Pawn);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyMeleeSweep), false, Enemy);
	for (const FTracePoint& Point : Points)
	{
		if (Point.Mesh.IsValid())
		{
			Params.AddIgnoredActor(Point.Mesh->GetOwner());
		}
	}

	/* 소켓마다 Sweep: 결과는 모아두고 알림은 나중에 (알림 중 GA 종료로 Points가 비워질 수 있음) */

	TArray<FHitResult> Candidates;

	for (FTracePoint& Point : Points)
	{
		const bool bSourceValid = Point.Mesh.IsValid() && Point.Mesh->IsRegistered()
			&& (!Point.bRequireBodyAttachment || Point.Mesh->IsAttachedTo(Enemy->GetMesh()));

		if (!bSourceValid)
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
			DrawSweepDebug(Point, Current);
		}

		Point.PreviousPosition = Current;
	}

	/* 대상마다 한 번만 알림 */

	for (const FHitResult& Hit : Candidates)
	{
		// 알림 콜백에서 판정이 중단되면 남은 결과를 버림
		if (Generation != SampleGeneration)
		{
			return;
		}

		AActor* Target = Hit.GetActor();
		if (!EnemyTargetUtils::IsHostileTarget(GetOwner(), Target)
			|| !EnemyTargetUtils::IsHitReceiverComponent(Target, Hit.GetComponent())
			|| HitActors.Contains(Target))
		{
			continue;
		}

		HitActors.Add(Target);
		OnTargetHit.Broadcast(Hit);
	}
}

void UEnemyMeleeTraceComponent::DrawSphereSweep(const UWorld* World, const FVector& Start, const FVector& End, float Radius, const FColor& Color)
{
	const FVector SweepDelta = End - Start;

	// 구를 직선으로 이동하며 검사한 영역은 캡슐 모양
	DrawDebugSphere(World, End, Radius, 16, Color, false, 0.f);
	if (!SweepDelta.IsNearlyZero())
	{
		DrawDebugCapsule(
			World,
			(Start + End) * 0.5f,
			Radius + SweepDelta.Size() * 0.5f, Radius,
			FQuat::FindBetweenNormals(FVector::UpVector, SweepDelta.GetSafeNormal()),
			Color,
			false,
			0.f
		);
	}
}

void UEnemyMeleeTraceComponent::DrawSweepDebug(const FTracePoint& Point, const FVector& Current) const
{
	UWorld* World = GetWorld();

	DrawSphereSweep(World, Point.PreviousPosition, Current, Point.Radius, FColor::Cyan);
	DrawDebugLine(World, Point.PreviousPosition, Current, FColor::Cyan, false, 0.f);
	DrawDebugString(
		World,
		Current + FVector(0.f, 0.f, Point.Radius + 8.f),
		FString::Printf(TEXT("%s.%s (%.1f cm)"),*Point.SourceName.ToString(), *Point.Socket.ToString(), Point.Radius),
		nullptr,
		FColor::Cyan,
		0.f,
		true);
}
