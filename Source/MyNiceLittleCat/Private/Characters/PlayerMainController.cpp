#include "Characters/PlayerMainController.h"
#include "Characters/PlayerComponents/PetModeDialComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubSystems.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Pet/PetCharacter.h"

APlayerMainController::APlayerMainController()
{
	PetModeDialComponent = CreateDefaultSubobject<UPetModeDialComponent>(TEXT("PetModeDialComponent"));
}

void APlayerMainController::FlushPressedKeys()
{
	// 입력 초기화 되기 전에 모드 변경 캔슬 내요
	HandleDialCanceled();
	HandleModeUseReleased();

	Super::FlushPressedKeys();
}

bool APlayerMainController::ShouldFlushKeysWhenViewportFocusChanges() const
{
	return true;
}

void APlayerMainController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	APetCharacter* Pet = PartnerPet.Get();

	if (false == IsValid(Pet)) return;
	// 다이얼 조작 중에는 마지막 조준 방향을 유지
	if (true == IsValid(PetModeDialComponent.Get()) && true == PetModeDialComponent->IsDialOpen()) return;

	FVector Direction;
	if (true == TryGetPetAimDirection(Direction))
		Pet->SetAimDirection(Direction);
}

void APlayerMainController::BeginPlay()
{
	Super::BeginPlay();

	// 마우스 플레이에 사용할 커서를 표시
	SetShowMouseCursor(true);

	// 입력을 UI 먼저 처리하고 커서를 숨기지 않도록 해요
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
        {
            if (IMC_Pet)
                Subsystem->AddMappingContext(IMC_Pet, 1);
        }
    }
}

void APlayerMainController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if ((nullptr == EnhancedInput) || (nullptr == IA_PetModeDial) || (nullptr == IA_PetModeUse) || (nullptr == IA_PetSpecialAction)) return;

    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Started, this, &APlayerMainController::HandleDialStarted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Triggered, this, &APlayerMainController::HandleDialTriggered);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Completed, this, &APlayerMainController::HandleDialCompleted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Canceled, this, &APlayerMainController::HandleDialCanceled);

	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Started, this, &APlayerMainController::HandleModeUseStarted);
	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Completed, this, &APlayerMainController::HandleModeUseReleased);
	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Canceled, this, &APlayerMainController::HandleModeUseReleased);

	EnhancedInput->BindAction(IA_PetSpecialAction, ETriggerEvent::Started, this, &APlayerMainController::HandlePetSpecialAction);
}

void APlayerMainController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleDialCanceled();
	HandleModeUseReleased();

	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (IMC_Pet)
				Subsystem->RemoveMappingContext(IMC_Pet);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void APlayerMainController::SetPartnerPet(APetCharacter* _pet)
{
	HandleDialCanceled();
	HandleModeUseReleased();

	PartnerPet = _pet;
}

void APlayerMainController::HandleDialStarted()
{
	if (false == IsValid(PetModeDialComponent.Get())) return;

	FVector2D PointerPosition;
	// 마우스 좌표를 UI 좌표로 변환했을 떄 유효하지 않으면?
	if (false == TryGetDialPointerPosition(PointerPosition)) return;
	
	// 보정 전 실제 커서 위치를 기억해요
	const FVector2D OriginalPointerPosition = PointerPosition;

	// 다이얼 전체를 표시할만큼 게임 화면 안에 들어와있지 않다면?
	if (false == TryClampDialCenterToViewport(PointerPosition)) return;
		
	// 다이얼 오픈?
	if (false == PetModeDialComponent->OpenDial(PointerPosition)) return;

	// 다이얼의 중심이 이동했다면? 실제 커서도 옮겨줘야지요
	if (false == PointerPosition.Equals(OriginalPointerPosition, 0.5))
	{
		const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);

		// 마우스 커서를 다이얼 중심 위치로!
		SetMouseLocation(FMath::RoundToInt(PointerPosition.X * ViewportScale),
			FMath::RoundToInt(PointerPosition.Y * ViewportScale));
	}
}

bool APlayerMainController::TryClampDialCenterToViewport(FVector2D& _outPosition) const
{
	// 게임 화면의 UI 배율
	const float ViewportScale = UWidgetLayoutLibrary::GetViewportScale(this);
	if (0 >= ViewportScale) return false;

	// 실제 화면의 픽셀 좌표 -> UI가 사용하는 좌표로 변환
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this) / ViewportScale;

	const double Radius = PetModeDialComponent->GetDialDiameter() * 0.5;
	const double Margin = 8.0;
	const double Inset = Radius + Margin;

	// 다이얼 전체를 표시할 공간이 없으면? 열지 않음
	if ((Inset * 2.0 > ViewportSize.X) || (Inset * 2.0 > ViewportSize.Y)) 
		return false;

	_outPosition.X = FMath::Clamp(_outPosition.X, Inset, ViewportSize.X - Inset);
	_outPosition.Y = FMath::Clamp(_outPosition.Y, Inset, ViewportSize.Y - Inset);

	return true;
}

bool APlayerMainController::TryGetPetAimDirection(FVector& _outDirection) const
{
	_outDirection = FVector::ZeroVector;

	APawn* PlayerPawn = GetPawn();
	if (false == IsValid(PlayerPawn)) return false;

	FHitResult Hit;
	if (false == TryGetPetCursorHit(Hit)) return false;

	FVector Direction = Hit.ImpactPoint - PlayerPawn->GetActorLocation();
	Direction.Z = 0.f;

	if (false == Direction.Normalize()) return false;

	_outDirection = Direction;
	return true;
}

bool APlayerMainController::TryGetPetCursorHit(FHitResult& _outHit) const
{
	_outHit = FHitResult();

	APawn* PlayerPawn = GetPawn();
	UWorld* World = GetWorld();

	if (false == IsValid(PlayerPawn) || false == IsValid(World)) return false;

	FVector RayOrigin;
	FVector RayDirection;

	// 화면의 마우스 커서 위치를 3D 월드 공간의 Ray로 변환하는 함수
	if (false == DeprojectMousePositionToWorld(RayOrigin, RayDirection))
		return false;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetAim));

	// 플레이어나 펫 자신에게 광선이 막히지 않도록
	QueryParams.AddIgnoredActor(PlayerPawn);

	if (APetCharacter* Pet = PartnerPet.Get())
		QueryParams.AddIgnoredActor(Pet);

	constexpr float TraceDistance = 100000.f;

	return World->LineTraceSingleByChannel(
		_outHit, RayOrigin, RayOrigin + RayDirection * TraceDistance, ECC_Visibility, QueryParams);
}

void APlayerMainController::HandleDialTriggered()
{
	if ((false == IsValid(PetModeDialComponent.Get())) || (false == PetModeDialComponent->IsDialOpen())) return;

	FVector2D PointerPosition;
	
	// 제대로 마우스 좌표가 변환되지 않았다면
	if (false == TryGetDialPointerPosition(PointerPosition))
	{
		HandleDialCanceled(); // 캔슬시켜용
		return;
	}

	PetModeDialComponent->UpdatePointerPosition(PointerPosition);
}

void APlayerMainController::HandleDialCompleted()
{
	if (false == IsValid(PetModeDialComponent.Get())) return;
	if (false == PetModeDialComponent->IsDialOpen()) return;

	FVector2D PointerPosition;

	// 제대로 마우스 좌표가 변환되지 않았다면
	if (false == TryGetDialPointerPosition(PointerPosition))
	{
		HandleDialCanceled(); // 캔슬시켜용
		return;
	}
	
	PetModeDialComponent->UpdatePointerPosition(PointerPosition);
	EPetMode SelectedMode = EPetMode::Basic;
	const bool bHasSelection = PetModeDialComponent->TryGetSelectedMode(SelectedMode);

	PetModeDialComponent->CloseDial();

	if (true == bHasSelection)
	{
		// 선택한 모드로 세팅
		if (APetCharacter* Pet = PartnerPet.Get())
			Pet->RequestMode(SelectedMode);
	}
}

void APlayerMainController::HandleDialCanceled()
{
	if (true == IsValid(PetModeDialComponent.Get()))
		PetModeDialComponent->CloseDial();
}

void APlayerMainController::HandleModeUseStarted()
{
	if (APetCharacter* Pet = PartnerPet.Get())
		Pet->SetModeAbilityInputHeld(true);
}

void APlayerMainController::HandleModeUseReleased()
{
	if (APetCharacter* Pet = PartnerPet.Get())
		Pet->SetModeAbilityInputHeld(false);
}

void APlayerMainController::HandlePetSpecialAction()
{
	if (APetCharacter* Pet = PartnerPet.Get())
		Pet->RequestSpecialAction();
}

bool APlayerMainController::TryGetDialPointerPosition(FVector2D& _outPosition) const
{
	// 마우스 위치를 UI 좌표와 같은 단위로 계산해 _outPosiion에 담아요
	return UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, _outPosition.X, _outPosition.Y);
}
