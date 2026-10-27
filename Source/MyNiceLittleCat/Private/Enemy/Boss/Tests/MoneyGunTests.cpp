#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Enemy/Boss/Datas/MoneyGunPatternSettings.h"
#include "Enemy/Boss/Objects/MoneyGun.h"
#include "Enemy/Boss/Objects/MoneyProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMoneyGunSequenceTest, "Project.Boss.MoneyGun.Sequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMoneyGunSequenceTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreateNavigation(false).CreateAISystem(false).CreateFXSystem(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
	UMoneyGunPatternSettings* Settings = NewObject<UMoneyGunPatternSettings>(World);
	Settings->GunClass = AMoneyGun::StaticClass();
	Settings->ProjectileClass = AMoneyProjectile::StaticClass();
	Settings->ProjectileCount = 3;
	Settings->AppearSeconds = 0.1f;
	Settings->LoadSeconds = 0.2f;
	Settings->PreFireSeconds = 0.1f;
	Settings->ShotInterval = 0.1f;
	TestTrue(TEXT("Valid settings"), Settings->IsValidConfiguration());
	Settings->ProjectileCount = 0;
	TestFalse(TEXT("Zero rounds rejected"), Settings->IsValidConfiguration());
	Settings->ProjectileCount = 3;
	// 콘텐츠 에셋 없이 테스트하도록 기본 객체에 임시 메시를 지정한다.
	UStaticMeshComponent* TemplateMesh = GetMutableDefault<AMoneyProjectile>()->GetMoneyMesh();
	UStaticMesh* PreviousMesh = TemplateMesh->GetStaticMesh();
	TemplateMesh->SetStaticMesh(NewObject<UStaticMesh>(World));
	AStaticMeshActor* Parent = World->SpawnActor<AStaticMeshActor>();
	AStaticMeshActor* Target = World->SpawnActor<AStaticMeshActor>();
	Target->SetActorLocation(FVector(1000.f, 0.f, 0.f));
	AMoneyGun* Gun = World->SpawnActor<AMoneyGun>();
	Gun->AttachToComponent(Parent->GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	TestTrue(TEXT("Pattern starts"), Gun->StartPattern(Settings, Target, 10.f));
	TestFalse(TEXT("Running gun cannot restart"), Gun->StartPattern(Settings, Target, 10.f));
	Gun->Tick(0.15f);
	TArray<UStaticMeshComponent*> Meshes;
	Gun->GetComponents(Meshes);
	TestEqual(TEXT("Three loading bills plus gun mesh"), Meshes.Num(), 4);
	Parent->SetActorLocation(FVector(100.f, 0.f, 0.f));
	Parent->SetActorRotation(FRotator(0.f, 90.f, 0.f));
	TestTrue(TEXT("Attached gun follows position"), Gun->GetActorLocation().Equals(Parent->GetActorLocation()));
	TestTrue(TEXT("Parent rotation does not overwrite aim"), Gun->GetActorRotation().IsNearlyZero());
	Target->SetActorLocation(FVector(100.f, 1000.f, 0.f));
	Gun->Tick(0.1f);
	TestTrue(TEXT("Gun rotates towards moving player at limited rate"), Gun->GetActorRotation().Yaw > 0.f && Gun->GetActorRotation().Yaw < 90.f);
	Gun->Tick(0.06f);
	Meshes.Reset();
	Gun->GetComponents(Meshes);
	TestEqual(TEXT("Loading visuals removed before firing"), Meshes.Num(), 1);
	int32 CountBeforeFire = 0;
	for (TActorIterator<AMoneyProjectile> It(World); It; ++It)
	{
		++CountBeforeFire;
	}
	TestEqual(TEXT("No projectile during prefire delay"), CountBeforeFire, 0);
	Gun->Tick(0.1f);
	Gun->Tick(0.11f);
	Gun->Tick(0.11f);
	int32 Count = 0;
	for (TActorIterator<AMoneyProjectile> It(World); It; ++It)
	{
		++Count;
		const UProjectileMovementComponent* Movement = It->FindComponentByClass<UProjectileMovementComponent>();
		TestTrue(TEXT("Configured non-homing speed"), FMath::IsNearlyEqual(Movement->Velocity.Size(), Settings->ProjectileSpeed, 0.01f) && !Movement->bIsHomingProjectile);
		const USphereComponent* Collision = It->FindComponentByClass<USphereComponent>();
		TestEqual(TEXT("Bills do not collide with each other"), Collision->GetCollisionResponseToChannel(ECC_GameTraceChannel1), ECR_Ignore);
	}
	TestEqual(TEXT("Exactly the loaded count is fired"), Count, 3);
	TestFalse(TEXT("Last shot starts exit instead of removing gun immediately"), Gun->HasFinished());
	Gun->Tick(0.05f);
	Meshes.Reset();
	Gun->GetComponents(Meshes);
	TestTrue(TEXT("Exit reverses the gun appearance scale"), Meshes[0]->GetRelativeScale3D().Equals(FVector(0.5f), 0.001f));
	TestFalse(TEXT("Gun remains visible during exit"), Gun->IsHidden());
	Gun->Tick(0.06f);
	TestTrue(TEXT("Exit animation ends pattern"), Gun->HasFinished());
	TestTrue(TEXT("Gun is hidden after exit"), Gun->IsHidden());
	TestTrue(TEXT("Gun scale reaches zero"), Meshes[0]->GetRelativeScale3D().IsNearlyZero());
	Gun->ClearProjectiles();
	int32 Remaining = 0;
	for (TActorIterator<AMoneyProjectile> It(World); It; ++It)
	{
		++Remaining;
	}
	TestEqual(TEXT("Cancellation cleanup removes live projectiles"), Remaining, 0);
	TemplateMesh->SetStaticMesh(PreviousMesh);
	World->DestroyWorld(false);
	return true;
}

#endif
