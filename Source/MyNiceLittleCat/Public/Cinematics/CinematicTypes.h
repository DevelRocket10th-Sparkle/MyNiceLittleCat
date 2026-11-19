// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.16.
* 레벨 시퀀스 재생 요청에 사용하는 공통 타입
* - 보스전, 레벨 연출, 튜토리얼 등 어디서든 같은 형식으로 시퀀스 재생을 요청
* - 이 헤더는 Enemy, Stage 등 게임 코드를 전혀 참조하지 않음 (범용 모듈)
* - 시퀀스와 게임 객체는 "바인딩 태그"로만 연결: 이름 문자열로 바인딩을 찾지 않음
*/

#pragma once

#include "CoreMinimal.h"
#include "CinematicTypes.generated.h"

class ULevelSequence;

// 재생 요청 하나를 식별하는 핸들
// - int64를 그대로 쓰지 않고 구조체로 감싸서 패턴 요청 번호, 전환 번호와 섞어 쓰는 실수를 컴파일 단계에서 막음
// - FTimerHandle, FGameplayAbilitySpecHandle과 같은 방식
USTRUCT(BlueprintType)
struct FCinematicHandle
{
	GENERATED_BODY()

	// 0이면 유효하지 않은 핸들
	UPROPERTY()
	int64 Id = 0;

	// 유효한 재생 요청인지 확인
	bool IsValid() const { return Id != 0; }

	// 핸들 초기화
	void Reset() { Id = 0; }

	bool operator==(const FCinematicHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FCinematicHandle& Other) const { return Id != Other.Id; }
};

// 재생이 끝난 이유
// - 시작 실패는 Play()가 무효 핸들을 반환하는 것으로 알림 (콜백 없음)
// - 요청자가 직접 Cancel()한 경우도 콜백 없음
UENUM(BlueprintType)
enum class ECinematicEndReason : uint8
{
	Finished,		// 마지막 프레임까지 정상 재생
	Skipped,		// 플레이어가 스킵: 마지막 프레임을 평가한 뒤 정지
	Interrupted,	// 다른 시스템이 시퀀스 플레이어를 직접 정지하거나 플레이어/액터가 사라짐
	TimedOut		// 요청에 지정한 제한 시간 초과
};

// 재생 종료 후 시퀀스가 바꾼 액터 상태 처리 방식
UENUM(BlueprintType)
enum class ECinematicEndState : uint8
{
	UseSectionSettings,	// 섹션마다 지정한 완료 모드 사용 (예: 플레이어 위치 트랙만 KeepState)
	RestoreState,		// 모든 트랙을 재생 전 상태로 복원
	KeepState			// 모든 트랙의 마지막 상태 유지
};

// 시퀀스 바인딩 태그에 연결할 객체
// - 시퀀서 바인딩 우클릭 → Tags에서 붙인 태그와 같은 이름을 사용
// - 액터와 컴포넌트 모두 연결 가능: 컴포넌트면 부모(액터) 바인딩은 컴포넌트의 소유 액터로 함께 연결
USTRUCT(BlueprintType)
struct FCinematicBinding
{
	GENERATED_BODY()

	FCinematicBinding() = default;
	FCinematicBinding(FName InTag, UObject* InObject) : Tag(InTag), Object(InObject) {}

	// 시퀀서에서 바인딩에 붙인 태그 (예: Boss, Player)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	FName Tag;

	// 태그에 연결할 액터 또는 컴포넌트: 시퀀스에 해당 태그가 없으면 무시 (카메라만 쓰는 연출 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	TObjectPtr<UObject> Object;
};

// 시퀀스 재생 요청
USTRUCT(BlueprintType)
struct FCinematicRequest
{
	GENERATED_BODY()

	// 재생할 시퀀스
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	TObjectPtr<ULevelSequence> Sequence;

	// 태그로 연결할 객체 목록 (Boss, Player, 자물쇠 컴포넌트 등)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	TArray<FCinematicBinding> Bindings;

	// 시퀀스 키를 다른 좌표계(레벨 인스턴스 로컬 좌표 등)에서 작업한 경우 true
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bUseTransformOrigin = false;

	// 시퀀스 좌표계 → 월드 좌표계 변환
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "bUseTransformOrigin"))
	FTransform TransformOrigin;

	// 컷신 여부: 플레이어 이동/시점 입력 차단, HUD 숨김, 카메라 가림 처리 일시 정지
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	bool bCutscene = true;

	// 상호작용 키로 스킵 가능 여부 (컷신일 때만 의미 있음)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (EditCondition = "bCutscene"))
	bool bSkippable = true;

	// 종료 후 액터 상태 처리 방식
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic")
	ECinematicEndState EndState = ECinematicEndState::UseSectionSettings;

	// 제한 시간: 0 이하면 제한 없음
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cinematic", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;
};

// 시퀀스 마커 (Sequencer의 Marked Frame)
USTRUCT(BlueprintType)
struct FCinematicMarker
{
	GENERATED_BODY()

	// 마커 라벨 (예: Door:1:Open:0.8)
	UPROPERTY(BlueprintReadOnly, Category = "Cinematic")
	FString Label;

	// 재생 범위 시작 기준 시각 (초)
	UPROPERTY(BlueprintReadOnly, Category = "Cinematic")
	double Seconds = 0.0;

	// true: 재생 중 실제로 지나간 마커
	// false: 스킵, 제한 시간, 외부 정지로 지나치지 못해 종료 시점에 한꺼번에 전달된 마커
	// → 받는 쪽은 false일 때 애니메이션 없이 최종 상태만 즉시 적용
	UPROPERTY(BlueprintReadOnly, Category = "Cinematic")
	bool bReachedByPlayback = true;
};

// "명령:인자0:인자1:..." 형식의 마커 라벨 해석 도우미
// - 예: Door:1:Open:0.8 → 명령 "Door", 인자 [1, Open, 0.8]
// - 라벨 해석 규칙을 한 곳에 모아서 마커를 쓰는 쪽이 문자열을 직접 쪼개지 않게 함
struct MYNICELITTLECAT_API FCinematicMarkerArgs
{
public :

	explicit FCinematicMarkerArgs(const FString& Label);

	// 첫 부분이 지정한 명령인지 확인 (대소문자 무시)
	bool IsCommand(const TCHAR* Command) const;

	// 인자 문자열 반환: 없으면 빈 문자열
	const FString& GetArg(int32 ArgIndex) const;

	// 인자가 지정한 값과 같은지 확인 (대소문자 무시)
	bool ArgEquals(int32 ArgIndex, const TCHAR* Value) const;

	// 정수 인자 해석: 숫자가 아니면 false
	bool TryGetInt(int32 ArgIndex, int32& OutValue) const;

	// 실수 인자 해석: 없거나 숫자가 아니면 기본값
	float GetFloat(int32 ArgIndex, float DefaultValue) const;

private :

	// ':'로 나눈 라벨 조각 (0번은 명령)
	TArray<FString> Parts;
};

// 재생 중 마커를 지날 때, 또는 종료 시 남은 마커를 전달할 때 호출
DECLARE_DELEGATE_TwoParams(FOnCinematicMarker, FCinematicHandle /*Handle*/, const FCinematicMarker& /*Marker*/);

// 재생이 끝났을 때 한 번 호출 (요청자가 Cancel()한 경우는 호출하지 않음)
DECLARE_DELEGATE_TwoParams(FOnCinematicEnded, FCinematicHandle /*Handle*/, ECinematicEndReason /*Reason*/);
