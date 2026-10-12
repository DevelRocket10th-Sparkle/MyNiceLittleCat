// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PlayerComponents/LockOnComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

#include "Interfaces/TargetableInterface.h"

// Sets default values for this component's properties
ULockOnComponent::ULockOnComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}

void ULockOnComponent::ToggleTargetLock()
{
    if (LockedTarget.IsValid())
    {
        LockedTarget.Reset();
        return;
    }

    if (CurrentCandidate.IsValid() && Candidates.Contains(CurrentCandidate))
    {
        LockedTarget = CurrentCandidate;
    }

	UpdateReticle();
}

void ULockOnComponent::SwitchTarget(float Direction)
{
    // 휠 입력 시 후보를 즉시 갱신
    RefreshCandidates();

    if (Candidates.Num() < 2) return;

    int32 CurrentIndex = Candidates.IndexOfByKey(CurrentCandidate);

    if (CurrentIndex == INDEX_NONE)
    {
        CurrentIndex = 0;
    }
    AActor* PreviousTarget = CurrentCandidate.Get();

    // 양수: 다음 후보, 음수: 이전 후보
    const int32 Step = Direction > 0.f ? 1 : -1;

    CurrentIndex = (CurrentIndex + Step + Candidates.Num())
        % Candidates.Num();

    CurrentCandidate = Candidates[CurrentIndex];

    UE_LOG(
        LogTemp,
        Log,
        TEXT("Target Changed: %s -> %s"),
        *GetNameSafe(PreviousTarget),
        *GetNameSafe(CurrentCandidate.Get())
    );

    if (LockedTarget.IsValid())
    {
        LockedTarget = CurrentCandidate;
    }
}


// Called when the game starts
void ULockOnComponent::BeginPlay()
{
	Super::BeginPlay();

    if (ACharacter* PlayerCharacter = Cast<ACharacter>(GetOwner()))
    {
        if (APlayerController* PC =
            Cast<APlayerController>(PlayerCharacter->GetController()))
        {
           // 위젯 설정

        }
    }

    CameraBoom = GetOwner()? GetOwner()->FindComponentByClass<USpringArmComponent>() : nullptr;

    if (CameraBoom)
    {
        BaseCameraTargetOffset = CameraBoom->TargetOffset;
        BaseCameraArmLength = CameraBoom->TargetArmLength;

        GetWorld()->GetTimerManager().SetTimer(
            CameraFocusTimerHandle,
            this,
            &ULockOnComponent::UpdateLockOnCamera,
            1.0f / 60.0f,
            true
        );
    }

    RefreshCandidates();

    // 스캔 타이머 설정
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            ScanTimerHandle,
            this,
            &ULockOnComponent::RefreshCandidates,
            FMath::Max(ScanInterval, 0.03f),
            true
        );
    }
	
}

void ULockOnComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ScanTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

void ULockOnComponent::RefreshCandidates()
{
    ACharacter* Character = Cast<ACharacter>(GetOwner());

    if (!Character)
    {
        Candidates.Reset();
        CurrentCandidate.Reset();
        LockedTarget.Reset();
		UpdateReticle();
        return;
    }

    APlayerController* PC = Cast<APlayerController>(Character->GetController());

    if (!PC || !PC->IsLocalController()) return;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsWithInterface(
        GetWorld(),
        UTargetableInterface::StaticClass(),
        FoundActors
    );

    TArray<TWeakObjectPtr<AActor>> NewCandidates;

    for (AActor* Actor : FoundActors)
    {
		if (!IsValidCandidate(Actor)) continue;

        FVector2D ScreenPosition;

        if (!GetTargetScreenPosition(Actor, ScreenPosition)) continue;

        NewCandidates.Add(Actor);
    }

    // 플레이어와 가까운 순서로 정렬
    NewCandidates.Sort(
        [Character](
            const TWeakObjectPtr<AActor>& A,
            const TWeakObjectPtr<AActor>& B)
        {
            if (!A.IsValid()) return false;
            if (!B.IsValid()) return true;

            return FVector::DistSquared(Character->GetActorLocation(), A->GetActorLocation())
                   <
                   FVector::DistSquared(Character->GetActorLocation(), B->GetActorLocation());
        }
    );

    Candidates = MoveTemp(NewCandidates);

    // 락온 대상이 더 이상 후보가 아니면 락온 해제
    if (LockedTarget.IsValid() && !Candidates.Contains(LockedTarget))
    {
        LockedTarget.Reset();
    }

    // 현재 선택이 사라졌을 때 가장 가까운 대상으로 변경
    if (!CurrentCandidate.IsValid() || !Candidates.Contains(CurrentCandidate))
    {
        AActor* PreviousTarget = CurrentCandidate.Get();

        CurrentCandidate.Reset();

        AActor* NewTarget = CurrentCandidate.Get();

        if (PreviousTarget != NewTarget)
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("Target Changed: %s -> %s"),
                *GetNameSafe(PreviousTarget),
                *GetNameSafe(NewTarget)
            );
        }

        if (!Candidates.IsEmpty())
        {
            CurrentCandidate = Candidates[0];
        }
    }

    // 락온 대상이 있다면 현재 선택과 동기화
    if (LockedTarget.IsValid())
    {
        CurrentCandidate = LockedTarget;
    }

	UpdateReticle();
}

bool ULockOnComponent::GetTargetScreenPosition(AActor* Target, FVector2D& OutScreenPosition) const
{
    if (!IsValid(Target)) false;

    const ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character) false;

    APlayerController* PC = Cast<APlayerController>(Character->GetController());
    if (!PC) false;

    FVector BoundsOrigin;
    FVector BoundsExtent;
    Target->GetActorBounds(false, BoundsOrigin, BoundsExtent);

    return PC->ProjectWorldLocationToScreen(
        BoundsOrigin,
        OutScreenPosition,
        true
    );
}

bool ULockOnComponent::IsValidCandidate(AActor* Target) const
{
    return IsValid(Target)
        && Target != GetOwner()
        && Target->GetClass()->ImplementsInterface(
			UTargetableInterface::StaticClass()
        );
}

void ULockOnComponent::UpdateLockOnCamera()
{
    if (!CameraBoom || !GetOwner()) return;

    FVector DesiredOffset = BaseCameraTargetOffset;
    float DesiredArmLength = BaseCameraArmLength;

    AActor* Target = LockedTarget.Get();

    if (IsValidCandidate(Target))
    {
        const FVector PlayerLocation =
            GetOwner()->GetActorLocation();

        FVector TargetLocation;
        FVector BoundsExtent;

        Target->GetActorBounds(
            false,
            TargetLocation,
            BoundsExtent
        );

        // 플레이어에서 타겟까지의 거리 벡터
        const FVector ToTarget =
            TargetLocation - PlayerLocation;

        // 두 대상 사이의 중간 지점을 카메라 중심으로 설정
        DesiredOffset += ToTarget * 0.5f;

        // 거리가 멀어질수록 카메라를 뒤로 이동
        const float HorizontalDistance = FVector(
            ToTarget.X,
            ToTarget.Y,
            0.0f
        ).Size();

        const float ExtraArmLength = FMath::Clamp(
            (HorizontalDistance - ZoomDistanceThreshold)
            * ZoomDistanceScale,
            0.0f,
            MaxExtraArmLength
        );

        DesiredArmLength =
            BaseCameraArmLength + ExtraArmLength;
    }

    // 카메라 중심과 줌을 부드럽게 보간
    constexpr float TimerDeltaTime = 1.0f / 60.0f;

    CameraBoom->TargetOffset = FMath::VInterpTo(
        CameraBoom->TargetOffset,
        DesiredOffset,
        TimerDeltaTime,
        CameraFocusInterpSpeed
    );

    CameraBoom->TargetArmLength = FMath::FInterpTo(
        CameraBoom->TargetArmLength,
        DesiredArmLength,
        TimerDeltaTime,
        CameraFocusInterpSpeed
    );
}

void ULockOnComponent::UpdateReticle()
{
    AActor* Target = LockedTarget.IsValid()
        ? LockedTarget.Get()
        : CurrentCandidate.Get();

    if (!IsValid(Target))
    {
        return;
    }

    FVector Origin;
    FVector Extent;
    Target->GetActorBounds(false, Origin, Extent);

    DrawDebugSphere(
        GetWorld(),
        Origin + FVector(0.f, 0.f, Extent.Z + 20.f),
        20.f,        // 반지름
        12,          // 세그먼트
        FColor::Red,
        false,       // 영구 표시 여부
        0.15f,       // 표시 시간
        0,           // 깊이 우선순위
        2.f          // 선 두께
    );
}




