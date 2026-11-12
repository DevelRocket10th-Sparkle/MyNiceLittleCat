#include "Characters/PlayerMainController.h"
#include "Characters/PlayerComponents/PetModeDialComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubSystems.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Pet/PetCharacter.h"
#include "Widgets/Pause/PauseMenuWidget.h"

APlayerMainController::APlayerMainController()
{
	PetModeDialComponent = CreateDefaultSubobject<UPetModeDialComponent>(TEXT("PetModeDialComponent"));
}

void APlayerMainController::FlushPressedKeys()
{
	// 입력 초기화 되기 전에 모드 변경 캔슬 내요
	HandleDialCanceled();
	HandleModeUseCanceled();

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

	// 일시정지 메뉴 (펫 입력 에셋이 없어도 동작하도록 먼저 바인딩)
	if (InputComponent)
	{
		InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APlayerMainController::TogglePauseMenu);
		InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &APlayerMainController::TogglePauseMenu);
	}

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if ((nullptr == EnhancedInput) || (nullptr == IA_PetModeDial) || (nullptr == IA_PetModeUse) || (nullptr == IA_PetSpecialAction)) return;

    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Started, this, &APlayerMainController::HandleDialStarted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Triggered, this, &APlayerMainController::HandleDialTriggered);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Completed, this, &APlayerMainController::HandleDialCompleted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Canceled, this, &APlayerMainController::HandleDialCanceled);

	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Started, this, &APlayerMainController::HandleModeUseStarted);
	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Completed, this, &APlayerMainController::HandleModeUseReleased);
	EnhancedInput->BindAction(IA_PetModeUse, ETriggerEvent::Canceled, this, &APlayerMainController::HandleModeUseCanceled);

	EnhancedInput->BindAction(IA_PetSpecialAction, ETriggerEvent::Started, this, &APlayerMainController::HandlePetSpecialAction);
}

void APlayerMainController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleDialCanceled();
	HandleModeUseCanceled();

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
	HandleModeUseCanceled();

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
	// 펫이 없으면 기본 모드로 취급해요
	const EPetMode CurrentMode = PartnerPet.IsValid() ? PartnerPet->GetCurrentMode() : EPetMode::Basic;

	// 펫에게 물어서 아직 해금 안 된 모드 목록을 만들어요 (다이얼에서 회색 "???" 칸)
	TSet<EPetMode> LockedModes;
	if (const APetCharacter* Pet = PartnerPet.Get())
	{
		for (const EPetMode Mode : { EPetMode::Basic, EPetMode::Magnet, EPetMode::Reflect })
		{
			if (false == Pet->IsModeUnlocked(Mode))
				LockedModes.Add(Mode);
		}
	}

	if (false == PetModeDialComponent->OpenDial(PointerPosition, CurrentMode, LockedModes)) return;

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

	FVector RayOrigin;
	FVector RayDirection;

	// 화면의 마우스 커서 위치를 월드 공간의 Ray로 변환
	if (false == DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return false;

	// Ray가 수평이거나 위를 향하면 플레이어 높이 평면과 만나지 않음
	if (RayDirection.Z >= -KINDA_SMALL_NUMBER) return false;

	// 의도: 월드에 LineTrace를 쏘지 않고 "플레이어 높이의 수평면"과의 교점만 계산
	//      -> 카메라 컬링으로 화면에서만 숨긴 천장(콜리전은 남아 있음)에 Ray가 막히지 않음
	//      -> 조준 방향은 수평 방향만 필요하므로 실제 바닥에 맞을 필요가 없음
	const double PlaneZ = PlayerPawn->GetActorLocation().Z;
	const double Distance = (PlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (Distance < 0.0) return false;

	const FVector CursorPoint = RayOrigin + RayDirection * Distance;

	// 플레이어에서 커서 교점으로 향하는 수평 방향
	FVector Direction = CursorPoint - PlayerPawn->GetActorLocation();
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
	constexpr int32 MaxSkipCount = 8; // 숨김 조각이 겹쳐 있어도 무한 반복하지 않도록 상한

	for (int32 i = 0; i < MaxSkipCount; ++i)
	{
		if (false == World->LineTraceSingleByChannel(
			_outHit, RayOrigin, RayOrigin + RayDirection * TraceDistance, ECC_Visibility, QueryParams))
			return false;

		// 의도: 카메라 컬링으로 메인 패스에서 숨겨진(=화면에 안 보이는) 조각은 커서 대상에서 제외
		UPrimitiveComponent* HitComp = _outHit.GetComponent();
		if (false == IsValid(HitComp) || true == HitComp->bRenderInMainPass)
			return true;

		QueryParams.AddIgnoredComponent(HitComp);
	}
	return false;
}

void APlayerMainController::SetPetInputEnabled(bool _bEnable)
{
	// 끌 때 다이얼이 열려 있거나 모드 사용 중이면 먼저 취소 -> 다이얼 UI 가 떠 있는 채로 남지 않게
	if (false == _bEnable)
	{
		HandleDialCanceled();
		HandleModeUseCanceled();
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	UEnhancedInputLocalPlayerSubsystem* Subsystem = (nullptr != LocalPlayer) ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (nullptr == Subsystem || nullptr == IMC_Pet) return;

	// 펫 키 매핑(IMC_Pet)을 통째로 빼고 넣음 -> 키를 눌러도 펫 입력 액션 자체가 발생하지 않음
	// (캐릭터 이동/공격 매핑은 따로라 영향 없음)
	if (true == _bEnable)
	{
		if (false == Subsystem->HasMappingContext(IMC_Pet))
			Subsystem->AddMappingContext(IMC_Pet, 1); // BeginPlay 와 같은 우선순위
	}
	else
		Subsystem->RemoveMappingContext(IMC_Pet);
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
		Pet->RequestModeUseInput(EPetModeUseInputPhase::Pressed);
}

void APlayerMainController::HandleModeUseReleased()
{
	if (APetCharacter* Pet = PartnerPet.Get())
		Pet->RequestModeUseInput(EPetModeUseInputPhase::Released);
}

void APlayerMainController::HandleModeUseCanceled()
{
	if(APetCharacter* Pet = PartnerPet.Get())
		Pet->RequestModeUseInput(EPetModeUseInputPhase::Canceled);
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

void APlayerMainController::TogglePauseMenu()
{
	if (PauseMenu && PauseMenu->IsOpen())
	{
		PauseMenu->Close();
		return;
	}

	// 다른 이유로 이미 멈춰 있거나 위젯이 지정되지 않았으면 무시
	if (IsPaused() || !PauseMenuClass)
	{
		return;
	}

	if (!PauseMenu)
	{
		PauseMenu = CreateWidget<UPauseMenuWidget>(this, PauseMenuClass);
	}
	if (PauseMenu)
	{
		HandleDialCanceled();
		HandleModeUseCanceled();
		PauseMenu->Open();
	}
}
