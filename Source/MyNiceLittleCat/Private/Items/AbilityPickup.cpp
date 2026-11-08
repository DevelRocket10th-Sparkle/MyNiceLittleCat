#include "Items/AbilityPickup.h"
#include "Items/InventorySubsystem.h"
#include "Items/ItemSettings.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

AAbilityPickup::AAbilityPickup()
{
	PrimaryActorTick.bCanEverTick = true; // 둥실거림 연출용

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(80.f);
	PickupSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic")); // 겹침만, 길을 막지 않음

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(PickupSphere);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.5f)); // 기본 구체(100cm) 기준 지름 50cm
	Mesh->SetCastShadow(false);              // 반투명 구슬이 진한 그림자를 만들면 어색해서
	Mesh->TranslucencySortPriority = 1;      // 껍질을 아이콘보다 나중에 그려서 "그림이 구슬 안에 있는" 느낌

	// 아이콘은 껍질에 붙여서 둥실거림을 같이 따라감 (빌보드라 회전은 무시되고 항상 카메라를 봄)
	IconBillboard = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("IconBillboard"));
	IconBillboard->SetupAttachment(Mesh);
	IconBillboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	IconBillboard->SetCastShadow(false);
	IconBillboard->TranslucencySortPriority = 0;

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(Mesh);
	GlowLight->SetCastShadows(false);         // 장식용 빛이라 그림자 비용 없음
	GlowLight->SetIntensityUnits(ELightUnits::Candelas);
	GlowLight->SetIntensity(15.f);
	GlowLight->SetAttenuationRadius(300.f);
}

#if WITH_EDITOR
void AAbilityPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyLook(); // 디테일 패널에서 아이콘/색을 바꾸면 뷰포트에 바로 반영
}
#endif

void AAbilityPickup::ApplyLook()
{
	// 껍질: 테두리 빛 색만 모드 색으로
	// 의도: 원본 머티리얼은 항상 BP 기본값(CDO)의 껍질 머티리얼에서 가져옴.
	//       지금 슬롯에 있는 걸 쓰면, 에디터에서 만든 인스턴스가 레벨에 저장됐다가
	//       BP 머티리얼을 바꿔도 옛 머티리얼(회색 기본 머티리얼 등)이 계속 남는 문제가 생김.
	const AAbilityPickup* Defaults = GetClass()->GetDefaultObject<AAbilityPickup>();
	UMaterialInterface* ShellBase = (nullptr != Defaults && nullptr != Defaults->Mesh) ? Defaults->Mesh->GetMaterial(0) : nullptr;
	if (UMaterialInstanceDynamic* BaseMID = Cast<UMaterialInstanceDynamic>(ShellBase))
		ShellBase = BaseMID->Parent; // 혹시 BP 기본값에 인스턴스가 들어가 있어도 원본으로
	if (nullptr != ShellBase)
	{
		UMaterialInstanceDynamic* ShellMID = UMaterialInstanceDynamic::Create(ShellBase, this);
		ShellMID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
		Mesh->SetMaterial(0, ShellMID);
	}

	// 아이콘: 머티리얼 인스턴스를 만들어 그림/색을 넣고 빌보드 한 장으로 표시
	if (nullptr != IconMaterial)
	{
		UMaterialInstanceDynamic* IconMID = UMaterialInstanceDynamic::Create(IconMaterial, this);
		if (nullptr != IconTexture)
			IconMID->SetTextureParameterValue(TEXT("Icon"), IconTexture);
		IconMID->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
		IconMID->SetVectorParameterValue(TEXT("IconColor"), IconColor);

		// 다시 호출돼도 한 장만 남도록 매번 새로 채움 (월드 크기 기준, 화면 크기 고정 아님)
		IconBillboard->Elements.Reset();
		IconBillboard->AddElement(IconMID, nullptr, false, IconSize, IconSize, nullptr);
	}

	GlowLight->SetLightColor(GlowColor);
}

void AAbilityPickup::BeginPlay()
{
	Super::BeginPlay();

	MeshBaseLocation = Mesh->GetRelativeLocation();
	ApplyLook(); // 쿠킹된 게임에서는 OnConstruction이 다시 안 돌 수 있어서 시작할 때 한 번 더

	// 이미 가진 능력이면 (세이브 로드/체크포인트 재시작) 구슬을 없앰
	UGameInstance* GI = GetGameInstance();
	UInventorySubsystem* Inventory = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
	if (nullptr != Inventory && nullptr != AbilityItem && true == Inventory->HasItem(AbilityItem))
	{
		Destroy();
		return;
	}

	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AAbilityPickup::HandleBeginOverlap);
}

void AAbilityPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 먹은 직후 맵이 바뀌는 등으로 사라지면, 연출 끝 알림은 바로 보내줌 (튜토리얼이 멈추지 않게)
	if (true == bPicked && GetWorldTimerManager().IsTimerActive(FinishTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(FinishTimerHandle);
		if (UGameInstance* GI = GetGameInstance())
			if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
				Inventory->NotifyAcquireFinished(AcquiredItems);
	}

	Super::EndPlay(EndPlayReason);
}

void AAbilityPickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (true == bPicked) return;

	// 위아래로 둥실 + 천천히 회전 (눈에 띄게)
	BobTime += DeltaSeconds;
	Mesh->SetRelativeLocation(MeshBaseLocation + FVector(0.f, 0.f, FMath::Sin(BobTime * BobSpeed) * BobHeight));
	Mesh->AddRelativeRotation(FRotator(0.f, SpinDegreesPerSecond * DeltaSeconds, 0.f));
}

void AAbilityPickup::HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (true == bPicked) return;

	// 플레이어만 먹을 수 있음 (펫/적이 닿아도 무시)
	const APawn* Pawn = Cast<APawn>(OtherActor);
	if (nullptr == Pawn || false == Pawn->IsPlayerControlled()) return;

	UGameInstance* GI = GetGameInstance();
	UInventorySubsystem* Inventory = GI ? GI->GetSubsystem<UInventorySubsystem>() : nullptr;
	if (nullptr == Inventory || nullptr == AbilityItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("[AbilityPickup] %s: AbilityItem 비어 있음"), *GetName());
		return;
	}

	bPicked = true;
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true); // 먹은 순간 사라져 보이게
	GlowLight->SetVisibility(false); // 빛도 같이 꺼서 주변이 계속 물들지 않게
	SetActorTickEnabled(false);

	// AddItem 안에서 OnAbilityUnlocked → 펫이 능력 부여, 그리고 획득 UI 표시
	AcquiredItems = Inventory->AddItems(this, { FItemStack(AbilityItem, 1) }, true);

	// 획득 UI가 떠 있는 동안 기다린 뒤 "연출 끝" 알림 (UI 표시 시간과 맞춤)
	const float ShowTime = FMath::Max(0.1f, GetDefault<UItemSettings>()->AcquireDisplayTime);
	GetWorldTimerManager().SetTimer(FinishTimerHandle, this, &AAbilityPickup::FinishAcquire, ShowTime, false);
}

void AAbilityPickup::FinishAcquire()
{
	// 타이머 콜백 중에는 핸들이 아직 활성으로 보여서, 아래 Destroy → EndPlay에서 알림이 한 번 더 가지 않게 먼저 정리
	GetWorldTimerManager().ClearTimer(FinishTimerHandle);

	if (UGameInstance* GI = GetGameInstance())
		if (UInventorySubsystem* Inventory = GI->GetSubsystem<UInventorySubsystem>())
			Inventory->NotifyAcquireFinished(AcquiredItems); // → 튜토리얼이 1단계 완료로 받음

	Destroy();
}
