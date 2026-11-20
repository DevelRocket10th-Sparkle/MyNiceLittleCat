// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyCharacter.h"
#include "Enemy/EnemyLog.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Enemy/EnemyCharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#include "AbilitySystemComponent.h"
#include "Enemy/GAS/EnemyAttributeSet.h"
#include "Enemy/Data/EnemyConfigData.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "AIController.h"
#include "BrainComponent.h"
#include "Enemy/AI/EnemyAIController.h"
#include "Enemy/Components/EnemyPatternComponent.h"
#include "Enemy/Components/EnemyMeleeTraceComponent.h"
#include "Enemy/Components/EnemyOverheadHealthBarComponent.h"
#include "Enemy/Systems/EnemyActivitySubsystem.h"

// 캐릭터의 기본 상태를 설정한다.
AEnemyCharacter::AEnemyCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UEnemyCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// 캐릭터의 프레임 갱신을 활성화한다.
	PrimaryActorTick.bCanEverTick = true;

	// 최적화 (2026.11.20., 전선현): 화면에 안 보이면 몽타주만 갱신 (멀리 배치된 적의 애니메이션 비용 절감, 보스는 ABossCharacter에서 항상 갱신)
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;

	/* 컴포넌트 추가 */

	// GAS 관련
	ASC = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("ASC"));
	AttributeSet = CreateDefaultSubobject<UEnemyAttributeSet>(TEXT("Stat"));

	// 전투 관련
	PatternComponent = CreateDefaultSubobject<UEnemyPatternComponent>(TEXT("PatternComponent"));
	MeleeTraceComponent = CreateDefaultSubobject<UEnemyMeleeTraceComponent>(TEXT("MeleeTraceComponent"));

	/* 기본 회전 속도 제한 */

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;
	
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = false;		// 이동 방향으로 자동 회전하지 않음
	Movement->bUseControllerDesiredRotation = true;		// 컨트롤러가 바라보는 방향으로 회전
	Movement->RotationRate = FRotator(0.f, 120.f, 0.f);	// 초당 최대 수평 회전 각도

	// 루트 모션 재생 중에도 컨트롤러 방향으로 회전할 수 있도록 허용
	Movement->bAllowPhysicsRotationDuringAnimRootMotion = true;

}

// Getter: 이 캐릭터의 능력 실행 컴포넌트
UAbilitySystemComponent* AEnemyCharacter::GetAbilitySystemComponent() const
{
	return ASC;
}

bool AEnemyCharacter::IsTargetAlive_Implementation() const
{
	return IsAlive();
}

// Health 변경 전에 무적 및 페이즈 체력 하한을 반영
float AEnemyCharacter::CalculateHealthAfterDamage(float CurrentHealth, float MaxHealth, float Damage) const
{
	if (!IsAlive())
	{
		return CurrentHealth;
	}

	return FMath::Clamp(CurrentHealth - FMath::Max(0.f, Damage), 0.f, MaxHealth);
}

// 게임 시작 또는 스폰 시 초기화
// 컴포넌트 초기화 후 실행 준비
void AEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (IsValid(MeshComp))
	{
		// 생존 중에는 애니메이션으로만 메시 제어
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetAllBodiesSimulatePhysics(false);
		MeshComp->SetAllBodiesPhysicsBlendWeight(0.f);
	}

	if (IsValid(ASC))
	{
		// 컴포넌트 초기화 순서와 무관하게 능력치 적용 전에 AttributeSet 등록 보장
		ASC->AddAttributeSetSubobject(AttributeSet.Get());
		ASC->InitAbilityActorInfo(this, this);	// GAS 소유자와 실제 행동 소유자 일치
		
		// Attributes 초기화
		if (EnemyConfig)
		{
			ApplyEnemyConfig();
		}
	}
	
	// Health 변경 델리게이트 구독
	AttributeSet->OnHealthChange.AddUObject(this, &AEnemyCharacter::HandleHealthChanged);
	
	if (UEnemyCharacterMovementComponent* Movement = Cast<UEnemyCharacterMovementComponent>(GetCharacterMovement()))
	{
		Movement->InitializeHoverHeight();
	}

	// 공격은 잡몹과 보스 모두 패턴 컴포넌트에서 실행
	if (HasAuthority() && EnemyConfig && EnemyConfig->PatternSet
		&& !PatternComponent->InitializePatterns(ASC, EnemyConfig->PatternSet))
	{
		UE_LOG(LogEnemy, Error, TEXT("%s: pattern initialization failed"), *GetName());
		return;
	}
	// 보스 여부와 관계없이 데이터 에셋의 설정을 개체별 실행 컴포넌트에 전달한다.
	if (HasAuthority() && EnemyConfig
		&& !PatternComponent->ConfigureAttackInterval(EnemyConfig->MinAttackInterval, EnemyConfig->MaxAttackInterval))
	{
		UE_LOG(LogEnemy, Error, TEXT("%s: attack interval configuration failed"), *GetName());
		return;
	}
	if (!HasAuthority() || !InitializeEnemyExtensions())
	{
		return;
	}
	bEnemyInitialized = true;	// 초기화 완료 기록

	// 활동 단계 관리 등록: 멀리 있거나 안 보이면 갱신을 줄임 (보스 등 bAllowActivityTiers가 꺼진 적은 제외)
	if (bAllowActivityTiers)
	{
		OverheadHealthBar = FindComponentByClass<UEnemyOverheadHealthBarComponent>();

		if (UEnemyActivitySubsystem* Activity = GetWorld()->GetSubsystem<UEnemyActivitySubsystem>())
		{
			Activity->RegisterEnemy(this);
		}
	}

	// Controller가 먼저 빙의한 경우, 여기서 AI 시작 요청
	if (AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()))
	{
		EnemyAI->TryStartEnemyAI();
	}
}

// 설정 데이터의 능력치 및 이동 속도를 캐릭터에 적용
void AEnemyCharacter::ApplyEnemyConfig()
{
	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetMaxHealthAttribute(),
		EnemyConfig->MaxHealth
	);

	// Health는 MaxHealth의 값을 그대로 사용
	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetHealthAttribute(),
		EnemyConfig->MaxHealth
	);

	ASC->SetNumericAttributeBase(
		UEnemyAttributeSet::GetAttackPowerAttribute(),
		EnemyConfig->AttackPower
	);

	// 이동 속도 적용
	BaseMoveSpeed = EnemyConfig->MoveSpeed;
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed;
	GetCharacterMovement()->MaxFlySpeed = BaseMoveSpeed;
}

// 중복 사망을 막고 공격과 AI를 정리한 뒤 사망 연출 시작
void AEnemyCharacter::BeginDeath()
{
	if (LifeState != EEnemyLifeState::Alive)
		return;

	LifeState = EEnemyLifeState::Dying;	// 중복 방지를 위해 최우선 처리 필요

	// 원래 Tick으로 되돌린 뒤 단계 관리에서 제외: 사망 연출과 정리가 제 속도로 진행
	ApplyActivityTier(EEnemyActivityTier::Active);
	if (UEnemyActivitySubsystem* Activity = GetWorld()->GetSubsystem<UEnemyActivitySubsystem>())
	{
		Activity->UnregisterEnemy(this);
	}

	OnDeathStarted.Broadcast(this);

	ClearPatternMoveSpeedMultiplier();
	PatternComponent->StopAndInvalidate();

	SetWantsToMove(false);

	// AI 중단
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Death"));
		}

		AI->StopMovement();
	}

	// 기존 Ability 취소
	if (IsValid(ASC))
	{
		ASC->CancelAllAbilities();
	}

	// 사망 중 이동 충돌 및 메시 피격 충돌 해제
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCharacterMovement()->StopMovementImmediately();	// 현재 움직임 제거
	GetCharacterMovement()->DisableMovement();	// 사망 시점 움직임 제거

	// 사망 시 카메라 셰이크 실행
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->ClientStartCameraShake(DeathCameraShakeClass, DeathShakeScale);
	}

	// 사망 소리와 이펙트: 몸 중심(캡슐 중심)에서 재생
	EnemyFeedback::PlayAtLocation(this, DeathCue, GetActorLocation(), GetActorRotation());

	// 사망 연출
	UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance();

	// AnimInstance(ABP)나 DeathMontage가 없으면 연출 없이 즉시 사망 처리
	if (!IsValid(AnimInstance) || !IsValid(DeathMontage))
	{
		UE_LOG(LogEnemy, Warning, TEXT("[%s] Death animation unavailable"), *GetName());

		FinishDeath();
		return;
	}

	const float Result = AnimInstance->Montage_Play(DeathMontage);

	// 재생 실패 시 연출 없이 즉시 사망 처리
	if (Result <= 0.f)
	{
		UE_LOG(LogEnemy, Warning, TEXT("[%s] Death montage failed to start"), *GetName());

		FinishDeath();
		return;
	}

	// 몽타주 종료 델리게이트 구독
	// 사망 연출 종료 처리를 Notify 방식으로 변경했지만, 혹시 모를 중단/누락 방어용으로 델리게이트 구독 유지
	FOnMontageEnded OnDeathMontageEnd;
	OnDeathMontageEnd.BindUObject(this, &AEnemyCharacter::HandleDeathMontageEnded);
	// 재생에 성공한 사망 몽타주의 종료 델리게이트를 AnimInstance에 연결
	AnimInstance->Montage_SetEndDelegate(OnDeathMontageEnd, DeathMontage);

	UE_LOG(LogEnemy, Verbose, TEXT("[%s] BeginDeath"), *GetName());

}

void AEnemyCharacter::FinishDeath()
{
	if (LifeState != EEnemyLifeState::Dying)
		return;

	LifeState = EEnemyLifeState::Dead;

	GetCharacterMovement()->StopMovementImmediately();	// 현재 움직임 제거
	GetCharacterMovement()->DisableMovement();	// 움직임 제한 설정

	// 캡슐 충돌 무시
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	USkeletalMeshComponent* MeshComp = GetMesh();

	// MeshComp 충돌 Ragdoll 설정
	MeshComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));

	// MeshComp 충돌이 플레이어, Enemy는 무시하도록 설정
	MeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// MeshComp 물리 설정 (중력을 적용해 공중의 시체가 아래로 떨어지도록 설정)
	MeshComp->SetEnableGravity(true);
	MeshComp->SetSimulatePhysics(true);

	// 이전 이동 속도는 시체에 전달하지 않음
	MeshComp->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	MeshComp->WakeAllRigidBodies();

	UE_LOG(LogEnemy, Verbose, TEXT("[%s] FinishDeath"), *GetName());

	// 시체 제거 유예 (초단위): 끝나면 LifeSpanExpired에서 사라짐 연출 후 제거
	// - SetLifeSpan(0)은 "무기한"이므로 0 이하는 바로 제거
	if (CorpseLifeSeconds > 0.f)
	{
		SetLifeSpan(CorpseLifeSeconds);
	}
	else
	{
		LifeSpanExpired();
	}
}

void AEnemyCharacter::LifeSpanExpired()
{
	// 래그돌로 떨어진 시체 위치(메시)에서 재생: 액터 위치는 사망 지점에 남아 있음
	const USkeletalMeshComponent* MeshComp = GetMesh();
	const FVector Location = IsValid(MeshComp) ? MeshComp->Bounds.Origin : GetActorLocation();

	// 숨긴 시체(연출 대역이 대신 연기 중인 보스 등)는 사라짐 연출 없이 제거
	if (!IsHidden())
	{
		EnemyFeedback::PlayAtLocation(this, CorpseVanishCue, Location);
	}

	Super::LifeSpanExpired();
}

// 체력 변경 시 사망 판정을 먼저 수행한 뒤 추가 상태 처리
void AEnemyCharacter::HandleHealthChanged(float NewHealth, float MaxHealth)
{
	UE_LOG(LogEnemy, Verbose, TEXT("[%s] Health Changed: %.1f / %.1f"), *GetName(), NewHealth, MaxHealth);

	// 피해를 받으면 다음 평가를 기다리지 않고 바로 Active (멀리서 맞은 적도 바로 반응)
	LastHealthChangedTime = GetWorld()->GetTimeSeconds();
	ApplyActivityTier(EEnemyActivityTier::Active);

	if (NewHealth <= 0.f)
	{
		BeginDeath();
	}
}

void AEnemyCharacter::HandleDeathMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (Montage != DeathMontage || LifeState != EEnemyLifeState::Dying)
		return;

	UE_LOG(LogEnemy, Verbose, TEXT("[%s] Death montage ended. Interrupted=%d"), *GetName(), bInterrupted);

	FinishDeath();
}

// Getter: 캐릭터별 패턴 실행 컴포넌트
UEnemyPatternComponent* AEnemyCharacter::GetPatternComponent() const
{
	return PatternComponent;
}

void AEnemyCharacter::SetPatternMoveSpeedMultiplier(float Multiplier)
{
	GetCharacterMovement()->MaxWalkSpeed = BaseMoveSpeed * Multiplier;
	GetCharacterMovement()->MaxFlySpeed = BaseMoveSpeed * Multiplier;
}

void AEnemyCharacter::ClearPatternMoveSpeedMultiplier()
{
	SetPatternMoveSpeedMultiplier(1.f);
}

// 초기화와 생존 및 보스방 전투 상태를 포함한 AI 실행 가능 여부 확인
bool AEnemyCharacter::CanRunEnemyAI() const
{
	return bEnemyInitialized && IsAlive();
}

bool AEnemyCharacter::CanReceivePlayerPush() const
{
	if (!IsAlive()						// 생존 상태 확인
		|| PlayerPushMultiplier <= 0.f	// 밀려나기 배수가 유효한지 확인
		|| !PatternComponent)			// 패턴 컴포넌트 준비 됐는지 확인
	{
		return false;
	}

	const bool bRecovering = PatternComponent->IsRecovering();

	// 몸을 쓰는 공격 실행 중에는 밀림 거부 (보조 패턴만 실행 중이면 허용)
	// 후딜 중에는 밀림 허용
	if (PatternComponent->IsBodyBusy() && !bRecovering)
		return false;

	// 후딜이 소유한 이동 잠금 1개는 밀림 판정에서만 제외
	// 다른 이유로 추가된 이동 잠금의 경우에는 여전히 밀림 거부
	const int32 AllowedLockCount = PatternComponent->OwnsRecoveryMovementLock() ? 1 : 0;

	return AttackMovementLockCount <= AllowedLockCount;
}

void AEnemyCharacter::ApplyPlayerPush(const FVector& PushVelocity, bool bDirectPush)
{
	if (!CanReceivePlayerPush())
		return;

	UEnemyCharacterMovementComponent* Movement = Cast<UEnemyCharacterMovementComponent>(GetCharacterMovement());
	if (Movement)
	{
		Movement->RequestPlayerPush(PushVelocity * PlayerPushMultiplier, bDirectPush);
	}
}

// 기본 초기화 뒤 파생 캐릭터의 추가 설정 준비
bool AEnemyCharacter::InitializeEnemyExtensions()
{
	return IsValid(ASC) && IsValid(AttributeSet) && IsValid(EnemyConfig);
}

bool AEnemyCharacter::IsRotationLocked() const
{
	// GA 등이 명시적으로 회전을 잠근 경우
	if (RotationLockCount > 0)
	{
		return true;
	}

	// 후딜 중에는 공격 종료 시점의 몸 방향을 유지
	return IsValid(PatternComponent) && PatternComponent->IsRecovering();
}

// 공격 중 회전 제한을 존중하여 Controller 방향 반영
void AEnemyCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime)
{
	if (!IsRotationLocked())
	{
		Super::FaceRotation(NewControlRotation, DeltaTime);
	}
}

/* 활동 단계 */

void AEnemyCharacter::ApplyActivityTier(EEnemyActivityTier NewTier, float ReducedTickInterval)
{
	if (NewTier == ActivityTier)
	{
		return;
	}

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	AAIController* AI = Cast<AAIController>(GetController());
	UBrainComponent* Brain = AI ? AI->GetBrainComponent() : nullptr;

	// Active를 떠날 때 원래 Tick 간격 기록 (BP에서 바꾼 값도 그대로 복원)
	if (ActivityTier == EEnemyActivityTier::Active)
	{
		ActiveActorTickInterval = GetActorTickInterval();
		ActiveMovementTickInterval = Movement->GetComponentTickInterval();
		ActiveControllerTickInterval = AI ? AI->GetActorTickInterval() : 0.f;
	}

	/* 이전 단계(Dormant)에서 멈춘 것 되돌림 */

	if (bActivityStoppedMovementTick)
	{
		Movement->SetComponentTickEnabled(true);
		bActivityStoppedMovementTick = false;
	}

	if (bActivityPausedLogic)
	{
		if (Brain)
		{
			Brain->ResumeLogic(TEXT("Activity"));
		}
		bActivityPausedLogic = false;
	}

	UE_LOG(LogEnemy, Verbose, TEXT("[%s] Activity tier %s -> %s"), *GetName(),
		*UEnum::GetDisplayValueAsText(ActivityTier).ToString(), *UEnum::GetDisplayValueAsText(NewTier).ToString());

	ActivityTier = NewTier;

	/* Active: 원래 설정 복원 */

	if (NewTier == EEnemyActivityTier::Active)
	{
		SetActorTickInterval(ActiveActorTickInterval);
		Movement->SetComponentTickInterval(ActiveMovementTickInterval);
		if (AI)
		{
			AI->SetActorTickInterval(ActiveControllerTickInterval);
		}
		if (OverheadHealthBar)
		{
			OverheadHealthBar->SetActivityTickEnabled(true);
		}
		return;
	}

	/* Reduced, Dormant: 액터(BP Tick 포함), 이동, AI 컨트롤러 Tick 간격 늘림 */

	SetActorTickInterval(FMath::Max(ActiveActorTickInterval, ReducedTickInterval));
	Movement->SetComponentTickInterval(FMath::Max(ActiveMovementTickInterval, ReducedTickInterval));
	if (AI)
	{
		AI->SetActorTickInterval(FMath::Max(ActiveControllerTickInterval, ReducedTickInterval));
	}

	// 화면 밖이라 체력바 회전이 필요 없음 (체력이 바뀌면 Active로 돌아와 다시 그림)
	if (OverheadHealthBar)
	{
		OverheadHealthBar->SetActivityTickEnabled(false);
	}

	if (NewTier != EEnemyActivityTier::Dormant)
	{
		return;
	}

	/* Dormant: 판단 로직 일시정지, 땅에 서 있으면 이동 Tick 끔 */

	// 일시정지 중 바뀐 Blackboard 값(인지로 잡은 타깃 등)은 재개할 때 한꺼번에 반영됨
	if (Brain && Brain->IsRunning())
	{
		Brain->PauseLogic(TEXT("Activity"));
		bActivityPausedLogic = true;
	}

	// 공중(비행형, 떨어지는 중)은 간격만 늘린 채 유지: 끄면 떠 있는 채 멈춤
	if (Movement->IsMovingOnGround() && Movement->IsComponentTickEnabled())
	{
		Movement->SetComponentTickEnabled(false);
		bActivityStoppedMovementTick = true;
	}
}

bool AEnemyCharacter::ShouldStayActive(float RecentHealthChangeSeconds) const
{
	if (!bAllowActivityTiers || !bEnemyInitialized || !IsAlive())
	{
		return true;
	}

	// 상시 패턴을 포함해 실행 중인 패턴이 있으면 유지
	if (IsValid(PatternComponent) && !PatternComponent->IsIdle())
	{
		return true;
	}

	// 전투 타깃이 있으면 유지 (인지, 보스방 매니저, 튜토리얼이 지정)
	if (const AEnemyAIController* EnemyAI = Cast<AEnemyAIController>(GetController()); EnemyAI && EnemyAI->GetCombatTarget())
	{
		return true;
	}

	return LastHealthChangedTime >= 0.0 && GetWorld()->TimeSince(LastHealthChangedTime) < RecentHealthChangeSeconds;
}

bool AEnemyCharacter::IsResting() const
{
	// 거의 멈춘 것으로 보는 속도 (cm/s)
	constexpr float RestingSpeed = 5.f;

	return !GetWantsToMove() && GetVelocity().SizeSquared() < FMath::Square(RestingSpeed);
}

// 플레이 종료 시 실행, 구독 및 직접 소유한 자원 정리 후 부모 호출
void AEnemyCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEnemyInitialized = false;
	if (UEnemyActivitySubsystem* Activity = GetWorld()->GetSubsystem<UEnemyActivitySubsystem>())
	{
		Activity->UnregisterEnemy(this);
	}
	PatternComponent->StopAndInvalidate();
	if (AttributeSet)
	{
		AttributeSet->OnHealthChange.RemoveAll(this);
	}
	Super::EndPlay(EndPlayReason);
}
