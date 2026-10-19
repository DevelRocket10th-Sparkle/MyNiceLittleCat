#include "Pet/GAS/Ability/GameplayAbility_PetReflect.h"
#include "Pet/PetCharacter.h"
#include "Pet/GAS/Task/AbilityTask_PetTick.h"
#include "Pet/GAS/AbilityObject/PetReflector.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Characters/PlayerMainController.h"

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
    bHasPendingPlacement = false;
    PendingPlacementTransform = FTransform::Identity;
    Pet->OnPetModeUseInput.AddUniqueDynamic(this, &UGameplayAbility_PetReflect::HandleModeUseInput);

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
    }

    bDraggingPlacement = false;
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
}

bool UGameplayAbility_PetReflect::CanPlaceReflector(const FHitResult& Hit, const FRotator& Rotation) const
{
    const APetReflector* ReflectorDO = ReflectorClass.GetDefaultObject();

    if (false == IsValid(GetWorld()) || false == IsValid(ReflectorDO) || false == Hit.bBlockingHit)
        return false;

    // 벽이나 너무 기울어진 표면은 제외
    const float GroundDot = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector);
    const float MinimumGroundDot = FMath::Cos(FMath::DegreesToRadians(MaxPlacementSlopeDegrees));
    if (GroundDot < MinimumGroundDot)
        return false;

    const FVector HalfExtents = ReflectorDO->GetPlacementHalfExtents();

    // 반사판의 원점은 박스 중앙이므로 반높이만큼 올림
    const FVector BoxCenter = Hit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PetReflectPlacement));

    const bool bBlocked = GetWorld()->OverlapBlockingTestByProfile(
        BoxCenter, Rotation.Quaternion(), TEXT("BlockAllDynamic"), FCollisionShape::MakeBox(HalfExtents), QueryParams);

    return (false == bBlocked);
}

void UGameplayAbility_PetReflect::HandleModeUseInput(EPetModeUseInputPhase Phase)
{
    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());

    if (false == IsActive() || false == IsValid(Pet) || Pet->GetCurrentMode() != EPetMode::Reflect) return;
    if (Phase == EPetModeUseInputPhase::Canceled)
    {
        bDraggingPlacement = false;
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
        HandleReflectTick(0.f);
        return;
    }

    if (Phase == EPetModeUseInputPhase::Released && true == bDraggingPlacement)
    {
        // 마지막 커서 위치와 공간 판정을 놓는 순간 다시 갱신
        HandleReflectTick(0.f);
        bDraggingPlacement = false;

        if (false == bCanPlaceReflector) return;

        const FVector HalfExtents = ReflectorClass.GetDefaultObject()->GetPlacementHalfExtents();
        const FVector ReflectorCenter = DragStartHit.ImpactPoint + FVector::UpVector * (HalfExtents.Z + PlacementGroundClearance);
        PendingPlacementTransform = FTransform(PlacementRotation, ReflectorCenter, FVector::OneVector);

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
