// 작성자: 강진구
// 작성일자: 2026-10-22
//
// 메인 위젯: 남은 목숨(오브 아이콘)과 락온 마커 표시
// 레이아웃은 WBP_MainWidget에서 구성 (BindWidget 이름 일치 필요)
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MainWidget.generated.h"

class UHorizontalBox;
class UImage;
class UTexture2D;
class UAbilitySystemComponent;
class APlayerCharacter;
struct FOnAttributeChangeData;

UCLASS()
class MYNICELITTLECAT_API UMainWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 표시 대상 플레이어 설정 (리스폰 시 다시 호출됨)
	void InitForPlayer(APlayerCharacter* InPlayer);

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void NativeDestruct() override;

	// 목숨 오브를 담는 박스
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UHorizontalBox> OrbBox;

	// 락온 대상 위에 표시할 마커 (Canvas Panel의 자식이어야 함)
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> LockOnMarker;

	// 목숨 오브 아이콘
	UPROPERTY(EditAnywhere, Category = "HUD|Orb")
	TObjectPtr<UTexture2D> OrbTexture;

	UPROPERTY(EditAnywhere, Category = "HUD|Orb")
	FVector2D OrbSize = FVector2D(32.f, 32.f);

	// 잃은 목숨 오브의 색 (반투명하게 표시)
	UPROPERTY(EditAnywhere, Category = "HUD|Orb")
	FLinearColor EmptyOrbColor = FLinearColor(1.f, 1.f, 1.f, 0.2f);

	// 대상 머리 위로 띄울 높이
	UPROPERTY(EditAnywhere, Category = "HUD|LockOn")
	float LockOnMarkerHeightOffset = 30.f;

private:
	void BindToPlayer();
	void UnbindFromPlayer();
	void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
	void RefreshOrbs();

	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<UAbilitySystemComponent> PlayerASC;

	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> Orbs;
};
