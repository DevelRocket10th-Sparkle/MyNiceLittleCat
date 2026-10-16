#include "Characters/PlayerMainController.h"
#include "Characters/PlayerComponents/PetModeDialComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubSystems.h"
#include "Blueprint/WidgetLayoutLibrary.h"

APlayerMainController::APlayerMainController()
{
	PetModeDialComponent = CreateDefaultSubobject<UPetModeDialComponent>(TEXT("PetModeDialComponent"));
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
	if ((nullptr == EnhancedInput) || (nullptr == IA_PetModeDial)) return;

    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Started, this, &APlayerMainController::HandleDialStarted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Triggered, this, &APlayerMainController::HandleDialTriggered);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Completed, this, &APlayerMainController::HandleDialCompleted);
    EnhancedInput->BindAction(IA_PetModeDial, ETriggerEvent::Canceled, this, &APlayerMainController::HandleDialCanceled);
}

void APlayerMainController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HandleDialCanceled();

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

void APlayerMainController::HandleDialStarted()
{
	if (false == IsValid(PetModeDialComponent.Get())) return;

	FVector2D PointerPosition;

	// 마우스 좌표를 UI 좌표로 변환해 그곳에 다이얼을 열어요
	if (true == TryGetDialPointerPosition(PointerPosition))
		PetModeDialComponent->OpenDial(PointerPosition);
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
	PetModeDialComponent->CloseDial();
}

void APlayerMainController::HandleDialCanceled()
{
	if (true == IsValid(PetModeDialComponent.Get()))
		PetModeDialComponent->CloseDial();
}

bool APlayerMainController::TryGetDialPointerPosition(FVector2D& _outPosition) const
{
	// 마우스 위치를 UI 좌표와 같은 단위로 계산해 _outPosiion에 담아요
	return UWidgetLayoutLibrary::GetMousePositionScaledByDPI(this, _outPosition.X, _outPosition.Y);
}