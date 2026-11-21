// Fill out your copyright notice in the Description page of Project Settings.
/*
* 작성자: 전선현
* 작성 일자: 2026.11.19.
* 연출 로그 분류
* - LogCinematic: 시네마틱 서브시스템, Play Cinematic 노드, 연출 데칼
* - LogStage: 스테이지 연출 담당자 (보스방 연출, 오프닝, 엔딩)
*/

#pragma once

#include "Logging/LogMacros.h"

// 시퀀스 재생, 바인딩, 마커, 스킵
DECLARE_LOG_CATEGORY_EXTERN(LogCinematic, Log, All);

// 스테이지 연출 흐름
DECLARE_LOG_CATEGORY_EXTERN(LogStage, Log, All);
