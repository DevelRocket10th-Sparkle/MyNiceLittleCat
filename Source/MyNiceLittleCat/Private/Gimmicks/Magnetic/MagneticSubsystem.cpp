/// 작성자: 최요환
/// 작성일자: 2026-10-18
///
/// 자력원 목록 관리 월드 서브시스템 구현

#include "Gimmicks/Magnetic/MagneticSubsystem.h"
#include "DrawDebugHelpers.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarMagneticDrawDebug(
	TEXT("Magnetic.DrawDebug"),
	0,
	TEXT("1이면 등록된 자력원의 자기장 범위를 그립니다."),
	ECVF_Cheat);

void UMagneticSubsystem::RegisterSource(AActor* SourceActor, const FMagneticSourceInfo& Info)
{
	if (!IsValid(SourceActor))
	{
		return;
	}

	Sources.Add(SourceActor, Info);

	if (CVarMagneticDrawDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugSource(GetWorld(), Info);
	}
}

void UMagneticSubsystem::UpdateSource(AActor* SourceActor, const FMagneticSourceInfo& Info)
{
	FMagneticSourceInfo* Found = Sources.Find(SourceActor);
	if (!Found)
	{
		return;
	}

	*Found = Info;

	if (CVarMagneticDrawDebug.GetValueOnGameThread() != 0)
	{
		DrawDebugSource(GetWorld(), Info);
	}
}

void UMagneticSubsystem::UnregisterSource(AActor* SourceActor)
{
	Sources.Remove(SourceActor);

	// 파괴된 자력원 정리
	for (auto It = Sources.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

bool UMagneticSubsystem::IsSourceRegistered(const AActor* SourceActor) const
{
	return Sources.Contains(SourceActor);
}

void UMagneticSubsystem::ForEachSource(TFunctionRef<void(AActor* SourceActor, const FMagneticSourceInfo& Info)> Func) const
{
	for (const TPair<TWeakObjectPtr<AActor>, FMagneticSourceInfo>& Pair : Sources)
	{
		if (AActor* SourceActor = Pair.Key.Get())
		{
			Func(SourceActor, Pair.Value);
		}
	}
}

void UMagneticSubsystem::DrawDebugSource(const UWorld* World, const FMagneticSourceInfo& Info, float Duration)
{
	if (!World || Info.Polarity == EMagneticPolarity::None)
	{
		return;
	}

	FVector Aim = Info.AimDirection;
	if (Info.bPlanar)
	{
		Aim.Z = 0.f;
	}
	Aim = Aim.GetSafeNormal();
	if (Aim.IsNearlyZero())
	{
		return;
	}

	const FColor Color = Info.Polarity == EMagneticPolarity::N ? FColor::Red : FColor::Blue;

	// 높이 제한이 있으면 위아래 HeightLimit 사이의 납작한 영역으로 표시
	if (Info.bLimitHeight)
	{
		if (Info.Shape == EMagneticFieldShape::Line)
		{
			const FVector Center = Info.Location + Aim * (Info.Range * 0.5f);
			const FVector Extent(Info.Range * 0.5f, Info.LineRadius, Info.HeightLimit);
			DrawDebugBox(World, Center, Extent, Aim.ToOrientationQuat(), Color, false, Duration);
		}
		else
		{
			constexpr int32 NumSegments = 16;
			const FVector Up = FVector::UpVector * Info.HeightLimit;
			const FVector LeftEdge = Aim.RotateAngleAxis(-Info.ConeHalfAngle, FVector::UpVector) * Info.Range;
			const FVector RightEdge = Aim.RotateAngleAxis(Info.ConeHalfAngle, FVector::UpVector) * Info.Range;

			for (const FVector& Offset : { Up, -Up })
			{
				const FVector Origin = Info.Location + Offset;
				DrawDebugLine(World, Origin, Origin + LeftEdge, Color, false, Duration);
				DrawDebugLine(World, Origin, Origin + RightEdge, Color, false, Duration);

				FVector Prev = Origin + LeftEdge;
				for (int32 Index = 1; Index <= NumSegments; ++Index)
				{
					const float Angle = FMath::Lerp(-Info.ConeHalfAngle, Info.ConeHalfAngle, static_cast<float>(Index) / NumSegments);
					const FVector Next = Origin + Aim.RotateAngleAxis(Angle, FVector::UpVector) * Info.Range;
					DrawDebugLine(World, Prev, Next, Color, false, Duration);
					Prev = Next;
				}
			}

			// 위아래 연결선
			DrawDebugLine(World, Info.Location - Up, Info.Location + Up, Color, false, Duration);
			DrawDebugLine(World, Info.Location + LeftEdge - Up, Info.Location + LeftEdge + Up, Color, false, Duration);
			DrawDebugLine(World, Info.Location + RightEdge - Up, Info.Location + RightEdge + Up, Color, false, Duration);
		}
		return;
	}

	if (Info.Shape == EMagneticFieldShape::Line)
	{
		const FVector Center = Info.Location + Aim * (Info.Range * 0.5f);
		const FQuat Rotation = FRotationMatrix::MakeFromZ(Aim).ToQuat();
		DrawDebugCapsule(World, Center, Info.Range * 0.5f + Info.LineRadius, Info.LineRadius, Rotation, Color, false, Duration);
	}
	else
	{
		const float AngleRad = FMath::DegreesToRadians(Info.ConeHalfAngle);
		DrawDebugCone(World, Info.Location, Aim, Info.Range, AngleRad, AngleRad, 16, Color, false, Duration);
	}
}

bool UMagneticSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
