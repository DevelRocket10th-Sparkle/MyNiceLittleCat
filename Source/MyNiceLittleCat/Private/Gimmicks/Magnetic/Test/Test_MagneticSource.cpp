/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자성 컴포넌트 테스트용 자력원 액터 구현

#include "Gimmicks/Magnetic/Test/Test_MagneticSource.h"
#include "Gimmicks/Magnetic/MagneticSubsystem.h"
#include "Components/ArrowComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

ATest_MagneticSource::ATest_MagneticSource()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Arrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	Arrow->SetupAttachment(Root);
	Arrow->ArrowSize = 2.f;
	Arrow->SetHiddenInGame(false);

	SourceInfo.Polarity = EMagneticPolarity::N;
}

void ATest_MagneticSource::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	if (bAimAtCursor)
	{
		PlayerController->bShowMouseCursor = true;
	}

	if (bBindTestKeys)
	{
		EnableInput(PlayerController);
		if (InputComponent)
		{
			InputComponent->BindKey(ToggleActiveKey, IE_Pressed, this, &ATest_MagneticSource::ToggleActive);
			InputComponent->BindKey(TogglePolarityKey, IE_Pressed, this, &ATest_MagneticSource::TogglePolarity);
			InputComponent->BindKey(ToggleShapeKey, IE_Pressed, this, &ATest_MagneticSource::ToggleShape);
		}
	}
}

void ATest_MagneticSource::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UMagneticSubsystem* MagneticSubsystem = GetWorld()->GetSubsystem<UMagneticSubsystem>())
	{
		MagneticSubsystem->UnregisterSource(this);
	}

	Super::EndPlay(EndPlayReason);
}

void ATest_MagneticSource::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateSourceTransform();
	SyncSource();

	if (bDrawDebug && bSourceActive)
	{
		UMagneticSubsystem::DrawDebugSource(GetWorld(), SourceInfo);
	}

	if (GEngine)
	{
		const FString Message = FString::Printf(TEXT("[자력원 테스트] %s | 극성 %s | %s  (Z on/off, X 극성, C 모양)"),
			bSourceActive ? TEXT("ON") : TEXT("OFF"),
			*UEnum::GetDisplayValueAsText(SourceInfo.Polarity).ToString(),
			*UEnum::GetDisplayValueAsText(SourceInfo.Shape).ToString());
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, FColor::Yellow, Message);
	}
}

void ATest_MagneticSource::UpdateSourceTransform()
{
	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();

	if (bFollowPlayerPawn && PlayerController && PlayerController->GetPawn())
	{
		SetActorLocation(PlayerController->GetPawn()->GetActorLocation() + FollowOffset);
	}

	FVector Aim = GetActorForwardVector();

	if (bAimAtCursor && PlayerController)
	{
		FHitResult Hit;
		if (PlayerController->GetHitResultUnderCursor(ECC_Visibility, false, Hit))
		{
			const FVector ToCursor = Hit.ImpactPoint - GetActorLocation();
			if (!ToCursor.IsNearlyZero())
			{
				Aim = ToCursor;
			}
		}
	}

	if (SourceInfo.bPlanar)
	{
		Aim.Z = 0.f;
	}

	Aim = Aim.GetSafeNormal();
	if (!Aim.IsNearlyZero())
	{
		SetActorRotation(Aim.Rotation());
	}

	SourceInfo.Location = GetActorLocation();
	SourceInfo.AimDirection = GetActorForwardVector();
}

void ATest_MagneticSource::SyncSource()
{
	UMagneticSubsystem* MagneticSubsystem = GetWorld()->GetSubsystem<UMagneticSubsystem>();
	if (!MagneticSubsystem)
	{
		return;
	}

	const bool bRegistered = MagneticSubsystem->IsSourceRegistered(this);

	if (bSourceActive)
	{
		if (bRegistered)
		{
			MagneticSubsystem->UpdateSource(this, SourceInfo);
		}
		else
		{
			MagneticSubsystem->RegisterSource(this, SourceInfo);
		}
	}
	else if (bRegistered)
	{
		MagneticSubsystem->UnregisterSource(this);
	}
}

void ATest_MagneticSource::ToggleActive()
{
	bSourceActive = !bSourceActive;
}

void ATest_MagneticSource::TogglePolarity()
{
	SourceInfo.Polarity = SourceInfo.Polarity == EMagneticPolarity::N ? EMagneticPolarity::S : EMagneticPolarity::N;
}

void ATest_MagneticSource::ToggleShape()
{
	SourceInfo.Shape = SourceInfo.Shape == EMagneticFieldShape::Line ? EMagneticFieldShape::Cone : EMagneticFieldShape::Line;
}
