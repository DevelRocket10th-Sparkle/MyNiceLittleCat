/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// 기믹 이동 컴포넌트 구현

#include "Gimmicks/GimmickMoverComponent.h"
#include "Curves/CurveFloat.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "SceneView.h"

// ---- 기준점 표시 컴포넌트 ----

namespace GimmickPivotVisualizer
{
	const FColor PivotColor(255, 140, 0);
	const FColor ClosedColor(230, 230, 230);
	const FColor OpenColor(60, 230, 90);
}

// 기준점·닫힘/열림 방향을 맨 앞(SDPG_Foreground)에 선으로 그리는 프록시
class FGimmickPivotSceneProxy final : public FPrimitiveSceneProxy
{
public:
	explicit FGimmickPivotSceneProxy(const UGimmickPivotVisualizerComponent* Component)
		: FPrimitiveSceneProxy(Component)
		, OpenRelativeTransform(Component->OpenRelativeTransform)
		, DrawScale(Component->DrawScale)
	{
		bWillEverBeLit = false;
	}

	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		using namespace GimmickPivotVisualizer;

		// 부모 스케일에 상관없이 같은 크기로 그림
		const FMatrix& ProxyLocalToWorld = GetLocalToWorld();
		const FVector Origin = ProxyLocalToWorld.GetOrigin();
		const FVector Forward = ProxyLocalToWorld.GetUnitAxis(EAxis::X);
		const FVector Right = ProxyLocalToWorld.GetUnitAxis(EAxis::Y);
		const FVector Up = ProxyLocalToWorld.GetUnitAxis(EAxis::Z);
		const FQuat WorldQuat = FMatrix(Forward, Right, Up, FVector::ZeroVector).ToQuat();

		const FVector OpenLocation = Origin + WorldQuat.RotateVector(OpenRelativeTransform.GetLocation());
		const FVector OpenForward = (WorldQuat * OpenRelativeTransform.GetRotation()).GetForwardVector();

		const float AxisLength = 120.f * DrawScale;
		const float ArrowLength = 90.f * DrawScale;
		const float HeadSize = 12.f * DrawScale;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// 회전 중심 + 경첩 축(위아래)
			DrawCircle(PDI, Origin, Forward, Right, PivotColor, 10.f * DrawScale, 16, SDPG_Foreground, 2.f);
			PDI->DrawLine(Origin - Up * AxisLength * 0.25f, Origin + Up * AxisLength, PivotColor, SDPG_Foreground, 2.f);

			// 닫힘 정면 (흰색)
			DrawArrow(PDI, Origin, Forward, ArrowLength, HeadSize, ClosedColor, 1.5f);

			// 열림 정면 (초록). 위치가 다르면 중심에서 이어 줌
			if (!OpenLocation.Equals(Origin, 1.f))
			{
				PDI->DrawLine(Origin, OpenLocation, OpenColor, SDPG_Foreground, 1.f);
				DrawCircle(PDI, OpenLocation, Forward, Right, OpenColor, 6.f * DrawScale, 12, SDPG_Foreground, 1.5f);
			}
			DrawArrow(PDI, OpenLocation, OpenForward, ArrowLength, HeadSize, OpenColor, 2.f);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		Result.bEditorPrimitiveRelevance = UseEditorCompositing(View);
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:
	static void DrawArrow(FPrimitiveDrawInterface* PDI, const FVector& Start, const FVector& Direction, float Length, float HeadSize, const FColor& Color, float Thickness)
	{
		const FVector End = Start + Direction * Length;
		PDI->DrawLine(Start, End, Color, SDPG_Foreground, Thickness);

		// 화살촉: 진행 방향에 수직인 두 축으로 4갈래
		FVector Side, SideUp;
		Direction.FindBestAxisVectors(Side, SideUp);
		const FVector Back = End - Direction * HeadSize;
		PDI->DrawLine(End, Back + Side * HeadSize * 0.5f, Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(End, Back - Side * HeadSize * 0.5f, Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(End, Back + SideUp * HeadSize * 0.5f, Color, SDPG_Foreground, Thickness);
		PDI->DrawLine(End, Back - SideUp * HeadSize * 0.5f, Color, SDPG_Foreground, Thickness);
	}

	FTransform OpenRelativeTransform;
	float DrawScale;
};

UGimmickPivotVisualizerComponent::UGimmickPivotVisualizerComponent()
{
	bHiddenInGame = true;
	bIsEditorOnly = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
	CastShadow = false;
	bUseEditorCompositing = true;
}

FPrimitiveSceneProxy* UGimmickPivotVisualizerComponent::CreateSceneProxy()
{
	return new FGimmickPivotSceneProxy(this);
}

FBoxSphereBounds UGimmickPivotVisualizerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	const float Radius = (200.f * DrawScale) + OpenRelativeTransform.GetLocation().Size();
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector(Radius), Radius);
}

// ---- 이동 컴포넌트 ----

UGimmickMoverComponent::UGimmickMoverComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UGimmickMoverComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	UpdatePivotVisualization();
#endif
}

void UGimmickMoverComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITORONLY_DATA
	DestroyPivotVisualization();
#endif

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

#if WITH_EDITOR
void UGimmickMoverComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdatePivotVisualization();
}
#endif

#if WITH_EDITORONLY_DATA
void UGimmickMoverComponent::UpdatePivotVisualization()
{
	UWorld* World = GetWorld();
	if (!bShowPivotInEditor || !World || World->IsGameWorld() || !GetOwner())
	{
		DestroyPivotVisualization();
		return;
	}

	if (!PivotVisualizer)
	{
		// 카메라 컴포넌트의 프록시 메시와 같은 방식: 에디터 월드에서만 만드는 표시용 컴포넌트
		PivotVisualizer = NewObject<UGimmickPivotVisualizerComponent>(GetOwner(), NAME_None, RF_Transactional | RF_TextExportTransient | RF_Transient);
		PivotVisualizer->SetupAttachment(this);
		PivotVisualizer->SetIsVisualizationComponent(true);
		PivotVisualizer->CreationMethod = CreationMethod;
		PivotVisualizer->RegisterComponentWithWorld(World);
	}

	// OpenOffset은 부모 기준이라 이 컴포넌트 기준으로 바꿔서 전달
	PivotVisualizer->OpenRelativeTransform = FTransform(OpenRotation, GetRelativeRotation().UnrotateVector(OpenOffset));
	PivotVisualizer->DrawScale = PivotArrowSize;
	PivotVisualizer->MarkRenderStateDirty();
	PivotVisualizer->UpdateBounds();
}

void UGimmickMoverComponent::DestroyPivotVisualization()
{
	if (PivotVisualizer)
	{
		PivotVisualizer->DestroyComponent();
		PivotVisualizer = nullptr;
	}
}
#endif

void UGimmickMoverComponent::BeginPlay()
{
	Super::BeginPlay();
	CaptureClosedTransform();
}

void UGimmickMoverComponent::CaptureClosedTransform()
{
	// BeginPlay 전에 신호가 올 수 있어 처음 필요할 때 한 번만 저장
	if (!bHasClosedTransform)
	{
		ClosedTransform = GetRelativeTransform();
		bHasClosedTransform = true;
	}
}

void UGimmickMoverComponent::SetOpen(bool bOpen, bool bInstant)
{
	CaptureClosedTransform();

	const float TargetProgress = bOpen ? 1.f : 0.f;
	const bool bChanged = bTargetOpen != bOpen;
	bTargetOpen = bOpen;

	const float Duration = bOpen ? OpenDuration : (CloseDuration < 0.f ? OpenDuration : CloseDuration);
	if (bInstant || Duration <= KINDA_SMALL_NUMBER)
	{
		const bool bWasMoving = bMoving;
		Progress = TargetProgress;
		bMoving = false;
		SetComponentTickEnabled(false);
		ApplyProgress();

		// 시간 0 이동은 연출용 이벤트를 보냄 (즉시 이동은 시작 상태 복원이라 보내지 않음)
		if (!bInstant && (bChanged || bWasMoving))
		{
			OnMoveStarted.Broadcast(this, bOpen);
			OnMoveFinished.Broadcast(this, bOpen);
		}
		return;
	}

	if (FMath::IsNearlyEqual(Progress, TargetProgress))
	{
		return;
	}

	if (bChanged || !bMoving)
	{
		bMoving = true;
		SetComponentTickEnabled(true);
		OnMoveStarted.Broadcast(this, bOpen);
	}
}

void UGimmickMoverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMoving)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float Duration = bTargetOpen ? OpenDuration : (CloseDuration < 0.f ? OpenDuration : CloseDuration);
	const float Step = Duration > KINDA_SMALL_NUMBER ? DeltaTime / Duration : 1.f;
	const float TargetProgress = bTargetOpen ? 1.f : 0.f;
	Progress = FMath::FInterpConstantTo(Progress, TargetProgress, 1.f, Step);
	ApplyProgress();

	if (FMath::IsNearlyEqual(Progress, TargetProgress))
	{
		Progress = TargetProgress;
		bMoving = false;
		SetComponentTickEnabled(false);
		OnMoveFinished.Broadcast(this, bTargetOpen);
	}
}

void UGimmickMoverComponent::ApplyProgress()
{
	const float Alpha = MoveCurve ? MoveCurve->GetFloatValue(Progress) : FMath::SmoothStep(0.f, 1.f, Progress);

	const FVector Location = ClosedTransform.GetLocation() + OpenOffset * Alpha;
	const FQuat Rotation = ClosedTransform.GetRotation() * (OpenRotation * Alpha).Quaternion();
	SetRelativeLocationAndRotation(Location, Rotation);
}
