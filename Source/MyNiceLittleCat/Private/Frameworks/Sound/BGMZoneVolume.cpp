#include "Frameworks/Sound/BGMZoneVolume.h"
#include "Components/BoxComponent.h"
#include "Frameworks/Sound/BGMSubSystem.h"
#include "GameFramework/PlayerController.h"

ABGMZoneVolume::ABGMZoneVolume()
{
 	PrimaryActorTick.bCanEverTick = false;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(FVector(1000.f, 1000.f, 500.f));

	// 오버랩 감지만 하고 아무것도 막지 않음
	// 쿼리(오버랩)만 사용, 물리 충돌 없음
	Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	// 오브젝트 타입을 Vehicle로: 레이저의 ObjectType 트레이스(WorldStatic/Dynamic/Pawn/PhysicsBody)에 안 걸리게 하려는 의도
	// (프로젝트에서 Vehicle 채널은 쓰는 곳이 없음)
	Box->SetCollisionObjectType(ECC_Vehicle);

	// 모든 채널 무시 → 터렛 프로젝타일(커스텀 채널) 등 아무것도 막지 않음
	Box->SetCollisionResponseToAllChannels(ECR_Ignore);

	// 플레이어 감지만 남김
	Box->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Box->SetGenerateOverlapEvents(true);
}

void ABGMZoneVolume::BeginPlay()
{
	Super::BeginPlay();

	Box->OnComponentBeginOverlap.AddDynamic(this, &ABGMZoneVolume::OnBoxBeginOverlap);
	Box->OnComponentEndOverlap.AddDynamic(this, &ABGMZoneVolume::OnBoxEndOverlap);

	// 플레이어가 이미 박스 안에서 스폰된 경우에 대비
	// --> BeginOverlap이 안 올 수 있어서 직접 확인
	TArray<AActor*> Overlapping;

	// 체크포인트 이동/리스폰 대비: 빙의 변경 이벤트 구독
	// (PC가 아직 없을 수 있어서 다음 틱 CheckInitialPlayerInside에서도 한 번 더 시도)
	BindPossessionChanged();

	// 플레이어 스폰/빙의가 모두 끝난 뒤 확인하도록 한 틱 미룸
	GetWorldTimerManager().SetTimerForNextTick(this, &ABGMZoneVolume::CheckInitialPlayerInside);
}

void ABGMZoneVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (APlayerController* PC = BoundController.Get())
	{
		PC->OnPossessedPawnChanged.RemoveDynamic(this, &ABGMZoneVolume::HandlePossessedPawnChanged);
	}
	BoundController.Reset();

	ExitNow();

	Super::EndPlay(EndPlayReason);
}

void ABGMZoneVolume::OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (true == bPlayerInside || false == IsPlayerPawn(OtherActor)) return;

	EnterWithPawn(Cast<APawn>(OtherActor));
}

void ABGMZoneVolume::OnBoxEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (false == bPlayerInside) return;

	// IsPlayerControlled()로 판정하지 않음:
	// 리스폰 시 폰은 컨트롤러가 떨어진 뒤(DetachFromControllerPendingDestroy) 파괴되므로 여기서 false가 나옴
	// → 들어올 때 기억한 폰과 같은지로 판정 (폰이 이미 무효면 정리 차원에서 이탈)
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (nullptr == Pawn) return;

	if (InsidePawn == Pawn || false == InsidePawn.IsValid())
	{
		ExitNow();
	}
}

void ABGMZoneVolume::BindPossessionChanged()
{
	if (BoundController.IsValid()) return;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (nullptr == PC) return;

	PC->OnPossessedPawnChanged.AddUniqueDynamic(this, &ABGMZoneVolume::HandlePossessedPawnChanged);
	BoundController = PC;
}

void ABGMZoneVolume::HandlePossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	// 빙의 해제(NewPawn 없음)는 무시: 폰 파괴 시 EndOverlap에서 처리됨
	if (nullptr == NewPawn) return;

	// 새 폰은 이미 스폰 위치에 있으므로 위치로 직접 판정
	if (true == IsLocationInsideBox(NewPawn->GetActorLocation()))
	{
		// 같은 폰으로 이미 들어와 있으면 그대로 (곡 재시작 방지)
		if (true == bPlayerInside && InsidePawn == NewPawn) return;

		EnterWithPawn(NewPawn);
	}
	else
	{
		// 이전 폰 기준으로 남아 있던 진입 상태 정리
		ExitNow();
	}
}

void ABGMZoneVolume::EnterWithPawn(APawn* Pawn)
{
	if (nullptr == Pawn) return;

	UGameInstance* GI = GetGameInstance();
	UBGMSubSystem* BGM = GI ? GI->GetSubsystem<UBGMSubSystem>() : nullptr;
	if (nullptr == BGM) return;

	// EnterZone은 같은 Zone 중복 진입 시 기존 항목을 교체하므로 다시 불러도 안전
	bPlayerInside = true;
	InsidePawn = Pawn;
	BGM->EnterZone(this, ZoneSound, Priority, FadeTime);
}

void ABGMZoneVolume::ExitNow()
{
	if (false == bPlayerInside) return;

	bPlayerInside = false;
	InsidePawn.Reset();

	if (UGameInstance* GI = GetGameInstance())
	{
		if (UBGMSubSystem* BGM = GI->GetSubsystem<UBGMSubSystem>())
			BGM->ExitZone(this, FadeTime);
	}
}

bool ABGMZoneVolume::IsPlayerPawn(const AActor* Actor) const
{
	const APawn* Pawn = Cast<APawn>(Actor);
	return Pawn && Pawn->IsPlayerControlled();
}

void ABGMZoneVolume::CheckInitialPlayerInside()
{
	// BeginPlay 때 PC가 없었으면 여기서 구독
	BindPossessionChanged();

	// 이미 오버랩 이벤트로 들어온 상태면 중복 처리 안 함
	if (true == bPlayerInside) return;

	APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (nullptr == Pawn) return;

	// 오버랩 대신 위치로 판정 -> 이벤트 타이밍과 무관하게 정확함
	if (true == IsLocationInsideBox(Pawn->GetActorLocation()))
		EnterWithPawn(Pawn);
}

bool ABGMZoneVolume::IsLocationInsideBox(const FVector& WorldLocation) const
{
	// 월드 좌표 → 박스 로컬 좌표(회전·스케일 제거)로 바꾼 뒤 Extent와 비교
	const FVector Local = Box->GetComponentTransform().InverseTransformPosition(WorldLocation);
	const FVector Extent = Box->GetUnscaledBoxExtent();
	
	return FMath::Abs(Local.X) <= Extent.X && FMath::Abs(Local.Y) <= Extent.Y && FMath::Abs(Local.Z) <= Extent.Z;
}
