/// 작성자: 최요환
/// 작성일자: 2026-10-29
///
/// E로 당기는 레버입니다. 상호작용할 때마다 켜짐/꺼짐이 바뀌고(bToggleable 끄면 한 번만), LinkedActors에 신호를 보냅니다.
/// Mesh(루트) = 받침, 손잡이는 HandleMover 아래 HandleMesh에 붙입니다 (회전 중심 = HandleMover 위치).
/// 메시는 비어 있어 BP에서 지정합니다. 상태 저장(bSaveState)이 기본으로 켜져 있습니다.

#pragma once

#include "CoreMinimal.h"
#include "Gimmicks/InteractableBase.h"
#include "GimmickLever.generated.h"

class UGimmickMoverComponent;

UCLASS()
class MYNICELITTLECAT_API AGimmickLever : public AInteractableBase
{
	GENERATED_BODY()

public:
	AGimmickLever();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGimmickMoverComponent> HandleMover;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HandleMesh;
};
