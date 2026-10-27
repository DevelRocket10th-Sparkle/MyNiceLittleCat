#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Characters/PlayerCharacter.h"
#include "Enemy/EnemyCharacter.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/Animations/AnimNotifyState_EnemyMeleeTrace.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyMeleeSweepTest, "MyNiceLittleCat.Enemy.MeleeSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyMeleeSweepTest::RunTest(const FString& Parameters)
{
	const UWorld::InitializationValues Values = UWorld::InitializationValues().AllowAudioPlayback(false)
		.CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
	AEnemyCharacter* Enemy = World->SpawnActor<AEnemyCharacter>();
	APlayerCharacter* Player = World->SpawnActor<APlayerCharacter>(FVector(500.f, 0.f, 100.f), FRotator::ZeroRotator);
	if (!Enemy || !Player)
	{
		AddError(TEXT("Cannot spawn melee test actors"));
		World->DestroyWorld(false);
		return false;
	}
	Player->GetCapsuleComponent()->SetCollisionProfileName(TEXT("Pawn"));
	UEnemyMeleeTraceComponent* Trace = Enemy->GetMeleeTraceComponent();
	UStaticMesh* MeshAsset = NewObject<UStaticMesh>(Enemy);
	for (FName SocketName : {FName(TEXT("Tip")), FName(TEXT("Mid"))})
	{
		UStaticMeshSocket* Socket = NewObject<UStaticMeshSocket>(MeshAsset);
		Socket->SocketName = SocketName;
		MeshAsset->AddSocket(Socket);
	}
	UStaticMeshComponent* Weapon = NewObject<UStaticMeshComponent>(Enemy);
	Enemy->AddInstanceComponent(Weapon);
	Weapon->SetStaticMesh(MeshAsset);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->RegisterComponent();
	const FVector Center = Player->GetActorLocation();
	Weapon->SetWorldLocation(Center - FVector(200.f, 0.f, 0.f));
	Trace->RegisterHitSource(TEXT("Weapon"), Weapon);
	UEnemyMeleeTraceSettings* Definition = NewObject<UEnemyMeleeTraceSettings>(Enemy);
	Definition->SourceName = TEXT("Weapon");
	Definition->SocketNames = {TEXT("Tip"), TEXT("Mid")};
	Definition->Radius = 15.f;
	int32 HitCount = 0;
	Trace->OnPlayerHit.AddLambda([&](const FHitResult& Hit) { if (Hit.GetActor() == Player) ++HitCount; });
	UObject* Window = NewObject<UAnimNotifyState_EnemyMeleeTrace>(Enemy);
	TestTrue(TEXT("Static weapon sockets can prepare an attack"), Trace->PrepareAttack({Definition}, 1));
	Trace->BeginWindow(Window);
	TestEqual(TEXT("Player outside the initial sphere is not hit"), HitCount, 0);
	Weapon->SetWorldLocation(Center + FVector(200.f, 0.f, 0.f));
	Trace->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Sweep catches a capsule between frames; two sockets deal one hit"), HitCount, 1);
	Weapon->SetWorldLocation(Center - FVector(200.f, 0.f, 0.f));
	Trace->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Repeated crossing in the same window is deduplicated"), HitCount, 1);
	Trace->EndWindow(Window);
	Trace->BeginWindow(Window);
	Weapon->SetWorldLocation(Center + FVector(200.f, 0.f, 0.f));
	Trace->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("A second window in one attack cannot hit the player again"), HitCount, 1);
	Trace->StopAttack(1);
	Weapon->SetWorldLocation(Center);
	Trace->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestEqual(TEXT("Cancelled attack cannot hit"), HitCount, 1);
	// 피해 이벤트가 공격을 즉시 취소해도 다른 소켓 결과를 계속 전달하지 않음
	Trace->OnPlayerHit.Clear();
	Trace->OnPlayerHit.AddLambda([&](const FHitResult&) { ++HitCount; Trace->StopAttack(2); });
	TestTrue(TEXT("Next request can prepare"), Trace->PrepareAttack({Definition}, 2));
	Trace->BeginWindow(Window);
	TestEqual(TEXT("New attack resets deduplication and supports synchronous cancellation"), HitCount, 2);
	Trace->OnPlayerHit.Clear();
	World->DestroyWorld(false);
	return true;
}
#endif
