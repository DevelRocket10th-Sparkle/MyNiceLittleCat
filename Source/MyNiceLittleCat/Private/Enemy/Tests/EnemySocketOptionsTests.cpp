#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Enemy/Datas/EnemyPatternSetData.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemySocketOptionsTest, "MyNiceLittleCat.Enemy.SocketOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemySocketOptionsTest::RunTest(const FString& Parameters)
{
	UEnemyMeleeTraceSettings* Trace = NewObject<UEnemyMeleeTraceSettings>();
	UStaticMesh* Staff = NewObject<UStaticMesh>();
	UStaticMeshSocket* Tip = NewObject<UStaticMeshSocket>(Staff);
	Tip->SocketName = TEXT("StaffTip");
	Staff->AddSocket(Tip);
	Trace->SocketSourceMesh = Staff;
	struct { TArray<FName> ReturnValue; } Options;
	Trace->ProcessEvent(Trace->FindFunctionChecked(GET_FUNCTION_NAME_CHECKED(UEnemyMeleeTraceSettings, GetSocketOptions)), &Options);
	TestTrue(TEXT("Reflected GetOptions returns the inline object's sockets"), Options.ReturnValue.Contains(TEXT("StaffTip")));
	USkeleton* Skeleton = NewObject<USkeleton>();
	USkeletalMeshSocket* Hand = NewObject<USkeletalMeshSocket>(Skeleton);
	Hand->SocketName = TEXT("HandHit");
	Skeleton->Sockets.Add(Hand);
	USkeletalMesh* Body = NewObject<USkeletalMesh>();
	Body->SetSkeleton(Skeleton);
	USkeletalMeshSocket* MeshOnly = NewObject<USkeletalMeshSocket>(Body);
	MeshOnly->SocketName = TEXT("MeshOnlyHit");
	Body->GetMeshOnlySocketList().Add(MeshOnly);
	Trace->SocketSourceMesh = Body;
	TestTrue(TEXT("Skeletal mesh includes mesh-only sockets"), Trace->GetSocketOptions().Contains(TEXT("MeshOnlyHit")));
	TestTrue(TEXT("Skeletal mesh includes skeleton sockets"), Trace->GetSocketOptions().Contains(TEXT("HandHit")));
	Trace->SocketNames = {TEXT("HandHit"), TEXT("StaffTip")};
	FProperty* MeshProperty = FindFProperty<FProperty>(UEnemyMeleeTraceSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UEnemyMeleeTraceSettings, SocketSourceMesh));
	FPropertyChangedEvent MeshChanged(MeshProperty);
	Trace->PostEditChangeProperty(MeshChanged);
	TestEqual(TEXT("Changing mesh removes unavailable selections"), Trace->SocketNames.Num(), 1);
	Trace->SocketSourceMesh.Reset();
	Trace->PostEditChangeProperty(MeshChanged);
	TestTrue(TEXT("Clearing mesh clears selections"), Trace->SocketNames.IsEmpty());
	TestTrue(TEXT("No mesh offers no sockets"), Trace->GetSocketOptions().IsEmpty());

	UEnemyPatternSetData* Data = NewObject<UEnemyPatternSetData>();
	FEnemyPatternDefinition& Pattern = Data->Patterns.AddDefaulted_GetRef();
	FEnemyMeleeTraceDefinition& Old = Pattern.MeleeTraces.AddDefaulted_GetRef();
	Old.SourceName = TEXT("Weapon");
	Old.SocketNames = {TEXT("StaffTip")};
	Old.SocketSourceMesh = Staff;
	Old.Radius = 37.f;
	Data->PostLoad();
	UEnemyMeleePatternSettings* Melee = Cast<UEnemyMeleePatternSettings>(Pattern.ExecutionSettings);
	TestNotNull(TEXT("Legacy trace migrates to GA-specific execution settings"), Melee);
	if (!Melee) return false;
	TestEqual(TEXT("Legacy trace migrates to an inline object"), Melee->MeleeTraceSettings.Num(), 1);
	if (!Melee->MeleeTraceSettings.IsEmpty())
	{
		const UEnemyMeleeTraceSettings* Migrated = Melee->MeleeTraceSettings[0];
		TestEqual(TEXT("Migration retains source"), Migrated->SourceName, FName(TEXT("Weapon")));
		TestEqual(TEXT("Migration retains sockets"), Migrated->SocketNames[0], FName(TEXT("StaffTip")));
		TestEqual(TEXT("Migration retains radius"), Migrated->Radius, 37.f);
		TestTrue(TEXT("Migration retains mesh"), Migrated->SocketSourceMesh.Get() == Staff);
		TestTrue(TEXT("Inline traces belong to the melee settings"), Migrated->GetOuter() == Melee);
	}
	TestTrue(TEXT("Legacy array is cleared after migration"), Pattern.MeleeTraces.IsEmpty());
	Data->PostLoad();
	TestEqual(TEXT("Migration does not duplicate settings"), Melee->MeleeTraceSettings.Num(), 1);
	TestTrue(TEXT("Legacy visible array is cleared"), Pattern.MeleeTraceSettings.IsEmpty());
	TestTrue(TEXT("Migration keeps the new settings object"), Pattern.ExecutionSettings == Melee);
	return true;
}
#endif
