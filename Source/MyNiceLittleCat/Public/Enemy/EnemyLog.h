// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* Enemy 로그 분류 (Enemy, 보스, 패턴, 페이즈, 보스방)
* - 맞을 때마다, 사망 단계마다 찍는 로그는 Verbose: 콘솔 "Log LogEnemy Verbose"로 켬
*/

#pragma once

#include "Logging/LogMacros.h"

// Enemy 공통 로그
DECLARE_LOG_CATEGORY_EXTERN(LogEnemy, Log, All);
