// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.21.
* 플레이어와 짝 펫을 찾는 공용 도우미
* - 스테이지 연출, 보스 전투, 기믹이 같은 규칙으로 찾도록 한 곳에 모음
* - 리스폰으로 Pawn이 바뀌므로 저장하지 않고 필요할 때마다 찾음
*/

#pragma once

#include "CoreMinimal.h"

class UObject;
class APlayerCharacter;
class APetCharacter;

namespace PlayerLookup
{
	// 첫 번째 로컬 플레이어의 캐릭터: 없거나 죽었으면 nullptr
	MYNICELITTLECAT_API APlayerCharacter* GetAlivePlayer(const UObject* WorldContext);

	// 첫 번째 로컬 플레이어 컨트롤러에 등록된 짝 펫: 없으면 nullptr
	MYNICELITTLECAT_API APetCharacter* GetPartnerPet(const UObject* WorldContext);
}
