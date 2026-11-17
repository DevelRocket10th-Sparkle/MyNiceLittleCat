/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 신호로 열리고 닫히는 문 구현

#include "Gimmicks/Stage/GimmickDoor.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Gimmicks/InteractPromptAnchorComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "NavigationSystem.h"
#include "NavigationData.h"

AGimmickDoor::AGimmickDoor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	DoorMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("DoorMover"));
	DoorMover->SetupAttachment(Root);
	DoorMover->OpenOffset = FVector(0.f, 0.f, -300.f);
	DoorMover->OpenDuration = 1.f;

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorMover);
	DoorMesh->SetMobility(EComponentMobility::Movable);
	// 내비 차단은 DoorwayVolume이 맡음 (메시가 열린 위치에서도 길을 막지 않게)
	DoorMesh->SetCanEverAffectNavigation(false);

	DoorwayVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("DoorwayVolume"));
	DoorwayVolume->SetupAttachment(Root);
	DoorwayVolume->SetBoxExtent(FVector(40.f, 150.f, 150.f));
	DoorwayVolume->SetRelativeLocation(FVector(0.f, 0.f, 150.f));
	DoorwayVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DoorwayVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	DoorwayVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DoorwayVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	DoorwayVolume->SetGenerateOverlapEvents(true);
	DoorwayVolume->bDynamicObstacle = true;
	// 배치 상태(닫힘) 기준으로 내비 빌드에도 반영, 열리면 BeginPlay·상태 변경에서 끔
	DoorwayVolume->SetCanEverAffectNavigation(true);
	DoorwayVolume->ShapeColor = FColor(255, 160, 40);
}

void AGimmickDoor::BeginPlay()
{
	// 잠금은 저장하지 않으므로 항상 배치 설정으로 시작
	bLocked = bStartLocked;

	DoorwayVolume->OnComponentBeginOverlap.AddDynamic(this, &AGimmickDoor::HandleDoorwayBeginOverlap);
	DoorwayVolume->OnComponentEndOverlap.AddDynamic(this, &AGimmickDoor::HandleDoorwayOverlapChanged);

	Super::BeginPlay();

	// 시작 상태가 닫힘이면 Super에서 상태 변경이 없으므로 여기서 내비 반영
	ApplyNavigationBlock();

	if (bBlockNavigationWhenClosed)
	{
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		const ANavigationData* NavData = NavSys ? NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
		if (NavData && NavData->GetRuntimeGenerationMode() == ERuntimeGenerationType::Static)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GimmickDoor] %s: 내비 런타임 생성이 Static이라 문 열림·닫힘이 내비게이션에 반영되지 않음 (Dynamic Modifiers Only 이상 필요)"), *GetName());
		}
	}
}

bool AGimmickDoor::IsDoorwayObstructed() const
{
	TArray<AActor*> Overlapping;
	DoorwayVolume->GetOverlappingActors(Overlapping);
	for (const AActor* Actor : Overlapping)
	{
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		if (ObstructionClasses.Num() == 0)
		{
			if (Actor->IsA<APawn>())
			{
				return true;
			}
			continue;
		}

		for (const TSubclassOf<AActor>& Class : ObstructionClasses)
		{
			if (Class && Actor->IsA(Class))
			{
				return true;
			}
		}
	}
	return false;
}

void AGimmickDoor::UpdateMovers(bool bInstant)
{
	// 연출 강제 상태가 있으면 신호보다 우선
	bool bOpen = bForced ? bForcedOpen : IsActivated();

	// 닫혀야 해도 문이 덜 닫혔고 공간이 막혀 있으면 열린 채 유지
	// - 연출 강제 중에는 대기하지 않음: 컷 타이밍대로 닫혀야 하므로
	if (!bForced && !bOpen && bWaitWhileObstructed && !bInstant && DoorMover->GetProgress() > 0.f && IsDoorwayObstructed())
	{
		bOpen = true;
	}

	bDoorOpen = bOpen;

	// 연출이 지정한 이동 시간 (신호로 움직일 때는 -1 = Mover 기본 시간)
	const float Duration = bForced ? ForcedDuration : -1.f;

	TInlineComponentArray<UGimmickMoverComponent*> Movers(this);
	for (UGimmickMoverComponent* Mover : Movers)
	{
		if (Mover->bFollowActivation)
		{
			Mover->SetOpen(bOpen, bInstant, Duration);
		}
	}

	ApplyNavigationBlock();
}

void AGimmickDoor::HandleDoorwayBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	HandleDoorwayOverlapChanged(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex);
}

void AGimmickDoor::HandleDoorwayOverlapChanged(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	// 닫히는 중·닫힘 대기 중에만 다시 판단 (연출 강제 중에는 판단하지 않음)
	if (!bForced && bWaitWhileObstructed && !IsActivated())
	{
		UpdateMovers(false);
	}
}

void AGimmickDoor::ApplyNavigationBlock()
{
	const bool bBlock = bBlockNavigationWhenClosed && !bDoorOpen;
	if (DoorwayVolume->CanEverAffectNavigation() != bBlock)
	{
		DoorwayVolume->SetCanEverAffectNavigation(bBlock);
	}
}

/* 연출 제어 */

void AGimmickDoor::ForceDoorState(bool bOpen, float Duration, bool bInstant)
{
	const bool bWasOpen = bDoorOpen;

	bForced = true;
	bForcedOpen = bOpen;
	ForcedDuration = Duration;
	UpdateMovers(bInstant);

	// 상태가 바뀐 애니메이션 이동에만 문 효과음 (스킵/초기화의 즉시 이동은 소리 없음)
	if (!bInstant && bWasOpen != bDoorOpen)
	{
		PlayGimmickSound(bDoorOpen ? ActivateSound : DeactivateSound);
	}
}

void AGimmickDoor::ReleaseForcedDoorState(bool bInstant)
{
	if (!bForced)
	{
		return;
	}

	bForced = false;
	ForcedDuration = -1.f;

	// 신호(켜짐) 상태 기준으로 다시 맞춤
	UpdateMovers(bInstant);
}

/* 잠금 */

void AGimmickDoor::SetLocked(bool bNewLocked, AActor* ChangeInstigator)
{
	// 잠금은 플레이어 상호작용만 막으므로 문 위치는 그대로 (닫으면서 잠그려면 연출 강제나 신호를 함께 사용)
	bLocked = bNewLocked;
}

/* IInteractableInterface */

bool AGimmickDoor::CanInteract_Implementation(AActor* InteractInstigator) const
{
	// 플레이어가 조종하는 Pawn만
	const APawn* Pawn = Cast<APawn>(InteractInstigator);
	if (!Pawn || !Pawn->IsPlayerControlled())
	{
		return false;
	}

	// 열린 문: 플레이어가 닫을 수 있는 문일 때만 (잠긴 문, 연출이 열어 둔 문은 상호작용 없음)
	if (bDoorOpen)
	{
		return bPlayerCanToggle && !bLocked && !bForced;
	}

	// 닫힌 문: 여닫을 수 있는 문이거나, 잠겨 있어 확인할 수 있는 문
	return bPlayerCanToggle || bLocked;
}

void AGimmickDoor::Interact_Implementation(AActor* InteractInstigator)
{
	// BP에서 CanInteract를 덮어쓴 경우도 따르도록 Execute로 확인
	if (!IInteractableInterface::Execute_CanInteract(this, InteractInstigator))
	{
		return;
	}

	// 잠긴 문, 연출이 닫아 둔 문 (전투 중 등): 확인만 (덜컹 소리)
	if (bLocked || bForced)
	{
		PlayGimmickSound(LockedSound);
		return;
	}

	// 신호와 같은 켜짐 상태를 바꿈: 이후 신호 변화가 오면 그쪽이 다시 덮어씀
	SetActivated(!IsActivated(), InteractInstigator);
}

FVector AGimmickDoor::GetInteractFocusLocation_Implementation(AActor* InteractInstigator) const
{
	if (!IsValid(InteractInstigator))
	{
		return DoorwayVolume->GetComponentLocation();
	}

	// 넓은 문(대문)도 자연스럽게 감지되도록 문 공간에서 플레이어와 가장 가까운 점
	const FVector From = InteractInstigator->GetActorLocation();
	FVector Point;
	if (DoorwayVolume->GetClosestPointOnCollision(From, Point) >= 0.f)
	{
		return Point;
	}

	return DoorwayVolume->Bounds.GetBox().GetClosestPointTo(From);
}

FInteractAlignment AGimmickDoor::GetInteractAlignment_Implementation(AActor* InteractInstigator) const
{
	// 제자리에서 문 쪽으로 몸만 돌림 (레버 기본값 FaceOnly와 같은 동작)
	FInteractAlignment Alignment;
	if (!IsValid(InteractInstigator))
	{
		return Alignment;
	}

	const FVector From = InteractInstigator->GetActorLocation();
	const FVector LookDirection = (DoorwayVolume->GetComponentLocation() - From).GetSafeNormal2D();
	if (!LookDirection.IsNearlyZero())
	{
		Alignment.Mode = EInteractAlignMode::FaceOnly;
		Alignment.TargetLocation = From;
		Alignment.TargetRotation = FRotator(0.f, LookDirection.Rotation().Yaw, 0.f);
	}

	return Alignment;
}

FInteractPromptInfo AGimmickDoor::GetInteractPrompt_Implementation(AActor* InteractInstigator) const
{
	FInteractPromptInfo Prompt;

	// 안내 UI 위치: InteractPromptAnchor 컴포넌트(자물쇠 위치 등), 없으면 문 공간 중심
	const UInteractPromptAnchorComponent* Anchor = FindComponentByClass<UInteractPromptAnchorComponent>();
	Prompt.WorldLocation = Anchor ? Anchor->GetComponentLocation() : DoorwayVolume->GetComponentLocation();

	// 동작 문구: 잠김·연출이 닫아 둠 > 열기/닫기
	if (bLocked || bForced)
	{
		Prompt.ActionText = LockedPromptText;
	}
	else
	{
		Prompt.ActionText = IsDoorOpen() ? ClosePromptText : OpenPromptText;
	}

	return Prompt;
}
