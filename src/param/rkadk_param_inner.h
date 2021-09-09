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

#include "rkadk_media_comm.h"
#include "rkadk_param.h"

#ifdef __cplusplus
extern "C" {
#endif

RKADK_PARAM_CONTEXT_S *RKADK_PARAM_GetCtx(RKADK_VOID);

RKADK_PARAM_COMM_CFG_S *RKADK_PARAM_GetCommCfg();

RKADK_PARAM_REC_CFG_S *RKADK_PARAM_GetRecCfg(RKADK_U32 u32CamId);

RKADK_PARAM_STREAM_CFG_S *
RKADK_PARAM_GetStreamCfg(RKADK_U32 u32CamId, RKADK_STREAM_TYPE_E enStrmType);

RKADK_PARAM_PHOTO_CFG_S *RKADK_PARAM_GetPhotoCfg(RKADK_U32 u32CamId);

RKADK_PARAM_SENSOR_CFG_S *RKADK_PARAM_GetSensorCfg(RKADK_U32 u32CamId);

RKADK_PARAM_DISP_CFG_S *RKADK_PARAM_GetDispCfg(RKADK_U32 u32CamId);

RKADK_PARAM_AUDIO_CFG_S *RKADK_PARAM_GetAudioCfg(RKADK_VOID);

RKADK_PARAM_THUMB_CFG_S *RKADK_PARAM_GetThumbCfg(RKADK_VOID);

VENC_RC_MODE_E RKADK_PARAM_GetRcMode(char *rcMode,
                                     RKADK_CODEC_TYPE_E enCodecType);

RKADK_S32 RKADK_PARAM_GetRcParam(RKADK_PARAM_VENC_ATTR_S stVencAttr,
                                 VENC_RC_PARAM_S *pstRcParam);

RKADK_STREAM_TYPE_E RKADK_PARAM_VencChnMux(RKADK_U32 u32CamId,
                                           RKADK_U32 u32ChnId);

IMAGE_TYPE_E RKADK_PARAM_GetPixFmt(char *pixFmt);

#ifdef __cplusplus
}
#endif
#endif
