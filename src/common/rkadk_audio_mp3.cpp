#include "rkadk_audio_mp3.h"
#include "rkadk_param.h"
extern "C" {
#include "mp3_enc_types.h"
#include "mp3_enc_table1.h"
#include "mp3_enc_table2.h"
#include "test_mp3_register.h"
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct _RKAencMp3Context {
  RK_S32 s32FrameLength;
  RK_U32 u32FrameSize;
  RK_S32 s32SampleRate;
  RK_S32 s32Channel;
  RK_S32 s32Bitwidth;
  RK_S32 s32Bitrate;
} RKAencMp3Context;

typedef struct _rkTEST_AENC_CTX_S {
  const char *srcFilePath;
  const char *dstFilePath;
  RK_S32 s32LoopCount;
  RK_S32 s32ChnNum;
  RK_S32 s32SampleRate;
  RK_S32 s32Channel;
  RK_S32 s32Bitrate;
  RK_S32 s32Format;
  RK_S32 s32MilliSec;
  RK_S32 s32ChnIndex;
  RK_S32 s32FrameSize;
  RK_S32 s32NbSamples;
  RK_S32 s32ExtCodecHandle;
  AENC_CHN_ATTR_S *pstChnAttr;
} TEST_AENC_CTX_S;

TEST_AENC_CTX_S *g_pMP3Ctx;
static RKADK_S32 u32MP3InitCnt = 0;

RK_S32 RKAduioMp3EncoderOpen(RK_VOID *pEncoderAttr, RK_VOID **ppEncoder) {
  if (pEncoderAttr == NULL) {
    RK_LOGE("pEncoderAttr is NULL");
    return RK_FAILURE;
  }

  AENC_ATTR_CODEC_S *attr = (AENC_ATTR_CODEC_S *)pEncoderAttr;
  if(attr->enType != RK_AUDIO_ID_MP3) {
    RK_LOGE("Invalid enType[%d]", attr->enType);
    return RK_FAILURE;
  }

  RKAencMp3Context *ctx = (RKAencMp3Context *)malloc(sizeof(RKAencMp3Context));
  if (!ctx) {
    RKADK_LOGE("malloc aenc context ctx failed, bufsize = %d", sizeof(RKAencMp3Context));
    return RK_FAILURE;
  }

  memset(ctx, 0, sizeof(RKAencMp3Context));

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
    RK_LOGE("Unsupported enBitwidth %d", attr->enBitwidth);
    goto __FAILED;
  }

  ctx->s32Bitrate = attr->u32Resv[1] / 1000;
  RK_LOGD("MP3Encode: sample_rate = %d, channel = %d, bitrate = %d.", ctx->s32SampleRate, ctx->s32Channel, ctx->s32Bitrate);
  ctx->u32FrameSize = Mp3EncodeVariableInit(ctx->s32SampleRate, ctx->s32Channel, ctx->s32Bitrate);
  if (ctx->u32FrameSize <= 0){
    RK_LOGE("MP3Encode init failed! r:%d c:%d\n", ctx->s32SampleRate, ctx->s32Channel);
    return -1;
  }

  RK_LOGD("MP3Encode FrameSize = %d", ctx->u32FrameSize);
  *ppEncoder = (RK_VOID *)ctx;

  return RK_SUCCESS;

__FAILED:
  RKAduioMp3EncoderClose((RK_VOID *)ctx);
  *ppEncoder = RK_NULL;
  return RK_FAILURE;
}

RK_S32 RKAduioMp3EncoderClose(RK_VOID *pEncoder) {
  RKAencMp3Context *ctx = (RKAencMp3Context *)pEncoder;

  if (ctx == NULL)
    return RK_SUCCESS;

  free(ctx);
  ctx = NULL;
  return RK_SUCCESS;
}

RK_S32 RKAduioMp3EncoderEncode(RK_VOID *pEncoder, RK_VOID *pEncParam) {
  RKAencMp3Context *ctx = (RKAencMp3Context *)pEncoder;
  AUDIO_ADENC_PARAM_S *pParam = (AUDIO_ADENC_PARAM_S *)pEncParam;

  if(ctx == NULL || pParam == NULL) {
    RK_LOGE("Invalid ctx or pParam");
    return AENC_ENCODER_ERROR;
  }

  RK_S32 encoded_size = 0;
  RK_U8 *inData = pParam->pu8InBuf;
  RK_U64 inPts = pParam->u64InTimeStamp;
  RK_U32 inbufSize = 0;
  RK_U32 copySize = 0;

  // if input buffer is NULL, this means eos(end of stream)
  if (inData == NULL) {
    pParam->u64OutTimeStamp = inPts;
  }
  inbufSize = 2 * ctx->s32FrameLength;
  copySize = (pParam->u32InLen > inbufSize) ? inbufSize : pParam->u32InLen;
  memcpy(&in_buf, inData, copySize);
  pParam->u32InLen = pParam->u32InLen - copySize;
  encoded_size = L3_compress(ctx->u32FrameSize, &out_ptr);

  memcpy(pParam->pu8OutBuf, out_ptr, encoded_size);
  pParam->u64OutTimeStamp = inPts;
  pParam->u32OutLen = encoded_size;

  return AENC_ENCODER_OK;
}

RK_S32 RegisterAencMp3(void) {
  if (!u32MP3InitCnt) {
    int ret;
    AENC_ENCODER_S aencCtx;
    AUDIO_SOUND_MODE_E soundMode;
    AIO_ATTR_S stAiAttr;
    AENC_CHN_ATTR_S stAencAttr;
    RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = NULL;
    memset(&aencCtx, 0, sizeof(AENC_ENCODER_S));
    g_pMP3Ctx = (TEST_AENC_CTX_S *)malloc(sizeof(TEST_AENC_CTX_S));
    if (!g_pMP3Ctx) {
      RKADK_LOGE("malloc aenc ctx failed, bufsize = %d", sizeof(TEST_AENC_CTX_S));
      return RK_FAILURE;
    }

    memset(g_pMP3Ctx, 0, sizeof(TEST_AENC_CTX_S));

    pstAudioParam = RKADK_PARAM_GetAudioCfg();
    if (!pstAudioParam) {
      RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
      return -1;
    }

    g_pMP3Ctx->s32Channel = pstAudioParam->channels;
    g_pMP3Ctx->s32SampleRate = pstAudioParam->samplerate;
    g_pMP3Ctx->s32NbSamples = pstAudioParam->samples_per_frame;
    g_pMP3Ctx->s32Format = pstAudioParam->bit_width;
    g_pMP3Ctx->s32Bitrate = pstAudioParam->bitrate;
    g_pMP3Ctx->s32ExtCodecHandle = -1;

    aencCtx.enType = RK_AUDIO_ID_MP3;
    snprintf((RK_CHAR*)(aencCtx.aszName),
                                sizeof(aencCtx.aszName), "rkaudio");
    aencCtx.u32MaxFrmLen = 9216;
    aencCtx.pfnOpenEncoder = RKAduioMp3EncoderOpen;
    aencCtx.pfnEncodeFrm = RKAduioMp3EncoderEncode;
    aencCtx.pfnCloseEncoder = RKAduioMp3EncoderClose;

    RK_LOGD("register external aenc(%s)", aencCtx.aszName);
    ret = RK_MPI_AENC_RegisterEncoder(&g_pMP3Ctx->s32ExtCodecHandle, &aencCtx);
    if (ret != RK_SUCCESS) {
      RK_LOGE("aenc %s register decoder fail", aencCtx.aszName, ret);
      return RK_FAILURE;
    }
  }

  u32MP3InitCnt++;
  return RK_SUCCESS;
}

RK_S32 UnRegisterAencMp3(void) {
  if (g_pMP3Ctx == NULL || g_pMP3Ctx->s32ExtCodecHandle == -1) {
    return RK_SUCCESS;
  }
  if (0 == u32MP3InitCnt) {
    return 0;
  } else if (1 == u32MP3InitCnt) {
    RK_LOGD("unregister external aenc");
    RK_S32 ret = RK_MPI_AENC_UnRegisterEncoder(g_pMP3Ctx->s32ExtCodecHandle);
    if (ret != RK_SUCCESS) {
      RK_LOGE("aenc unregister decoder fail", ret);
      return RK_FAILURE;
    }

    g_pMP3Ctx->s32ExtCodecHandle = -1;
    if (g_pMP3Ctx) {
      free(g_pMP3Ctx);
      g_pMP3Ctx = NULL;
    }
  }
  u32MP3InitCnt--;
  return RK_SUCCESS;
}
