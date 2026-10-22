#include "Enemy/Datas/EnemyPatternSetData.h"
#include "Animation/AnimMontage.h"

// 기존 에셋의 근접 필드만 전용 설정으로 이전; 이미 전용 설정이 있으면 덮어쓰지 않음
void UEnemyPatternSetData::PostLoad()
{
	Super::PostLoad();
	for (FEnemyPatternDefinition& Pattern : Patterns)
	{
		if (!Pattern.ExecutionSettings && (Pattern.Montage || !Pattern.MeleeTraceSettings.IsEmpty() || !Pattern.MeleeTraces.IsEmpty()))
		{
			// 기존 몽타주와 피해 배율을 보관할 근접 전용 객체
			UEnemyMeleePatternSettings* Melee = NewObject<UEnemyMeleePatternSettings>(this, NAME_None, RF_Transactional);
			Melee->Montage = Pattern.Montage;
			Melee->MontageSection = Pattern.MontageSection;
			Melee->DamageMultiplier = Pattern.DamageMultiplier;
			for (const UEnemyMeleeTraceSettings* Previous : Pattern.MeleeTraceSettings)
				if (Previous) Melee->MeleeTraceSettings.Add(DuplicateObject<UEnemyMeleeTraceSettings>(Previous, Melee));
			if (Melee->MeleeTraceSettings.IsEmpty())
			{
				for (const FEnemyMeleeTraceDefinition& Previous : Pattern.MeleeTraces)
				{
					UEnemyMeleeTraceSettings* Settings = NewObject<UEnemyMeleeTraceSettings>(Melee, NAME_None, RF_Transactional);
					Settings->SourceName = Previous.SourceName;
					Settings->SocketNames = Previous.SocketNames;
					Settings->Radius = Previous.Radius;
#if WITH_EDITORONLY_DATA
					Settings->SocketSourceMesh = Previous.SocketSourceMesh;
#endif
					Melee->MeleeTraceSettings.Add(Settings);
				}
			}
			Pattern.ExecutionSettings = Melee;
		}
		Pattern.Montage = nullptr;
		Pattern.MontageSection = NAME_None;
		Pattern.MeleeTraceSettings.Reset();
		Pattern.MeleeTraces.Reset();
	}
}

#if WITH_EDITOR
// 전용 객체의 판정 배열은 해당 설정 클래스에서 편집 처리
void UEnemyPatternSetData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// GA별 설정의 편집 처리는 전용 객체에 위임하여 공통 에셋이 판정 방식을 알 필요가 없게 함
	for (FEnemyPatternDefinition& Pattern : Patterns)
		if (Pattern.ExecutionSettings) Pattern.ExecutionSettings->PostEditChangeChainProperty(PropertyChangedEvent);
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}
#endif
