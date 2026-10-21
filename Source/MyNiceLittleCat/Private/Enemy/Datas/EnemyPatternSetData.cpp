#include "Enemy/Datas/EnemyPatternSetData.h"

void UEnemyPatternSetData::PostLoad()
{
	Super::PostLoad();
	for (FEnemyPatternDefinition& Pattern : Patterns)
	{
		if (Pattern.MeleeTraceSettings.IsEmpty())
		{
			for (const FEnemyMeleeTraceDefinition& Previous : Pattern.MeleeTraces)
			{
				UEnemyMeleeTraceSettings* Settings = NewObject<UEnemyMeleeTraceSettings>(this, NAME_None, RF_Transactional);
				Settings->SourceName = Previous.SourceName;
				Settings->SocketNames = Previous.SocketNames;
				Settings->Radius = Previous.Radius;
#if WITH_EDITORONLY_DATA
				Settings->SocketSourceMesh = Previous.SocketSourceMesh;
#endif
				Pattern.MeleeTraceSettings.Add(Settings);
			}
		}
		Pattern.MeleeTraces.Reset();
	}
}

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "UObject/UnrealType.h"

void UEnemyPatternSetData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	// 배열 추가/붙여넣기로 생긴 빈 항목은 고정된 설정 클래스로 바로 생성.
	for (FEnemyPatternDefinition& Pattern : Patterns)
	{
		for (TObjectPtr<UEnemyMeleeTraceSettings>& Settings : Pattern.MeleeTraceSettings)
		{
			if (!Settings)
			{
				Settings = NewObject<UEnemyMeleeTraceSettings>(this, NAME_None, RF_Transactional);
			}
		}
	}
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

TArray<FName> UEnemyMeleeTraceSettings::GetSocketOptions() const
{
	TArray<FName> Names;
	UObject* Mesh = SocketSourceMesh.LoadSynchronous();
	if (const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Mesh))
	{
		for (const USkeletalMeshSocket* Socket : SkeletalMesh->GetActiveSocketList())
		{
			if (Socket) Names.AddUnique(Socket->SocketName);
		}
	}
	else if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(Mesh))
	{
		for (const UStaticMeshSocket* Socket : StaticMesh->Sockets)
		{
			if (Socket) Names.AddUnique(Socket->SocketName);
		}
	}
	Names.Sort(FNameLexicalLess());
	return Names;
}

void UEnemyMeleeTraceSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(UEnemyMeleeTraceSettings, SocketSourceMesh))
	{
		const TArray<FName> Available = GetSocketOptions();
		SocketNames.RemoveAll([&Available](FName Name) { return !Available.Contains(Name); });
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
