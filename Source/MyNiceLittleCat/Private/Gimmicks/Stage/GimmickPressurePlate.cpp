/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 감압 발판 구현

#include "Gimmicks/Stage/GimmickPressurePlate.h"
#include "Gimmicks/GimmickMoverComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "TimerManager.h"

AGimmickPressurePlate::AGimmickPressurePlate()
{
	// 발판은 밟아서만 작동 (신호로 켜려면 BP에서 켬)
	bReceiveSignals = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	PlateMover = CreateDefaultSubobject<UGimmickMoverComponent>(TEXT("PlateMover"));
	PlateMover->SetupAttachment(Root);
	PlateMover->OpenOffset = FVector(0.f, 0.f, -5.f);
	PlateMover->OpenDuration = 0.15f;

	PlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlateMesh"));
	PlateMesh->SetupAttachment(PlateMover);
	PlateMesh->SetMobility(EComponentMobility::Movable);

	// 감지는 주기 검사로 하므로 콜리전 없음 (영역 표시·모양용)
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Root);
	Trigger->SetBoxExtent(FVector(50.f, 50.f, 30.f));
	Trigger->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Trigger->SetGenerateOverlapEvents(false);
	Trigger->SetCanEverAffectNavigation(false);
	Trigger->ShapeColor = FColor(80, 255, 120);

	ActivatorClasses.Add(APlayerCharacter::StaticClass());
	ActivatorObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_Pawn));
	ActivatorObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_PhysicsBody));
	ActivatorObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_WorldDynamic));
}

void AGimmickPressurePlate::BeginPlay()
{
	Super::BeginPlay();

	// 저장된 Latch 상태로 시작하면 감지할 필요 없음
	if (!(Mode == EPressurePlateMode::Latch && IsActivated()))
	{
		StartChecking();
	}
}

void AGimmickPressurePlate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopChecking();
	GetWorldTimerManager().ClearTimer(ReleaseTimer);
	Super::EndPlay(EndPlayReason);
}

void AGimmickPressurePlate::HandleActivationChanged(bool bNewActivated, AActor* ChangeInstigator)
{
	Super::HandleActivationChanged(bNewActivated, ChangeInstigator);

	if (Mode != EPressurePlateMode::Latch || !HasActorBegunPlay())
	{
		return;
	}

	// Latch: 켜지면 감지 멈춤, 밖에서 끄면(리셋) 다시 감지
	if (bNewActivated)
	{
		StopChecking();
	}
	else
	{
		StartChecking();
	}
}

void AGimmickPressurePlate::StartChecking()
{
	if (!GetWorldTimerManager().IsTimerActive(CheckTimer))
	{
		GetWorldTimerManager().SetTimer(CheckTimer, this, &AGimmickPressurePlate::CheckOccupants, CheckInterval, true, 0.f);
	}
}

void AGimmickPressurePlate::StopChecking()
{
	GetWorldTimerManager().ClearTimer(CheckTimer);
}

void AGimmickPressurePlate::CheckOccupants()
{
	const int32 NewCount = CountOccupants();
	if (NewCount != OccupantCount)
	{
		OccupantCount = NewCount;
		OnOccupantsChanged.Broadcast(this, OccupantCount);
	}

	if (OccupantCount >= RequiredCount)
	{
		GetWorldTimerManager().ClearTimer(ReleaseTimer);
		SetActivated(true, this);
		return;
	}

	if (Mode != EPressurePlateMode::Hold || !IsActivated())
	{
		return;
	}

	if (ReleaseDelay <= 0.f)
	{
		SetActivated(false, this);
	}
	else if (!GetWorldTimerManager().IsTimerActive(ReleaseTimer))
	{
		GetWorldTimerManager().SetTimer(ReleaseTimer, this, &AGimmickPressurePlate::HandleReleaseDelayExpired, ReleaseDelay, false);
	}
}

void AGimmickPressurePlate::HandleReleaseDelayExpired()
{
	if (OccupantCount < RequiredCount)
	{
		SetActivated(false, this);
	}
}

int32 AGimmickPressurePlate::CountOccupants() const
{
	UWorld* World = GetWorld();
	if (!World || ActivatorObjectTypes.Num() == 0)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectParams;
	for (const TEnumAsByte<EObjectTypeQuery>& ObjectType : ActivatorObjectTypes)
	{
		ObjectParams.AddObjectTypesToQuery(UEngineTypes::ConvertToCollisionChannel(ObjectType));
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PressurePlate), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Trigger->GetComponentLocation(), Trigger->GetComponentQuat(), ObjectParams, Trigger->GetCollisionShape(), QueryParams);

	TSet<const AActor*> Counted;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		const AActor* Actor = Overlap.GetActor();
		if (!IsValid(Actor) || Counted.Contains(Actor))
		{
			continue;
		}

		bool bAllowed = ActivatorClasses.Num() == 0;
		for (const TSubclassOf<AActor>& Class : ActivatorClasses)
		{
			if (Class && Actor->IsA(Class))
			{
				bAllowed = true;
				break;
			}
		}

		if (bAllowed)
		{
			Counted.Add(Actor);
		}
	}
	return Counted.Num();
}
