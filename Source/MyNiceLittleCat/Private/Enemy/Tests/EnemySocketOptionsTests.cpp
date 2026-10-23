#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Enemy/Datas/EnemyMeleePatternSettings.h"
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

	return true;
}
#endif
