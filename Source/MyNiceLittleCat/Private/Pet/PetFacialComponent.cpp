#include "Pet/PetFacialComponent.h"

UPetFacialComponent::UPetFacialComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPetFacialComponent::BeginPlay()
{
	Super::BeginPlay();

	Mesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
	BlinkTimer = FMath::FRandRange(BlinkIntervalRange.X, BlinkIntervalRange.Y);
}

void UPetFacialComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (nullptr == Mesh) return;

	// 감정 모프 보간
	// 현재 감정 1, 나머지 0으로 수렴
	for (uint8 i = 1; i <= (uint8)EPetEmotion::Dead; ++i)
	{
		const EPetEmotion Emo = (EPetEmotion)i;
		const float Target = (Emo == CurrentEmotion) ? 1.f : 0.f;

		float& Wei = EmotionWeights.FindOrAdd(Emo);
		Wei = FMath::FInterpTo(Wei, Target, DeltaTime, EmotionBlendSpeed);

		// 실제로 메시에 가중치 적용
		Mesh->SetMorphTarget(GetMorphName(Emo), Wei);
	}

	// 까암빠아아악
	UpdateBlink(DeltaTime);
}

void UPetFacialComponent::SetEmotion(EPetEmotion NewEmotion)
{
	// 값을 바꿔두면 Tick에서 알아서 부드럽게 전환됨
	CurrentEmotion = NewEmotion;
}

FName UPetFacialComponent::GetMorphName(EPetEmotion Emotion)
{
	switch (Emotion)
	{
	case EPetEmotion::Happy:   return TEXT("eyes_happy");
	case EPetEmotion::Sad:     return TEXT("eyes_sad");
	case EPetEmotion::Annoyed: return TEXT("eyes_annoyed");
	case EPetEmotion::Excited: return TEXT("eyes_excited");
	case EPetEmotion::Cry:     return TEXT("eyes_cry");
	case EPetEmotion::Sleep:   return TEXT("eyes_sleep");
	case EPetEmotion::Dead:    return TEXT("eyes_dead");
	default:                   return NAME_None;
	}
}

void UPetFacialComponent::UpdateBlink(float DeltaTime)
{
	// 자는 중이거나 죽은 상태라면 깜빡이지 않음
	if (EPetEmotion::Sleep == CurrentEmotion || EPetEmotion::Dead == CurrentEmotion)
	{
		Mesh->SetMorphTarget(TEXT("eyes_blink"), 0.f);
		return;
	}

	if (BlinkElapsed < 0.f)
	{
		// 대기 중 - 시간이 다 되면 깜빡임 시작
		BlinkTimer -= DeltaTime;
		if (BlinkTimer <= 0.f) BlinkElapsed = 0.f;
		return;
	}

	// 깜빡이는 중 0 -> 1 -> 0 싸인 반주기로 표현 (감았다 뜨는)
	BlinkElapsed += DeltaTime;
	const float Alpha = FMath::Clamp(BlinkElapsed / BlinkDuration, 0.f, 1.f);
	Mesh->SetMorphTarget(TEXT("eyes_blink"), FMath::Sin(Alpha * PI));

	if (Alpha >= 1.f)
	{
		// 끝나면 다음 깜빡임 예약
		BlinkElapsed = -1.f;
		BlinkTimer = FMath::FRandRange(BlinkIntervalRange.X, BlinkIntervalRange.Y);
	}
}

