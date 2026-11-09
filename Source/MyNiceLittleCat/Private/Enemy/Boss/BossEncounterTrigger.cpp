#include "Enemy/Boss/BossEncounterTrigger.h"
#include "Enemy/Boss/BossEncounterManager.h"
#include "Characters/PlayerCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/Level.h"

ABossEncounterTrigger::ABossEncounterTrigger()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->SetBoxExtent(FVector(300.f, 300.f, 200.f));
	// 플레이어 폰만 겹침으로 감지
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetGenerateOverlapEvents(true);
}

// 매니저 지정 여부 확인 및 겹침 이벤트 구독
void ABossEncounterTrigger::BeginPlay()
{
	Super::BeginPlay();

	EncounterManager = ResolveEncounterManager();
	if (!IsValid(EncounterManager))
	{
		UE_LOG(LogTemp, Error, TEXT("Boss encounter trigger has no EncounterManager: %s"), *GetName());
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("Boss encounter trigger %s -> %s"), *GetName(), *EncounterManager->GetName());
	TriggerBox->OnComponentBeginOverlap.AddUniqueDynamic(this, &ABossEncounterTrigger::HandleBeginOverlap);
}

// 직접 지정한 매니저 우선, 비어 있으면 같은 레벨(레벨 인스턴스)에 하나뿐인 매니저 사용
// - 월드 전체 검색(GetActorOfClass)은 보스방이 여러 개일 때 대상이 바뀌므로 사용하지 않음
ABossEncounterManager* ABossEncounterTrigger::ResolveEncounterManager() const
{
	if (IsValid(EncounterManager))
		return EncounterManager;

	const ULevel* Level = GetLevel();
	if (!Level)
		return nullptr;

	ABossEncounterManager* Found = nullptr;
	for (AActor* Actor : Level->Actors)
	{
		ABossEncounterManager* Manager = Cast<ABossEncounterManager>(Actor);
		if (!IsValid(Manager))
			continue;
		if (Found)
		{
			UE_LOG(LogTemp, Error, TEXT("Multiple encounter managers in trigger level, assign one explicitly: %s"), *GetName());
			return nullptr;
		}
		Found = Manager;
	}
	return Found;
}

// 겹침 이벤트 구독 해제 후 부모 호출
void ABossEncounterTrigger::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	TriggerBox->OnComponentBeginOverlap.RemoveDynamic(this, &ABossEncounterTrigger::HandleBeginOverlap);
	Super::EndPlay(EndPlayReason);
}

void ABossEncounterTrigger::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryRequestStart(OtherActor);
}

// 살아 있는 플레이어일 때만 매니저에 시작 요청
void ABossEncounterTrigger::TryRequestStart(AActor* OtherActor)
{
	if (!HasAuthority() || !IsValid(EncounterManager))
		return;

	APlayerCharacter* Player = Cast<APlayerCharacter>(OtherActor);
	if (!Player || !Player->IsPlayerControlled() || Player->GetbIsDead())
		return;

	// 이미 전투 중이거나 연출 중이면 매니저가 거절하므로 재진입해도 중복 시작되지 않음
	EncounterManager->RequestEncounterStart(Player);
}
