#include "Enemy/Datas/EnemyMeleePatternSettings.h"

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "UObject/UnrealType.h"

// 에디터에서 판정 항목을 추가하면 별도 에셋 없이 인라인 설정 생성
void UEnemyMeleePatternSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	for (TObjectPtr<UEnemyMeleeTraceSettings>& Settings : MeleeTraceSettings)
		if (!Settings) Settings = NewObject<UEnemyMeleeTraceSettings>(this, NAME_None, RF_Transactional);
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
