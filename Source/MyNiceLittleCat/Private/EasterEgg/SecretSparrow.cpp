/// 작성자: 우진
/// 작성일자: 2026-11-09
///
/// 이스터에그 참새 구현

#include "EasterEgg/SecretSparrow.h"
#include "EasterEgg/EasterEggChest.h"
#include "Gimmicks/Stage/GimmickChest.h"
#include "Pet/PetCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "TimerManager.h"
#include "Engine/World.h"

ASecretSparrow::ASecretSparrow()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SparrowMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SparrowMesh"));
	SparrowMesh->SetupAttachment(SceneRoot);
	SparrowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SparrowMesh->SetGenerateOverlapEvents(false);

	// 9 → 2 → 2 ("구~ 이~ 이~")
	SecretCode = { 9, 2, 2 };

	HintText = NSLOCTEXT("EasterEgg", "SparrowHint", "오... 시작을 기억하는 참새인가봐!");
	PetReactionText = NSLOCTEXT("EasterEgg", "SparrowReaction", "잘 가! 처음 만난 날처럼 또 만나자!");
}

void ASecretSparrow::BeginPlay()
{
	Super::BeginPlay();

	PlayAnim(IdleAnim, true);

	// 상자가 없거나 이미 열린 채로 시작하면(세이브) 바로 대기
	if (!Chest || Chest->IsOpened())
	{
		EnterWaiting();
	}
	else if (bHideWhileChestClosed)
	{
		SetActorHiddenInGame(true);
	}
}

void ASecretSparrow::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindCodeInput();
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void ASecretSparrow::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	StateElapsed += DeltaSeconds;

	switch (State)
	{
	case ESecretSparrowState::InClosedChest:
		// 뚜껑이 열리기 시작하면 (저장 상태와 상관없이 IsOpened가 true) 등장
		if (!Chest || Chest->IsOpened())
		{
			EnterWaiting();
		}
		break;
	case ESecretSparrowState::Waiting:
		TickWaiting(DeltaSeconds);
		break;
	case ESecretSparrowState::Circling:
		TickCircling(DeltaSeconds);
		break;
	case ESecretSparrowState::FlyingAway:
		TickFlyingAway(DeltaSeconds);
		break;
	default:
		break;
	}
}

// =====================================================================
// 상태 진행
// =====================================================================

void ASecretSparrow::EnterWaiting()
{
	State = ESecretSparrowState::Waiting;
	StateElapsed = 0.f;

	SetActorHiddenInGame(false);
	PlayAnim(IdleAnim, true);
	ScheduleIdleHop();
	BindCodeInput();
}

void ASecretSparrow::TickWaiting(float DeltaSeconds)
{
	// 시작 직후 플레이어 컨트롤러가 아직 없었으면 여기서 다시 연결
	if (!CodeInput)
	{
		BindCodeInput();
	}

	// 플레이어가 근처면 플레이어 쪽을 봄 (살아 있는 느낌)
	APawn* Player = GetPlayerPawn();
	if (Player && LookAtPlayerRadius > 0.f && IsPlayerInRange(LookAtPlayerRadius))
	{
		FaceDirection(Player->GetActorLocation() - GetActorLocation(), DeltaSeconds, false);
	}

	// 힌트: 플레이어가 반경에 "들어올" 때마다 한 번 (상자가 열린 직후엔 뚜껑 연출 뒤로 조금 미룸)
	const bool bInHintRadius = IsPlayerInRange(HintTriggerRadius);
	if (!bInHintRadius)
	{
		bPlayerWasInHintRadius = false;
	}
	else if (!bPlayerWasInHintRadius
		&& StateElapsed >= HintDelayAfterOpen
		&& GetWorld()->GetTimeSeconds() >= NextHintAllowedTime)
	{
		// 멀어졌다가 쿨타임 뒤 다시 오면 또 말함. 괴냥이는 따라다니는 중이라 플레이어 근처에서 말풍선이 뜸
		bPlayerWasInHintRadius = true;
		NextHintAllowedTime = GetWorld()->GetTimeSeconds() + HintCooldown;
		AEasterEggChest::PetSay(this, HintText, HintDuration);
	}
}

void ASecretSparrow::TickCircling(float DeltaSeconds)
{
	// 깡총 동작 동안은 제자리
	if (StateElapsed < TakeOffDelay)
	{
		return;
	}

	// 플레이어가 움직여도 따라가며 머리 위를 돎
	if (const APawn* Player = GetPlayerPawn())
	{
		LastCircleCenter = Player->GetActorLocation();
	}

	const float StepDegrees = CircleDegreesPerSecond * DeltaSeconds;
	CircleAngle += StepDegrees;
	CircledDegrees += StepDegrees;

	const float Rad = FMath::DegreesToRadians(CircleAngle);
	const FVector Target = LastCircleCenter + FVector(FMath::Cos(Rad) * CircleRadius, FMath::Sin(Rad) * CircleRadius, CircleHeight);

	// 궤도로 바로 순간이동하지 않고 부드럽게 따라붙음 → 상자에서 날아오르는 모습이 자연스럽게 나옴
	const FVector Current = GetActorLocation();
	const FVector NewLocation = FMath::VInterpTo(Current, Target, DeltaSeconds, CircleFollowSpeed);
	FaceDirection(NewLocation - Current, DeltaSeconds, true);
	SetActorLocation(NewLocation);

	if (CircledDegrees >= CircleLaps * 360.f)
	{
		// 바깥쪽 + 위로 떠남
		const FVector Outward = (NewLocation - LastCircleCenter).GetSafeNormal2D();
		FlyAwayDirection = (Outward + FVector::UpVector * 1.2f).GetSafeNormal();
		FlyAwaySpeed = FlyAwayStartSpeed;

		State = ESecretSparrowState::FlyingAway;
		StateElapsed = 0.f;
	}
}

void ASecretSparrow::TickFlyingAway(float DeltaSeconds)
{
	FlyAwaySpeed += FlyAwayAcceleration * DeltaSeconds;

	const FVector Delta = FlyAwayDirection * FlyAwaySpeed * DeltaSeconds;
	FaceDirection(Delta, DeltaSeconds, true);
	SetActorLocation(GetActorLocation() + Delta);

	if (StateElapsed >= FlyAwayDuration)
	{
		FinishFlyAway();
	}
}

void ASecretSparrow::FinishFlyAway()
{
	State = ESecretSparrowState::Gone;
	K2_OnFlownAway();
	UE_LOG(LogTemp, Log, TEXT("[SecretSparrow] 참새가 날아갔습니다"));
	Destroy();
}

void ASecretSparrow::TriggerSecret()
{
	if (State != ESecretSparrowState::Waiting)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SecretSparrow] 비밀 커맨드 성공!"));

	State = ESecretSparrowState::Circling;
	StateElapsed = 0.f;
	CodeProgress = 0;
	UnbindCodeInput();
	GetWorldTimerManager().ClearTimer(IdleHopTimer);

	// 깡총 → (TakeOffDelay 뒤) 날갯짓
	PlayAnim(HopAnim, false);
	GetWorldTimerManager().SetTimer(AnimReturnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		PlayAnim(FlyAnim, true);
	}), FMath::Max(TakeOffDelay, KINDA_SMALL_NUMBER), false);

	// 선회 시작 각도 = 플레이어에서 본 참새 방향 → 참새 쪽에서 원을 그리기 시작
	if (const APawn* Player = GetPlayerPawn())
	{
		LastCircleCenter = Player->GetActorLocation();
		const FVector FromPlayer = GetActorLocation() - LastCircleCenter;
		CircleAngle = FMath::RadiansToDegrees(FMath::Atan2(FromPlayer.Y, FromPlayer.X));
	}
	else
	{
		LastCircleCenter = GetActorLocation();
		CircleAngle = 0.f;
	}
	CircledDegrees = 0.f;

	// 반짝·소리·괴냥이 반응
	if (SecretEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SecretEffect, GetActorLocation());
	}
	if (SecretSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, SecretSound, GetActorLocation());
	}
	AEasterEggChest::PetSay(this, PetReactionText, 2.5f);

	K2_OnSecretTriggered();
}

// =====================================================================
// 커맨드 입력
// =====================================================================

void ASecretSparrow::BindCodeInput()
{
	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController || CodeInput)
	{
		return;
	}

	// 아무 키나 받아서 숫자만 골라냄. 다른 입력은 막지 않음 (이동·점프 등 그대로)
	CodeInput = NewObject<UInputComponent>(this, TEXT("SecretSparrowCodeInput"));
	CodeInput->bBlockInput = false;
	FInputKeyBinding& Binding = CodeInput->BindKey(EKeys::AnyKey, IE_Pressed, this, &ASecretSparrow::HandleAnyKey);
	Binding.bConsumeInput = false;

	PlayerController->PushInputComponent(CodeInput);
	CodeInputController = PlayerController;
}

void ASecretSparrow::UnbindCodeInput()
{
	if (CodeInputController.IsValid() && CodeInput)
	{
		CodeInputController->PopInputComponent(CodeInput);
	}
	CodeInputController.Reset();
	CodeInput = nullptr;
}

void ASecretSparrow::HandleAnyKey(FKey Key)
{
	if (State != ESecretSparrowState::Waiting || SecretCode.Num() == 0)
	{
		return;
	}

	// 숫자가 아닌 키(이동 등)는 무시 → 걸으면서 눌러도 됨
	const int32 Digit = KeyToDigit(Key);
	if (Digit == INDEX_NONE)
	{
		return;
	}

	// 참새 근처가 아니면 무효
	if (!IsPlayerInRange(CodeListenRadius))
	{
		CodeProgress = 0;
		return;
	}

	// 너무 늦게 이어 누르면 처음부터
	const double Now = GetWorld()->GetTimeSeconds();
	if (CodeProgress > 0 && Now - LastCodeKeyTime > CodeKeyInterval)
	{
		CodeProgress = 0;
	}
	LastCodeKeyTime = Now;

	if (SecretCode.IsValidIndex(CodeProgress) && Digit == SecretCode[CodeProgress])
	{
		++CodeProgress;
	}
	else
	{
		// 틀렸어도 그 숫자가 첫 숫자면 새로 시작한 것으로 인정 (9, 9, 2, 2도 성공)
		CodeProgress = (Digit == SecretCode[0]) ? 1 : 0;
	}

	if (CodeProgress > 0)
	{
		K2_OnCodeProgress(CodeProgress);
	}

	if (CodeProgress >= SecretCode.Num())
	{
		TriggerSecret();
	}
}

int32 ASecretSparrow::KeyToDigit(const FKey& Key)
{
	// 상단 숫자키와 넘패드를 같은 숫자로
	static const FKey TopRowKeys[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	static const FKey NumPadKeys[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour,
		EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };

	for (int32 Index = 0; Index < 10; ++Index)
	{
		if (Key == TopRowKeys[Index] || Key == NumPadKeys[Index])
		{
			return Index;
		}
	}
	return INDEX_NONE;
}

// =====================================================================
// 애니메이션
// =====================================================================

void ASecretSparrow::PlayAnim(UAnimSequence* Anim, bool bLoop)
{
	if (Anim && SparrowMesh)
	{
		SparrowMesh->PlayAnimation(Anim, bLoop);
	}
}

void ASecretSparrow::ScheduleIdleHop()
{
	if (!HopAnim)
	{
		return;
	}

	const float Delay = FMath::FRandRange(IdleHopIntervalMin, FMath::Max(IdleHopIntervalMin, IdleHopIntervalMax));
	GetWorldTimerManager().SetTimer(IdleHopTimer, this, &ASecretSparrow::PlayIdleHop, Delay, false);
}

void ASecretSparrow::PlayIdleHop()
{
	if (State != ESecretSparrowState::Waiting)
	{
		return;
	}

	// 깡총 한 번 → 끝나면 Idle로 돌아가고 다음 깡총 예약
	PlayAnim(HopAnim, false);
	const float HopLength = HopAnim ? FMath::Max(HopAnim->GetPlayLength(), 0.1f) : 0.1f;
	GetWorldTimerManager().SetTimer(AnimReturnTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (State == ESecretSparrowState::Waiting)
		{
			PlayAnim(IdleAnim, true);
			ScheduleIdleHop();
		}
	}), HopLength, false);
}

// =====================================================================
// 유틸
// =====================================================================

APawn* ASecretSparrow::GetPlayerPawn() const
{
	return UGameplayStatics::GetPlayerPawn(this, 0);
}

bool ASecretSparrow::IsPlayerInRange(float Radius) const
{
	const APawn* Player = GetPlayerPawn();
	return Player && FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= Radius;
}

void ASecretSparrow::FaceDirection(const FVector& Direction, float DeltaSeconds, bool bAllowPitch)
{
	if (Direction.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		return;
	}

	FRotator Target = Direction.Rotation();
	Target.Roll = 0.f;
	// 날 때만 살짝 기울임 (너무 꺾이면 부자연스러워 ±30도 제한)
	Target.Pitch = bAllowPitch ? FMath::Clamp(Target.Pitch, -30.f, 30.f) : 0.f;

	SetActorRotation(FMath::RInterpTo(GetActorRotation(), Target, DeltaSeconds, 8.f));
}
