// Fill out your copyright notice in the Description page of Project Settings.


#include "Cinematics/CinematicTypes.h"

FCinematicMarkerArgs::FCinematicMarkerArgs(const FString& Label)
{
	// 빈 조각은 버림: "Door::Open" 같은 오타가 인자 위치를 밀지 않도록 함
	Label.ParseIntoArray(Parts, TEXT(":"), true);
}

bool FCinematicMarkerArgs::IsCommand(const TCHAR* Command) const
{
	return Parts.Num() > 0 && Parts[0].Equals(Command, ESearchCase::IgnoreCase);
}

const FString& FCinematicMarkerArgs::GetArg(int32 ArgIndex) const
{
	// 인자는 명령 다음 조각부터 시작
	const int32 PartIndex = ArgIndex + 1;

	static const FString Empty;
	return Parts.IsValidIndex(PartIndex) ? Parts[PartIndex] : Empty;
}

bool FCinematicMarkerArgs::ArgEquals(int32 ArgIndex, const TCHAR* Value) const
{
	return GetArg(ArgIndex).Equals(Value, ESearchCase::IgnoreCase);
}

bool FCinematicMarkerArgs::TryGetInt(int32 ArgIndex, int32& OutValue) const
{
	const FString& Arg = GetArg(ArgIndex);
	if (Arg.IsEmpty() || !Arg.IsNumeric())
	{
		return false;
	}

	OutValue = FCString::Atoi(*Arg);
	return true;
}

float FCinematicMarkerArgs::GetFloat(int32 ArgIndex, float DefaultValue) const
{
	const FString& Arg = GetArg(ArgIndex);
	if (Arg.IsEmpty() || !Arg.IsNumeric())
	{
		return DefaultValue;
	}

	// 음수 시간은 의미가 없으므로 0으로 보정
	return FMath::Max(0.f, FCString::Atof(*Arg));
}
