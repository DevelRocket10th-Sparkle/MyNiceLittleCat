// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/Components/EnemyOverheadHealthBarComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#include "Enemy/EnemyCharacter.h"
#include "Widgets/Enemy/EnemyOverheadHealthBarWidget.h"

UEnemyOverheadHealthBarComponent::UEnemyOverheadHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	SetWidgetSpace(EWidgetSpace::World);
	SetDrawSize(FVector2D(750.f, 50.f));
	SetRelativeScale3D(FVector(0.2f));
	SetRelativeLocation(FVector(0.f, 0.f, 120.f));

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetVisibility(true);
}

void UEnemyOverheadHealthBarComponent::SetShowHealthText(bool bShow)
{
	bShowHealthText = bShow;

	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->SetShowHealthText(bShowHealthText);
	}
}

void UEnemyOverheadHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	AEnemyCharacter* Enemy = Cast<AEnemyCharacter>(GetOwner());
	if (!IsValid(Enemy))
		return;
	
	// 소유자의 사망 델리게이트 구독
	DeathStartedHandle = Enemy->OnDeathStarted.AddUObject(this, &UEnemyOverheadHealthBarComponent::HandleOwnerDeathStarted);

	HealthBarWidget = Cast<UEnemyOverheadHealthBarWidget>(GetUserWidgetObject());
	if (!IsValid(HealthBarWidget))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: Check the overhead Widget Class."), *GetName());

		SetVisibility(false);
		SetComponentTickEnabled(false);
		
		return;
	}

	HealthBarWidget->InitializeOverheadHealthBar(Enemy);
	SetShowHealthText(bShowHealthText);

	// 이미 사망한 액터에 나중에 추가된 경우도 처리
	if (!Enemy->IsAlive())
	{
		HandleOwnerDeathStarted(Enemy);
		return;
	}

	SetVisibility(true);

	if (bFaceCamera)
	{
		UpdateWidgetRotation();
	}
}

void UEnemyOverheadHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AEnemyCharacter* Enemy = OwnerEnemy.Get())
	{
		Enemy->OnDeathStarted.Remove(DeathStartedHandle);
	}

	DeathStartedHandle.Reset();
	OwnerEnemy.Reset();
	
	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->UnbindOverheadHealthBar();
	}

	HealthBarWidget = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UEnemyOverheadHealthBarComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction * ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetWorld() || !GetWorld()->IsGameWorld())
		return;

	// 위젯이 항상 플레이어 카메라 방향을 향하도록 회전 갱신
	if (bFaceCamera && IsVisible())
	{
		UpdateWidgetRotation();
	}
}

void UEnemyOverheadHealthBarComponent::HandleOwnerDeathStarted(AEnemyCharacter * Enemy)
{
	if (Enemy != OwnerEnemy.Get())
		return;

	SetVisibility(false);
	SetComponentTickEnabled(false);

	if (IsValid(HealthBarWidget))
	{
		HealthBarWidget->UnbindOverheadHealthBar();
	}
}

void UEnemyOverheadHealthBarComponent::UpdateWidgetRotation()
{
	APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	if (!CameraManager)
		return;

	const FVector CameraForward = CameraManager->GetCameraRotation().Vector();

	FRotator WidgetRotation = (-CameraForward).Rotation();

	if (bLockWidgetPitch)
	{
		WidgetRotation.Pitch = 0.f;
	}

	if (bLockWidgetRoll)
	{
		WidgetRotation.Roll = 0.f;
	}

	SetWorldRotation(WidgetRotation);
}
