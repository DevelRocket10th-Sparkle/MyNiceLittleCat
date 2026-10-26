#include "Pet/GAS/Ability/GameplayAbility_PetReflect.h"
#include "Pet/PetCharacter.h"
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
//#include "DrawDebugHelpers.h"

UGameplayAbility_PetReflect::UGameplayAbility_PetReflect()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UGameplayAbility_PetReflect::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if ((false == IsValid(Pet)) || (false == Pet->IsPetInitialized()) || 
        (EPetMode::Reflect != Pet->GetCurrentMode()) || (nullptr == ReflectorClass) 
        || false == IsValid(PreviewMaterial.Get()) || false == IsValid(PreviewMeshAsset.Get()))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (false == CommitAbility(Handle, ActorInfo, ActivationInfo))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 실제 반사판 너비에 맞춰 프리뷰 크기 계산
    const APetReflector* ReflectorDefaults = ReflectorClass.GetDefaultObject();

    const float TargetWidth = ReflectorDefaults->GetHalfWidth() * 2.f;
    const float MeshWidth = PreviewMeshAsset->GetBoundingBox().GetSize().Y;

    if (MeshWidth <= KINDA_SMALL_NUMBER)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const float PreviewScale = TargetWidth / MeshWidth;

    // 소유자는 펫이지만, 위치는 커서의 월드 위치로 직접 관리
    PreviewMesh = NewObject<UStaticMeshComponent>(Pet, NAME_None, RF_Transient);

    PreviewMesh->SetMobility(EComponentMobility::Movable);
    PreviewMesh->SetStaticMesh(PreviewMeshAsset.Get());

    PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PreviewMesh->SetGenerateOverlapEvents(false);
    PreviewMesh->SetCastShadow(false);
    PreviewMesh->SetReceivesDecals(false);
    PreviewMesh->SetCanEverAffectNavigation(false);
    PreviewMesh->SetVisibility(false);

    PreviewMesh->SetWorldScale3D(FVector(PreviewScale));
    PreviewMesh->RegisterComponent();

    PreviewMID = PreviewMesh->CreateDynamicMaterialInstance(0, PreviewMaterial.Get());

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

    if (true == IsValid(PreviewMesh.Get()))
        PreviewMesh->DestroyComponent();

    PreviewMesh = nullptr;
    PreviewMID = nullptr;
    bCanPlaceReflector = false;

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGameplayAbility_PetReflect::HandleReflectTick(float DeltaTime)
{
    UStaticMeshComponent* Mesh = PreviewMesh.Get();
    if (false == IsValid(Mesh)) return;

    if (true == bHasPendingPlacement)
    {
        Mesh->SetVisibility(false);
        return;
    }

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    APlayerMainController* PC = GetPartnerPlayerController();

    if (false == IsValid(Pet) || false == IsValid(PC))
    {
        bCanPlaceReflector = false;
        Mesh->SetVisibility(false);
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
            Mesh->SetVisibility(false);
            return;
        }
    }
    else
    {
        if (false == PC->TryGetPetCursorHit(Hit))
        {
            bCanPlaceReflector = false;
            Mesh->SetVisibility(false);
            return;
        }

        PlacementRotation = FRotator(0.f, Pet->GetAimDirection().Rotation().Yaw, 0.f);
    }

    const FVector GroundLocation = Hit.ImpactPoint + FVector::UpVector * PlacementGroundClearance;
    bCanPlaceReflector = CanPlaceReflector(Hit, PlacementRotation);
    Mesh->SetWorldLocationAndRotation(GroundLocation, PlacementRotation);

    if (true == IsValid(PreviewMID.Get()))
    {
        PreviewMID->SetVectorParameterValue(TEXT("PreviewColor"), 
            bCanPlaceReflector ? ValidPreviewColor : InvalidPreviewColor);
    }

    Mesh->SetVisibility(true);

    // 반구는 회전해도 같아 보이므로 실제 판이 놓일 선 표시
    const float HalfWidth = ReflectorClass.GetDefaultObject()->GetHalfWidth();
    const FVector WidthDirection = PlacementRotation.RotateVector(FVector::RightVector);

    DrawDebugLine(GetWorld(),
        GroundLocation - WidthDirection * HalfWidth, GroundLocation + WidthDirection * HalfWidth,
        FColor::White, false, -1.f, 0, 2.f);

    // 반사판 앞면 방향
    DrawDebugDirectionalArrow(GetWorld(),
        GroundLocation, GroundLocation + PlacementRotation.Vector() * 60.f,
        15.f, FColor::Yellow, false, -1.f, 0, 2.f);

    // 프리뷰 반사 방향
    DrawLaserReflectionPreview();
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
        CachedPreviewLasers.Reset();
        bCanPlaceReflector = false;
        return;
    }

    if (true == bHasPendingPlacement) return;

    if (Phase == EPetModeUseInputPhase::Pressed)
    {
        APlayerMainController* PC = GetPartnerPlayerController();

        if (false == IsValid(PC) || false == PC->TryGetPetCursorHit(DragStartHit))
            return;

        PlacementRotation = FRotator(0.f, Pet->GetAimDirection().Rotation().Yaw, 0.f);
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
        CachedPreviewLasers.Reset();

        if (false == bCanPlaceReflector) return;

        const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();
        const FVector ReflectorCenter = DragStartHit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);
        PendingPlacementTransform = FTransform(PlacementRotation, ReflectorCenter, FVector::OneVector);

        if (false == BeginPlacementMove())
        {
            PendingPlacementTransform = FTransform::Identity;
            UE_LOG(LogTemp, Warning, TEXT("Reflect: approach point unavailable or move request rejected."));
            return;
        }

        bHasPendingPlacement = true;
        PreviewMesh->SetVisibility(false);

        // 요청이 확정된 위치를 3초 동안 표시
        DrawDebugBox(GetWorld(), ReflectorCenter, HalfExtents, PlacementRotation.Quaternion(), FColor::Cyan, false, 3.f, 0, 2.f);
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
    if (false == PC->DeprojectMousePositionToWorld(RayOrigin, RayDirection))
        return false;

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

    if (true == bSucceeded)
    {
        const bool bPlaced = TryPlaceReflector();
        UE_LOG(LogTemp, Log, TEXT("Reflect: placement %s."), bPlaced ? TEXT("succeeded") : TEXT("canceled"));
    }
    else
    {
        if (APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo()))
            Pet->SetAnimationMovementStyle(EPetMovementStyle::Normal);

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
        QueryParams.AddIgnoredComponent(Pet->GetMesh());

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

void UGameplayAbility_PetReflect::DrawLaserReflectionPreview() const
{
#if ENABLE_DRAW_DEBUG
    if (false == bDraggingPlacement || false == bCanPlaceReflector || true == bHasPendingPlacement)
        return;

    const APetReflector* Defaults = ReflectorClass.GetDefaultObject();
    if (false == IsValid(Defaults)) return;

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
            if (true == bAfterVirtualSurface)
                DrawDebugLine( GetWorld(), Segment.Start, Segment.End, FColor::Magenta, false, -1.f, 0, 3.f);

            if (Segment.bHitVirtualSurface)
                bAfterVirtualSurface = true;
        }
    }
#endif
}
