/*
 * Copyright (c) 2021 Rockchip, Inc. All Rights Reserved.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef __RKADK_PARAM_INNER_H__
#define __RKADK_PARAM_INNER_H__

#include "rkadk_common.h"
#include "rkadk_media_comm.h"
#include "rkadk_record.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RKADK_BUFFER_LEN 64

#define RECORD_AUDIO_CODEC_TYPE RKADK_CODEC_TYPE_MP3
#define RECORD_AI_CHN 0
#define RECORD_AENC_CHN 0
#define STREAM_AI_CHN RECORD_AI_CHN
#define STREAM_AENC_CHN 1

typedef struct tagRKADK_PARAM_VI_CFG_S {
  RKADK_U32 chn_id;
  char device_name[RKADK_BUFFER_LEN];
  RKADK_U32 buf_cnt;
  RKADK_U32 width;
  RKADK_U32 height;
} RKADK_PARAM_VI_CFG_S;

typedef struct tagRKADK_PARAM_COMM_CFG_S {
  RKADK_U32 sensor_count;
  bool enable_speaker;      /* speaker enable, default true */
  RKADK_U32 speaker_volume; /* speaker volume, [0,100] */
  bool mic_unmute;          /* 0:close mic(mute),  1:open mic(unmute) */
  RKADK_U32 mic_volume;     /* mic input volume, [0,100] */
  RKADK_U32 osd_time_format;
  bool osd;                 /* Whether to display OSD */
  bool boot_sound;          /* boot sound */
} RKADK_PARAM_COMM_CFG_S;

typedef struct tagRKADK_PARAM_SENSOR_CFG_S {
  RKADK_U32 max_width;
  RKADK_U32 max_height;
  RKADK_U32 framerate;
  bool enable_record; /* record  enable*/
  bool enable_photo;  /* photo enable, default true */
  bool flip;          /* FLIP */
  bool mirror;        /* MIRROR */
  RKADK_U32 ldc;      /* LDC level, [0,255]*/
  RKADK_U32 wdr;      /* WDR level, [0,10] */
  RKADK_U32 hdr;      /* hdr, [0: normal, 1: HDR2, 2: HDR3] */
  RKADK_U32 antifog;  /* antifog value, [0,10] */
} RKADK_PARAM_SENSOR_CFG_S;

typedef struct tagRKADK_PARAM_AUDIO_CFG_S {
  char audio_node[RKADK_BUFFER_LEN];
  SAMPLE_FORMAT_E sample_format;
  RKADK_U32 channels;
  RKADK_U32 samplerate;
  RKADK_U32 samples_per_frame;
  RKADK_U32 bitrate;
} RKADK_PARAM_AUDIO_CFG_S;

typedef struct tagRKADK_PARAM_VENC_ATTR_S {
  RKADK_U32 width;
  RKADK_U32 height;
  RKADK_U32 bitrate;
  RKADK_U32 gop;
  RKADK_U32 profile;
  RKADK_U32 venc_chn;
} RKADK_PARAM_VENC_ATTR_S;

typedef struct {
  RKADK_U32 u32ViChn;
  VI_CHN_ATTR_S stChnAttr;
} RKADK_PRAAM_VI_ATTR_S;

typedef struct tagRKADK_PARAM_REC_CFG_S {
  RKADK_CODEC_TYPE_E codec_type;
  RKADK_REC_TYPE_E record_type;
  RKADK_U32 record_time;
  RKADK_U32 splite_time;
  RKADK_U32 pre_record_time;
  RKADK_U32 lapse_interval;
  RKADK_U32 lapse_multiple;
  RKADK_U32 file_num;
  RKADK_PARAM_VENC_ATTR_S attribute[RECORD_FILE_NUM_MAX];
  RKADK_PRAAM_VI_ATTR_S vi_attr[RECORD_FILE_NUM_MAX];
} RKADK_PARAM_REC_CFG_S;

typedef struct tagRKADK_PARAM_STREAM_CFG_S {
  RKADK_PARAM_VENC_ATTR_S attribute;
  RKADK_PRAAM_VI_ATTR_S vi_attr;
} RKADK_PARAM_STREAM_CFG_S;

typedef struct tagRKADK_PARAM_PHOTO_CFG_S {
  RKADK_U32 image_width;
  RKADK_U32 image_height;
  RKADK_U32 snap_num;
  RKADK_U32 venc_chn;
  RKADK_PRAAM_VI_ATTR_S vi_attr;
} RKADK_PARAM_PHOTO_CFG_S;

typedef struct tagRKADK_PARAM_MEDIA_CFG_S {
  RKADK_PARAM_VI_CFG_S stViCfg[RKADK_ISPP_VI_NODE_CNT];
  RKADK_PARAM_REC_CFG_S stRecCfg;
  RKADK_PARAM_STREAM_CFG_S stStreamCfg;
  RKADK_PARAM_PHOTO_CFG_S stPhotoCfg;
} RKADK_PARAM_MEDIA_CFG_S;

typedef struct tagRKADK_PARAM_THUMB_CFG_S {
  RKADK_U32 thumb_width;
  RKADK_U32 thumb_height;
  RKADK_U32 venc_chn;
} RKADK_PARAM_THUMB_CFG_S;

typedef struct tagPARAM_CFG_S {
  RKADK_PARAM_COMM_CFG_S stCommCfg;
  RKADK_PARAM_AUDIO_CFG_S stAudioCfg;
  RKADK_PARAM_THUMB_CFG_S stThumbCfg;
  RKADK_PARAM_SENSOR_CFG_S stSensorCfg[RKADK_MAX_SENSOR_CNT];
  RKADK_PARAM_MEDIA_CFG_S stMediaCfg[RKADK_MAX_SENSOR_CNT];
} RKADK_PARAM_CFG_S;

/* Param Context */
typedef struct {
  bool bInit;                /* module init status */
  pthread_mutex_t mutexLock; /* param lock, protect pstCfg */
  RKADK_PARAM_CFG_S stCfg;   /* param config */
} RKADK_PARAM_CONTEXT_S;

RKADK_PARAM_CONTEXT_S *RKADK_PARAM_GetCtx(RKADK_VOID);

RKADK_PARAM_REC_CFG_S *RKADK_PARAM_GetRecCfg(RKADK_U32 u32CamId);

RKADK_PARAM_STREAM_CFG_S *RKADK_PARAM_GetStreamCfg(RKADK_U32 u32CamId);

RKADK_PARAM_PHOTO_CFG_S *RKADK_PARAM_GetPhotoCfg(RKADK_U32 u32CamId);

RKADK_PARAM_SENSOR_CFG_S *RKADK_PARAM_GetSensorCfg(RKADK_U32 u32CamId);

RKADK_PARAM_AUDIO_CFG_S *RKADK_PARAM_GetAudioCfg(RKADK_VOID);

RKADK_PARAM_THUMB_CFG_S *RKADK_PARAM_GetThumbCfg(RKADK_VOID);

#ifdef __cplusplus
}
#endif
#endif
