#include "Characters/PlayerComponents/PetModeDialComponent.h"
#include "Widgets/Pet/PetModeDialWidget.h"

UPetModeDialComponent::UPetModeDialComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPetModeDialComponent::OpenDial(const FVector2D& _centerInViewport)
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
	DialWidget->SetIsFocusable(false);

	if (false == DialWidget->IsInViewport())
		DialWidget->AddToViewport(100);

	const float Diameter = DialWidget->GetDialDiameter();
	DialWidget->SetDesiredSizeInViewport(FVector2D(Diameter, Diameter));

	DialWidget->SetAlignmentInViewport(FVector2D(0.5f, 0.5f));

	DialWidget->SetPositionInViewport(DialCenter, false);

	DialWidget->SetVisibility(ESlateVisibility::HitTestInvisible);

	bDialOpen = true;
	return true;
}

void UPetModeDialComponent::CloseDial()
{
	bDialOpen = false;

	if (true == IsValid(DialWidget.Get()))
	{
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

