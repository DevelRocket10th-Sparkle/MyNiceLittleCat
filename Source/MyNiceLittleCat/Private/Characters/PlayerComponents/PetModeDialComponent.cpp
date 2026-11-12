#include "Characters/PlayerComponents/PetModeDialComponent.h"
#include "Widgets/Pet/PetModeDialWidget.h"

UPetModeDialComponent::UPetModeDialComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPetModeDialComponent::OpenDial(const FVector2D& _centerInViewport, EPetMode _currentMode, const TSet<EPetMode>& _lockedModes)
{
	if (true == bDialOpen) return true;

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (false == IsValid(PC)) return false;

	if ((nullptr == DialWidgetClass) || (true == _centerInViewport.ContainsNaN()))
		return false;

	// 위젯이 없으면 만들어요
	if (nullptr == DialWidget)
		DialWidget = CreateWidget<UPetModeDialWidget>(PC, DialWidgetClass);
	if (false == IsValid(DialWidget.Get())) return false;

	DialCenter = _centerInViewport;
	DialWidget->ResetSelection();
	DialWidget->SetCurrentMode(_currentMode); // 현재 모드 칸에 체크 배지 표시
	DialWidget->SetLockedModes(_lockedModes); // 잠긴 모드 칸은 회색 "???"
	DialWidget->SetIsFocusable(false);

	if (false == DialWidget->IsInViewport())
		DialWidget->AddToViewport(100);

	const float Diameter = DialWidget->GetDialDiameter();
	DialWidget->SetDesiredSizeInViewport(FVector2D(Diameter, Diameter));

	DialWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

	DialWidget->SetPositionInViewport(DialCenter, false);

	DialWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	DialWidget->PlayOpenSound();

	bDialOpen = true;
	return true;
}

void UPetModeDialComponent::CloseDial()
{
	// 열려 있던 경우에만 닫힘 소리
	// 의도: EndPlay 등에서 이미 닫힌 다이얼을 또 닫을 때 소리가 나지 않게
	const bool bWasOpen = bDialOpen;
	bDialOpen = false;

	if (true == IsValid(DialWidget.Get()))
	{
		// ResetSelection 전에 불러야 마지막 호버 칸(잠김 여부)을 알 수 있어요
		if (true == bWasOpen)
			DialWidget->PlayCloseSound();

		DialWidget->ResetSelection();
		DialWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPetModeDialComponent::UpdatePointerPosition(const FVector2D& _pointerInViewport)
{
	if (false == bDialOpen || false == IsValid(DialWidget.Get())) return;

	// 다이얼 중심으로 부터 마우스 위치를 통해 뭐가 선택되고 있는지 계산해여
	DialWidget->UpdateSelectionFromOffset(_pointerInViewport - DialCenter);
}

bool UPetModeDialComponent::TryGetSelectedMode(EPetMode& _outMode) const
{
	_outMode = EPetMode::Basic;

	if ((false == bDialOpen) || false == IsValid(DialWidget.Get()))
		return false;

	return DialWidget->TryGetSelectedMode(_outMode);
}

float UPetModeDialComponent::GetDialDiameter() const
{
	if (true == IsValid(DialWidget))
		return DialWidget->GetDialDiameter();

	// 아직 위젯이 생성되지 않았다면 클래스 기본값을 읽어옴
	if (nullptr != DialWidgetClass)
	{
		const UPetModeDialWidget* WidgetDefaults = DialWidgetClass.GetDefaultObject();

		if (true == IsValid(WidgetDefaults))
			return WidgetDefaults->GetDialDiameter();
	}

	return 280.f; // 진짜 이건 기본값
}

void UPetModeDialComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseDial();

	if (true == IsValid(DialWidget.Get()))
	{
		DialWidget->RemoveFromParent();
		DialWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

