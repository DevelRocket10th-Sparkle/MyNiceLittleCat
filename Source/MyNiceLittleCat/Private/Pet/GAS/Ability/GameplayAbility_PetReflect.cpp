#include "Pet/GAS/Ability/GameplayAbility_PetReflect.h"
#include "Pet/PetCharacter.h"
#include "Pet/Data/PetSoundData.h"
#include "Pet/GAS/Task/AbilityTask_PetTick.h"
#include "Pet/GAS/AbilityObject/PetReflector.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Characters/PlayerMainController.h"
#include "NavigationSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Gimmicks/Laser/LaserBeamComponent.h"
#include "Gimmicks/Reflection/ReflectionMath.h"
//#include "UObject/UObjectIterator.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Components/WidgetComponent.h"
#include "WIdgets/Interaction/InteractionPromptWidget.h"

UGameplayAbility_PetReflect::UGameplayAbility_PetReflect()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGameplayAbility_PetReflect::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if ((false == IsValid(Pet)) || (false == Pet->IsPetInitialized()) || 
        (EPetMode::Reflect != Pet->GetCurrentMode()) || (nullptr == ReflectorClass))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (false == CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 설치 프리뷰는 실제 반사판 모양의 고스트로 보여줘요 (못 만들면 설치 위치를 보여줄 수 없으니 종료)
    if (false == CreatePlacementGhost(Pet))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 방향 화살표는 없어도 설치는 되니까 실패해도 계속 진행
    CreateFacingArrow(Pet);

    bDraggingPlacement = false;
    CachedPreviewLasers.Reset();
    bHasPendingPlacement = false;
    PendingPlacementTransform = FTransform::Identity;
    Pet->OnPetModeUseInput.AddUniqueDynamic(this, &UGameplayAbility_PetReflect::HandleModeUseInput);

    PlacementMoveRequestId.Invalidate();
    Pet->OnPetAbilityMoveFinished.AddUniqueDynamic(this, &UGameplayAbility_PetReflect::HandlePlacementMoveFinished);

    UAbilityTask_PetTick* TickTask = UAbilityTask_PetTick::CreatePetTick(this);
    TickTask->OnTick.AddDynamic(this, &UGameplayAbility_PetReflect::HandleReflectTick);
    TickTask->ReadyForActivation();

    // 모드 선택 직후 바로 위치 갱신
    HandleReflectTick(0.f);

    // 철거 안내 UI: 프리뷰 메시처럼 펫 소유, 위치는 반사판 위로 직접 옮김
    if (nullptr != RemovePromptWidgetClass)
    {
        RemovePrompt = NewObject<UWidgetComponent>(Pet, NAME_None, RF_Transient);
        RemovePrompt->SetWidgetSpace(EWidgetSpace::Screen);
        RemovePrompt->SetWidgetClass(RemovePromptWidgetClass);
        RemovePrompt->SetDrawAtDesiredSize(true);
        RemovePrompt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        RemovePrompt->SetGenerateOverlapEvents(false);
        RemovePrompt->SetCastShadow(false);
        RemovePrompt->SetVisibility(false);
        RemovePrompt->RegisterComponent();
        RemovePrompt->InitWidget();
    }
    HoveredReflector.Reset();

    // Q 입력은 자성 모드와 같은 Pet.SpecialAction 이벤트로 받음 (반사 모드 동안만 대기)
    const FGameplayTag SpecialActionTag = FGameplayTag::RequestGameplayTag(TEXT("Pet.SpecialAction"));
    UAbilityTask_WaitGameplayEvent* SpecialActionTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
        this, SpecialActionTag, nullptr, false, true);
    SpecialActionTask->EventReceived.AddDynamic(this, &UGameplayAbility_PetReflect::HandleSpecialAction);
    SpecialActionTask->ReadyForActivation();
}

void UGameplayAbility_PetReflect::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo()))
    {
        Pet->OnPetModeUseInput.RemoveDynamic(this, &UGameplayAbility_PetReflect::HandleModeUseInput);
        Pet->OnPetAbilityMoveFinished.RemoveDynamic(this, &UGameplayAbility_PetReflect::HandlePlacementMoveFinished);

        // 이 능력이 요청한 이동만 취소
        const FGuid RequestId = PlacementMoveRequestId;
        PlacementMoveRequestId.Invalidate();

        Pet->CancelAbilityMove(RequestId);

        if (EPetMovementStyle::ReflectPlacement == Pet->GetAnmationMovementStyle())
            Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);
    }

    PlacementMoveRequestId.Invalidate();

    bDraggingPlacement = false;
    CachedPreviewLasers.Reset();
    bHasPendingPlacement = false;
    PendingPlacementTransform = FTransform::Identity;
    DragStartHit = FHitResult();

    // 모드가 바뀌면 하이라이트·안내 UI 정리 (반사 모드일 때만 보이도록)
    SetHoveredReflector(nullptr);

    if (true == IsValid(RemovePrompt.Get()))
        RemovePrompt->DestroyComponent();
    RemovePrompt = nullptr;

    if (true == IsValid(PlacementGhostMesh.Get()))
        PlacementGhostMesh->DestroyComponent();

    PlacementGhostMesh = nullptr;
    PlacementGhostMIDs.Reset();

    if (true == IsValid(FacingArrowComponent.Get()))
        FacingArrowComponent->DestroyComponent();
    FacingArrowComponent = nullptr;

    for (UStaticMeshComponent* Segment : LaserPreviewSegments)
    {
        if (true == IsValid(Segment))
            Segment->DestroyComponent();
    }
    LaserPreviewSegments.Reset();
    LaserPreviewMIDs.Reset();

    bCanPlaceReflector = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGameplayAbility_PetReflect::HandleReflectTick(float DeltaTime)
{
    // 펫이 설치하러 가는 중에도 철거는 가능해야 하므로 가장 먼저 갱신
    UpdateRemoveHover();

    if (false == IsValid(PlacementGhostMesh.Get())) return;

    // 설치하러 가는 중에는 고스트가 설치될 자리에 그대로 머물러요 (도착/실패 시 숨김)
    if (true == bHasPendingPlacement) return;

    // 철거할 반사판을 가리키는 동안은 설치 프리뷰를 숨겨서 헷갈리지 않게
    if (true == HoveredReflector.IsValid())
    {
        bCanPlaceReflector = false;
        HidePlacementGhost();
        return;
    }

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    APlayerMainController* PC = GetPartnerPlayerController();

    if (false == IsValid(Pet) || false == IsValid(PC))
    {
        bCanPlaceReflector = false;
        HidePlacementGhost();
        return;
    }

    FHitResult Hit;
    if (true == bDraggingPlacement)
    {
        // 드래그 중에는 최초 위치를 유지
        Hit = DragStartHit;

        if (false == UpdateDragRotation())
        {
            bCanPlaceReflector = false;
            HidePlacementGhost();
            return;
        }
    }
    else
    {
        // 플레이어 바닥 높이 기준으로 위치를 잡음
        if (false == TryGetPlacementCursorHit(Hit))
        {
            bCanPlaceReflector = false;
            HidePlacementGhost();
            return;
        }

        PlacementRotation = FRotator(0.f, Pet->GetAimDirection().Rotation().Yaw, 0.f);
    }

    bCanPlaceReflector = CanPlaceReflector(Hit, PlacementRotation);

    // 실제 반사판이 생길 위치(판 중앙)와 똑같이 계산해서 고스트를 놓아요
    // 의도: 프리뷰 → 설치 대기 → 실제 생성까지 위치가 한 번도 튀지 않게
    const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();
    const FVector ReflectorCenter = Hit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);
    ShowPlacementGhost(FTransform(PlacementRotation, ReflectorCenter, FVector::OneVector), bCanPlaceReflector);

    // 드래그로 방향을 정하는 동안만 앞면 방향 화살표 표시
    if (true == bDraggingPlacement)
        ShowFacingArrow(Hit.ImpactPoint);
    else
        HideFacingArrow();

    // 프리뷰 반사 방향
    UpdateLaserReflectionPreview();
}

bool UGameplayAbility_PetReflect::CanPlaceReflector(const FHitResult& Hit, const FRotator& Rotation) const
{
    const APetReflector* ReflectorDefaults = ReflectorClass.GetDefaultObject();

    if (false == IsValid(ReflectorDefaults) || false == Hit.bBlockingHit)
        return false;

    const FVector HalfExtents = ReflectorDefaults->GetPlacementHalfExtents();
    const FVector BoxCenter = Hit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);

    return CanPlaceReflectorAtTransform(FTransform(Rotation, BoxCenter, FVector::OneVector));
}

void UGameplayAbility_PetReflect::HandleModeUseInput(EPetModeUseInputPhase Phase)
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if (false == IsActive() || false == IsValid(Pet) || Pet->GetCurrentMode() != EPetMode::Reflect) return;
    if (Phase == EPetModeUseInputPhase::Canceled)
    {
        bDraggingPlacement = false;
        HideFacingArrow();
        HideLaserPreviewFrom(0);
        CachedPreviewLasers.Reset();
        bCanPlaceReflector = false;
        return;
    }

    if (true == bHasPendingPlacement) return;

    if (Phase == EPetModeUseInputPhase::Pressed)
    {
        APlayerMainController* PC = GetPartnerPlayerController();

        if (false == IsValid(PC) || false == TryGetPlacementCursorHit(DragStartHit))
            return;

        PlacementRotation = FRotator(0.f, Pet->GetAimDirection().Rotation().Yaw, 0.f);
        LastRotateTickYaw = PlacementRotation.Yaw; // 드래그 시작 방향 기준으로 틱 계산
        bDraggingPlacement = true;
        CachePreviewLasers();
        HandleReflectTick(0.f);
        return;
    }

    if (Phase == EPetModeUseInputPhase::Released && true == bDraggingPlacement)
    {
        // 마지막 커서 위치와 공간 판정을 놓는 순간 다시 갱신
        HandleReflectTick(0.f);
        bDraggingPlacement = false;
        HideFacingArrow(); // 방향이 확정됐으니 화살표는 숨김 (설치 대기 중엔 Tick이 갱신하지 않아서 여기서 직접)
        HideLaserPreviewFrom(0); // 레이저 경로 미리보기도 같은 이유로 여기서 숨김
        CachedPreviewLasers.Reset();

        if (false == bCanPlaceReflector)
        {
            // 설치가 안 된 이유에 따라 다른 실패음
            // 의도: 개수 초과인지 위치 문제인지 플레이어가 소리로 구분할 수 있게
            if (const UPetSoundData* Sounds = Pet->GetSoundData())
            {
                const bool bMaxReached = GetActiveReflectorCount() >= MaxReflectorCount;
                Pet->PlayPetSound2D(bMaxReached ? Sounds->MaxCountReachedSound : Sounds->PlacementBlockedSound);
            }
            return;
        }

        const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();
        const FVector ReflectorCenter = DragStartHit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);
        PendingPlacementTransform = FTransform(PlacementRotation, ReflectorCenter, FVector::OneVector);

        if (false == BeginPlacementMove())
        {
            PendingPlacementTransform = FTransform::Identity;
            UE_LOG(LogTemp, Warning, TEXT("Reflect: approach point unavailable or move request rejected."));

            // 펫이 설 자리를 못 찾음 = 설치 불가
            if (const UPetSoundData* Sounds = Pet->GetSoundData())
                Pet->PlayPetSound2D(Sounds->PlacementBlockedSound);
            return;
        }

        bHasPendingPlacement = true;

        // 클릭 확정: 펫이 걸어가는 동안에도 입력이 먹었다는 걸 바로 알려줘요
        if (const UPetSoundData* Sounds = Pet->GetSoundData())
            Pet->PlayPetSound2D(Sounds->PlacementConfirmSound);

        // 펫이 도착할 때까지 설치될 자리에 고스트를 고정 (설치가 확정됐으니 설치 가능 색)
        ShowPlacementGhost(PendingPlacementTransform, true);
    }
}

APlayerMainController* UGameplayAbility_PetReflect::GetPartnerPlayerController() const
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    APawn* Player = (true == IsValid(Pet)) ? Pet->GetPartnerPlayer() : nullptr;

    return (true == IsValid(Player)) ? Cast<APlayerMainController>(Player->GetController()): nullptr;
}

bool UGameplayAbility_PetReflect::UpdateDragRotation()
{
    APlayerMainController* PC = GetPartnerPlayerController();
    if (false == IsValid(PC)) return false;

    FVector RayOrigin;
    FVector RayDirection;
    if (false == PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return false;

    if (FMath::IsNearlyZero(RayDirection.Z)) return false;

    // 처음 클릭한 높이의 수평면에서 커서 위치 계산
    const double Distance = (DragStartHit.ImpactPoint.Z - RayOrigin.Z) / RayDirection.Z;
    if (Distance < 0.0) return false;

    const FVector CursorPoint = RayOrigin + RayDirection * Distance;
   
    // 처음 클릭한 A에서 현재 커서 B로 향하는 방향
    FVector Direction = CursorPoint - DragStartHit.ImpactPoint;
    Direction.Z = 0.f;
    
    // 클릭점 근처에서는 작은 움직임으로 방향이 흔들리지 않도록
    if (Direction.SizeSquared() < FMath::Square(MinDragDistance))
        return true;

    PlacementRotation = FRotator(0.f, Direction.Rotation().Yaw, 0.f);

    // 일정 각도 이상 돌아갈 때마다 틱
    // 의도: 회전량이 손에 느껴지게 (매 프레임 울리면 시끄러우니 각도 간격으로)
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    const UPetSoundData* Sounds = IsValid(Pet) ? Pet->GetSoundData() : nullptr;
    if (nullptr != Sounds
        && FMath::Abs(FMath::FindDeltaAngleDegrees(LastRotateTickYaw, PlacementRotation.Yaw)) >= Sounds->RotateTickAngle)
    {
        LastRotateTickYaw = PlacementRotation.Yaw;
        Pet->PlayPetSound2D(Sounds->RotateTickSound);
    }

    return true;
}

bool UGameplayAbility_PetReflect::TryGetPlacementCursorHit(FHitResult& OutHit)
{
    OutHit = FHitResult();

    APlayerMainController* PC = GetPartnerPlayerController();
    if (false == IsValid(PC) || false == IsValid(GetWorld())) return false;

    float FloorZ = 0.f;
    if (false == TryGetPlayerFloorZ(FloorZ)) return false;

    FVector RayOrigin;
    FVector RayDirection;
    if (false == PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return false;

    // 커서 Ray가 아래를 향하지 않으면 바닥 평면과 만나지 않음
    if (RayDirection.Z >= -KINDA_SMALL_NUMBER) return false;

    // 커서 Ray와 "플레이어 바닥 높이의 수평면"의 교점
    // 의도: 커서가 테이블 등 다른 물체에 걸려도 플레이어가 밟은 높이 기준으로 위치를 잡음
    const double Distance = (FloorZ - RayOrigin.Z) / RayDirection.Z;
    const FVector PlanePoint = RayOrigin + RayDirection * Distance;

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetReflectCursor));
    QueryParams.AddIgnoredActor(PC->GetPawn());

    if (AActor* Pet = GetAvatarActorFromActorInfo())
        QueryParams.AddIgnoredActor(Pet);

    // 교점 위아래 허용 오차 안의 실제 바닥만 인정 (범위 밖 높이 = 설치 불가 → 프리뷰 숨김)
    const FVector TraceStart = PlanePoint + FVector::UpVector * MaxPlacementHeightDifference;
    const FVector TraceEnd = PlanePoint - FVector::UpVector * MaxPlacementHeightDifference;

    return GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

bool UGameplayAbility_PetReflect::TryGetPlayerFloorZ(float& OutFloorZ)
{
    APlayerMainController* PC = GetPartnerPlayerController();
    const ACharacter* PlayerCharacter = IsValid(PC) ? Cast<ACharacter>(PC->GetPawn()) : nullptr;
    if (false == IsValid(PlayerCharacter)) return false;

    const UCharacterMovementComponent* Movement = PlayerCharacter->GetCharacterMovement();
    if (false == IsValid(Movement)) return false;

    // 땅을 밟고 있을 때만 기준 높이 갱신
    // 의도: 점프 중에 프리뷰 높이가 같이 떠오르거나 빨개지지 않도록
    if (true == Movement->IsMovingOnGround() || false == bHasPlayerFloorZ)
    {
        LastPlayerFloorZ = Movement->GetActorFeetLocation().Z;
        bHasPlayerFloorZ = true;
    }

    OutFloorZ = LastPlayerFloorZ;
    return true;
}

bool UGameplayAbility_PetReflect::BeginPlacementMove()
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if (false == IsValid(Pet) || false == IsValid(NavSystem)) return false;

    const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();

    // 저장된 Transform은 실제 반사판 중앙 위치
    const FVector GroundLocation = PendingPlacementTransform.GetLocation() - FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);
    const FVector Forward = PendingPlacementTransform.GetRotation().GetForwardVector();
    const float PetRadius = Pet->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const float ApproachDistance = HalfExtents.X + PetRadius + PlacementStandOffDistance;
   
    //const FVector ApproachPoint = GroundLocation + Forward * ApproachDistance;
    APawn* Player = Pet->GetPartnerPlayer();
    if (false == IsValid(Player)) return false;

    const FVector ToPlayer = Player->GetActorLocation() - GroundLocation;

    // 플레이어가 판의 앞쪽에 있으면 앞, 뒤쪽에 있으면 뒤
    const FVector ApproachDirection = FVector::DotProduct(ToPlayer, Forward) >= 0.f ? Forward : -Forward;
    const FVector ApproachPoint = GroundLocation + ApproachDirection * ApproachDistance;

    // 설치 위치 자체는 옮기지 않고, 펫이 설 위치만 보정
    FNavLocation NavLocation;
    const bool bFoundNavPoint = NavSystem->ProjectPointToNavigation(
        ApproachPoint, NavLocation, FVector(30.f, 30.f, 100.f), &Pet->GetNavAgentPropertiesRef());

    if (false == bFoundNavPoint) return false;

    constexpr float DebugDuration = 15.f;

    //// 초록 구: 반사판을 설치할 바닥 위치
    //DrawDebugSphere(
    //    GetWorld(), GroundLocation + FVector(0.f, 0.f, 10.f),
    //    15.f, 12, FColor::Green, false, DebugDuration, 0, 2.f);

    //// 빨간 구: NavMesh 보정 후 펫이 실제로 도착해야 하는 위치
    //DrawDebugSphere(
    //    GetWorld(), NavLocation.Location + FVector(0.f, 0.f, 10.f),
    //    15.f, 12, FColor::Red, false, DebugDuration, 0, 2.f);

    //// 노란 선: 요청 당시 펫 위치 → 실제 목적지
    //DrawDebugLine(
    //    GetWorld(), Pet->GetActorLocation(),
    //    NavLocation.Location + FVector(0.f, 0.f, 10.f),
    //    FColor::Yellow, false, DebugDuration, 0, 2.f);

    //UE_LOG(LogTemp, Warning,
    //    TEXT("[ReflectApproach] Ground=%s, Approach=%s, NavGoal=%s"),
    //    *GroundLocation.ToString(),
    //    *ApproachPoint.ToString(),
    //    *NavLocation.Location.ToString());

    PlacementMoveRequestId = Pet->RequestAbilityMoveTo(NavLocation.Location);
    if (false == PlacementMoveRequestId.IsValid()) return false;

    Pet->SetAnimationMovementStyle(EPetMovementStyle::ReflectPlacement);
    return true;
}

void UGameplayAbility_PetReflect::HandlePlacementMoveFinished(FGuid RequestId, bool bSucceeded)
{
    if (false == PlacementMoveRequestId.IsValid() || RequestId != PlacementMoveRequestId)
        return;

    PlacementMoveRequestId.Invalidate();

    // 도착했든 실패했든 고스트는 숨김 
    // (성공이면 바로 아래에서 진짜 반사판이 생겨요)
    HidePlacementGhost();

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    const UPetSoundData* Sounds = IsValid(Pet) ? Pet->GetSoundData() : nullptr;

    if (true == bSucceeded)
    {
        const bool bPlaced = TryPlaceReflector();

        // 도착은 했는데 그 사이 자리가 막혔으면 설치 불가음
        // (설치 완료음은 TryPlaceReflector 안에서 반사판 위치로 재생)
        if (false == bPlaced && nullptr != Sounds)
            Pet->PlayPetSound2D(Sounds->PlacementBlockedSound);

        UE_LOG(LogTemp, Log, TEXT("Reflect: placement %s."), bPlaced ? TEXT("succeeded") : TEXT("canceled"));
    }
    else
    {
        if (true == IsValid(Pet))
        {
            Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);

            // 이동 실패로 설치 취소 = 설치 불가음 재사용
            if (nullptr != Sounds)
                Pet->PlayPetSound2D(Sounds->PlacementBlockedSound);
        }

        UE_LOG(LogTemp, Log, TEXT("Reflect: placement move failed or canceled."));
    }

    bHasPendingPlacement = false;
    PendingPlacementTransform = FTransform::Identity;

    // 복귀 중에도 다음 설치 입력을 받을 수 있도록 복구
    HandleReflectTick(0.f);
}

int32 UGameplayAbility_PetReflect::GetActiveReflectorCount() const
{
    int32 Count = 0;

    for (const TWeakObjectPtr<APetReflector>& Entry : PlacedReflectors)
    {
        const APetReflector* Reflector = Entry.Get();
        if (true == IsValid(Reflector) && false == Reflector->IsActorBeingDestroyed())
            ++Count;
    }

    return Count;
}

bool UGameplayAbility_PetReflect::CanPlaceReflectorAtTransform(const FTransform& PlacementTransform) const
{
    const APetReflector* ReflectorDefaults = ReflectorClass.GetDefaultObject();
    if (false == IsValid(GetWorld()) || false == IsValid(ReflectorDefaults) || GetActiveReflectorCount() >= MaxReflectorCount)
        return false;

    const FVector HalfExtents = ReflectorDefaults->GetPlacementHalfExtents();
    const FVector ExpectedGround = PlacementTransform.GetLocation() - FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetReflectPlacement));

    if (APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo()))
        QueryParams.AddIgnoredActor(Pet);

    // 클릭 당시의 바닥 정보만 믿지 않고 현재 바닥 확인
    FHitResult GroundHit;
    const bool bFoundGround = GetWorld()->LineTraceSingleByChannel(
        GroundHit, ExpectedGround + FVector::UpVector * 20.f, ExpectedGround - FVector::UpVector * 20.f,
        ECC_Visibility, QueryParams);

    if (false == bFoundGround)
        return false;

    // 바닥이 사라지거나 높이가 달라졌다면 취소
    if (FVector::DistSquared(GroundHit.ImpactPoint, ExpectedGround) > FMath::Square(1.f))
        return false;

    const float GroundDot = FVector::DotProduct(GroundHit.ImpactNormal, FVector::UpVector);

    const float MinimumGroundDot = FMath::Cos(FMath::DegreesToRadians(MaxPlacementSlopeDegrees));

    if (GroundDot < MinimumGroundDot)
        return false;

    const bool bBlocked = GetWorld()->OverlapBlockingTestByProfile(
        PlacementTransform.GetLocation(), PlacementTransform.GetRotation(),
        TEXT("BlockAllDynamic"), FCollisionShape::MakeBox(HalfExtents), QueryParams);

    return (false == bBlocked);
}

bool UGameplayAbility_PetReflect::TryPlaceReflector()
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if (false == IsActive() || false == IsValid(Pet) || Pet->GetCurrentMode() != EPetMode::Reflect || false == bHasPendingPlacement)
        return false;

    // 수명이 끝난 참조 정리
    PlacedReflectors.RemoveAll([](const TWeakObjectPtr<APetReflector>& Entry)
        {
            const APetReflector* Reflector = Entry.Get();
            return false == IsValid(Reflector) || Reflector->IsActorBeingDestroyed();
        });

    // 도착한 순간 다시 검사
    if (false == CanPlaceReflectorAtTransform(PendingPlacementTransform))
        return false;

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = Pet;
    SpawnParams.Instigator = Pet;

    // 위에서 검사한 위치 그대로 생성
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APetReflector* Reflector = GetWorld()->SpawnActor<APetReflector>(
        ReflectorClass, PendingPlacementTransform, SpawnParams);

    if (false == IsValid(Reflector))
        return false;

    PlacedReflectors.Add(Reflector);

    // 설치 완료: 펫이 아니라 반사판이 생긴 자리에서 들리게
    if (const UPetSoundData* Sounds = Pet->GetSoundData())
        Pet->PlayPetSoundAtLocation(Sounds->PlacementCompleteSound, Reflector->GetActorLocation());

    return true;
}

void UGameplayAbility_PetReflect::CachePreviewLasers()
{
    // 레이저 수집해요오
    CachedPreviewLasers.Reset();

    // 드래그 시작 시 한 번만 검색
    for (TObjectIterator<ULaserBeamComponent> iter; iter; ++iter)
    {
        ULaserBeamComponent* Laser = *iter;
        if (false == IsValid(Laser) || true == Laser->IsTemplate()
            || false == Laser->IsRegistered() || GetWorld() != Laser->GetWorld())
            continue;
  
        CachedPreviewLasers.Add(TWeakObjectPtr<ULaserBeamComponent>(Laser));
    }
}

void UGameplayAbility_PetReflect::UpdateLaserReflectionPreview()
{
    // 이번 틱에 실제로 쓴 점선 구간 수 (나머지는 마지막에 숨김)
    int32 UsedCount = 0;

    const APetReflector* Defaults = ReflectorClass.GetDefaultObject();

    // 드래그로 방향을 정하는 중이고, 그 자리에 설치할 수 있을 때만 표시
    if (false == bDraggingPlacement || false == bCanPlaceReflector || true == bHasPendingPlacement
        || false == IsValid(Defaults))
    {
        HideLaserPreviewFrom(0);
        return;
    }

    // 실제 설치할 반사판과 같은 크기, 위치, 회전
    FVirtualReflectiveSurface VirtualSurface;
    VirtualSurface.HalfExtents = Defaults->GetPlacementHalfExtents();

    VirtualSurface.Center = DragStartHit.ImpactPoint + 
        FVector::UpVector * (VirtualSurface.HalfExtents.Z + PlacementGroundClearance);

    VirtualSurface.Rotation = PlacementRotation.Quaternion();

    TArray<FLaserBeamSegment> PreviewSegments;

    for (const TWeakObjectPtr<ULaserBeamComponent>& Entry : CachedPreviewLasers)
    {
        ULaserBeamComponent* Laser = Entry.Get();

        if (false == IsValid(Laser)|| Laser->GetWorld() != GetWorld()
            || false == Laser->IsLaserActive())
            continue;

        // 실제 빔 배열 대신 프리뷰 전용 배열에 계산
        Laser->CalculateBeamPath(PreviewSegments, &VirtualSurface);
        bool bAfterVirtualSurface = false;

        for (const FLaserBeamSegment& Segment : PreviewSegments)
        {
            // 가상 반사판에 맞은 이후 구간만 표시
            if (true == bAfterVirtualSurface && true == ShowLaserPreviewSegment(UsedCount, Segment.Start, Segment.End))
                ++UsedCount;

            if (Segment.bHitVirtualSurface)
                bAfterVirtualSurface = true;
        }
    }

    // 이번 틱에 안 쓴 구간은 숨겨서 다음에 재사용
    HideLaserPreviewFrom(UsedCount);
}

bool UGameplayAbility_PetReflect::ShowLaserPreviewSegment(int32 Index, const FVector& Start, const FVector& End)
{
    if (Index >= MaxLaserPreviewSegments || false == IsValid(LaserPreviewMesh.Get()) || false == IsValid(LaserPreviewMaterial.Get()))
        return false;

    const FVector Delta = End - Start;
    const float Length = Delta.Size();
    if (Length < 1.f) return false; // 너무 짧은 구간은 건너뜀

    // 풀에 이 번호의 구간 메시가 없으면 새로 만들어요 (처음 한 번만)
    while (LaserPreviewSegments.Num() <= Index)
    {
        AActor* Owner = GetAvatarActorFromActorInfo();
        if (false == IsValid(Owner)) return false;

        UStaticMeshComponent* NewSegment = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
        NewSegment->SetMobility(EComponentMobility::Movable);
        NewSegment->SetStaticMesh(LaserPreviewMesh.Get());

        NewSegment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        NewSegment->SetGenerateOverlapEvents(false);
        NewSegment->SetCastShadow(false);
        NewSegment->SetReceivesDecals(false);
        NewSegment->SetCanEverAffectNavigation(false);
        NewSegment->SetVisibility(false);
        NewSegment->SetTranslucentSortPriority(12); //! 화살표(11)·고스트(10)보다 위에 그려지도록

        // 구간 길이를 넘겨줄 동적 머티리얼 (구간마다 하나, 처음 한 번만 생성)
        UMaterialInstanceDynamic* NewMID = NewSegment->CreateDynamicMaterialInstance(0, LaserPreviewMaterial.Get());
        NewSegment->RegisterComponent();

        LaserPreviewSegments.Add(NewSegment);
        LaserPreviewMIDs.Add(NewMID);
    }

    UStaticMeshComponent* SegmentMesh = LaserPreviewSegments[Index].Get();
    if (false == IsValid(SegmentMesh)) return false;

    // 평면의 로컬 +X를 레이저 진행 방향, 로컬 +Z를 위로 → 바닥과 나란히 누운 띠
    const FVector Direction = Delta / Length;
    const FRotator Rotation = FRotationMatrix::MakeFromXZ(Direction, FVector::UpVector).Rotator();

    SegmentMesh->SetWorldLocationAndRotation((Start + End) * 0.5f, Rotation);
    SegmentMesh->SetWorldScale3D(FVector(Length / 100.f, LaserPreviewWidth / 100.f, 1.f)); // 평면 100x100 기준

    // 점선 간격이 구간 길이와 상관없이 일정하도록 실제 길이를 넘겨줘요
    if (LaserPreviewMIDs.IsValidIndex(Index) && true == IsValid(LaserPreviewMIDs[Index].Get()))
        LaserPreviewMIDs[Index]->SetScalarParameterValue(TEXT("SegmentLength"), Length);

    SegmentMesh->SetVisibility(true);
    return true;
}

void UGameplayAbility_PetReflect::HideLaserPreviewFrom(int32 Index)
{
    for (int32 SegmentIndex = FMath::Max(Index, 0); SegmentIndex < LaserPreviewSegments.Num(); ++SegmentIndex)
    {
        UStaticMeshComponent* SegmentMesh = LaserPreviewSegments[SegmentIndex].Get();
        if (true == IsValid(SegmentMesh) && true == SegmentMesh->IsVisible())
            SegmentMesh->SetVisibility(false);
    }
}

bool UGameplayAbility_PetReflect::CreatePlacementGhost(APetCharacter* Pet)
{
    // 고스트 머티리얼이 없으면 만들지 않아요 (불투명 메시가 진짜 반사판처럼 보이면 헷갈리니까)
    if (false == IsValid(Pet) || true == PlacementGhostMaterials.IsEmpty()) return false;

    // 반사판 BP에 설정된 메시를 그대로 가져와요
    const APetReflector* ReflectorDefaults = ReflectorClass.GetDefaultObject();
    const UStaticMeshComponent* SourceMesh = IsValid(ReflectorDefaults) ? ReflectorDefaults->GetReflectorMesh() : nullptr;
    if (false == IsValid(SourceMesh) || false == IsValid(SourceMesh->GetStaticMesh())) return false;

    // 펫 소유, 위치는 커서/설치 Transform으로 직접 지정
    PlacementGhostMesh = NewObject<UStaticMeshComponent>(Pet, NAME_None, RF_Transient);
    PlacementGhostMesh->SetMobility(EComponentMobility::Movable);
    PlacementGhostMesh->SetStaticMesh(SourceMesh->GetStaticMesh());

    PlacementGhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PlacementGhostMesh->SetGenerateOverlapEvents(false);
    PlacementGhostMesh->SetCastShadow(false);
    PlacementGhostMesh->SetReceivesDecals(false);
    PlacementGhostMesh->SetCanEverAffectNavigation(false);
    PlacementGhostMesh->SetVisibility(false);
    PlacementGhostMesh->SetTranslucentSortPriority(10); //! 다른 반투명 이펙트보다 나중에(위에) 그려지도록

    // 슬롯마다 같은 순서의 고스트 머티리얼로 동적 머티리얼을 만들어요 (설치 가능/불가 색을 바꾸기 위해)
    // 배열이 슬롯 수보다 짧거나 비어 있는 칸은 원래 머티리얼(불투명) 그대로라, 6칸 다 채워야 해요
    PlacementGhostMIDs.Reset();
    const int32 SlotCount = FMath::Min(PlacementGhostMaterials.Num(), PlacementGhostMesh->GetNumMaterials());
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        if (false == IsValid(PlacementGhostMaterials[Index].Get())) continue;

        UMaterialInstanceDynamic* MID = PlacementGhostMesh->CreateDynamicMaterialInstance(Index, PlacementGhostMaterials[Index].Get());
        if (true == IsValid(MID))
            PlacementGhostMIDs.Add(MID);
    }

    PlacementGhostMesh->RegisterComponent();
    return true;
}

void UGameplayAbility_PetReflect::ShowPlacementGhost(const FTransform& ReflectorTransform, bool bPlaceable)
{
    UStaticMeshComponent* Ghost = PlacementGhostMesh.Get();
    const APetReflector* ReflectorDefaults = ReflectorClass.GetDefaultObject();
    const UStaticMeshComponent* SourceMesh = IsValid(ReflectorDefaults) ? ReflectorDefaults->GetReflectorMesh() : nullptr;
    const USceneComponent* SourceRoot = IsValid(ReflectorDefaults) ? ReflectorDefaults->GetRootComponent() : nullptr;

    if (false == IsValid(Ghost) || false == IsValid(SourceMesh) || false == IsValid(SourceRoot)) return;

    // 진짜 반사판이 생길 때와 똑같은 위치·회전·크기
    // = (반사판 안에서 메시의 상대 Transform) × (루트 크기) × (반사판 Transform)
    const FTransform RootScale(FQuat::Identity, FVector::ZeroVector, SourceRoot->GetRelativeScale3D());
    Ghost->SetWorldTransform(SourceMesh->GetRelativeTransform() * RootScale * ReflectorTransform);

    // 설치 가능하면 원래 색 그대로(검정을 더함), 불가면 빨갛게 빛나게 (머티리얼 발광에 GhostTint를 더해요)
    const FLinearColor Tint = bPlaceable ? ValidPreviewColor : InvalidPreviewColor;
    for (UMaterialInstanceDynamic* MID : PlacementGhostMIDs)
    {
        if (true == IsValid(MID))
            MID->SetVectorParameterValue(TEXT("GhostTint"), Tint);
    }

    Ghost->SetVisibility(true);
}

void UGameplayAbility_PetReflect::HidePlacementGhost()
{
    if (true == IsValid(PlacementGhostMesh.Get()))
        PlacementGhostMesh->SetVisibility(false);

    // 고스트가 없으면 화살표·레이저 미리보기도 의미가 없으니 같이 숨김
    HideFacingArrow();
    HideLaserPreviewFrom(0);
}

void UGameplayAbility_PetReflect::CreateFacingArrow(APetCharacter* Pet)
{
    if (false == IsValid(Pet) || false == IsValid(FacingArrowMesh.Get()) || false == IsValid(FacingArrowMaterial.Get())) return;

    // 고스트처럼 펫 소유, 위치는 매 틱 직접 지정
    FacingArrowComponent = NewObject<UStaticMeshComponent>(Pet, NAME_None, RF_Transient);
    FacingArrowComponent->SetMobility(EComponentMobility::Movable);
    FacingArrowComponent->SetStaticMesh(FacingArrowMesh.Get());
    FacingArrowComponent->SetMaterial(0, FacingArrowMaterial.Get());

    FacingArrowComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FacingArrowComponent->SetGenerateOverlapEvents(false);
    FacingArrowComponent->SetCastShadow(false);
    FacingArrowComponent->SetReceivesDecals(false);
    FacingArrowComponent->SetCanEverAffectNavigation(false);
    FacingArrowComponent->SetVisibility(false);
    FacingArrowComponent->SetTranslucentSortPriority(11); //! 고스트(10)보다 위에 그려지도록

    // 평면 메시(100x100) 기준 → 원하는 길이/폭으로 늘림 (X = 앞, Y = 옆)
    FacingArrowComponent->SetWorldScale3D(FVector(FacingArrowLength / 100.f, FacingArrowWidth / 100.f, 1.f));
    FacingArrowComponent->RegisterComponent();
}

void UGameplayAbility_PetReflect::ShowFacingArrow(const FVector& GroundLocation)
{
    if (false == IsValid(FacingArrowComponent.Get()) || nullptr == ReflectorClass) return;

    // 반사판 앞면(+X)에서 간격만큼 떨어진 곳부터 화살표가 시작되도록 화살표 중심을 계산
    const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();
    const FVector Forward = PlacementRotation.Vector();
    const float CenterDistance = HalfExtents.X + FacingArrowGap + FacingArrowLength * 0.5f;

    const FVector ArrowCenter = GroundLocation + Forward * CenterDistance + FVector::UpVector * FacingArrowHeight;

    // 머티리얼이 평면의 로컬 +X 방향으로 화살표를 그리므로 반사판 회전을 그대로 써요
    FacingArrowComponent->SetWorldLocationAndRotation(ArrowCenter, PlacementRotation);
    FacingArrowComponent->SetVisibility(true);
}

void UGameplayAbility_PetReflect::HideFacingArrow()
{
    if (true == IsValid(FacingArrowComponent.Get()))
        FacingArrowComponent->SetVisibility(false);
}

void UGameplayAbility_PetReflect::UpdateRemoveHover()
{
    // 드래그로 설치 방향을 정하는 중에는 철거 안내를 띄우지 않음
    APetReflector* NewHovered = (true == bDraggingPlacement) ? nullptr : FindHoveredReflector();
    SetHoveredReflector(NewHovered);

    if (true == IsValid(NewHovered) && true == IsValid(RemovePrompt.Get()))
        RemovePrompt->SetWorldLocation(NewHovered->GetActorLocation() + RemovePromptOffset);
}

APetReflector* UGameplayAbility_PetReflect::FindHoveredReflector() const
{
    APlayerMainController* PC = GetPartnerPlayerController();
    if (false == IsValid(PC)) return nullptr;

    FVector RayOrigin;
    FVector RayDirection;
    if (false == PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection)) return nullptr;

    constexpr float TraceDistance = 100000.f;
    const FVector RayEnd = RayOrigin + RayDirection * TraceDistance;

    // 펫이 설치한 반사판 중 커서 광선에 가장 가까이 맞는 것
    APetReflector* Best = nullptr;
    float BestDistance = TNumericLimits<float>::Max();

    for (const TWeakObjectPtr<APetReflector>& Entry : PlacedReflectors)
    {
        APetReflector* Reflector = Entry.Get();
        if (false == IsValid(Reflector) || true == Reflector->IsActorBeingDestroyed()) continue;

        FHitResult Hit;
        if (false == Reflector->TraceCursorRay(RayOrigin, RayEnd, Hit)) continue;

        const float Distance = FVector::Dist(RayOrigin, Hit.ImpactPoint);
        if (Distance < BestDistance)
        {
            BestDistance = Distance;
            Best = Reflector;
        }
    }

    if (false == IsValid(Best)) return nullptr;

    // 벽 너머 반사판 제외: 같은 광선으로 월드를 쏴서 반사판보다 앞에 다른 게 있으면 가려진 것
    FHitResult WorldHit;
    if (true == PC->TryGetPetCursorHit(WorldHit) && Best != WorldHit.GetActor())
    {
        constexpr float OcclusionTolerance = 1.f;
        if (FVector::Dist(RayOrigin, WorldHit.ImpactPoint) + OcclusionTolerance < BestDistance)
            return nullptr;
    }

    return Best;
}

void UGameplayAbility_PetReflect::SetHoveredReflector(APetReflector* NewReflector)
{
    APetReflector* OldReflector = HoveredReflector.Get();
    if (OldReflector != NewReflector)
    {
        if (true == IsValid(OldReflector))
            OldReflector->SetRemoveHighlighted(false);

        if (true == IsValid(NewReflector))
        {
            NewReflector->SetRemoveHighlighted(true);

            // 새 반사판에 올라간 순간에만 (철거 후 nullptr로 바뀔 때는 안 울림)
            if (APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo()))
                if (const UPetSoundData* Sounds = Pet->GetSoundData())
                    Pet->PlayPetSound2D(Sounds->RemoveHoverSound);
        }

        HoveredReflector = NewReflector;
    }

    // 대상이 수명 만료로 사라진 경우에도 UI가 남지 않도록 표시 여부는 매번 맞춤
    UWidgetComponent* Prompt = RemovePrompt.Get();
    if (false == IsValid(Prompt)) return;

    const bool bShow = IsValid(NewReflector);
    if (Prompt->IsVisible() == bShow) return;

    Prompt->SetVisibility(bShow);

    if (UInteractionPromptWidget* Widget = Cast<UInteractionPromptWidget>(Prompt->GetUserWidgetObject()))
    {
        if (true == bShow)
        {
            Widget->SetPrompt(RemovePromptKeyText, RemovePromptActionText);
            Widget->NotifyShown();
        }
        else
            Widget->NotifyHidden();
    }
}

void UGameplayAbility_PetReflect::HandleSpecialAction(FGameplayEventData Payload)
{
    APetReflector* Reflector = HoveredReflector.Get();
    if (false == IsValid(Reflector)) return;

    // 펫은 하던 일 계속, 반사판만 즉시 제거
    SetHoveredReflector(nullptr);
    PlacedReflectors.Remove(Reflector);

    // 철거음: 사라지는 반사판 자리에서 (Destroy 전에 위치를 읽어야 해요)
    if (APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo()))
        if (const UPetSoundData* Sounds = Pet->GetSoundData())
            Pet->PlayPetSoundAtLocation(Sounds->RemoveSound, Reflector->GetActorLocation());

    Reflector->Destroy();
}
