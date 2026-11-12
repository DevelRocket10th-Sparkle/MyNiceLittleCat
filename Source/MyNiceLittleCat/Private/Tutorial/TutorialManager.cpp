#include "Tutorial/TutorialManager.h"
#include "Kismet/GameplayStatics.h"
#include "Interfaces/ActivatableInterface.h"
#include "Frameworks/GameMode/MainGameMode.h"
#include "Pet/PetCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Pet/SpeechBubbleComponent.h"
#include "AIController.h"                 // 비행 전 길찾기 이동 멈추기
#include "Components/CapsuleComponent.h"  // 착지 높이 계산
#include "Characters/PlayerComponents/LockOnComponent.h"   // 플레이어 락온 상태 읽기
#include "Interfaces/TargetableInterface.h"                // 적 생존 확인 (락온 시스템과 같은 기준)
#include "Navigation/PathFollowingComponent.h" // 펫이 걷는 중인지 확인 (EPathFollowingStatus)
#include "UObject/UnrealType.h" // FFloatProperty (락온 컴포넌트의 protected 값을 이 레벨에서만 바꾸기)
#include "Enemy/EnemyCharacter.h"              // 보스 스폰 / 생존 확인
#include "AbilitySystemBlueprintLibrary.h"     // 플레이어 ASC 찾기
#include "AbilitySystemComponent.h"
#include "Characters/GAS/PlayerAttributeSet.h" // 플레이어 체력(목숨) 채우기
#include "Enemy/AI/EnemyAIController.h"    // 보스 타깃을 플레이어로 바로 지정
#include "Characters/PlayerMainController.h" // 튜토리얼 동안 펫 입력 막기
#include "NiagaraFunctionLibrary.h"       // 효과 스폰
#include "Components/AudioComponent.h"    // 반복 소리 끄기
#include "Tutorial/TutorialPortal.h"  // 보스 처치 후 열리는 관문
#include "Characters/PlayerCharacter.h" // 포오오션

namespace TutorialTag
{
	//! 이동 구간
	const FName CameraDoor01(TEXT("Tutorial.Camera.Door01")); // 이동 구간 시작 연출 카메라 (문 쪽을 보여줌)

	const FName TriggerDoor01(TEXT("Tutorial.Trigger.Door01")); // 이동 구간 문 앞 트리거
	const FName Gate01(TEXT("Tutorial.Gate.01"));               // 이동 → 대시 사이 문
	const FName DecalWASD(TEXT("Tutorial.Decal.WASD")); // 이동 구간 바닥 WASD 안내

	const FName PetPointStart(TEXT("Tutorial.PetPoint.Start"));             // 펫 첫 등장 위치
	const FName PetPointDoor01(TEXT("Tutorial.PetPoint.Door01"));           // 이동 구간 문 앞
	const FName PetPointAfterDoor01(TEXT("Tutorial.PetPoint.AfterDoor01")); // 이동 구간 문 너머

	//! 대시 구간
	const FName TriggerDashEnter(TEXT("Tutorial.Trigger.DashEnter"));          // 대시 방 입구 (구간 실제 시작)
	const FName CameraDashCorridor(TEXT("Tutorial.Camera.DashCorridor"));      // 카메라 C
	const FName PetPointDashEnd(TEXT("Tutorial.PetPoint.DashEnd"));            // 펫 착지 지점 (통로 끝)
	const FName PetPointAfterDoor02(TEXT("Tutorial.PetPoint.AfterDoor02"));    // 다음 문 너머
	const FName DecalShift(TEXT("Tutorial.Decal.Shift"));                      // Shift 데칼
	const FName TriggerDoor02(TEXT("Tutorial.Trigger.Door02"));                // 통로 끝 문 앞
	const FName Gate02(TEXT("Tutorial.Gate.02"));                              // 대시 -> 락온 사이 문

	//! 락온 구간
	const FName TriggerLockOnEnter(TEXT("Tutorial.Trigger.LockOnEnter"));      // 아레나 입구 (Gate02 보다 안쪽)
	const FName CameraLockOnArena(TEXT("Tutorial.Camera.LockOnArena"));        // 아레나 전체 샷
	const FName EnemyLockOn(TEXT("Tutorial.Enemy.LockOn"));                    // 락온 구간 적 (여러 마리에 같은 태그 OK)
	const FName Gate03(TEXT("Tutorial.Gate.03"));                              // 락온 -> 전투 사이 문

	//! 락온 -> 전투 사이 통로
	const FName Gate04(TEXT("Tutorial.Gate.04"));                              // 통로 -> 전투 구간 문
	const FName PetPointAfterDoor04(TEXT("Tutorial.PetPoint.AfterDoor04"));    // 전투 구간 문 너머
	const FName TriggerCorridor(TEXT("Tutorial.Trigger.Corridor"));            // 통로 입구 쪽 (들어오면 회복 안내 시작)
	const FName PetPointCorridor(TEXT("Tutorial.PetPoint.Corridor"));          // 통로에서 펫이 서서 말할 자리 (Gate04 앞)

	//! 전투 구간
	const FName TriggerBossRoomEnter(TEXT("Tutorial.Trigger.BossRoomEnter"));	// 전투 방 안쪽 (Gate04 를 지난 곳)
	const FName PointBossSpawn(TEXT("Tutorial.Point.BossSpawn"));				// 펫이 걸어가서 보스로 변하는 자리 (바닥)
	const FName CameraBossArena(TEXT("Tutorial.Camera.BossArena"));				// 보스 아레나 전체 샷
	const FName CheckpointCombat(TEXT("Tutorial.Checkpoint.Combat"));			// 보스 잡으면 나타나는 체크포인트
	const FName PetPointCheckpoint(TEXT("Tutorial.PetPoint.Checkpoint"));		// 체크포인트 옆 펫 자리

	const FName PortalExit(TEXT("Tutorial.Portal.Exit"));						// 보스 잡으면 벽이 녹아 열리는 관문

	const FName PointBossRetry(TEXT("Tutorial.Point.BossRetry"));				// 보스전에서 죽으면 다시 태어날 자리 (보스방 입구, 아레나 쪽을 봄)
}

ATutorialManager::ATutorialManager()
{
 	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ATutorialManager::BeginPlay()
{
	Super::BeginPlay();
	
	// 리스폰되면 연출을 강제로 끝내기 위해 구독 (게임모드는 리스폰돼도 그대로라 한 번만)
	if (AMainGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameMode>())
		GameMode->OnPlayerRespawned.AddUniqueDynamic(this, &ATutorialManager::HandlePlayerRespawned);

	// 펫은 GameMode 가 플레이어를 스폰한 "뒤에" 만들기 때문에 지금은 아직 없을 수 있음
	// -> 0.1초마다 확인하다가 준비되면 시작 (게임모드, 펫 코드는 건드리지 않음)
	GetWorldTimerManager().SetTimer(StartTimerHandle, this, &ATutorialManager::TryStartTutorial, 0.1f, true);
	TryStartTutorial(); // 먼저 걸어봄
}

void ATutorialManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr)
	{
		StopPetFly();
		bPetTurning = false;
		SetActorTickEnabled(false);
		return;
	}

	//! ---- 비행 ----
	if (true == bPetFlying)
	{
		FlyElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(FlyElapsed / FlyDuration, 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f); // 천천히 출발해서 천천히 착지

		// 직선 위치 + 포물선 높이 (가운데에서 가장 높고 양 끝에서 0)
		FVector Location = FMath::Lerp(FlyStart, FlyEnd, Eased);
		Location.Z += PetFlyArcHeight * 4.f * Eased * (1.f - Eased);
		PetPtr->SetActorLocation(Location);

		if (Alpha >= 1.f)
			StopPetFly(); // 착지	
	}

	//! ---- 플레이어 쪽으로 돌기 ----
	if (true == bPetTurning)
	{
		const FRotator Target(0.f, PetTurnTargetYaw, 0.f);
		const FRotator NewRot = FMath::RInterpConstantTo(PetPtr->GetActorRotation(), Target, DeltaSeconds, PetTurnSpeed);
		PetPtr->SetActorRotation(NewRot);

		// 거의 다 돌았으면 딱 맞추고 끝 (-180/180 경계도 NormalizeAxis 로 처리)
		if (FMath::Abs(FRotator::NormalizeAxis(NewRot.Yaw - PetTurnTargetYaw)) < 1.f)
		{
			PetPtr->SetActorRotation(Target);
			bPetTurning = false;
		}
	}

	//! ---- 펫 메시 크기 변화 (보스 변신) ----
	if (true == bPetScaling)
	{
		PetScaleElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(PetScaleElapsed / PetScaleDuration, 0.f, 1.f);
		const float Eased = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f); // 천천히 시작해서 천천히 멈춤
		SetPetMeshScale(FMath::Lerp(PetScaleFrom, PetScaleTo, Eased));

		if (Alpha >= 1.f) bPetScaling = false;
	}

	// 할 일이 다 끝나면 Tick 끄기 -> 평소엔 비용 없음
	if (false == bPetFlying && false == bPetTurning && false == bPetScaling)
		SetActorTickEnabled(false);
}

void ATutorialManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 매니저가 사라질 때 잠금이 남아 있으면 조작 불가 버그 → 무조건 정리
	EndCinematic(0.f);
	StopPetFly();
	bPetTurning = false;
	StopWaitPetArrive();
	StopWaitCondition();
	StopPetFollowLockTarget();
	CancelLockOnRespawns();
	RestorePetForm(); // 변신 도중에 구간이 끝나도 펫이 거대하거나 숨은 채로 남지 않게

	// 튜토리얼에서 막았던 펫 입력 복구 (다음 레벨에서 펫 조작이 안 되는 일 방지)
	if (APlayerMainController* MainPC = Cast<APlayerMainController>(UGameplayStatics::GetPlayerController(this, 0)))
		MainPC->SetPetInputEnabled(true);

	StopLoopFX(); // 커지는 도중에 레벨이 끝나도 효과/소리가 남지 않게

	Super::EndPlay(EndPlayReason);
}

void ATutorialManager::TryStartTutorial()
{
	// 1) 플레이어가 생기자마자 시작 연출 (카메라 + 잠금)
	// (펫을 기다리는 동안 원래 카메라가 보이지 읺게)
	if (false == bIntroApplied)
	{
		if (false == ApplyIntroCinematic()) return; // 플레이어가 아직 없는 상태 -> 다음 확인 때 다시
		bIntroApplied = true;

		// 플레이어가 생긴 시점에 한 번 (리스폰된 캐릭터는 HandlePlayerRespawned 에서 다시 적용)
		ApplyTutorialCamera(UGameplayStatics::GetPlayerPawn(this, 0));

		// 튜토리얼 동안은 펫 모드 변경 등 펫 입력을 전부 막음 (매니저가 사라질 때 EndPlay 에서 되돌림)
		// 한 틱 미룸: 이 함수는 매니저 BeginPlay 에서 바로 불릴 수 있는데, 그때 컨트롤러 BeginPlay 가 아직이면
		// 여기서 뺀 IMC_Pet 을 컨트롤러 BeginPlay 가 다시 넣어버림 -> 모든 BeginPlay 가 끝난 다음 틱에 뺌
		GetWorldTimerManager().SetTimerForNextTick([this]
			{
				if (APlayerMainController* MainPC = Cast<APlayerMainController>(UGameplayStatics::GetPlayerController(this, 0)))
					MainPC->SetPetInputEnabled(false);
			});
	}

	AMainGameMode* GameMode = GetWorld()->GetAuthGameMode<AMainGameMode>();
	APetCharacter* FoundPet = (nullptr != GameMode) ? GameMode->GetPet() : nullptr;

	// 펫이 아직 게임 모드에서 안 만들어졌거나, 초기화 전이라면 다음 확인할 때 다시 확인..
	if (nullptr == FoundPet || false == FoundPet->IsPetInitialized())
		return;

	GetWorldTimerManager().ClearTimer(StartTimerHandle); // 준비 완료 -> 펫 더 확인 안 함
	Pet = FoundPet;
	PetDefaultSpeed = FoundPet->GetCharacterMovement()->MaxWalkSpeed; // 기존 속도 기억

	ChangeStage(StartStage);
}

void ATutorialManager::ChangeStage(ETutorialStage NewStage)
{
	if (NewStage == CurrentStage || true == bChangingStage) return;

	bChangingStage = true;

	const ETutorialStage PreviousStage = CurrentStage;

	if (ETutorialStage::None != PreviousStage)
		StageEnd(PreviousStage);

	CurrentStage = NewStage;
	StageStart(CurrentStage);

	bChangingStage = false;
}

void ATutorialManager::StageStart(ETutorialStage Stage)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] StageStart: %s"), *UEnum::GetValueAsString(Stage));

	Actions.Reset();
	ActionIndex = INDEX_NONE;

	// 현재 구간의 동작 채워넣기
	BuildActions(Stage, Actions);
	RunNextAction();
}

void ATutorialManager::StageEnd(ETutorialStage Stage)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] StageEnd: %s"), *UEnum::GetValueAsString(Stage));

	// 초기화 및 정리
	GetWorldTimerManager().ClearTimer(WaitTimerHandle);
	StopWaitPlayerEnter();
	StopWaitPetArrive(); // 펫 도착 대기 중이었다면 정리
	EndCinematic(0.f);   // 구간이 바뀌면 연출은 무조건 끝 (잠금이 다음 구간으로 새지 않게)
	StopPetFly(); // 비행 도중 구간이 바뀌어도 펫이 공중에 남지 않게
	bPetTurning = false;
	StopWaitCondition();
	StopPetFollowLockTarget();
	RestorePetForm(); // 변신 도중에 구간이 끝나도 펫이 거대하거나 숨은 채로 남지 않게
	CancelLockOnRespawns();
	LockOnEnemySlots.Reset();
	LockOnEnemies.Reset();
	Actions.Reset();
	ActionIndex = INDEX_NONE;
}

void ATutorialManager::BuildActions(ETutorialStage Stage, TArray<FTutorialAction>& OutActions)
{
	switch (Stage)
	{
	case ETutorialStage::Movement: BuildMovementActions(OutActions); break;
	case ETutorialStage::Dash: BuildDashActions(OutActions); break;
	case ETutorialStage::LockOn: BuildLockOnActions(OutActions); break;
	case ETutorialStage::Combat: BuildCombatActions(OutActions); break;
	default: break;
	}
}

void ATutorialManager::BuildMovementActions(TArray<FTutorialAction>& OutActions)
{
	// 이 구간에서 쓸 레벨 액터 찾기 (구간이 시작될 때 한 번)
	TWeakObjectPtr<AActor> CameraDoor01 = FindActorByTag(TutorialTag::CameraDoor01);

	AActor* TriggerDoor01 = FindActorByTag(TutorialTag::TriggerDoor01);
	TWeakObjectPtr<AActor> Gate01 = FindActorByTag(TutorialTag::Gate01);
	TWeakObjectPtr<AActor> DecalWASD = FindActorByTag(TutorialTag::DecalWASD);

	TWeakObjectPtr<AActor> PetStart = FindActorByTag(TutorialTag::PetPointStart);
	TWeakObjectPtr<AActor> PetDoor01 = FindActorByTag(TutorialTag::PetPointDoor01);
	TWeakObjectPtr<AActor> PetAfterDoor01 = FindActorByTag(TutorialTag::PetPointAfterDoor01);

	// 0) 시작 연출: 문 쪽 앵글 + 조작 잠금
	OutActions.Add(Do([this, CameraDoor01] { BeginCinematic(CameraDoor01.Get(), 0.f); }));

	// 1) 펫이 플레이어 근처에 반짝하며 나타나 인사 (순간이동이 "툭" 생기지 않게 효과로 가려줌)
	OutActions.Add(Do([this, PetStart] { PlacePet(PetStart.Get()); PlayFXSetAtGround(PetAppearFX, Pet.Get()); }));
	OutActions.Add(Do([this] { PetSay(INVTEXT("안녕!")); }));
	OutActions.Add(WaitSeconds(1.5f));

	// 2) 이동 키 안내
	OutActions.Add(Do([this, DecalWASD]{SetActivated(DecalWASD.Get(), true); }));
	OutActions.Add(WaitSeconds(1.f));

	// 3) 펫이 문쪽으로 천천히 걸어가며 따라오라고 함
	OutActions.Add(Do([this, PetDoor01] {PetMoveTo(PetDoor01.Get(), SlowPetSpeed); }));
	OutActions.Add(Do([this] { PetSay(INVTEXT("따라와!")); }));
	OutActions.Add(WaitPetArrive(PetDoor01.Get())); // 펫이 문 앞에 도착할 때까지 (최대 8초)
	OutActions.Add(Do([this] { PetFacePlayer(); })); // 문 앞에 도착하면 플레이어 쪽으로 돌아봄
	OutActions.Add(WaitSeconds(0.2f));                // 돌아보는 모습이 보이고 나서 카메라 복귀

	// 3-1) 연출 끝 : 플레이어 카메라로 복귀 + 조작 되돌리기
	OutActions.Add(Do([this] {EndCinematic(CameraReturnBlendTime); }));
	
	// 4) 플레이어가 문 앞에 오면 문을 열고, 문이 어느 정도 열린 뒤 펫이 먼저 지나감
	OutActions.Add(WaitPlayerEnter(TriggerDoor01)); // (플레이어가 문 앞까지 올 때까지 멈춤)
	OutActions.Add(Do([this, Gate01] {SetActivated(Gate01.Get(), true); })); // 문 열기
	OutActions.Add(WaitSeconds(0.6f)); // 문짝이 다 열리기 전에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetAfterDoor01] {PetMoveTo(PetAfterDoor01.Get()); }));
}

void ATutorialManager::BuildDashActions(TArray<FTutorialAction>& OutActions)
{
	AActor* TriggerDashEnter = FindActorByTag(TutorialTag::TriggerDashEnter);
	AActor* TriggerDoor02 = FindActorByTag(TutorialTag::TriggerDoor02);

	TWeakObjectPtr<AActor> CamCorridor = FindActorByTag(TutorialTag::CameraDashCorridor);
	TWeakObjectPtr<AActor> PetDashEnd = FindActorByTag(TutorialTag::PetPointDashEnd);
	TWeakObjectPtr<AActor> PetAfterDoor02 = FindActorByTag(TutorialTag::PetPointAfterDoor02);
	TWeakObjectPtr<AActor> DecalShift = FindActorByTag(TutorialTag::DecalShift);
	TWeakObjectPtr<AActor> Gate02 = FindActorByTag(TutorialTag::Gate02);

	// 0) 구간은 이미 시작됐지만, 실제 연출은 플레이어가 대시 방에 들어와야 시작
	OutActions.Add(WaitPlayerEnter(TriggerDashEnter));

	// 1) (잠금) 통로 전체 샷으로 전환. 카메라가 다 도착한 뒤에 펫이 출발해야 비행 전체가 보임
	OutActions.Add(Do([this, CamCorridor] { BeginCinematic(CamCorridor.Get(), DashCameraBlendTime); }));
	OutActions.Add(WaitSeconds(DashCameraBlendTime));

	// 2) 카메라는 가만히, 펫이 레이저 위를 날아감 -> 플레이어 시선이 펫을 따라 레이저와 끝 지점을 봄
	OutActions.Add(Do([this, PetDashEnd] { PetFlyTo(PetDashEnd.Get(), DashFlyTime); }));
	OutActions.Add(WaitSeconds(DashFlyTime));

	// 3) 착지 직후 부르기 + Shift 데칼 ("날아가서 -> 저기서 부른다" 가 한 흐름으로 읽히게)
	OutActions.Add(Do([this] { PetFacePlayer(); })); // 착지하면 플레이어 쪽으로 돌아봄
	OutActions.Add(WaitSeconds(0.3f));                // 돌고 나서 "Dash!" (등 돌린 채 말하지 않게)
	OutActions.Add(Do([this] { PetSay(INVTEXT("따라와!")); })); 
	OutActions.Add(Do([this, DecalShift] { SetActivated(DecalShift.Get(), true); }));
	OutActions.Add(WaitSeconds(DashShowTime));

	// 4) (잠금 해제) 플레이어 카메라로 복귀
	OutActions.Add(Do([this] { EndCinematic(CameraReturnBlendTime); }));

	// 5) 플레이어가 통로 끝에 도착하면 문 열고 펫 통과
	OutActions.Add(WaitPlayerEnter(TriggerDoor02));
	OutActions.Add(Do([this, Gate02] { SetActivated(Gate02.Get(), true); }));
	OutActions.Add(WaitSeconds(0.6f)); // 문짝에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetAfterDoor02] { PetMoveTo(PetAfterDoor02.Get()); }));
}

void ATutorialManager::BuildLockOnActions(TArray<FTutorialAction>& OutActions)
{
	AActor* TriggerLockOnEnter = FindActorByTag(TutorialTag::TriggerLockOnEnter);

	TWeakObjectPtr<AActor> CamArena = FindActorByTag(TutorialTag::CameraLockOnArena);
	TWeakObjectPtr<AActor> Gate03 = FindActorByTag(TutorialTag::Gate03);
	TWeakObjectPtr<AActor> PetCorridor = FindActorByTag(TutorialTag::PetPointCorridor); // 펫이 통로에서 기다릴 자리

	// 적은 여러 마리라 FindActorByTag(하나만 찾음) 대신 같은 태그를 전부 모아둠
	LockOnEnemies.Reset();
	LockOnEnemySlots.Reset();
	bLockOnLessonDone = false;

	TArray<AActor*> FoundEnemies;
	UGameplayStatics::GetAllActorsWithTag(this, TutorialTag::EnemyLockOn, FoundEnemies);
	for (AActor* Enemy : FoundEnemies)
	{
		FTutorialEnemySlot& Slot = LockOnEnemySlots.AddDefaulted_GetRef();
		Slot.EnemyClass = Enemy->GetClass();
		Slot.SpawnTransform = Enemy->GetActorTransform();

		LockOnEnemies.Add(Enemy);
		RegisterLockOnEnemy(Enemy);
	}

	UE_LOG(LogTemp, Log, TEXT("[Tutorial] 락온 구간 적 %d 마리"), LockOnEnemies.Num());

	// 1) 플레이어가 구간에 들어오면 시작
	OutActions.Add(WaitPlayerEnter(TriggerLockOnEnter));

	// 2) 아레나 전체 샷으로 적 보여주기 (들어온 문은 열어 둠 -> 언제든 되돌아갈 수 있음)
	OutActions.Add(Do([this, CamArena] { BeginCinematic(CamArena.Get(), LockOnCameraBlendTime); }));
	OutActions.Add(WaitSeconds(LockOnCameraBlendTime + LockOnShowTime));

	// 2-1) 잠금은 유지한 채 카메라만 플레이어로 돌림
	// (BeginCinematic 은 연출 중이면 카메라만 바꾸니까, 플레이어 폰을 "카메라"로 넘기면 됨)
	// -> 펫이 날아오는 동안 플레이어가 움직이지 않아서 펫이 정확히 앞에 착지함
	OutActions.Add(Do([this] { BeginCinematic(UGameplayStatics::GetPlayerPawn(this, 0), CameraReturnBlendTime); }));
	OutActions.Add(WaitSeconds(CameraReturnBlendTime));
	// 펫이 플레이어 앞까지 걸어옴 (플레이어는 잠겨 있어서 목적지가 그대로 유지됨)
	OutActions.Add(Do([this] { PetWalkToLocation(GetPetPointNearPlayer()); }));
	OutActions.Add(WaitUntil([this] { return false == IsPetWalking(); })); // 거리마다 걸리는 시간이 달라서 "도착"을 기다림
	OutActions.Add(Do([this] { PetFacePlayer(); }));
	OutActions.Add(WaitSeconds(0.3f)); // 돌고 나서 말하기
	OutActions.Add(Do([this] { PetSay(INVTEXT("적들이 있어!")); }));
	OutActions.Add(WaitSeconds(1.5f));

	// 3) 펫이 "휠 누르면 잡힐 적" 옆으로 걸어가서, 도착하면 휠 클릭 안내
	// (아직 잠금 유지 -> 펫이 적을 가리키는 걸 다 보여준 뒤에 조작 시작)
	OutActions.Add(Do([this] { PetWalkToEnemy(FindLockOnHintTarget()); }));
	OutActions.Add(WaitUntil([this] { return false == IsPetWalking(); })); // 거리마다 걸리는 시간이 달라서 "도착"을 기다림
	OutActions.Add(Do([this] { PetSay(INVTEXT("휠 클릭으로 조준!")); }));

	// 3-1) 휠 클릭 안내와 동시에 조작 시작 (카메라는 이미 플레이어 -> 사실상 잠금만 풂)
	OutActions.Add(Do([this]
		{
			EndCinematic(0.f);
			LockOnHintSayTime = GetWorld()->GetTimeSeconds(); // 방금 안내했으니 여기서부터 시간 잼
		}));

	// 4) 락온할 때까지 기다림 (반드시 해야 함)
	// 그 전에 적을 잡아도 펫이 다시 불러내서 "락온 없이 통과"가 안 됨
	OutActions.Add(WaitUntil([this]
		{
			if (true == IsPlayerLockedOn()) return true;

			// 오래 안 하면 다시 알려줌
			const float Now = GetWorld()->GetTimeSeconds();
			if (Now - LockOnHintSayTime >= LockOnHintRepeatTime)
			{
				LockOnHintSayTime = Now;

				// 펫이 가리키던 적이 쓰러졌으면 살아있는 적 옆으로 옮겨 감 (시체 옆에서 안내하지 않게)
				if (false == IsEnemyAlive(PetFollowedTarget.Get()))
				{
					if (AActor* Hint = FindLockOnHintTarget())
						PetWalkToEnemy(Hint);
				}
				PetSay(INVTEXT("휠 클릭으로 조준해 봐!"));
			}
			return false;
		}));

	// 4) 락온 성공 -> 이제 "대상 바꾸기" 안내
	// 이때부터 펫이 락온 대상을 따라다님 -> 스크롤로 바꾸면 펫이 새 대상으로 가서 "바뀐 게" 눈에 보임
	OutActions.Add(Do([this]
		{
			ULockOnComponent* LockOn = GetPlayerLockOn();
			SwitchBaseTarget = (nullptr != LockOn) ? LockOn->GetLockedTarget() : nullptr; // 처음 잡은 적 = 판정 기준
			bLockReleasedDuringSwitch = false;

			StartPetFollowLockTarget();
			PetSay(INVTEXT("휠을 굴리면 대상이 바껴!"));
			LockOnHintSayTime = GetWorld()->GetTimeSeconds();
		}));

	// 4-1) 다른 적으로 바꿀 때까지 기다림 (반드시 해야 함)
	OutActions.Add(WaitUntil([this]
		{
			if (true == HasPlayerSwitchedLockTarget()) return true;

			const float Now = GetWorld()->GetTimeSeconds();
			if (Now - LockOnHintSayTime >= LockOnHintRepeatTime)
			{
				LockOnHintSayTime = Now;
				// 락온을 풀어 버렸으면 다시 잡으라고, 잡고 있으면 굴리라고
				PetSay(true == IsPlayerLockedOn() ? INVTEXT("휠을 굴려 봐!") : INVTEXT("휠 클릭으로 다시 조준해!"));
			}
			return false;
		}));
	OutActions.Add(WaitSeconds(0.5f)); // 펫이 새 대상 쪽으로 움직이는 걸 볼 틈

	// 4-2) 다 배움 -> 더는 다시 불러내지 않음 + 이제 공격 안내
	OutActions.Add(Do([this]
		{
			bLockOnLessonDone = true;
			CancelLockOnRespawns(); // 기다리던 다시 불러내기도 취소 (남은 적만 잡으면 끝)
			PetSay(INVTEXT("좋아, 공격해!"));
		}));

	// 5) 전부 처치하면 입력을 잠그고, 펫이 그 자리에서 플레이어를 돌아보고 칭찬
	OutActions.Add(WaitUntil([this] { return AreLockOnEnemiesDead(); }));
	OutActions.Add(Do([this]
		{
			// 카메라는 그대로(플레이어) 두고 입력만 잠금 -> 칭찬/문 열리는 연출 중에 플레이어가 먼저 뛰어가지 않게
			BeginCinematic(UGameplayStatics::GetPlayerPawn(this, 0), 0.f);
			StopPetFollowLockTarget();
			PetFacePlayer();
		}));
	OutActions.Add(WaitSeconds(0.2f));
	OutActions.Add(Do([this] { PetSay(INVTEXT("잘했어!")); }));
	OutActions.Add(WaitSeconds(0.8f));

	// 6) Gate03 만 열고 펫이 통로로 먼저 감
	// (Gate04는 통로에서 회복 안내가 끝난 뒤 전투 구간에서 엶 -> 보스 연출과 회복 안내 안 섞임!)
	OutActions.Add(Do([this, Gate03] { SetActivated(Gate03.Get(), true); }));
	OutActions.Add(WaitSeconds(0.8f)); // Gate03 문짝에 펫이 부딪히지 않게
	OutActions.Add(Do([this, PetCorridor]
		{
			if (true == PetCorridor.IsValid())
				PetWalkToLocation(PetCorridor->GetActorLocation());

			EndCinematic(0.f); // 펫이 출발하는 순간 입력 해제 -> 펫을 따라 통로로
		}));
}

void ATutorialManager::BuildCombatActions(TArray<FTutorialAction>& OutActions)
{
	AActor* TriggerBossRoomEnter = FindActorByTag(TutorialTag::TriggerBossRoomEnter);

	TWeakObjectPtr<AActor> BossSpawn = FindActorByTag(TutorialTag::PointBossSpawn);
	TWeakObjectPtr<AActor> CamBossArena = FindActorByTag(TutorialTag::CameraBossArena);
	TWeakObjectPtr<AActor> Checkpoint = FindActorByTag(TutorialTag::CheckpointCombat);
	TWeakObjectPtr<AActor> PetCheckpoint = FindActorByTag(TutorialTag::PetPointCheckpoint);
	TWeakObjectPtr<AActor> PortalExit = FindActorByTag(TutorialTag::PortalExit);

	AActor* TriggerCorridor = FindActorByTag(TutorialTag::TriggerCorridor);
	TWeakObjectPtr<AActor> Gate04 = FindActorByTag(TutorialTag::Gate04);
	TWeakObjectPtr<AActor> PetAfterDoor04 = FindActorByTag(TutorialTag::PetPointAfterDoor04);

	TWeakObjectPtr<AActor> PetAfterDoor04Retry = FindActorByTag(TutorialTag::PetPointAfterDoor04); // 재도전 때 펫을 둘 자리

	// 0) 체크포인트는 보스를 잡기 전까지 숨김
	OutActions.Add(Do([this, Checkpoint] { SetActorShown(Checkpoint.Get(), false); }));

	if(false == bBossRetry) // 재도전이면 통로 회복 안내 / 
	{
		// 1) 통로에 들어오면 (잠금) 펫이 회복 이야기 -> (해제) 직접 마시기 / 읽을 시간
	// Gate04 가 닫혀 있어서 플레이어가 보스방으로 먼저 뛰어갈 수 없음 -> 대사와 보스 연출이 꼬이지 않음
		OutActions.Add(WaitPlayerEnter(TriggerCorridor));
		OutActions.Add(WaitUntil([this] { return false == IsPetWalking(); })); // 플레이어가 앞질러 와도 펫이 자리에 선 뒤에 말함
		OutActions.Add(Do([this]
			{
				// 카메라는 그대로(플레이어) 두고 입력만 잠금 -> 펫 말을 끝까지 보게
				BeginCinematic(UGameplayStatics::GetPlayerPawn(this, 0), 0.f);

				RefillPlayerPotions(); // 앞에서 다 써버렸어도 연습할 수 있게 + 보스전 공정하게
				bBossHealHintShown = false;

				HealLessonPotions = GetPlayerPotionCount();
				bHealLesson = GetPlayerHealthRatio() < 1.f && HealLessonPotions > 0; // 다쳤을 때만 직접 마셔 봄 (풀피에 마시면 포션 낭비)

				PetFacePlayer();
			}));
		OutActions.Add(WaitSeconds(0.3f)); // 돌고 나서 말하기
		OutActions.Add(Do([this]
			{
				PetSay(true == bHealLesson ? INVTEXT("다쳤네! R 키로 포션 마셔!") : INVTEXT("한 대도 안 맞았어? 대단해!"));
			}));
		OutActions.Add(WaitSeconds(1.5f)); // 말풍선 읽을 틈 (잠금 중)
		OutActions.Add(Do([this]
			{
				if (false == bHealLesson)
					PetSay(INVTEXT("다치면 R 키로 포션을 마셔!"));

				// 잠금 해제 -> 다쳤으면 직접 R 을 눌러 봐야 하니까
				EndCinematic(0.f);
				HealLessonSayTime = GetWorld()->GetTimeSeconds();
			}));

		// 다쳤으면 마실 때까지 기다림 (포션 수가 줄면 = 마심). 안 마시면 일정 시간마다 다시 알려줌
		// 풀피면 바로 통과 (아래 대기 시간 동안 "다치면 R..." 을 읽음)
		OutActions.Add(WaitUntil([this]
			{
				if (false == bHealLesson) return true;
				if (GetPlayerPotionCount() < HealLessonPotions) return true;

				const float Now = GetWorld()->GetTimeSeconds();
				if (Now - HealLessonSayTime >= HealLessonRepeatTime)
				{
					HealLessonSayTime = Now;
					PetSay(INVTEXT("R 키를 눌러서 포션을 마셔 봐!"));
				}
				return false;
			}));
		OutActions.Add(WaitSeconds(1.f)); // 마시는 모션이 끝날 틈 / 풀피면 메시지 읽는 시간
		OutActions.Add(Do([this]
			{
				if (true == bHealLesson)
					PetSay(INVTEXT("좋아! 위험할 땐 그렇게 마셔!"));
			}));
		OutActions.Add(WaitSeconds(1.8f)); // 말풍선 읽을 틈

		// 1-1) 회복 안내가 끝나면 전투 구간 문을 열고 펫이 먼저 보스방으로
		OutActions.Add(Do([this, Gate04] { SetActivated(Gate04.Get(), true); }));
		OutActions.Add(WaitSeconds(0.6f)); // Gate04 문짝에 펫이 부딪히지 않게
		OutActions.Add(Do([this, PetAfterDoor04]
			{
				if (true == PetAfterDoor04.IsValid())
					PetWalkToLocation(PetAfterDoor04->GetActorLocation());
			}));
	}
	else
	{
		// 재도전: 펫을 보스 자리에 바로 세움
		// - 리스폰 때 게임모드가 펫을 "플레이어 따라가기"로 다시 초기화함 -> 매니저가 다시 잡아서 멈춰 세움
		// - 플레이어 재시작 자리와 겹치지 않게 보스 자리로 (겹치면 순간이동이 막혀서 엉뚱한 곳에서 변신함)
		OutActions.Add(Do([this, BossSpawn]
			{
				APetCharacter* PetPtr = Pet.Get();
				if (nullptr == PetPtr || false == BossSpawn.IsValid()) return;

				PetPtr->RequestBehavior(EPetBehavior::Idle);
				if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
					AIC->StopMovement();

				// 지점은 바닥 -> 캡슐 반높이만큼 올려서 세움 (바닥에 박혀서 순간이동이 실패하지 않게)
				const FVector HalfUp(0.f, 0.f, PetPtr->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
				PetPtr->TeleportTo(GetGroundLocation(BossSpawn.Get()) + HalfUp, BossSpawn->GetActorRotation());
			}));
	}

	// 1-2) 플레이어가 보스방 안까지 들어오면 보스 연출 시작
	OutActions.Add(WaitPlayerEnter(TriggerBossRoomEnter));
	OutActions.Add(Do([this] { bInBossFight = true; })); // 여기부터 죽으면 보스방에서 다시 시작
	OutActions.Add(WaitSeconds(0.8f)); // 방에 들어선 걸 느낄 틈 -> 카메라 전환

	// 2) (잠금) 아레나 샷 -> 펫이 보스 자리로 걸어가서 돌아보고 "마지막 시험이야!"
	OutActions.Add(Do([this, CamBossArena] { BeginCinematic(CamBossArena.Get(), BossCameraBlendTime); }));
	OutActions.Add(WaitSeconds(BossCameraBlendTime));
	OutActions.Add(Do([this, BossSpawn]
		{
			if (true == BossSpawn.IsValid())
				PetWalkToLocation(BossSpawn->GetActorLocation());
		}));
	OutActions.Add(WaitUntil([this] { return false == IsPetWalking(); }));
	OutActions.Add(Do([this] { PetFacePlayer(); }));
	OutActions.Add(WaitSeconds(0.3f)); // 돌고 나서 말하기
	OutActions.Add(Do([this] { PetSay(INVTEXT("마지막 시험이야!")); }));
	OutActions.Add(WaitSeconds(1.2f));

	// 3) "나를 이겨 봐!" -> 펫이 보스 크기까지 커짐 -> 같은 자리에 보스로 교체
	// (말풍선을 먼저: 커진 뒤에 말하면 말풍선이 거대한 몸에 파묻힘)
	OutActions.Add(Do([this] { PetSay(INVTEXT("나를 이겨 봐!")); }));
	OutActions.Add(WaitSeconds(0.3f));

	// 커지는 동안 발밑 효과 + 차오르는 소리를 계속 재생
	OutActions.Add(Do([this] { StartLoopFXOnPet(PetGrowFX); StartPetMeshScale(BossMeshScale, PetGrowTime); }));
	OutActions.Add(WaitUntil([this] { return false == IsPetMeshScaling(); }));
	OutActions.Add(Do([this]
		{
			// 다 커진 순간: 반복 효과를 멈추고 -> 펑 + 쿵 + 흔들림 -> 보스로 교체
			// (터지는 효과가 교체 순간을 가려줘서 펫 -> 보스가 더 자연스럽게 이어 보임)
			StopLoopFX();
			PlayFXSetAtGround(BossAppearFX, Pet.Get()); // 교체 직전 펫 자리 = 보스가 설 자리
			SwapPetToBoss();
		}));
	OutActions.Add(WaitSeconds(BossShowTime)); // 보스를 잠깐 보여줌 (AI 는 아직 꺼져 있어서 공격 안 함)

	// 3-1) (잠금 해제) 플레이어 카메라 복귀와 동시에 보스 AI 시작 -> 전투 시작
	OutActions.Add(Do([this] { EndCinematic(CameraReturnBlendTime); StartBossAI(); }));

	// 4) 보스가 쓰러질 때까지 전투
	OutActions.Add(WaitUntil([this] { CheckBossHealHint();  return IsBossDead(); }));
	OutActions.Add(Do([this] { bInBossFight = false; })); // 보스를 잡은 뒤엔 재시작하지 않음

	// 5) (잠금) 아레나 샷 -> 보스 스폰 자리에 거대한 펫 -> 원래 크기로 줄어듦 -> "아야! 네가 이겼어!"
	OutActions.Add(Do([this, CamBossArena, BossSpawn]
		{
			SwapBossToPet(BossSpawn.Get(), CamBossArena.Get()); // 보스가 래그돌로 늘어지는 게 보이기 전에 바로 교체 (카메라 쪽을 보며 등장)
			BeginCinematic(CamBossArena.Get(), BossCameraBlendTime);
		}));
	OutActions.Add(WaitSeconds(BossCameraBlendTime));
	// 줄어들기 시작할 때 반짝이 + 퐁 (보스 기운이 빠져나가는 느낌)
	OutActions.Add(Do([this] { PlayFXSetAtGround(PetShrinkFX, Pet.Get()); StartPetMeshScale(1.f, PetShrinkTime); }));
	OutActions.Add(WaitUntil([this] { return false == IsPetMeshScaling(); }));
	OutActions.Add(Do([this] { PetFacePlayer(); }));
	OutActions.Add(WaitSeconds(0.3f));
	OutActions.Add(Do([this] { PetSay(INVTEXT("아야! 네가 이겼어!")); }));
	OutActions.Add(WaitSeconds(1.5f));

	// 6) 끝 벽이 관문으로 바뀜 (아레나 샷 정면에 보임) -> 다 열리면 펫이 관문 앞으로 -> (잠금 해제) "저기로 가자!"
	OutActions.Add(Do([this, PortalExit] { SetActivated(PortalExit.Get(), true); }));
	OutActions.Add(WaitUntil([PortalExit]
		{
			// 관문이 없거나(배치 안 함) 다 열리면 통과 -> 관문이 없어도 튜토리얼이 멈추지 않게
			const ATutorialPortal* Portal = Cast<ATutorialPortal>(PortalExit.Get());
			return nullptr == Portal || Portal->IsOpen();
		}));
	OutActions.Add(WaitSeconds(0.5f)); // 열린 관문을 잠깐 보여줌
	OutActions.Add(Do([this, PetCheckpoint]
		{
			if (true == PetCheckpoint.IsValid())
				PetWalkToLocation(PetCheckpoint->GetActorLocation()); // 관문 바로 앞 자리
		}));
	OutActions.Add(WaitUntil([this] { return false == IsPetWalking(); }));
	OutActions.Add(Do([this] { PetFacePlayer(); }));
	OutActions.Add(WaitSeconds(0.3f));
	OutActions.Add(Do([this] { EndCinematic(CameraReturnBlendTime); PetSay(INVTEXT("저기로 가자!")); }));

	// 7) 튜토리얼 끝: 괴냥이가 다시 플레이어를 따라다님 (관문 앞에 서 있지 않음)
	// -> 관문 들어가기 전 이스터에그 상자 등에서 괴냥이 말풍선이 플레이어 근처에 보이게
	OutActions.Add(Do([this]
		{
			if (APetCharacter* PetPtr = Pet.Get())
				PetPtr->RequestBehavior(EPetBehavior::Follow);
		}));
}

void ATutorialManager::RunNextAction()
{
	while (true)
	{
		// 즉시 끝나는 동작은 반복문으로 연달아 실행하고,
		// 기다려야 하는 동작을 만나면 return 으로 멈춰요 (기다리는 게 끝났다면? 다시 이 함수 호출함)
		++ActionIndex;
		if (false == Actions.IsValidIndex(ActionIndex))
		{
			OnActionsFinished();
			return;
		}

		const FTutorialAction& Action = Actions[ActionIndex];
		if (Action.Run) // 이 동작이 할 일 실행 (대기 전용 동작이면 Run이 비어있음)
			Action.Run();

		switch (Action.Wait)
		{
		case ETutorialWait::None: continue; // 기다릴 거 없음 -> 바로 다음 동작

		case ETutorialWait::Seconds: // N초 뒤에 이 함수를 다시 부름 -> 다음 동작으로 진행
			GetWorldTimerManager().SetTimer(WaitTimerHandle, this, &ATutorialManager::RunNextAction, Action.Seconds, false);
			return;

		case ETutorialWait::PlayerEnter: // 트리거에 들어올 때까지 멈춤 -> 신호가 오면 다음 동작으로 진행
			if (true == BeginWaitPlayerEnter(Action.Target.Get())) return;
			continue; // 트리거가 없거나 플레이어가 이미 안에 있으면 바로 다음 동작

		case ETutorialWait::PetArrive: // 펫이 지점에 도착할 떄 까지 멈춤 -> 도착하면 다음 동작
			if (true == BeginWaitPetArrive(Action.Target.Get(), Action.Seconds)) return;
			continue; // 펫이나 지점이 없거나 이미 도착해있으면 바로 다음 동작
		
		case ETutorialWait::Condition: // 조건이 참이 될 때까지 멈춤 -> 참이 되면 다음 동작
			if (true == BeginWaitCondition(Action.Condition)) return;
			continue; // 조건이 비었거나 이미 참이면 바로 다음 동작
		}
	}
}

void ATutorialManager::OnActionsFinished()
{
	if (ETutorialStage::Completed == CurrentStage) return;
	const ETutorialStage NextStage = static_cast<ETutorialStage>(static_cast<uint8>(CurrentStage) + 1);

	// ChangeStage 도중 (bChangingStage)에 여기로 올 수 있으니, 한 틱 미뤄서 구간 전환
	GetWorldTimerManager().SetTimerForNextTick([this, NextStage] {ChangeStage(NextStage); });
}

FTutorialAction ATutorialManager::Do(TFunction<void()> _Run)
{
	FTutorialAction Action;
	Action.Run = MoveTemp(_Run);
	return Action;
}

FTutorialAction ATutorialManager::WaitSeconds(float _Seconds)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::Seconds;
	Action.Seconds = _Seconds;
	return Action;
}

FTutorialAction ATutorialManager::WaitPlayerEnter(AActor* _Trigger)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::PlayerEnter;
	Action.Target = _Trigger;
	return Action;
}

FTutorialAction ATutorialManager::WaitPetArrive(AActor* _Point, float _Timeout)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::PetArrive;
	Action.Target = _Point;
	Action.Seconds = _Timeout;
	return Action;
}

FTutorialAction ATutorialManager::WaitUntil(TFunction<bool()> _Condition)
{
	FTutorialAction Action;
	Action.Wait = ETutorialWait::Condition;
	Action.Condition = MoveTemp(_Condition);
	return Action;
}

AActor* ATutorialManager::FindActorByTag(FName _Tag) const
{
	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(this, _Tag, Found);

	// 태그는 액터 하나에만 붙어있어야 함.
	//? 아니라면 세팅 실수인 거시야
	if (1 != Found.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 태그 %s 를 가진 액터가 %d개"), 
			*_Tag.ToString(), Found.Num());
	}

	return (true == Found.IsEmpty()) ? nullptr : Found[0];
}

bool ATutorialManager::BeginWaitPlayerEnter(AActor* _Trigger)
{
	//! 트리거를 못 찾았다면.. 영원히 멈추지 않아요.. 디버깅용
	if (nullptr == _Trigger)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 기다릴 트리거가 없어 건너뜀"));
		return false;
	}

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// 플레이어가 이미 트리거 안에 있으면 바로 통과
	if (nullptr != PlayerPawn && true == _Trigger->IsOverlappingActor(PlayerPawn))
		return false;

	// 트리거를 기다릴 동안만 바인딩
	WaitingTrigger = _Trigger;
	_Trigger->OnActorBeginOverlap.AddUniqueDynamic(this, &ATutorialManager::OnWaitTriggerBeginOverlap);
	return true;
}

void ATutorialManager::StopWaitPlayerEnter()
{
	// 트리거 델리게이트 바인딩 해제
	if (AActor* Trigger = WaitingTrigger.Get())
			Trigger->OnActorBeginOverlap.RemoveDynamic(this, &ATutorialManager::OnWaitTriggerBeginOverlap);

		WaitingTrigger.Reset();
}

void ATutorialManager::SetActivated(AActor* _Target, bool _bActivate)
{
	if (nullptr == _Target || false == _Target->Implements<UActivatableInterface>())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] %s 는 Activatable 이 아님"), *GetNameSafe(_Target));
		return;
	}

	// 상호작용 활/비활
	if (true == _bActivate)
		IActivatableInterface::Execute_Activate(_Target, this);
	else
		IActivatableInterface::Execute_Deactivate(_Target, this);
}

void ATutorialManager::OnWaitTriggerBeginOverlap(AActor* _OverlappedActor, AActor* _OtherActor)
{
	if (WaitingTrigger.Get() != _OverlappedActor || false == IsPlayerPawn(_OtherActor))
		return;

	StopWaitPlayerEnter(); // 한번만 반응하도록 먼저 바인딩 해제
	RunNextAction(); // 멈춰있던 동작 목록 이어서 진행
}

bool ATutorialManager::IsPlayerPawn(const AActor* _Actor) const
{
	const APawn* Pawn = Cast<APawn>(_Actor);
	return (nullptr != Pawn && true == Pawn->IsPlayerControlled());
}

void ATutorialManager::PlacePet(AActor* _Point)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr || nullptr == _Point) return;

	// 순간이동
	PetPtr->TeleportTo(_Point->GetActorLocation(), _Point->GetActorRotation());
}

void ATutorialManager::PetMoveTo(AActor* _Point, float _Speed)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr || nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetMoveTo: 펫 또는 지점이 없음."));
		return;
	}

	// 속도 지정이 없으면(0) 원래 속도로 되돌려요 -> 느린 속도가 남아 Follow가 꼬이는 걸 방지
	PetPtr->GetCharacterMovement()->MaxWalkSpeed = (_Speed > 0.f) ? _Speed : PetDefaultSpeed;

	// 이동은 펫의 StateTree(MoveTo)가 처리함. 매니저는 목적지만 알려줘요
	if (false == PetPtr->RequestMoveTo(_Point->GetActorLocation()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetMoveTo: 요청 거절됨 (펫 초기화 전?)"));
	}
}

void ATutorialManager::PetSay(const FText& _Message)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] 펫: %s"), *_Message.ToString());

	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	if (USpeechBubbleComponent* Bubble = PetPtr->GetSpeechBubble())
		Bubble->Say(_Message, 4.f); // 말풍선 소리는 SpeechBubbleComponent::Say 안에서 재생 (DA_PetSound)
}

void ATutorialManager::PetFlyTo(AActor* _Point, float _Duration)
{
	if (nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetFlyTo: 지점이 없음"));
		return;
	}

	// 지점 액터 -> 좌표로 바꿔서 실제 비행은 PetFlyToLocation 이 처리
	PetFlyToLocation(_Point->GetActorLocation(), _Duration);
}

void ATutorialManager::PetFlyToLocation(const FVector& _Location, float _Duration)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetFlyToLocation: 펫이 없음"));
		return;
	}

	// 1) StateTree 를 Idle 로 비켜 세움 -> 날아가는 동안 StateTree 가 펫을 끌고 가지 않음
	PetPtr->RequestBehavior(EPetBehavior::Idle);
	if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
		AIC->StopMovement(); // 진행 중이던 길찾기 이동도 멈춤

	// 2) 중력 끄기 (캐릭터라 공중에 두면 매 프레임 떨어지려 함)
	UCharacterMovementComponent* Move = PetPtr->GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->SetMovementMode(MOVE_Flying);

	// 3) Fly 애니메이션. RequestBehavior 가 Normal 로 되돌리므로 반드시 그 "뒤에"
	PetPtr->SetAnimationMovementStyle(EPetMovementStyle::Fly);

	// 비행 경로: 지점은 바닥 위치라, 캡슐 절반 높이만큼 올려야 바닥에 묻히지 않음
	FlyStart = PetPtr->GetActorLocation();
	FlyEnd = _Location;
	FlyEnd.Z += PetPtr->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FlyDuration = FMath::Max(_Duration, 0.1f);
	FlyElapsed = 0.f;

	// 도착 방향을 바라보게
	const FVector Dir = FlyEnd - FlyStart;
	PetPtr->SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw, 0.f));

	bPetFlying = true;
	SetActorTickEnabled(true); // 비행 중에만 Tick
}

void ATutorialManager::PetWalkToLocation(const FVector& _Location)
{
	APetCharacter* PetPtr = Pet.Get();
	AAIController* AIC = (nullptr != PetPtr) ? Cast<AAIController>(PetPtr->GetController()) : nullptr;
	if (nullptr == AIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetWalkToLocation: 펫 또는 AI 컨트롤러가 없음"));
		return;
	}

	StopPetFly(); // 날던 중이었다면 걷기 모드 + 기본 애니메이션으로

	// StateTree 를 Idle 로 비켜 세움 (Idle Task 는 아무것도 안 함)
	// -> StateTree 의 MoveTo 와 섞이지 않고, 매니저가 직접 이동을 관리
	// (StateTree MoveTo 는 이미 MoveTo 상태면 목적지가 바뀌어도 다시 출발하지 않을 수 있음)
	PetPtr->RequestBehavior(EPetBehavior::Idle);
	PetPtr->GetCharacterMovement()->MaxWalkSpeed = PetDefaultSpeed; // 느린 안내 속도가 남아있지 않게

	// 걷는 도중에 다시 불러도 이전 이동은 자동으로 취소되고 새 목적지로 감
	// 마지막 인자 true = 목적지를 NavMesh 위로 맞춰줌 (계산한 좌표가 살짝 벗어나도 실패하지 않게)
	const EPathFollowingRequestResult::Type Result = AIC->MoveToLocation(_Location, 30.f, true, true, true);
	if (EPathFollowingRequestResult::Failed == Result)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetWalkToLocation: 길을 못 찾음 (NavMesh 확인)"));
	}
}

bool ATutorialManager::IsPetWalking() const
{
	APetCharacter* PetPtr = Pet.Get();
	AAIController* AIC = (nullptr != PetPtr) ? Cast<AAIController>(PetPtr->GetController()) : nullptr;

	// 길찾기 이동이 진행 중이면 true. 도착/실패/취소면 false -> 대기가 영원히 걸리지 않음
	return nullptr != AIC && EPathFollowingStatus::Moving == AIC->GetMoveStatus();
}

void ATutorialManager::StopPetFly()
{
	if (false == bPetFlying) return;
	bPetFlying = false;
	//SetActorTickEnabled(false);

	if (APetCharacter* PetPtr = Pet.Get())
	{
		// 원래대로: 걷기 모드 + Normal 애니메이션. 행동은 Idle 그대로 (도착 지점에서 기다림)
		PetPtr->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		PetPtr->SetAnimationMovementStyle(EPetMovementStyle::Normal);
	}
}

void ATutorialManager::PetFacePlayer()
{
	APetCharacter* PetPtr = Pet.Get();
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (nullptr == PetPtr || nullptr == PlayerPawn) return;

	// 도착 반경 안에 들어왔어도 마지막 몇 cm 를 걷는 중일 수 있음
	// -> 걸으면 "이동 방향 바라보기"가 회전을 덮어쓰니, 먼저 확실히 세워둠
	PetPtr->RequestBehavior(EPetBehavior::Idle);
	if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
		AIC->StopMovement();

	// 높이 차이는 무시하고 수평 방향만 (위아래로 고개 꺾이지 않게)
	FVector ToPlayer = PlayerPawn->GetActorLocation() - PetPtr->GetActorLocation();
	ToPlayer.Z = 0.f;
	if (true == ToPlayer.IsNearlyZero()) return;

	PetTurnTargetYaw = ToPlayer.Rotation().Yaw;
	bPetTurning = true;
	SetActorTickEnabled(true);
}

bool ATutorialManager::BeginWaitPetArrive(AActor* _Point, float _TimeOut)
{
	APetCharacter* PetPtr = Pet.Get();
	if(nullptr == PetPtr || nullptr == _Point)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 펫 또는 도착 지점이 없어 건너뜀"));
		return false;
	}

	// 이미 도착해있으면 바로 통과
	if (FVector::Dist2D(PetPtr->GetActorLocation(), _Point->GetActorLocation()) <= PetArriveRadius)
		return false;

	WaitingPetPoint = _Point;
	PetArriveTimeLeft = _TimeOut;

	// 0.1초 마다 거리를 확인
	GetWorldTimerManager().SetTimer(PetArriveTimerHandle, this, &ATutorialManager::CheckPetArrive, 0.1f, true);
	return true;
}

void ATutorialManager::CheckPetArrive()
{
	APetCharacter* PetPtr = Pet.Get();
	AActor* Point = WaitingPetPoint.Get();

	// 높이(Z)는 무시: 지점 액터와 펫 캡슐 중심 높이가 달라서
	const bool bArrived = (nullptr == PetPtr || nullptr == Point)
		|| FVector::Dist2D(PetPtr->GetActorLocation(), Point->GetActorLocation()) <= PetArriveRadius;

	PetArriveTimeLeft -= 0.1f;
	if (false == bArrived && PetArriveTimeLeft > 0.f)
		return; // 아직 가는 중 -> 다음 확인

	// 시간 초과 : 펫이 길에 막혀도 조작이 영원히 잠기지 않게 진행
	if(false == bArrived)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 펫 도착 대기 시간 초과 -> 그냥 진행"));
	}

	StopWaitPetArrive();
	RunNextAction();
}

void ATutorialManager::StopWaitPetArrive()
{
	GetWorldTimerManager().ClearTimer(PetArriveTimerHandle);
	WaitingPetPoint.Reset();
}

bool ATutorialManager::BeginWaitCondition(const TFunction<bool()>& _Condition)
{
	// 조건이 비었거나 이미 참이면 기다릴 필요 없음
	if (!_Condition || true == _Condition())
		return false;

	WaitingCondition = _Condition;

	// 락온/사망은 알려주는 이벤트가 없어서..(아직은) 0.1초마다 직접 확인
	GetWorldTimerManager().SetTimer(ConditionTimerHandle, this, &ATutorialManager::CheckCondition, 0.1f, true);
	return true;
}

void ATutorialManager::CheckCondition()
{
	if (WaitingCondition && false == WaitingCondition())
		return; // 아직 -> 다음 확인

	StopWaitCondition(); // 한 번만 반응하도록 먼저 정리
	RunNextAction();
}

void ATutorialManager::StopWaitCondition()
{
	GetWorldTimerManager().ClearTimer(ConditionTimerHandle);
	WaitingCondition.Reset();
}

bool ATutorialManager::ApplyIntroCinematic()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC || nullptr == PC->GetPawn())
		return false;

	// 이동 구간 시작할 떄만
	if (ETutorialStage::Movement == StartStage)
		BeginCinematic(FindActorByTag(TutorialTag::CameraDoor01), 0.f);

	return true;
}

void ATutorialManager::BeginCinematic(AActor* _Camera, float _BlendTime)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC) return;

	// 이미 연출 중이면 잠금은 그대로, 카메라만 바꿈
	if (false == bInCinematic)
	{
		SetPlayerInputLocked(PC, true);
		bInCinematic = true;
	}

	// 카메라 못 찾아도 잠금은 유지
	if(nullptr == _Camera)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] 연출 카메라가 없어 플레이어 카메라 유지"));
		return;
	}

	// 이미 그 카메라를 보고 있으면 다시 전환핮 ㅣ 않음
	if (PC->GetViewTarget() != _Camera)
		PC->SetViewTargetWithBlend(_Camera, _BlendTime, VTBlend_Cubic); // Cubic 천천히 출발해서 천천히 멈춤 
}

void ATutorialManager::EndCinematic(float _BlendTime)
{
	if (false == bInCinematic) return; // 연출중이 아니라면 할일 읎음
	bInCinematic = false;

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (nullptr == PC)
	{
		LockedPawn.Reset();
		return;
	}

	// 플레이어 카메라로 복귀
	if (APawn* PlayerPawn = PC->GetPawn())
		PC->SetViewTargetWithBlend(PlayerPawn, _BlendTime, VTBlend_Cubic);

	SetPlayerInputLocked(PC, false);
}

void ATutorialManager::SetPlayerInputLocked(APlayerController* _PC, bool _bLock)
{
	// 캐릭터 입력 = 이동/대시/공격/락온, 
	// 컨트롤러 입력 = 펫 다이얼/펫 모드/펫 특수 행동
	// 둘 다 꺼야 모든 플레이어 입력이 막힘!!

	if (true == _bLock)
	{
		LockedPawn = _PC->GetPawn();
		if (APawn* PlayerPawn = LockedPawn.Get())
			PlayerPawn->DisableInput(_PC);
		_PC->DisableInput(_PC);
	}
	else
	{
		// 잠갔던 그 캐릭터만 켬 (리스폰으로 이미 사라졌으면 건너뜀. 새 캐릭터는 원래 켜져 있음)
		if (APawn* PlayerPawn = LockedPawn.Get())
			PlayerPawn->EnableInput(_PC);
		LockedPawn.Reset();
		_PC->EnableInput(_PC);
	}
}

ULockOnComponent* ATutorialManager::GetPlayerLockOn() const
{
	// 플레이어 클래스에 getter 가 없음...!!!
	//? 일단 이렇게 하고 나중에 요청하기
	// 리스폰되면 폰이 바뀌니까 저장하지 않고 매번 찾음
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	return (nullptr != PlayerPawn) ? PlayerPawn->FindComponentByClass<ULockOnComponent>() : nullptr;
}

bool ATutorialManager::IsPlayerLockedOn() const
{
	ULockOnComponent* LockOn = GetPlayerLockOn();
	return nullptr != LockOn && true == LockOn->IsLockedOn();
}

bool ATutorialManager::IsEnemyAlive(AActor* _Enemy)
{
	// 락온 시스템이 쓰는 기준과 똑같이 판단 -> "락온이 안 잡히는데 살아있음" 같은 어긋남 방지
	// 시체가 사라져서(삭제) 무효가 된 적도 죽은 걸로 봄
	return IsValid(_Enemy)
		&& _Enemy->Implements<UTargetableInterface>()
		&& ITargetableInterface::Execute_IsTargetAlive(_Enemy);
}

bool ATutorialManager::AreLockOnEnemiesDead() const
{
	for (const TWeakObjectPtr<AActor>& Enemy : LockOnEnemies)
	{
		if (true == IsEnemyAlive(Enemy.Get())) 
			return false; // 한 마리라도 살아있으면 아직
	}
	return true;
}

AActor* ATutorialManager::FindLockOnHintTarget() const
{
	// 1순위: 지금 휠을 누르면 잡힐 적 -> 펫이 가리킨 적 = 실제로 잠기는 적
	if (ULockOnComponent* LockOn = GetPlayerLockOn())
	{
		AActor* Candidate = LockOn->GetCurrentCandidate();
		if (true == IsEnemyAlive(Candidate)) return Candidate;
	}

	// 2순위: 적이 화면 밖이면 후보가 없음 -> 플레이어와 가장 가까운 살아있는 적
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	AActor* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (const TWeakObjectPtr<AActor>& Enemy : LockOnEnemies)
	{
		AActor* EnemyPtr = Enemy.Get();
		if (false == IsEnemyAlive(EnemyPtr)) continue;

		const float DistSq = (nullptr != PlayerPawn) ? 
			FVector::DistSquared(PlayerPawn->GetActorLocation(), EnemyPtr->GetActorLocation()) : 0.f;

		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = EnemyPtr;
		}
	}
	return Nearest;
}

bool ATutorialManager::HasPlayerSwitchedLockTarget()
{
	// 살아있는 적이 1마리 이하라면 바꿀 대상이 없음 -> 막히지 않게 통과
	// (안내 중에 플레이어가 먼저 적을 잡아버린 경우에 대비한 거시예요)
	int32 AliveCount = 0;
	for (const TWeakObjectPtr<AActor>& Enemy : LockOnEnemies)
	{
		if (true == IsEnemyAlive(Enemy.Get())) ++AliveCount;
	}
	
	// 배치 실수로 적이 2마리 미만이면 바꿀 대상이 영원히 없음 -> 막히지 않게 통과
	// (살아있는 수로 판단하지 않음: 적을 빨리 잡아서 바꾸기를 건너뛰는 걸 막기 위해.
	//  쓰러진 적은 RespawnLockOnEnemy 가 다시 불러내니 조금 기다리면 됨)
	if (LockOnEnemySlots.Num() < 2) return true;

	ULockOnComponent* LockOn = GetPlayerLockOn();
	AActor* Target = (nullptr != LockOn) ? LockOn->GetLockedTarget() : nullptr;

	// 휠 클릭으로 락온을 풀었으면 기준을 비워 둠
	// -> 다시 잡았을 때 그 적을 새 기준으로 삼음 ("풀고 다른 적 잡기"를 스크롤로 치지 않기 위해)
	if (nullptr == Target)
	{
		SwitchBaseTarget.Reset();
		bLockReleasedDuringSwitch = true;
		return false;
	}

	// 풀었다가 다시 잡은 순간 -> 새 기준 + 안내를 한 번 더 보여줌
	if (false == SwitchBaseTarget.IsValid())
	{
		SwitchBaseTarget = Target;
		if (true == bLockReleasedDuringSwitch)
		{
			bLockReleasedDuringSwitch = false;
			PetSay(INVTEXT("휠을 굴리면 대상이 바껴!"));
		}
		return false;
	}

	// 기준 적이 죽어서 락온이 자동으로 다음 적에 넘어간 건 스크롤이 아님 -> 기준만 갱신
	if (false == IsEnemyAlive(SwitchBaseTarget.Get()))
	{
		SwitchBaseTarget = Target;
		return false;
	}

	// 기준과 다른 적을 잡고 있음 = 휠 스크롤로 바꿈
	return Target != SwitchBaseTarget.Get();
}

FVector ATutorialManager::GetGroundLocation(const AActor* _Actor) const
{
	FVector Location = _Actor->GetActorLocation();

	// 캐릭터는 액터 위치 = 캡슐 중심 -> 캡슐 절반만큼 내려야 발바닥
	// (PetFlyToLocation 이 바닥 기준으로 펫 캡슐 절반을 다시 올려줌)
	if (const ACharacter* Character = Cast<ACharacter>(_Actor))
		Location.Z -= Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	return Location;
}

FVector ATutorialManager::GetPetPointNearPlayer() const
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (nullptr == PlayerPawn)
		return Pet.IsValid() ? GetGroundLocation(Pet.Get()) : GetActorLocation(); // 플레이어가 없으면 제자리

	// 플레이어가 바라보는 방향(수평)으로 PetFrontDistance 만큼 앞
	return GetGroundLocation(PlayerPawn) + PlayerPawn->GetActorForwardVector().GetSafeNormal2D() * PetFrontDistance;
}

FVector ATutorialManager::GetPetPointNearEnemy(const AActor* _Enemy) const
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);

	// 적 -> 플레이어 방향 (플레이어가 없으면 적의 정면)
	const FVector ToPlayer = (nullptr != PlayerPawn)
		? (PlayerPawn->GetActorLocation() - _Enemy->GetActorLocation()).GetSafeNormal2D()
		: _Enemy->GetActorForwardVector().GetSafeNormal2D();

	// 위쪽 축과 외적 -> 수평으로 90도 꺾인 "옆" 방향
	const FVector Side = FVector::CrossProduct(FVector::UpVector, ToPlayer);

	// 플레이어 쪽으로 조금 나오고 옆으로 비켜섬
	// -> 플레이어 눈에 잘 보이면서, 플레이어와 적 사이를 막지 않음 (공격이 펫에 걸리지 않게)
	return GetGroundLocation(_Enemy) + ToPlayer * PetEnemyFrontOffset + Side * PetEnemySideOffset;
}

void ATutorialManager::PetWalkToEnemy(AActor* _Enemy)
{
	if (nullptr == _Enemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] PetFlyToEnemy: 갈 적이 없음 (태그 확인)"));
		return;
	}

	PetFollowedTarget = _Enemy; // "이미 이 적 옆에 있음" 판단용 -> 같은 적이면 다시 날지 않음
	PetWalkToLocation(GetPetPointNearEnemy(_Enemy));
}

void ATutorialManager::StartPetFollowLockTarget()
{
	GetWorldTimerManager().SetTimer(PetFollowTimerHandle, this, &ATutorialManager::UpdatePetFollowLockTarget, 0.1f, true);
	UpdatePetFollowLockTarget(); // 펫이 가리킨 적이 아닌 다른 적을 잠갔다면 바로 그쪽으로
}

void ATutorialManager::UpdatePetFollowLockTarget()
{
	ULockOnComponent* LockOn = GetPlayerLockOn();
	AActor* Target = (nullptr != LockOn) ? LockOn->GetLockedTarget() : nullptr;

	// 락온을 풀었거나, 이미 그 적 옆에 있으면 그대로 둠
	// (잡은 적이 죽으면 락온 컴포넌트가 다음 적으로 바꿔줌 -> 여기서 대상이 달라진 걸 보고 이동)
	if (nullptr == Target || Target == PetFollowedTarget.Get()) return;
	PetWalkToEnemy(Target);
}

void ATutorialManager::StopPetFollowLockTarget()
{
	GetWorldTimerManager().ClearTimer(PetFollowTimerHandle);
	PetFollowedTarget.Reset();
}

void ATutorialManager::RegisterLockOnEnemy(AActor* _Enemy)
{
	if (nullptr == _Enemy) return;

	// 쓰러진 적은 3초 뒤 갑자기 사라짐 -> 사라지는 순간 연기 팡 + (배우기 전이면) 다시 불러내기 예약
	_Enemy->OnEndPlay.AddUniqueDynamic(this, &ATutorialManager::HandleLockOnEnemyEndPlay);

	// 쓰러지는 순간부터 몸에서 연기 (OnDeathStarted 는 다이내믹 델리게이트가 아니라서 AddUObject)
	if (AEnemyCharacter* EnemyChar = Cast<AEnemyCharacter>(_Enemy))
		EnemyChar->OnDeathStarted.AddUObject(this, &ATutorialManager::HandleLockOnEnemyDeathStarted);
}

void ATutorialManager::RespawnLockOnEnemy(int32 _SlotIdx)
{
	// 기다리는 사이 구간이 바뀌었거나 다 배웠으면 안 함
	if (ETutorialStage::LockOn != CurrentStage || true == bLockOnLessonDone) return;
	if (false == LockOnEnemySlots.IsValidIndex(_SlotIdx) || false == LockOnEnemies.IsValidIndex(_SlotIdx)) return;

	const FTutorialEnemySlot& Slot = LockOnEnemySlots[_SlotIdx];
	if (nullptr == Slot.EnemyClass) return;

	// 자리에 플레이어가 서 있어도 옆으로 비켜서 반드시 나타나게
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* NewEnemy = GetWorld()->SpawnActor<AActor>(Slot.EnemyClass, Slot.SpawnTransform, Params);
	if (nullptr == NewEnemy)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] RespawnLockOnEnemy: 적 다시 불러내기 실패"));
		return;
	}

	// 처음 배치한 적과 똑같이 다룸 (태그 + 목록 같은 자리 + 구독)
	NewEnemy->Tags.AddUnique(TutorialTag::EnemyLockOn);
	LockOnEnemies[_SlotIdx] = NewEnemy;
	RegisterLockOnEnemy(NewEnemy);

	// 적 BP 가 "Placed In World" 면 스폰만으로는 AI 가 안 붙음 -> 직접 붙임 (보스와 같은 방식)
	if (APawn* EnemyPawn = Cast<APawn>(NewEnemy))
	{
		if (nullptr == EnemyPawn->GetController())
			EnemyPawn->SpawnDefaultController();
	}

	// 반짝 + 펫이 지금 단계에 맞게 안내
	PlayFXSetAtGround(EnemyRespawnFX, NewEnemy);
	PetSay(true == IsPlayerLockedOn() ? INVTEXT("휠을 굴려서 바꿔 봐!") : INVTEXT("조준부터 해 봐!"));
	LockOnHintSayTime = GetWorld()->GetTimeSeconds(); // 방금 알려줬으니 다시 알려주기 시간 초기화
}

void ATutorialManager::CancelLockOnRespawns()
{
	for (FTutorialEnemySlot& Slot : LockOnEnemySlots)
		GetWorldTimerManager().ClearTimer(Slot.RespawnTimerHandle);
}

void ATutorialManager::HandlePlayerRespawned(AController* _Controller, APawn* _NewPawn)
{
	// 컨트롤러는 리스폰돼도 그대로라, 꺼둔 입력이 남으면 새 캐릭터도 조작 불가 -> 강제 해제
	EndCinematic(0.f);

	ApplyTutorialCamera(_NewPawn); // 새 캐릭터는 락온 줌아웃 값이 기본값(700)이라 다시 줄여줌

	// 보스전 중에 죽었으면 -> 튜토리얼 처음(PlayerStart)이 아니라 보스방 입구에서 다시
	// (이 함수는 리스폰 화면 전환으로 화면이 어두운 동안 불림 -> 순간이동이 안 보임)
	if (ETutorialStage::Combat == CurrentStage && true == bInBossFight)
	{
		if (AActor* RetryPoint = FindActorByTag(TutorialTag::PointBossRetry))
		{
			// 지점은 바닥에 놓음 -> 캡슐 반높이만큼 올려서 세움
			const ACharacter* PlayerChar = Cast<ACharacter>(_NewPawn);
			const float HalfHeight = (nullptr != PlayerChar) ? PlayerChar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 90.f;
			const FRotator Facing(0.f, RetryPoint->GetActorRotation().Yaw, 0.f);

			_NewPawn->TeleportTo(RetryPoint->GetActorLocation() + FVector(0.f, 0.f, HalfHeight), Facing);
			if (nullptr != _Controller) _Controller->SetControlRotation(Facing); // 카메라도 아레나 쪽을 보게
		}

		RestartBossFight();
	}
}

void ATutorialManager::RestartBossFight()
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] 보스전 재시작"));

	bBossRetry = true;
	bInBossFight = false;
	bBossHealHintShown = false; // 재도전 때도 체력이 낮아지면 다시 알려줌

	// 남아 있는 보스 제거 (펫은 StageEnd 의 RestorePetForm 에서 원래 크기 + 보이게 돌아옴)
	if (AEnemyCharacter* BossPtr = Boss.Get()) BossPtr->Destroy();
	Boss.Reset();
	StopLoopFX();

	RefillPlayerPotions(); // 재도전은 포션 가득으로 공정하게

	// 같은 구간이라 ChangeStage 는 무시됨 -> 정리 후 직접 다시 시작
	// (bBossRetry 라서 통로 단계는 건너뛰고 보스방 입장부터)
	StageEnd(ETutorialStage::Combat);
	StageStart(ETutorialStage::Combat);
}

void ATutorialManager::ApplyTutorialCamera(APawn* _PlayerPawn)
{
	if (nullptr == _PlayerPawn) return;

	ULockOnComponent* LockOn = _PlayerPawn->FindComponentByClass<ULockOnComponent>();
	if (nullptr == LockOn) return;

	// 튜토리얼 맵은 천장이 낮은 실내
	// -> 락온하면 카메라가 최대 +700cm 뒤로 빠지면서 방 밖의 빈 공간(검은색)이 보임
	// 카메라 붐 충돌은 천장에 계속 걸려서 화면이 들쭉날쭉 -> 대신 "얼마나 멀어질지" 최대치를 줄임
	//
	// MaxExtraArmLength 는 LockOnComponent(팀원 코드)의 protected 값이라 setter 가 없음
	// -> 리플렉션으로 이 레벨의 플레이어 인스턴스 값만 바꿈 (BP 기본값, 다른 레벨은 그대로)
	//? 나중에 LockOnComponent 에 setter 가 생기면 그걸로 교체
	if (FFloatProperty* Prop = FindFProperty<FFloatProperty>(ULockOnComponent::StaticClass(), TEXT("MaxExtraArmLength")))
	{
		Prop->SetPropertyValue_InContainer(LockOn, TutorialLockOnMaxExtraArm);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] LockOnComponent 에 MaxExtraArmLength 가 없음 (이름 바뀜?)"));
	}
}

void ATutorialManager::RefillPlayerHealth()
{
	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerPawn);
	if (nullptr == ASC || false == ASC->HasAttributeSetForAttribute(UPlayerAttributeSet::GetHealthAttribute()))
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] RefillPlayerHealth: 플레이어 ASC/체력 없음"));
		return;
	}

	// 최대 목숨 숫자를 직접 쓰지 않고 MaxHealth 를 읽음 -> 나중에 최대치가 바뀌어도 그대로 맞음
	// 체력이 바뀌면 HUD(목숨 구슬)는 알아서 갱신됨 (MainWidget 이 체력 변화를 구독 중)
	const float MaxHealth = ASC->GetNumericAttribute(UPlayerAttributeSet::GetMaxHealthAttribute());
	ASC->SetNumericAttributeBase(UPlayerAttributeSet::GetHealthAttribute(), MaxHealth);
}

void ATutorialManager::SetActorShown(AActor* _Actor, bool _bShow)
{
	if (nullptr == _Actor)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] SetActorShown: 액터 없음 (태그 확인)"));
		return;
	}

	// 숨길 때 충돌도 같이 꺼야 안 보이는 체크포인트에 상호작용되지 않음
	_Actor->SetActorHiddenInGame(false == _bShow);
	_Actor->SetActorEnableCollision(_bShow);
}

void ATutorialManager::SetPetHidden(bool _bHidden)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	bPetHidden = _bHidden;
	PetPtr->SetActorHiddenInGame(_bHidden); // 말풍선 컴포넌트도 같이 숨겨짐
	PetPtr->SetActorEnableCollision(false == _bHidden);

	if (true == _bHidden)
	{
		// 충돌이 꺼진 채 걷기 모드면 바닥을 뚫고 떨어짐 -> 이동 자체를 멈춤
		if (AAIController* AIC = Cast<AAIController>(PetPtr->GetController()))
			AIC->StopMovement();
		PetPtr->GetCharacterMovement()->DisableMovement();
	}
	else
		PetPtr->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ATutorialManager::SetPetMeshScale(float _Scale)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	USkeletalMeshComponent* Mesh = PetPtr->GetMesh();

	// 처음 한 번 원래 크기를 기억 (배율은 이 크기 기준)
	if (false == bPetMeshBaseSaved)
	{
		PetMeshBaseScale = Mesh->GetRelativeScale3D();
		bPetMeshBaseSaved = true;
	}

	// 캡슐은 그대로 두고 "메시만" 키움
	// -> 캡슐까지 키우면 벽/천장에 끼거나 밀려나서 자리가 틀어짐. 어차피 곧 보스로 바뀌니 겉모습만 커지면 충분
	Mesh->SetRelativeScale3D(PetMeshBaseScale * _Scale);
}

void ATutorialManager::StartPetMeshScale(float _TargetScale, float _Duration)
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	SetPetMeshScale(PetScaleFrom = (true == bPetMeshBaseSaved)
		? PetPtr->GetMesh()->GetRelativeScale3D().X / PetMeshBaseScale.X // 지금 배율에서 시작
		: 1.f);

	PetScaleTo = _TargetScale;
	PetScaleDuration = FMath::Max(_Duration, KINDA_SMALL_NUMBER);
	PetScaleElapsed = 0.f;
	bPetScaling = true;
	SetActorTickEnabled(true); // 진행은 Tick 에서 (끝나면 Tick 이 알아서 꺼짐)
}

void ATutorialManager::RestorePetForm()
{
	bPetScaling = false;
	if (true == bPetMeshBaseSaved) SetPetMeshScale(1.f);
	if (true == bPetHidden) SetPetHidden(false);
}

void ATutorialManager::SwapPetToBoss()
{
	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr || nullptr == BossClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] SwapPetToBoss: 펫 또는 BossClass 없음 (매니저에 BossClass 지정했는지 확인)"));
		return;
	}

	// 펫이 서 있던 바닥 + 펫이 보던 방향 그대로 보스를 세움 -> 커진 펫과 보스가 같은 자리/방향이라 바뀐 티가 안 남
	const FVector Ground = GetGroundLocation(PetPtr);
	const FRotator Rotation(0.f, PetPtr->GetActorRotation().Yaw, 0.f);

	// 보스 캡슐 중심 = 바닥 + 캡슐 반높이 (BP 기본값에서 읽음 -> 보스 크기를 바꿔도 코드 수정 없음)
	const AEnemyCharacter* BossCDO = BossClass->GetDefaultObject<AEnemyCharacter>();
	const float HalfHeight = BossCDO->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// 보스 BP 의 Auto Possess AI 가 "Placed In World" 라서 스폰만으로는 AI 가 안 붙음
	// -> 등장 연출 동안 가만히 서 있고, StartBossAI() 에서 AI 를 붙여 전투 시작
	Boss = GetWorld()->SpawnActor<AEnemyCharacter>(BossClass, Ground + FVector(0.f, 0.f, HalfHeight), Rotation, Params);
	if (false == Boss.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] SwapPetToBoss: 보스 스폰 실패"));
		return;
	}

	SetPetHidden(true);
}

void ATutorialManager::StartBossAI()
{
	AEnemyCharacter* BossPtr = Boss.Get();
	if (nullptr == BossPtr) return;

	// AI 컨트롤러가 붙는 순간 비헤이비어 트리가 돌기 시작함
	if (nullptr == BossPtr->GetController()) BossPtr->SpawnDefaultController();

	// 보스는 시야 이벤트로만 타깃을 잡는데, 플레이어가 가만히 서 있으면 이벤트가 안 와서 타깃이 빈 채로 멈출 수 있음
	// -> 튜토리얼 보스는 상대가 정해져 있으니 플레이어를 바로 타깃으로 넣어 줌
	// (SpawnDefaultController -> OnPossess 에서 블랙보드가 이미 준비돼 있어서 바로 넣어도 됨)
	if (AEnemyAIController* AIC = Cast<AEnemyAIController>(BossPtr->GetController()))
		AIC->SetCombatTarget(UGameplayStatics::GetPlayerPawn(this, 0));
}

void ATutorialManager::SwapBossToPet(AActor* _AppearPoint, AActor* _FaceTarget)
{
	APetCharacter* PetPtr = Pet.Get();
	AEnemyCharacter* BossPtr = Boss.Get();

	if (nullptr != PetPtr)
	{
		// 펫 캡슐 중심 = 바닥 + 펫 캡슐 반높이
		const FVector HalfUp(0.f, 0.f, PetPtr->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

		// 세울 위치에서 _FaceTarget(카메라) 쪽을 보는 방향 (높이 차이는 무시하고 수평만)
		// -> 거대한 펫이 줄어드는 모습이 카메라에 정면으로 잡힘. 대상이 없으면 기본 방향 사용
		auto GetFaceYaw = [_FaceTarget](const FVector& _From, float _DefaultYaw) -> float // 반환 타입 명시 (UE5 Yaw 는 double 이라 float 와 섞이면 추론 실패)
			{
				if (nullptr == _FaceTarget) return _DefaultYaw;
				const FVector ToTarget = (_FaceTarget->GetActorLocation() - _From).GetSafeNormal2D();
				return ToTarget.IsNearlyZero() ? _DefaultYaw : static_cast<float>(ToTarget.Rotation().Yaw);
			};

		// 보스가 어디서 죽든 처음 등장했던 자리에 세움 -> 고정된 아레나 카메라에 항상 잡힘
		bool bPlaced = false;
		if (nullptr != _AppearPoint)
		{
			const FVector Location = GetGroundLocation(_AppearPoint) + HalfUp;
			bPlaced = PetPtr->TeleportTo(Location, FRotator(0.f, GetFaceYaw(Location, _AppearPoint->GetActorRotation().Yaw), 0.f));
		}

		// 지점이 없거나, 플레이어가 그 자리에 서 있어서 순간이동이 막히면 -> 보스가 쓰러진 자리에
		if (false == bPlaced && nullptr != BossPtr)
		{
			const FVector Location = GetGroundLocation(BossPtr) + HalfUp;
			PetPtr->TeleportTo(Location, FRotator(0.f, GetFaceYaw(Location, BossPtr->GetActorRotation().Yaw), 0.f));
		}
	}

	// 보스와 같은 크기로 나타났다가 -> 이후 StartPetMeshScale 로 줄어듦
	SetPetMeshScale(BossMeshScale);
	SetPetHidden(false);

	// 보스는 바로 제거 (래그돌로 쓰러지는 모습이 보이지 않게)
	if (nullptr != BossPtr) BossPtr->Destroy();
	Boss.Reset();
}

bool ATutorialManager::IsBossDead() const
{
	// 락온 구간과 같은 기준 (체력 0 이 되는 순간 Dying 으로 바뀌어 false)
	return false == IsEnemyAlive(Boss.Get());
}

float ATutorialManager::GetPlayerHealthRatio() const
{
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(UGameplayStatics::GetPlayerPawn(this, 0));
	if (nullptr == ASC || false == ASC->HasAttributeSetForAttribute(UPlayerAttributeSet::GetHealthAttribute())) return 1.f;

	const float MaxHealth = ASC->GetNumericAttribute(UPlayerAttributeSet::GetMaxHealthAttribute());
	if (MaxHealth <= 0.f) return 1.f;

	return ASC->GetNumericAttribute(UPlayerAttributeSet::GetHealthAttribute()) / MaxHealth;
}

int32 ATutorialManager::GetPlayerPotionCount() const
{
	const APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0));
	return (nullptr != Player) ? Player->GetPotionCount() : 0;
}

void ATutorialManager::RefillPlayerPotions()
{
	if (APlayerCharacter* Player = Cast<APlayerCharacter>(UGameplayStatics::GetPlayerPawn(this, 0)))
		Player->RefillPotions(); // HUD 포션 개수도 알아서 갱신됨 (OnPotionCountChanged)
}

void ATutorialManager::BossSay(const FText& _Message)
{
	UE_LOG(LogTemp, Log, TEXT("[Tutorial] 보스: %s"), *_Message.ToString());

	// 보스전 중엔 펫이 숨어 있어서 펫 말풍선을 못 씀 -> 보스 BP 에 붙인 말풍선 사용
	AEnemyCharacter* BossPtr = Boss.Get();
	USpeechBubbleComponent* Bubble = (nullptr != BossPtr) ? BossPtr->FindComponentByClass<USpeechBubbleComponent>() : nullptr;
	if (nullptr == Bubble)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Tutorial] BossSay: 보스에 SpeechBubble 컴포넌트가 없음"));
		return;
	}

	Bubble->Say(_Message, 4.f);
	if (nullptr != PetSaySound)
		UGameplayStatics::PlaySound2D(this, PetSaySound);
}

void ATutorialManager::CheckBossHealHint()
{
	if (true == bBossHealHintShown || true == IsBossDead()) return;
	if (GetPlayerHealthRatio() > BossHealHintRatio || GetPlayerPotionCount() <= 0) return;

	// 처음으로 체력이 낮아진 순간 딱 한 번 (계속 뜨면 거슬림)
	// "공격 끝나면" -> 마시는 동안 느려지니까 보스 공격 사이에 마셔야 한다는 것까지 알려줌
	bBossHealHintShown = true;
	BossSay(INVTEXT("아프지? 공격 끝나면 R로 포션 마셔!"));
}

void ATutorialManager::PlayFXSetAtGround(const FTutorialFXSet& _Set, AActor* _Actor)
{
	//if (nullptr == _Actor) return;

	//// 캐릭터는 캡슐 중심이 아니라 발바닥 기준 -> 바닥에서 터지고, 소리도 그 자리에서 남
	//const FVector Location = GetGroundLocation(_Actor);
	//if (nullptr != _Set.Effect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, _Set.Effect, Location);
	//if (nullptr != _Set.Sound) UGameplayStatics::PlaySoundAtLocation(this, _Set.Sound, Location, _Set.VolumeMultiplier);

	//PlayCameraShake(_Set.CameraShake, _Set.ShakeScale);

	// 캐릭터는 캡슐 중심이 아니라 발바닥 기준 -> 바닥에서 터지고, 소리도 그 자리에서 남
	PlayFXSetAtLocation(_Set, GetGroundLocation(_Actor));
}

void ATutorialManager::StartLoopFXOnPet(const FTutorialFXSet& _Set)
{
	StopLoopFX(); // 이전 반복 효과가 남아 있으면 먼저 정리 (두 번 겹쳐 재생되지 않게)

	APetCharacter* PetPtr = Pet.Get();
	if (nullptr == PetPtr) return;

	// 펫 캡슐에 붙이고 캡슐 반높이만큼 내림 -> 발밑 기준으로 재생되고, 펫이 움직여도 따라감
	USceneComponent* Root = PetPtr->GetRootComponent();
	const FVector FootOffset(0.f, 0.f, -PetPtr->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());

	if (nullptr != _Set.Effect)
	{
		// 캡슐이 아니라 "메시"에 붙임 -> 메시가 커지면 효과 범위도 같이 커짐 (캡슐은 크기가 그대로라서)
		// 메시 원점 = 발바닥이라 따로 내릴 필요 없음
		LoopFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			_Set.Effect, PetPtr->GetMesh(), NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true); // true = 끝나면 컴포넌트 자동 삭제
	}

	if (nullptr != _Set.Sound)
	{
		// true = 펫이 사라지면 소리도 같이 멈춤
		LoopSoundComp = UGameplayStatics::SpawnSoundAttached(
			_Set.Sound, Root, NAME_None, FootOffset,
			EAttachLocation::KeepRelativeOffset, true, _Set.VolumeMultiplier);
	}

	PlayCameraShake(_Set.CameraShake, _Set.ShakeScale);
}

void ATutorialManager::StopLoopFX()
{
	// 효과는 바로 지우지 않고 "새 입자만" 멈춤 -> 이미 나온 입자는 자연스럽게 사라짐
	if (nullptr != LoopFXComp)
	{
		LoopFXComp->Deactivate();
		LoopFXComp = nullptr;
	}

	// 소리는 뚝 끊기지 않게 짧게 줄이면서 끔
	if (nullptr != LoopSoundComp)
	{
		LoopSoundComp->FadeOut(0.2f, 0.f);
		LoopSoundComp = nullptr;
	}
}

void ATutorialManager::PlayCameraShake(TSubclassOf<UCameraShakeBase> _Shake, float _Scale)
{
	if (nullptr == _Shake) return;

	// 카메라 매니저가 "지금 보고 있는 카메라"를 흔듦 -> 아레나 연출 카메라로 보고 있어도 흔들림
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		PC->ClientStartCameraShake(_Shake, _Scale);
}

void ATutorialManager::PlayFXSetAtLocation(const FTutorialFXSet& _Set, const FVector& _Location)
{
	if (nullptr != _Set.Effect)
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, _Set.Effect, _Location);

	if (nullptr != _Set.Sound)
		UGameplayStatics::PlaySoundAtLocation(this, _Set.Sound, _Location, _Set.VolumeMultiplier);

	PlayCameraShake(_Set.CameraShake, _Set.ShakeScale);
}

void ATutorialManager::HandleLockOnEnemyEndPlay(AActor* _Actor, EEndPlayReason::Type _Reason)
{
	// 레벨이 끝나서 정리되는 경우는 제외 (실제로 처치돼서 사라질 때만)
	if (EEndPlayReason::Destroyed != _Reason || nullptr == _Actor) return;

	// 시체는 래그돌이라 캡슐 자리에서 굴러가 있음 -> 메시(몸) 위치에서 터뜨려야 "그 자리에서 사라짐"으로 보임
	FVector Location = _Actor->GetActorLocation();
	if (const ACharacter* Character = Cast<ACharacter>(_Actor))
		Location = Character->GetMesh()->GetComponentLocation();

	PlayFXSetAtLocation(EnemyVanishFX, Location);

	// 배우기 전에 사라졌으면 -> 같은 자리에 다시 불러내기 예약
	if (ETutorialStage::LockOn != CurrentStage || true == bLockOnLessonDone) return;

	// 사라지는 중인 액터라 일반 Get() 은 nullptr 일 수 있음 -> Get(true) 로 비교
	const int32 SlotIndex = LockOnEnemies.IndexOfByPredicate([_Actor](const TWeakObjectPtr<AActor>& Enemy)
		{
			return Enemy.Get(true) == _Actor;
		});
	if (false == LockOnEnemySlots.IsValidIndex(SlotIndex)) return;

	// 시간이 0 이면 SetTimer 가 타이머를 지워 버림 -> 아주 작은 값이라도 넣음
	const FTimerDelegate Delegate = FTimerDelegate::CreateUObject(this, &ATutorialManager::RespawnLockOnEnemy, SlotIndex);
	GetWorldTimerManager().SetTimer(LockOnEnemySlots[SlotIndex].RespawnTimerHandle, Delegate,
		FMath::Max(LockOnRespawnDelay, 0.01f), false);
}

void ATutorialManager::HandleLockOnEnemyDeathStarted(AEnemyCharacter* _Enemy)
{
	if (nullptr == _Enemy) return;

	USkeletalMeshComponent* Mesh = _Enemy->GetMesh();

	// 메시에 붙임 -> 래그돌로 굴러가도 몸을 따라가고, 적이 사라질 때 같이 사라짐 (따로 끌 필요 없음)
	if (nullptr != EnemyDyingFX.Effect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			EnemyDyingFX.Effect, Mesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, true);
	}

	if (nullptr != EnemyDyingFX.Sound)
		UGameplayStatics::PlaySoundAtLocation(this, EnemyDyingFX.Sound, Mesh->GetComponentLocation(), EnemyDyingFX.VolumeMultiplier);
}
