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

    UAbilityTask_PetTick* TickTask = UAbilityTask_PetTick::CreatePetTick(this);
    TickTask->OnTick.AddDynamic(this, &UGameplayAbility_PetReflect::HandleReflectTick);
    TickTask->ReadyForActivation();

    // 모드 선택 직후 바로 위치 갱신
    HandleReflectTick(0.f);
}

void UGameplayAbility_PetReflect::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (IsValid(PreviewMesh.Get()))
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

    APetCharacter* Pet = Cast<APetCharacter>(GetAvatarActorFromActorInfo());
    APawn* Player = (true == IsValid(Pet)) ? Pet->GetPartnerPlayer() : nullptr;
    APlayerMainController* PlayerController = (true == IsValid(Player)) ? Cast<APlayerMainController>(Player->GetController()) : nullptr;

    FHitResult Hit;
    if (false == IsValid(PlayerController) || false == PlayerController->TryGetPetCursorHit(Hit))
    {
        bCanPlaceReflector = false;
        Mesh->SetVisibility(false);
        return;
    }

    const FVector PreviewLocation = Hit.ImpactPoint + FVector::UpVector * PlacementGroundClearance;
    const float Yaw = Pet->GetAimDirection().Rotation().Yaw;
    const FRotator PlacementRotation(0.f, Yaw, 0.f);

    bCanPlaceReflector = CanPlaceReflector(Hit, PlacementRotation);

    Mesh->SetWorldLocationAndRotation(PreviewLocation, PlacementRotation);

    if (true == IsValid(PreviewMID.Get()))
    {
        PreviewMID->SetVectorParameterValue(TEXT("PreviewColor"), 
            bCanPlaceReflector ? ValidPreviewColor : InvalidPreviewColor);
    }

    Mesh->SetVisibility(true);

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
