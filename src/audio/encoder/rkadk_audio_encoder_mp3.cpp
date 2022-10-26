/*
 * Copyright (c) 2022 Rockchip, Inc. All Rights Reserved.
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

#include "rkadk_audio_encoder_mp3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern "C" {
  #include "mp3_enc_types.h"
  #include "mp3_enc_table1.h"
  #include "mp3_enc_table2.h"
  #include "aenc_mp3_register.h"
}

typedef struct _RKADK_AENC_MP3_CTX_S {
  RKADK_S32 s32FrameLength;
  RKADK_U32 u32FrameSize;
  RKADK_S32 s32SampleRate;
  RKADK_S32 s32Channel;
  RKADK_S32 s32Bitwidth;
  RKADK_S32 s32Bitrate;
} RKADK_AENC_MP3_CTX_S;

static RKADK_S32 s32ExtCodecHandle;
static RKADK_U32 u32MP3InitCnt = 0;

RKADK_S32 RKAduioMp3EncoderOpen(RK_VOID *pEncoderAttr, RK_VOID **ppEncoder) {
  if (pEncoderAttr == NULL) {
    RKADK_LOGE("pEncoderAttr is NULL");
    return RKADK_FAILURE;
  }

  AENC_ATTR_CODEC_S *attr = (AENC_ATTR_CODEC_S *)pEncoderAttr;
  if(attr->enType != RK_AUDIO_ID_MP3) {
    RKADK_LOGE("Invalid enType[%d]", attr->enType);
    return RKADK_FAILURE;
  }

  RKADK_AENC_MP3_CTX_S *ctx = (RKADK_AENC_MP3_CTX_S *)malloc(sizeof(RKADK_AENC_MP3_CTX_S));
  if (!ctx) {
    RKADK_LOGE("malloc aenc mp3 ctx failed");
    return RKADK_FAILURE;
  }

  memset(ctx, 0, sizeof(RKADK_AENC_MP3_CTX_S));
  if (attr->u32Resv[0] > 1152) {
    RKADK_LOGE("error: MP3 FrameLength is too large, FrameLength = %d", attr->u32Resv[0]);
    goto __FAILED;
  }

  ctx->s32FrameLength = attr->u32Resv[0];
  ctx->s32SampleRate = attr->u32SampleRate;
  ctx->s32Channel = attr->u32Channels;

  switch(attr->enBitwidth) {
  case AUDIO_BIT_WIDTH_8:
    ctx->s32Bitwidth = 8;
    break;
  case AUDIO_BIT_WIDTH_16:
    ctx->s32Bitwidth = 16;
    break;
  case AUDIO_BIT_WIDTH_24:
    ctx->s32Bitwidth = 24;
    break;
  case AUDIO_BIT_WIDTH_32:
    ctx->s32Bitwidth = 32;
    break;
  default:
    RKADK_LOGE("Unsupported enBitwidth %d", attr->enBitwidth);
    goto __FAILED;
  }

  ctx->s32Bitrate = attr->u32Resv[1] / 1000;
  RKADK_LOGD("MP3Encode: sample_rate = %d, channel = %d, bitrate = %d.", ctx->s32SampleRate, ctx->s32Channel, ctx->s32Bitrate);
  ctx->u32FrameSize = Mp3EncodeVariableInit(ctx->s32SampleRate, ctx->s32Channel, ctx->s32Bitrate);
  if (ctx->u32FrameSize <= 0){
    RKADK_LOGE("MP3Encode init failed! r:%d c:%d\n", ctx->s32SampleRate, ctx->s32Channel);
    goto __FAILED;
  }

  RKADK_LOGD("MP3Encode FrameSize = %d", ctx->u32FrameSize);
  *ppEncoder = (RK_VOID *)ctx;

  return RKADK_SUCCESS;

__FAILED:
  RKAduioMp3EncoderClose((RK_VOID *)ctx);
  *ppEncoder = RK_NULL;
  return RKADK_FAILURE;
}

RKADK_S32 RKAduioMp3EncoderClose(RK_VOID *pEncoder) {
  RKADK_AENC_MP3_CTX_S *ctx = (RKADK_AENC_MP3_CTX_S *)pEncoder;

  if (ctx == NULL)
    return RKADK_SUCCESS;

  free(ctx);
  ctx = NULL;
  return RKADK_SUCCESS;
}

RKADK_S32 RKAduioMp3EncoderEncode(RK_VOID *pEncoder, RK_VOID *pEncParam) {
  RKADK_AENC_MP3_CTX_S *ctx = (RKADK_AENC_MP3_CTX_S *)pEncoder;
  AUDIO_ADENC_PARAM_S *pParam = (AUDIO_ADENC_PARAM_S *)pEncParam;

  if(ctx == NULL || pParam == NULL) {
    RKADK_LOGE("Invalid ctx or pParam");
    return AENC_ENCODER_ERROR;
  }

  RKADK_S32 encoded_size = 0;
  RKADK_U8 *inData = pParam->pu8InBuf;
  RKADK_U64 inPts = pParam->u64InTimeStamp;
  RKADK_U32 inbufSize = 0;
  RKADK_U32 copySize = 0;

  // if input buffer is NULL, this means eos(end of stream)
  if (inData == NULL) {
    pParam->u64OutTimeStamp = inPts;
  }

  inbufSize = 2 * ctx->s32FrameLength;
  copySize = (pParam->u32InLen > inbufSize) ? inbufSize : pParam->u32InLen;
  memcpy(&in_buf, inData, copySize);
  pParam->u32InLen = pParam->u32InLen - copySize;
  encoded_size = L3_compress(ctx->u32FrameSize, &out_ptr);

  encoded_size = (encoded_size > pParam->u32OutLen) ? pParam->u32OutLen : encoded_size;
  memcpy(pParam->pu8OutBuf, out_ptr, encoded_size);
  pParam->u64OutTimeStamp = inPts;
  pParam->u32OutLen = encoded_size;

  return AENC_ENCODER_OK;
}

RKADK_S32 RegisterAencMp3(void) {
  if (!u32MP3InitCnt) {
    RKADK_S32 ret;
    AENC_ENCODER_S aencCtx;
    AUDIO_SOUND_MODE_E soundMode;
    memset(&aencCtx, 0, sizeof(AENC_ENCODER_S));

    s32ExtCodecHandle = -1;
    aencCtx.enType = RK_AUDIO_ID_MP3;
    snprintf((RK_CHAR*)(aencCtx.aszName),
             sizeof(aencCtx.aszName), "rkaudio");
    aencCtx.u32MaxFrmLen = 9216;
    aencCtx.pfnOpenEncoder = RKAduioMp3EncoderOpen;
    aencCtx.pfnEncodeFrm = RKAduioMp3EncoderEncode;
    aencCtx.pfnCloseEncoder = RKAduioMp3EncoderClose;

    RKADK_LOGD("register external aenc(%s)", aencCtx.aszName);
    ret = RK_MPI_AENC_RegisterEncoder(&s32ExtCodecHandle, &aencCtx);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("aenc %s register decoder fail", aencCtx.aszName, ret);
      return RKADK_FAILURE;
    }
  }

  u32MP3InitCnt++;
  return RKADK_SUCCESS;
}

RKADK_S32 UnRegisterAencMp3(void) {
  if (s32ExtCodecHandle == -1) {
    return RKADK_SUCCESS;
  }

  if (0 == u32MP3InitCnt) {
    return 0;
  } else if (1 == u32MP3InitCnt) {
    RKADK_LOGD("unregister external aenc");
    RKADK_S32 ret = RK_MPI_AENC_UnRegisterEncoder(s32ExtCodecHandle);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("aenc unregister decoder fail", ret);
      return RKADK_FAILURE;
    }

    s32ExtCodecHandle = -1;
  }
  u32MP3InitCnt--;
  return RKADK_SUCCESS;
}
