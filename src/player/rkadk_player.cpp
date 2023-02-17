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

#include "rkadk_common.h"
#include "rkadk_log.h"
#include "rkadk_param.h"
#include "rkadk_media_comm.h"
#include "rkadk_player.h"
#include "rkadk_demuxer.h"
#include "rkadk_audio_decoder.h"
#include "rk_debug.h"
#include "rk_defines.h"
#include <math.h>
#ifdef RV1126_1109
#include "rk_comm_vdec.h"
#endif
#include "rk_mpi_ao.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "rkdemuxer.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/types.h>
#include <fcntl.h>

typedef enum {
  RKADK_PLAYER_PAUSE_FALSE = 0x0,
  RKADK_PLAYER_PAUSE_START,
} RKADK_PLAYER_PAUSE_STATUS_E;

typedef enum {
  RKADK_PLAYER_SEEK_FALSE = 0x0,
  RKADK_PLAYER_SEEK_WAIT,
  RKADK_PLAYER_SEEK_START,
  RKADK_PLAYER_SEEK_SEND,
} RKADK_PLAYER_SEEK_STATUS_E;

#ifdef RV1126_1109
typedef struct {
  const RKADK_CHAR *srcFileUri;
  const RKADK_CHAR *dstFilePath;
  RKADK_U32   srcWidth;
  RKADK_U32   srcHeight;
  RKADK_U32   chnIndex;
  RKADK_U32   chNum;
  RKADK_U32   inputMode;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_U32   compressMode;
  RKADK_U32   frameBufferCnt;
  RKADK_U32   extraDataSize;
  RKADK_U32   readSize;
  RKADK_S32   chnFd;
  MB_POOL     pool;
  RKADK_S32   outputPixFmt;
  RKADK_BOOL  bEnableDei;
  RKADK_BOOL  bEnableColmv;
} RKADK_PLAYER_VDEC_CTX_S;

typedef struct {
  RKADK_S32  enIntfSync;
  RKADK_U32  u32Width;
  RKADK_U32  u32Height;
} VO_Timing_S;

typedef struct {
  RKADK_U32 u32FrameBufferCnt;
  COMPRESS_MODE_E enCompressMode;
} VDEC_CFG_S;

typedef struct {
  RKADK_U32 u32Screen0VoLayer;
  RKADK_U32 u32Screen1VoLayer;

  RKADK_U32 u32Screen0Rows;
  RKADK_U32 u32Screen1Rows;
  RK_BOOL bDoubleScreen;
} VO_CFG_S;

typedef struct {
  RKADK_U32 mode;
  VO_INTF_SYNC_E enIntfSync;
} VO_MODE_S;

typedef struct {
  RKADK_S32  VoDev;
  RKADK_S32  VoLayer;
  RKADK_S32  VoLayerMode;

  RKADK_S32  windows;
  RKADK_U32  enIntfType; /* 0 HDMI 1 edp */
  RKADK_U32  enIntfSync; /* VO_OUTPUT_1080P50 */
  RKADK_U32  x;
  RKADK_U32  y;

  RKADK_U32  dispWidth;
  RKADK_U32  dispHeight;
  RKADK_U32  imgeWidth;
  RKADK_U32  imageHeight;
  RKADK_U32  pixFormat;
  RKADK_U32  dispFrmRt;
  RKADK_U32  uEnMode;

  MIRROR_E    enMirror;
  ROTATION_E  enRotation;
  RKADK_BOOL  bUseRga;
} RKADK_PLAYER_VO_CTX_S;
#endif

typedef struct {
  RKADK_S32      chnNum;
  RKADK_S32      sampleRate;
  RKADK_S32      channel;
  RKADK_CODEC_TYPE_E eCodecType;
  RKADK_S32      decMode;
  RK_BOOL        bBlock;
  RKADK_S32      chnIndex;
  RKADK_S32      clrChnBuf;
} RKADK_PLAYER_ADEC_CTX_S;

typedef struct {
  const RKADK_CHAR *dstFilePath;
  RKADK_BOOL bopenChannelFlag;
  RKADK_S32 chnNum;
  RKADK_S32 sampleRate;
  RKADK_S32 reSmpSampleRate;
  RKADK_S32 channel;
  RKADK_S32 deviceChannel;
  RKADK_S32 bitWidth;
  RKADK_S32 devId;
  RKADK_S32 periodCount;
  RKADK_S32 periodSize;
  const RKADK_CHAR *cardName;
  RKADK_S32 chnIndex;
  RKADK_S32 setVolume;
  RKADK_S32 setMute;
  RKADK_S32 setFadeRate;
  RKADK_S32 setTrackMode;
  RKADK_S32 getVolume;
  RKADK_S32 getMute;
  RKADK_S32 getTrackMode;
  RKADK_S32 queryChnStat;
  RKADK_S32 pauseResumeChn;
  RKADK_S32 saveFile;
  RKADK_S32 queryFileStat;
  RKADK_S32 clrChnBuf;
  RKADK_S32 clrPubAttr;
  RKADK_S32 getPubAttr;
  RKADK_S32 openFlag;
} RKADK_PLAYER_AO_CTX_S;

typedef struct {
  pthread_t tidEof;
  pthread_t tidVideoSend;
  pthread_t tidAudioSend;
  pthread_t tidAudioCommand;
} RKADK_PLAYER_THREAD_PARAM_S;

typedef struct {
  #ifdef RV1126_1109
  RKADK_BOOL bEnableVideo;
  RKADK_BOOL bVideoExist;
  RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx;
  RKADK_PLAYER_VO_CTX_S *pstVoCtx;
  pthread_mutex_t PauseVideoMutex;
  #endif

  RKADK_BOOL bEnableAudio;
  RKADK_BOOL bAudioExist;
  RKADK_BOOL bStopFlag;
  RKADK_BOOL bEofFlag;
  RKADK_BOOL bWavEofFlag;
  RKADK_BOOL bAudioStopFlag;
  RKADK_BOOL bGetPtsFlag;
  RKADK_VOID *pListener;
  RKADK_VOID *pDemuxerCfg;
  RKADK_VOID *pAudioDecoder;
  RKADK_S8 pauseFlag;
  RKADK_S8 demuxerFlag;
  RKADK_S8 seekFlag;
  RKADK_S8 audioDecoderMode;
  RKADK_S64 seekTimeStamp;
  RKADK_S64 positionTimeStamp;
  RKADK_S64 videoTimeStamp;
  const RKADK_CHAR *pFilePath;
  RKADK_DEMUXER_INPUT_S *pstDemuxerInput;
  RKADK_DEMUXER_PARAM_S *pstDemuxerParam;
  RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx;
  RKADK_PLAYER_AO_CTX_S *pstAoCtx;
  RKADK_PLAYER_THREAD_PARAM_S stThreadParam;
  RKADK_PLAYER_EVENT_FN pfnPlayerCallback;
  pthread_mutex_t WavMutex;
  pthread_cond_t WavCond;
  pthread_mutex_t PauseAudioMutex;
} RKADK_PLAYER_HANDLE_S;

#ifdef RV1126_1109
static RKADK_S32 CreateVdecCtx(RKADK_PLAYER_VDEC_CTX_S **pVdecCtx) {
  RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx = (RKADK_PLAYER_VDEC_CTX_S *)malloc(sizeof(RKADK_PLAYER_VDEC_CTX_S));
  if (!pstVdecCtx) {
    RKADK_LOGE("malloc pstVdecCtx falied");
    return RKADK_FAILURE;
  }

  memset(pstVdecCtx, 0, sizeof(RKADK_PLAYER_VDEC_CTX_S));

  pstVdecCtx->inputMode = VIDEO_MODE_FRAME;
  pstVdecCtx->compressMode = COMPRESS_MODE_NONE;

  pstVdecCtx->frameBufferCnt = 3;
  pstVdecCtx->readSize = 1024;
  pstVdecCtx->chNum = 1;
  pstVdecCtx->chnIndex = 0;
  pstVdecCtx->bEnableColmv = RKADK_TRUE;
  pstVdecCtx->outputPixFmt = (RKADK_S32)RK_FMT_YUV420SP;
  pstVdecCtx->eCodecType = RKADK_CODEC_TYPE_H264;
  pstVdecCtx->srcWidth = 320;
  pstVdecCtx->srcHeight = 240;

  (*pVdecCtx) = (RKADK_PLAYER_VDEC_CTX_S *)pstVdecCtx;
  return RKADK_SUCCESS;
}

static RKADK_S32 VdecSetParam(RKADK_PLAYER_VDEC_CTX_S *pstVdecCtx) {
  RKADK_S32 ret = RKADK_SUCCESS;
  VDEC_CHN_ATTR_S stAttr;
  VDEC_CHN_PARAM_S stVdecParam;
  VDEC_PIC_BUF_ATTR_S stVdecPicBufAttr;
  MB_PIC_CAL_S stMbPicCalResult;
  VDEC_MOD_PARAM_S stModParam;

  memset(&stAttr, 0, sizeof(VDEC_CHN_ATTR_S));
  memset(&stVdecParam, 0, sizeof(VDEC_CHN_PARAM_S));
  memset(&stModParam, 0, sizeof(VDEC_MOD_PARAM_S));
  memset(&stVdecPicBufAttr, 0, sizeof(VDEC_PIC_BUF_ATTR_S));
  memset(&stMbPicCalResult, 0, sizeof(MB_PIC_CAL_S));

  RKADK_LOGI("found video width %d height %d pixfmt %d",
              pstVdecCtx->srcWidth, pstVdecCtx->srcHeight, pstVdecCtx->outputPixFmt);

  stVdecPicBufAttr.enCodecType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stVdecPicBufAttr.stPicBufAttr.u32Width = pstVdecCtx->srcWidth;
  stVdecPicBufAttr.stPicBufAttr.u32Height = pstVdecCtx->srcHeight;
  stVdecPicBufAttr.stPicBufAttr.enPixelFormat = (PIXEL_FORMAT_E)pstVdecCtx->outputPixFmt;
  stVdecPicBufAttr.stPicBufAttr.enCompMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;
  ret = RK_MPI_CAL_VDEC_GetPicBufferSize(&stVdecPicBufAttr, &stMbPicCalResult);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("get picture buffer size failed. err 0x%X", ret);
    goto __FAILED;
  }

  stAttr.enMode = VIDEO_MODE_FRAME;
  stAttr.enType = RKADK_MEDIA_GetRkCodecType(pstVdecCtx->eCodecType);
  stAttr.u32PicWidth = pstVdecCtx->srcWidth;
  stAttr.u32PicHeight = pstVdecCtx->srcHeight;
  if (pstVdecCtx->bEnableDei) {
    // iep must remain 5 buffers for deinterlace
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt + 5;
  } else
    stAttr.u32FrameBufCnt = pstVdecCtx->frameBufferCnt;

  stAttr.u32StreamBufCnt = MAX_STREAM_CNT;
  /*
    * if decode 10bit stream, need specify the u32FrameBufSize,
    * other conditions can be set to 0, calculated internally.
    */
  stAttr.u32FrameBufSize = stMbPicCalResult.u32MBSize;

  if (!pstVdecCtx->bEnableColmv)
    stAttr.stVdecVideoAttr.bTemporalMvpEnable = RK_FALSE;

  ret = RK_MPI_VDEC_CreateChn(pstVdecCtx->chnIndex, &stAttr);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("create %d vdec failed! ", pstVdecCtx->chnIndex);
    goto __FAILED;
  }

  stVdecParam.stVdecVideoParam.enCompressMode = (COMPRESS_MODE_E)pstVdecCtx->compressMode;

  if (pstVdecCtx->bEnableDei)
    stVdecParam.stVdecVideoParam.bDeiEn = (RK_BOOL)pstVdecCtx->bEnableDei;

  // it is only effective to disable MV when decoding sequence output
  if (!pstVdecCtx->bEnableColmv)
    stVdecParam.stVdecVideoParam.enOutputOrder = VIDEO_OUTPUT_ORDER_DEC;

  ret = RK_MPI_VDEC_SetChnParam(pstVdecCtx->chnIndex, &stVdecParam);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("set chn %d param failed %X! ", pstVdecCtx->chnIndex, ret);
    goto __FAILED;
  }

  pstVdecCtx->chnFd = RK_MPI_VDEC_GetFd(pstVdecCtx->chnIndex);
  if (pstVdecCtx->chnFd <= 0) {
    RKADK_LOGE("get fd chn %d failed %d", pstVdecCtx->chnIndex, pstVdecCtx->chnFd);
    goto __FAILED;
  }

  return RKADK_SUCCESS;
__FAILED:
  if (pstVdecCtx) {
    free(pstVdecCtx);
    pstVdecCtx = NULL;
  }

  return ret;
}

static RKADK_S32 DestroyVdec(RKADK_PLAYER_VDEC_CTX_S *ctx) {
  RKADK_S32 ret = 0;
  ret = RK_MPI_VDEC_StopRecvStream(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("stop Vdec stream failed, ret = %X\n", ret);
    return RKADK_FAILURE;
  }

  if (ctx->chnFd > 0) {
    ret = RK_MPI_VDEC_CloseFd(ctx->chnIndex);
    if (ret) {
      RKADK_LOGE("close Vdec fd failed, ret = %X\n", ret);
      return RKADK_FAILURE;
    }
  }

  ret = RK_MPI_VDEC_DestroyChn(ctx->chnIndex);
  if (ret) {
    RKADK_LOGE("destroy Vdec channel failed, ret = %X\n", ret);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static void RKADK_PLAYER_CheckParameter(RKADK_PLAYER_VO_CTX_S *pstVoCtx,
                                        RKADK_U32 u32Width, RKADK_U32 u32Height){
  RKADK_U32 sum = 0;

  if (pstVoCtx->x > u32Width ||
      pstVoCtx->x < 0) {
    RKADK_LOGE("Display x(%d) is out of width range(%d, %d), force to 0",
                pstVoCtx->x, u32Width, u32Height);
    pstVoCtx->x = 0;
  }

  if (pstVoCtx->y > u32Height ||
      pstVoCtx->y < 0) {
    RKADK_LOGE("Display y is out of width range(%d, %d), force to 0",
                pstVoCtx->y, u32Height);
    pstVoCtx->y = 0;
  }

  if (pstVoCtx->dispWidth <= 0 || pstVoCtx->dispWidth > u32Width) {
    RKADK_LOGE("\tDisplay width(%d) of is less than or equal to 0 or\n"
               "\tgreater the screen max width (%d),\n"
               "\tforce to the screen max width(%d)",
                pstVoCtx->dispWidth, u32Width, u32Width);
    pstVoCtx->dispWidth = u32Width;
  }

  if (pstVoCtx->dispHeight <= 0 || pstVoCtx->dispHeight > u32Height) {
    RKADK_LOGE("\tDisplay width(%d) of is less than or equal to 0 or\n"
               "\tgreater the screen max height(%d),\n"
               "\tforce to the screen max height(%d)",
                pstVoCtx->dispHeight, u32Height, u32Height);
    pstVoCtx->dispHeight = u32Height;
  }

  sum = pstVoCtx->x + pstVoCtx->dispWidth;
  if (sum > u32Width) {
    RKADK_LOGW("The sum(%d) of the display x coordinate(%d) of the VO \nand the width(%d) of "\
               "the display image is greater than the screen max width(%d)",
                sum, pstVoCtx->x, pstVoCtx->dispWidth, u32Width);

    pstVoCtx->dispWidth = u32Width - pstVoCtx->x;
  }

  sum = pstVoCtx->y + pstVoCtx->dispHeight;
  if (sum > u32Height) {
    RKADK_LOGW("The sum(%d) of the display y coordinate(%d) of the VO \nand the height(%d) of "\
               "the display image is greater than the screen max height(%d)",
                sum, pstVoCtx->y, pstVoCtx->dispHeight, MAX_VO_DISPLAY_HEIGTHT);

    pstVoCtx->dispHeight = u32Height - pstVoCtx->y;
  }

  pstVoCtx->imgeWidth = u32Width;
  pstVoCtx->imageHeight = u32Height;
}

static RKADK_S32 VoStopDev(VO_DEV VoDev) {
  RKADK_S32 ret = RKADK_SUCCESS;

  ret = RK_MPI_VO_Disable(VoDev);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_Disable failed, ret = %X\n", ret);
    return RKADK_FAILURE;
  }

  return ret;
}

static RKADK_S32 VoMultiWindownsStart(RKADK_PLAYER_VO_CTX_S *ctx) {
  VO_CHN_ATTR_S stChnAttr;
  VO_CHN_PARAM_S stChnParam;
  RKADK_U32 u32Row, u32Column;
  RKADK_U32 u32WinWidth, u32WinHeight;
  RKADK_U32 u32X, u32Y;
  RKADK_S32 ret = RKADK_SUCCESS;

  if (ctx->windows <= 2)
    u32Row = 1;
  else if (ctx->windows <= 4)
    u32Row = 2;
  else if (ctx->windows <= 9)
    u32Row = 3;
  else if (ctx->windows <= 16)
    u32Row = 4;
  else if (ctx->windows <= 36)
    u32Row = 6;
  else if (ctx->windows <= 64)
    u32Row = 8;
  else
    return RKADK_FAILURE;

  u32Column = ctx->windows / u32Row + ((ctx->windows % u32Row)? 1: 0);

  if (ret != RKADK_SUCCESS)
    return RKADK_FAILURE;

  u32WinWidth = ctx->dispWidth / u32Column;
  u32WinHeight = ctx->dispHeight / u32Row;

  for (RKADK_S32 i = 0; i < ctx->windows; i++) {
    u32X = i % u32Column;
    u32Y = i / u32Column;
    stChnAttr.stRect.u32Width = u32WinWidth;
    stChnAttr.stRect.u32Height = u32WinHeight;
    stChnAttr.stRect.s32X = ctx->x + u32X * stChnAttr.stRect.u32Width;
    stChnAttr.stRect.s32Y = ctx->y + u32Y * stChnAttr.stRect.u32Height;
    stChnAttr.u32FgAlpha = 255;
    stChnAttr.u32BgAlpha = 0;
    stChnAttr.enMirror = ctx->enMirror;
    stChnAttr.enRotation = ctx->enRotation;
    // set priority
    stChnAttr.u32Priority = 1;
    RKADK_LOGE("rect: [%d %d %d %d]",
              stChnAttr.stRect.s32X, stChnAttr.stRect.s32Y = ctx->y,
              stChnAttr.stRect.u32Width, stChnAttr.stRect.u32Height);

    ret = RK_MPI_VO_SetChnAttr(ctx->VoLayer, i, &stChnAttr);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_SetChnAttr failed, ret = %X\n", ret);
      return RKADK_FAILURE;
    }

    // set video aspect ratio
    if (ctx->uEnMode == 2) {
      stChnParam.stAspectRatio.enMode = ASPECT_RATIO_MANUAL;
      stChnParam.stAspectRatio.stVideoRect.s32X = ctx->x + u32X * u32WinWidth;
      stChnParam.stAspectRatio.stVideoRect.s32Y = ctx->y + u32Y * u32WinHeight;
      stChnParam.stAspectRatio.stVideoRect.u32Width = u32WinWidth/2;
      stChnParam.stAspectRatio.stVideoRect.u32Height = u32WinHeight/2;
      RK_MPI_VO_SetChnParam(ctx->VoLayer, i, &stChnParam);
    } else if (ctx->uEnMode == 1) {
      stChnParam.stAspectRatio.enMode = ASPECT_RATIO_AUTO;
      stChnParam.stAspectRatio.stVideoRect.s32X = ctx->x + u32X * u32WinWidth;
      stChnParam.stAspectRatio.stVideoRect.s32Y = ctx->y + u32Y * u32WinHeight;
      stChnParam.stAspectRatio.stVideoRect.u32Width = u32WinWidth;
      stChnParam.stAspectRatio.stVideoRect.u32Height = u32WinHeight;
      RK_MPI_VO_SetChnParam(ctx->VoLayer, i, &stChnParam);
    }

    ret = RK_MPI_VO_EnableChn(ctx->VoLayer, i);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_EnableChn failed, ret = %X\n", ret);
      return RKADK_FAILURE;
    }
  }

  return ret;
}

static RKADK_S32 VoMultiWindownsStop(VO_LAYER VoLayer, RKADK_U32 windows) {
  RKADK_S32 ret = RKADK_SUCCESS;

  for (RKADK_U32 i = 0; i < windows; i++) {
    ret = RK_MPI_VO_DisableChn(VoLayer, i);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_VO_DisableChn failed, ret = %X\n", ret);
      return RKADK_FAILURE;
    }
  }

  return ret;
}

static RKADK_S32 VoStartLayer(VO_LAYER VoLayer, const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr,
                                RKADK_BOOL bUseRga) {
  RKADK_S32 ret = RKADK_SUCCESS;

  ret = RK_MPI_VO_SetLayerAttr(VoLayer, pstLayerAttr);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_SetLayerAttr fail = %X", ret);
    return RKADK_FAILURE;
  }

  if (bUseRga)
    RK_MPI_VO_SetLayerSpliceMode(VoLayer, VO_SPLICE_MODE_RGA);

  return ret;
}

static RKADK_S32 VoStopLayer(VO_LAYER VoLayer) {
  RKADK_S32 ret = RKADK_SUCCESS;

  ret = RK_MPI_VO_DisableLayer(VoLayer);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_DisableLayer fail = %X", ret);
    return RKADK_FAILURE;
  }

  return ret;
}

static RKADK_S32 VoStartDev(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr) {
  RKADK_S32 ret = RKADK_SUCCESS;

  ret = RK_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_SetPubAttr fail = %X", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_VO_Enable(VoDev);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_Enable fail = %X", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_VO_GetPubAttr(VoDev, pstPubAttr);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_GetPubAttr failed, ret = %x", ret);
    return ret;
  }

  return ret;

}

static RKADK_S32 VoDevStart(RKADK_PLAYER_VO_CTX_S *pstVoCtx) {
  RKADK_S32                ret = RKADK_SUCCESS;
  VO_LAYER_MODE_E          Vo_layer_mode;
  VO_VIDEO_LAYER_ATTR_S    stLayerAttr;
  VO_PUB_ATTR_S            stPubAttr;

  memset(&stPubAttr, 0, sizeof(VO_PUB_ATTR_S));
  memset(&stLayerAttr, 0, sizeof(VO_VIDEO_LAYER_ATTR_S));

  /* Bind Layer */
  switch (pstVoCtx->VoLayerMode) {
    case 0:
      Vo_layer_mode = VO_LAYER_MODE_CURSOR;
      break;
    case 1:
      Vo_layer_mode = VO_LAYER_MODE_GRAPHIC;
      break;
    case 2:
      Vo_layer_mode = VO_LAYER_MODE_VIDEO;
      break;
    default:
      Vo_layer_mode = VO_LAYER_MODE_VIDEO;
  }

  ret = RK_MPI_VO_BindLayer(pstVoCtx->VoLayer, pstVoCtx->VoDev, Vo_layer_mode);
  if (ret != RK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_BindLayer failed, ret = %x", ret);
    return ret;
  }

  switch (pstVoCtx->enIntfType) {
    case DISPLAY_TYPE_VGA:
      stPubAttr.enIntfType = VO_INTF_VGA;
      break;
    case DISPLAY_TYPE_HDMI:
      stPubAttr.enIntfType = VO_INTF_HDMI;
      break;
    case DISPLAY_TYPE_EDP:
      stPubAttr.enIntfType = VO_INTF_EDP;
      break;
    case DISPLAY_TYPE_DP:
      stPubAttr.enIntfType = VO_INTF_DP;
      break;
    case DISPLAY_TYPE_HDMI_EDP:
      stPubAttr.enIntfType = VO_INTF_HDMI | VO_INTF_EDP;
      break;
    case DISPLAY_TYPE_MIPI:
      stPubAttr.enIntfType = VO_INTF_MIPI;
      break;
    default:
      stPubAttr.enIntfType = VO_INTF_DEFAULT;
      RKADK_LOGE("IntfType not set,use INTF_HDMI default");
  }

  stPubAttr.enIntfSync = VO_OUTPUT_DEFAULT;

  ret = VoStartDev(pstVoCtx->VoDev, &stPubAttr);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("Vo start dev fail = %X", ret);
    return RKADK_FAILURE;
  }

  RKADK_PLAYER_CheckParameter(pstVoCtx, stPubAttr.stSyncInfo.u16Hact,
                              stPubAttr.stSyncInfo.u16Vact);

  /* Enable Layer */ //RK_FMT_RGBA8888
  stLayerAttr.enPixFormat           = (PIXEL_FORMAT_E)pstVoCtx->pixFormat;
  stLayerAttr.stDispRect.s32X       = pstVoCtx->x;
  stLayerAttr.stDispRect.s32Y       = pstVoCtx->y;
  stLayerAttr.stDispRect.u32Width   = pstVoCtx->dispWidth;
  stLayerAttr.stDispRect.u32Height  = pstVoCtx->dispHeight;
  stLayerAttr.stImageSize.u32Width  = stPubAttr.stSyncInfo.u16Hact;
  stLayerAttr.stImageSize.u32Height = stPubAttr.stSyncInfo.u16Vact;
  stLayerAttr.u32DispFrmRt          = pstVoCtx->dispFrmRt;
  stLayerAttr.bBypassFrame          = RK_FALSE;

  ret = VoStartLayer(pstVoCtx->VoLayer, &stLayerAttr, pstVoCtx->bUseRga);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("Vo start layer fail = %X", ret);
    return RKADK_FAILURE;
  }

  RKADK_LOGI("Vo [dev: %d, layer: %d] start success",
              pstVoCtx->VoDev, pstVoCtx->VoLayer);

  return RKADK_SUCCESS;
}

static RKADK_S32 VoSetParam(RKADK_PLAYER_VO_CTX_S *pstVoCtx, RKADK_PLAYER_FRAMEINFO_S *pstFrameInfo,
                            RKADK_S32 videoAvgFrameRate) {
  RKADK_S32 ret = RKADK_SUCCESS, sum = 0;

  pstVoCtx->VoDev = pstFrameInfo->u32VoDev;
  pstVoCtx->dispFrmRt = videoAvgFrameRate;
  pstVoCtx->enIntfType = pstFrameInfo->u32EnIntfType;
  pstVoCtx->uEnMode = pstFrameInfo->u32EnMode;
  pstVoCtx->enIntfSync = pstFrameInfo->enIntfSync;
  if (pstVoCtx->enIntfSync >= VO_OUTPUT_BUTT) {
    RKADK_LOGE("IntfSync(%d) if unsupport", pstVoCtx->enIntfSync);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->u32FrmInfoX > DISP_WIDTH ||
      pstFrameInfo->u32FrmInfoX < 0) {
    RKADK_LOGE("Display x coordinate(%d) of VO is out of width range(%d)",
                pstFrameInfo->u32FrmInfoX, DISP_WIDTH);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->u32FrmInfoY > DISP_HEIGHT ||
      pstFrameInfo->u32FrmInfoY < 0) {
    RKADK_LOGE("Display y coordinate(%d) of VO is out of height range(%d)",
                pstFrameInfo->u32FrmInfoY, DISP_WIDTH);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->u32DispWidth <= 0) {
    RKADK_LOGE("Display width(%d) of VO is less than or equal to 0",
                pstFrameInfo->u32DispWidth);
    return RKADK_FAILURE;
  }

  if (pstFrameInfo->u32DispWidth <= 0) {
    RKADK_LOGE("Display height(%d) of VO is less than or equal to 0",
                pstFrameInfo->u32DispHeight);
    return RKADK_FAILURE;
  }

  sum = pstFrameInfo->u32FrmInfoX + pstFrameInfo->u32DispWidth;
  if (sum > DISP_WIDTH) {
    RKADK_LOGW("The sum(%d) of the display x coordinate(%d) of the VO \nand the width(%d) of "\
               "the display image is greater than the screen max width(%d)",
                sum, pstFrameInfo->u32FrmInfoX, pstFrameInfo->u32DispWidth, DISP_WIDTH);

    pstFrameInfo->u32DispWidth = DISP_WIDTH - pstFrameInfo->u32FrmInfoX;
  }

  sum = pstFrameInfo->u32FrmInfoY + pstFrameInfo->u32DispHeight;
  if (sum > DISP_HEIGHT) {
    RKADK_LOGW("The sum(%d) of the display y coordinate(%d) of the VO \nand the height(%d) of "\
               "the display image is greater than the screen max height(%d)",
                sum, pstFrameInfo->u32FrmInfoY, pstFrameInfo->u32DispHeight, DISP_HEIGHT);

    pstFrameInfo->u32DispHeight = DISP_HEIGHT - pstFrameInfo->u32FrmInfoY;
  }

  pstVoCtx->x = pstFrameInfo->u32FrmInfoX;
  pstVoCtx->y = pstFrameInfo->u32FrmInfoY;
  pstVoCtx->dispWidth = pstFrameInfo->u32DispWidth;
  pstVoCtx->dispHeight = pstFrameInfo->u32DispHeight;

  switch (pstFrameInfo->u32VoFormat) {
    case VO_FORMAT_RGB888:
      pstVoCtx->pixFormat = RK_FMT_RGB888;
      break;
    default:
      RKADK_LOGW("unsupport pix format: %d, use default(%d)", pstVoCtx->pixFormat, RK_FMT_RGB888);
      pstVoCtx->pixFormat = RK_FMT_RGB888;
  }

  if (pstFrameInfo->bMirror && !pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_HORIZONTAL;
  else if (pstFrameInfo->bFlip && !pstFrameInfo->bMirror)
    pstVoCtx->enMirror = MIRROR_VERTICAL;
  else if (pstFrameInfo->bMirror && pstFrameInfo->bFlip)
    pstVoCtx->enMirror = MIRROR_BOTH;
  else
    pstVoCtx->enMirror = MIRROR_NONE;

  pstVoCtx->enRotation = (ROTATION_E)pstFrameInfo->u32Rotation;

  return ret;
}

static RKADK_S32 VoStart(RKADK_PLAYER_VO_CTX_S *pstVoCtx) {
  RKADK_S32 ret = RK_MPI_VO_EnableLayer(pstVoCtx->VoLayer);
  if (ret != RKADK_SUCCESS) {
    RKADK_LOGE("RK_MPI_VO_EnableLayer fail = %X", ret);
    return RKADK_FAILURE;
  }

  if (VoMultiWindownsStart(pstVoCtx)) {
    RKADK_LOGE("Vo start windows failed");
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 CreateVoCtx(RKADK_PLAYER_VO_CTX_S **pVoCtx) {
  RKADK_PLAYER_VO_CTX_S *pstVoCtx = (RKADK_PLAYER_VO_CTX_S *)malloc(sizeof(RKADK_PLAYER_VO_CTX_S));
  if (!pstVoCtx) {
    RKADK_LOGE("malloc pstVoCtx falied");
    return RKADK_FAILURE;
  }

  memset(pstVoCtx, 0, sizeof(RKADK_PLAYER_VO_CTX_S));

  pstVoCtx->windows = 1;
  pstVoCtx->enIntfType = VO_INTF_MIPI;
  pstVoCtx->enIntfSync = VO_OUTPUT_DEFAULT;

  pstVoCtx->VoDev = VO_DEVICE;
  pstVoCtx->VoLayer = VOP_LAYER;
  pstVoCtx->VoLayerMode = VO_LAYER_MODE_GRAPHIC; /* CURSOR = 0,GRAPHIC = 1,VIDEO = 2,*/

  pstVoCtx->imgeWidth = DISP_WIDTH;
  pstVoCtx->imageHeight = DISP_HEIGHT;

  pstVoCtx->x = 0;
  pstVoCtx->y = 0;
  pstVoCtx->dispWidth  = DISP_WIDTH;
  pstVoCtx->dispHeight = DISP_HEIGHT;
  pstVoCtx->pixFormat = RK_FMT_RGB888;
  pstVoCtx->dispFrmRt = 60;
  pstVoCtx->uEnMode = 1;

  pstVoCtx->enMirror = MIRROR_NONE;
  pstVoCtx->enRotation = ROTATION_90;
  pstVoCtx->bUseRga = RKADK_TRUE;

  (*pVoCtx) = pstVoCtx;
  return RKADK_SUCCESS;
}

static RKADK_S32 DestroyVo(RKADK_PLAYER_VO_CTX_S *pstVoCtx) {
  RKADK_S32 ret = RKADK_SUCCESS;
  ret = RK_MPI_VO_UnBindLayer(pstVoCtx->VoLayer, pstVoCtx->VoDev);
  if (ret) {
    RKADK_LOGE("unbind VO layer failed");
  }

  RK_MPI_VO_CloseFd();

  return ret;
}
#endif

static RKADK_S32 CreateADECCtx(RKADK_PLAYER_ADEC_CTX_S **pAdecCtx) {
  RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx = (RKADK_PLAYER_ADEC_CTX_S *)malloc(sizeof(RKADK_PLAYER_ADEC_CTX_S));
  if (!pstAdecCtx) {
    RKADK_LOGE("malloc pstAdecCtx falied");
    return RKADK_FAILURE;
  }

  memset(pstAdecCtx, 0, sizeof(RKADK_PLAYER_ADEC_CTX_S));

  pstAdecCtx->chnIndex = 0;
  pstAdecCtx->sampleRate = 16000;
  pstAdecCtx->channel = 2;
  pstAdecCtx->decMode = ADEC_MODE_STREAM;
  pstAdecCtx->eCodecType = RKADK_CODEC_TYPE_MP3;
  pstAdecCtx->bBlock = RK_TRUE;

  (*pAdecCtx) = (RKADK_PLAYER_ADEC_CTX_S *)pstAdecCtx;
  return RKADK_SUCCESS;
}

static RKADK_S32 AdecSetParam(RKADK_PLAYER_ADEC_CTX_S *pstAdecCtx) {
  RKADK_S32 ret = 0;
  ADEC_CHN_ATTR_S pstChnAttr;
  memset(&pstChnAttr, 0, sizeof(ADEC_CHN_ATTR_S));
  pstChnAttr.stCodecAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.stCodecAttr.u32Channels = pstAdecCtx->channel;
  pstChnAttr.stCodecAttr.u32SampleRate = pstAdecCtx->sampleRate;
  pstChnAttr.stCodecAttr.u32BitPerCodedSample = 4;

  pstChnAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAdecCtx->eCodecType);
  pstChnAttr.enMode = (ADEC_MODE_E)pstAdecCtx->decMode;
  pstChnAttr.u32BufCount = 4;
  pstChnAttr.u32BufSize = 50 * 1024;

  ret = RK_MPI_ADEC_CreateChn(pstAdecCtx->chnIndex, &pstChnAttr);
  if (ret)
    RKADK_LOGE("create adec chn %d err:0x%X\n", pstAdecCtx->chnIndex, ret);

  return ret;
}

static RKADK_S32 CreateAOCtx(RKADK_PLAYER_AO_CTX_S **pAoCtx) {
  RKADK_PLAYER_AO_CTX_S *pstAoCtx = (RKADK_PLAYER_AO_CTX_S *)malloc(sizeof(RKADK_PLAYER_AO_CTX_S));
  if (!pstAoCtx) {
    RKADK_LOGE("malloc pstAoCtx falied");
    return RKADK_FAILURE;
  }

  memset(pstAoCtx, 0, sizeof(RKADK_PLAYER_AO_CTX_S));

  pstAoCtx->dstFilePath     = RKADK_NULL;
  pstAoCtx->chnNum          = 1;
  pstAoCtx->sampleRate      = AUDIO_SAMPLE_RATE;
  pstAoCtx->reSmpSampleRate = 0;
  pstAoCtx->deviceChannel   = AUDIO_DEVICE_CHANNEL;
  pstAoCtx->channel         = 2;
  pstAoCtx->bitWidth        = AUDIO_BIT_WIDTH;
  pstAoCtx->periodCount     = 2;
  pstAoCtx->periodSize      = AUDIO_FRAME_COUNT;
  pstAoCtx->cardName         = AI_DEVICE_NAME;
  pstAoCtx->devId           = 0;
  pstAoCtx->setVolume       = 100;
  pstAoCtx->setMute         = 0;
  pstAoCtx->setTrackMode    = 0;
  pstAoCtx->setFadeRate     = 0;
  pstAoCtx->getVolume       = 0;
  pstAoCtx->getMute         = 0;
  pstAoCtx->getTrackMode    = 0;
  pstAoCtx->queryChnStat    = 0;
  pstAoCtx->pauseResumeChn  = 0;
  pstAoCtx->saveFile        = 0;
  pstAoCtx->queryFileStat   = 0;
  pstAoCtx->clrChnBuf       = 0;
  pstAoCtx->clrPubAttr      = 0;
  pstAoCtx->getPubAttr      = 0;

  (*pAoCtx) = (RKADK_PLAYER_AO_CTX_S *)pstAoCtx;
  return RKADK_SUCCESS;
}

static RKADK_VOID QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 ret = 0;
  AO_CHN_STATE_S pstStat;
  memset(&pstStat, 0, sizeof(AO_CHN_STATE_S));
  ret = RK_MPI_AO_QueryChnStat(aoDevId, aoChn, &pstStat);
  if (ret == RKADK_SUCCESS) {
    RKADK_LOGI("query ao flow status:");
    RKADK_LOGI("total number of channel buffer : %d", pstStat.u32ChnTotalNum);
    RKADK_LOGI("free number of channel buffer : %d", pstStat.u32ChnFreeNum);
    RKADK_LOGI("busy number of channel buffer : %d", pstStat.u32ChnBusyNum);
  }
}

static AUDIO_SOUND_MODE_E FindSoundMode(RKADK_S32 ch) {
  AUDIO_SOUND_MODE_E channel = AUDIO_SOUND_MODE_BUTT;
  switch (ch) {
    case 1:
      channel = AUDIO_SOUND_MODE_MONO;
      break;
    case 2:
      channel = AUDIO_SOUND_MODE_STEREO;
      break;
    default:
      RKADK_LOGE("channel = %d not support", ch);
      return AUDIO_SOUND_MODE_BUTT;
  }

  return channel;
}

static AUDIO_BIT_WIDTH_E FindBitWidth(RKADK_S32 bit) {
  AUDIO_BIT_WIDTH_E bitWidth = AUDIO_BIT_WIDTH_BUTT;
  switch (bit) {
    case 8:
      bitWidth = AUDIO_BIT_WIDTH_8;
      break;
    case 16:
      bitWidth = AUDIO_BIT_WIDTH_16;
      break;
    case 24:
      bitWidth = AUDIO_BIT_WIDTH_24;
      break;
    default:
      RKADK_LOGE("bitwidth(%d) not support", bit);
      return AUDIO_BIT_WIDTH_BUTT;
  }

  return bitWidth;
}

static RKADK_S32 OpenDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  RKADK_S32 ret = 0, bytes = 2; // if the requirement is 16bit
  AUDIO_DEV aoDevId = ctx->devId;
  AUDIO_SOUND_MODE_E soundMode;
  AIO_ATTR_S aoAttr;
  memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

  if (ctx->cardName) {
    snprintf((RKADK_CHAR *)(aoAttr.u8CardName),
              sizeof(aoAttr.u8CardName), "%s", ctx->cardName);
  }

  aoAttr.soundCard.channels = ctx->deviceChannel;
  aoAttr.soundCard.sampleRate = ctx->sampleRate;
  aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

  AUDIO_BIT_WIDTH_E bitWidth = FindBitWidth(ctx->bitWidth);
  if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
    RKADK_LOGE("audio bitWidth unsupport, bitWidth = %d", bitWidth);
    return RKADK_FAILURE;
  }

  bytes = ctx->bitWidth / 8;
  aoAttr.enBitwidth = bitWidth;
  aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->reSmpSampleRate;
  soundMode = FindSoundMode(ctx->channel);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    RKADK_LOGE("audio soundMode unsupport, soundMode = %d", soundMode);
    return RKADK_FAILURE;
  }

  aoAttr.enSoundmode = soundMode;
  aoAttr.u32FrmNum = ctx->periodCount;
  aoAttr.u32PtNumPerFrm = bytes * ctx->periodSize;

  aoAttr.u32EXFlag = 0;
  aoAttr.u32ChnCnt = 2;
  ret = RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_SetPubAttr fail, ret = %X", ret);
    return RKADK_FAILURE;
  }

  ret = RK_MPI_AO_Enable(aoDevId);
  if (ret != 0) {
    RKADK_LOGE("RK_MPI_AO_Enable fail, ret = %X", ret);
    return RKADK_FAILURE;
  }

  ctx->openFlag = 1;
  return RKADK_SUCCESS;
}

static RKADK_S32 InitMpiAO(RKADK_PLAYER_AO_CTX_S *params) {
  RKADK_S32 result;

  result = RK_MPI_AO_EnableChn(params->devId, params->chnIndex);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, aoChn = %d, reason = %X", params->chnIndex, result);
    return RKADK_FAILURE;
  }

  // set sample rate of input data
  result = RK_MPI_AO_EnableReSmp(params->devId, params->chnIndex,
                                (AUDIO_SAMPLE_RATE_E)params->reSmpSampleRate);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, reason = %X, aoChn = %d", result, params->chnIndex);
    return RKADK_FAILURE;
  }

  params->bopenChannelFlag = RKADK_TRUE;
  return RKADK_SUCCESS;
}

static RKADK_S32 DeInitMpiAO(AUDIO_DEV aoDevId, AO_CHN aoChn, RKADK_BOOL *openFlag) {
  if (*openFlag) {
    RKADK_S32 result = RK_MPI_AO_DisableReSmp(aoDevId, aoChn);
    if (result != 0) {
      RKADK_LOGE("ao disable resample fail, reason = %X", result);
      return RKADK_FAILURE;
    }

    result = RK_MPI_AO_DisableChn(aoDevId, aoChn);
    if (result != 0) {
      RKADK_LOGE("ao disable channel fail, reason = %X", result);
      return RKADK_FAILURE;
    }

    *openFlag = RKADK_FALSE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 CloseDeviceAO(RKADK_PLAYER_AO_CTX_S *ctx) {
  AUDIO_DEV aoDevId = ctx->devId;
  if (ctx->openFlag == 1) {
    RKADK_S32 result = RK_MPI_AO_Disable(aoDevId);
    if (result != 0) {
      RKADK_LOGE("ao disable fail, reason = %X", result);
      return RKADK_FAILURE;
    }
    ctx->openFlag = 0;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 SetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 result = 0;
  AO_CHN_PARAM_S pstParams;
  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));
  //aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0)
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  else if (aoChn == 1)
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;

  result = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (result != RKADK_SUCCESS) {
    RKADK_LOGE("ao set channel params, aoChn = %d", aoChn);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

static RKADK_S32 VdecPollEvent(RKADK_S32 timeoutMsec, RKADK_S32 fd) {
  RKADK_S32 num_fds = 1;
  struct pollfd pollFds[num_fds];
  RKADK_S32 ret = 0;

  RK_ASSERT(fd > 0);
  memset(pollFds, 0, sizeof(pollFds));
  pollFds[0].fd = fd;
  pollFds[0].events = (POLLPRI | POLLIN | POLLERR | POLLNVAL | POLLHUP);

  ret = poll(pollFds, num_fds, timeoutMsec);
  if (ret > 0 && (pollFds[0].revents & (POLLERR | POLLNVAL | POLLHUP))) {
    RKADK_LOGE("fd:%d polled error", fd);
    return -1;
  }

  return ret;
}

#ifdef RV1126_1109
static RKADK_VOID* SendVideoDataThread(RKADK_VOID *ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  RKADK_S32 windowCount = 0;
  VIDEO_FRAME_INFO_S sFrame;
  struct timespec t_begin, t_end;
  RKADK_S32 ret = 0;
  RKADK_S32 voSendTime = 0, frameTime = 0, costtime = 0;

  if (pstPlayer->pstDemuxerParam->videoAvgFrameRate <= 0) {
    RKADK_LOGE("video frame rate(%d) is out of range",
                pstPlayer->pstDemuxerParam->videoAvgFrameRate);
    return RKADK_NULL;
  }

  frameTime = 1000000 / pstPlayer->pstDemuxerParam->videoAvgFrameRate;
  memset(&sFrame, 0, sizeof(VIDEO_FRAME_INFO_S));
  pstPlayer->bGetPtsFlag = RKADK_TRUE;
  while (1) {
    if (pstPlayer->pauseFlag != RKADK_PLAYER_PAUSE_START) {
      if (pstPlayer->pstVdecCtx->chnFd > 0) {
        ret = VdecPollEvent(MAX_TIME_OUT_MS, pstPlayer->pstVdecCtx->chnFd);
        if (ret < 0)
          continue;
      }

      ret = RK_MPI_VDEC_GetFrame(pstPlayer->pstVdecCtx->chnIndex, &sFrame, MAX_TIME_OUT_MS);
      if (ret >= 0) {
        if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
          RK_MPI_VDEC_ReleaseFrame(pstPlayer->pstVdecCtx->chnIndex, &sFrame);
          RKADK_LOGI("chn %d reach eos frame.", pstPlayer->pstVdecCtx->chnIndex);
          break;
        }

        if (!pstPlayer->bStopFlag) {
          if (pstPlayer->pstVoCtx->VoLayer >= 0) {
            if (!pstPlayer->bAudioExist)
              pstPlayer->positionTimeStamp = sFrame.stVFrame.u64PTS;

            clock_gettime(CLOCK_MONOTONIC, &t_end);

            if (sFrame.stVFrame.u64PTS) {
              costtime = (t_end.tv_sec - t_begin.tv_sec) * 1000000 + (t_end.tv_nsec - t_begin.tv_nsec) / 1000;
              if (sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp > costtime) {
                voSendTime = sFrame.stVFrame.u64PTS - pstPlayer->videoTimeStamp - costtime;
                usleep(voSendTime);
              }
              voSendTime = frameTime;
            } else {
              voSendTime = frameTime;
            }
            pstPlayer->videoTimeStamp = sFrame.stVFrame.u64PTS;

            ret = RK_MPI_VO_SendFrame(pstPlayer->pstVoCtx->VoLayer, windowCount, &sFrame, -1);
            clock_gettime(CLOCK_MONOTONIC, &t_begin);
            usleep(voSendTime);
          }
        }

        RK_MPI_VDEC_ReleaseFrame(pstPlayer->pstVdecCtx->chnIndex, &sFrame);
      } else {
        if ((sFrame.stVFrame.u32FrameFlag & (RKADK_U32)FRAME_FLAG_SNAP_END) == (RKADK_U32)FRAME_FLAG_SNAP_END) {
          RK_MPI_VDEC_ReleaseFrame(pstPlayer->pstVdecCtx->chnIndex, &sFrame);
          RKADK_LOGI("chn %d reach eos frame.", pstPlayer->pstVdecCtx->chnIndex);
          break;
        }
      }
    } else {
      usleep(1000);
    }
  }

  if (VoMultiWindownsStop(pstPlayer->pstVoCtx->VoLayer, pstPlayer->pstVoCtx->windows)) {
    RKADK_LOGE("Vo stop windowns failed");
    return RKADK_NULL;
  }

  if (VoStopLayer(pstPlayer->pstVoCtx->VoLayer)) {
    RKADK_LOGE("Vo stop layer failed");
    return RKADK_NULL;
  }

  RKADK_LOGI("%s end", __FUNCTION__);
  return RKADK_NULL;
}
#endif

static RKADK_VOID* SendAudioDataThread(RKADK_VOID *ptr) {
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  RKADK_S32 s32MilliSec = -1, result = 0, size = 0;
  AUDIO_FRAME_INFO_S *pstFrmInfo = RKADK_NULL;

  pstFrmInfo = (AUDIO_FRAME_INFO_S *)(malloc(sizeof(AUDIO_FRAME_INFO_S)));
  memset(pstFrmInfo, 0, sizeof(AUDIO_FRAME_INFO_S));

  #ifdef WRITE_DECODER_FILE
  FILE *fp = RKADK_NULL;
  RKADK_CHAR name[128] = {0};
  mkdir("tmp", 0777);
  snprintf(name, sizeof(name), "/tmp/audio_out.pcm");

  fp = fopen(name, "wb");
  if (fp == RKADK_NULL) {
    RKADK_LOGE("can't open output file %s\n", name);
    return NULL;
  }
  #endif

  pstPlayer->bGetPtsFlag = RKADK_TRUE;
  while (1) {
    ret = RK_MPI_ADEC_GetFrame(pstPlayer->pstAdecCtx->chnIndex, pstFrmInfo, pstPlayer->pstAdecCtx->bBlock);
    if (!ret) {
      size = pstFrmInfo->pstFrame->u32Len;
      if (!pstPlayer->bStopFlag) {
        if (size > 0)
          pstPlayer->positionTimeStamp = pstFrmInfo->pstFrame->u64TimeStamp;
        result = RK_MPI_AO_SendFrame(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, pstFrmInfo->pstFrame, s32MilliSec);
        if (result < 0) {
          RKADK_LOGE("send frame fail, result = %X, TimeStamp = %lld, s32MilliSec = %d",
                      result, pstFrmInfo->pstFrame->u64TimeStamp, s32MilliSec);
        }
      }

      #ifdef WRITE_DECODER_FILE
      MB_BLK bBlk = pstFrmInfo->pstFrame->pMbBlk;
      RK_VOID *pstFrame = RK_MPI_MB_Handle2VirAddr(bBlk);
      fwrite(pstFrame, 1, size, fp);
      #endif

      RK_MPI_ADEC_ReleaseFrame(pstPlayer->pstAdecCtx->chnIndex, pstFrmInfo);

      if (size <= 0) {
        if (!pstPlayer->bStopFlag)
          RKADK_LOGI("audio data send eof");

        break;
      }
    } else {
      RKADK_LOGE("adec stop");
      break;
    }

    if (pstPlayer->pstAdecCtx->clrChnBuf) {
      RKADK_LOGI("adec clear chn(%d) buf", pstPlayer->pstAdecCtx->chnIndex);
      RK_MPI_ADEC_ClearChnBuf(pstPlayer->pstAdecCtx->chnIndex);
      pstPlayer->pstAdecCtx->clrChnBuf = 0;
    }
  }

  #ifdef WRITE_DECODER_FILE
  if (fp)
    fclose(fp);
  #endif

  if (!pstPlayer->bStopFlag)
    RK_MPI_AO_WaitEos(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, s32MilliSec);

  if (pstFrmInfo) {
    free(pstFrmInfo);
    pstFrmInfo = RKADK_NULL;
  }

  return RKADK_NULL;
}

static RKADK_VOID* CommandThread(RKADK_VOID * ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;

  {
    AUDIO_FADE_S aFade;
    aFade.bFade = RK_FALSE;
    aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)pstPlayer->pstAoCtx->setFadeRate;
    aFade.enFadeInRate = (AUDIO_FADE_RATE_E)pstPlayer->pstAoCtx->setFadeRate;
    RK_BOOL mute = (pstPlayer->pstAoCtx->setMute == 0) ? RK_FALSE : RK_TRUE;
    RK_MPI_AO_SetMute(pstPlayer->pstAoCtx->devId, mute, &aFade);
    RK_MPI_AO_SetVolume(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->setVolume);
  }

  if (pstPlayer->pstAoCtx->setTrackMode) {
    RKADK_LOGI("info : set track mode = %d", pstPlayer->pstAoCtx->setTrackMode);
    RK_MPI_AO_SetTrackMode(pstPlayer->pstAoCtx->devId, (AUDIO_TRACK_MODE_E)pstPlayer->pstAoCtx->setTrackMode);
  }

  if (pstPlayer->pstAoCtx->getVolume) {
    RKADK_S32 volume = 0;
    RK_MPI_AO_GetVolume(pstPlayer->pstAoCtx->devId, &volume);
    RKADK_LOGI("info : get volume = %d", volume);
    pstPlayer->pstAoCtx->getVolume = 0;
  }

  if (pstPlayer->pstAoCtx->getMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(pstPlayer->pstAoCtx->devId, &mute, &fade);
    RKADK_LOGI("info : is mute = %d", mute);
    pstPlayer->pstAoCtx->getMute = 0;
  }

  if (pstPlayer->pstAoCtx->getTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(pstPlayer->pstAoCtx->devId, &trackMode);
    RKADK_LOGI("info : get track mode = %d", trackMode);
    pstPlayer->pstAoCtx->getTrackMode = 0;
  }

  if (pstPlayer->pstAoCtx->queryChnStat) {
    QueryAoFlowGraphStat(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
    pstPlayer->pstAoCtx->queryChnStat = 0;
  }

  if (pstPlayer->pstAoCtx->saveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pstPlayer->pstAoCtx->dstFilePath) {
      saveFile.bCfg = RK_TRUE;
      saveFile.u32FileSize = 1024 * 1024;
      snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
      snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", pstPlayer->pstAoCtx->dstFilePath);
    }
    RK_MPI_AO_SaveFile(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, &saveFile);
    pstPlayer->pstAoCtx->saveFile = 0;
  }

  if (pstPlayer->pstAoCtx->queryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, &fileStat);
    RKADK_LOGI("info : query save file status = %d", fileStat.bSaving);
    pstPlayer->pstAoCtx->queryFileStat = 0;
  }

  if (pstPlayer->pstAoCtx->pauseResumeChn) {
    usleep(500 * 1000);
    RK_MPI_AO_PauseChn(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
    RKADK_LOGI("AO pause");
    usleep(1000 * 1000);
    RK_MPI_AO_ResumeChn(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
    RKADK_LOGI("AO resume");
    pstPlayer->pstAoCtx->pauseResumeChn = 0;
  }

  if (pstPlayer->pstAoCtx->clrChnBuf) {
    RK_MPI_AO_ClearChnBuf(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
    pstPlayer->pstAoCtx->clrChnBuf = 0;
  }

  if (pstPlayer->pstAoCtx->clrPubAttr) {
    RK_MPI_AO_ClrPubAttr(pstPlayer->pstAoCtx->devId);
    pstPlayer->pstAoCtx->clrPubAttr = 0;
  }

  if (pstPlayer->pstAoCtx->getPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(pstPlayer->pstAoCtx->devId, &pstAttr);
    RKADK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RKADK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RKADK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RKADK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    pstPlayer->pstAoCtx->getPubAttr = 0;
  }

  return RKADK_NULL;
}

static RKADK_S32 BufferFree(RKADK_VOID *opaque) {
  if (opaque) {
    free(opaque);
    opaque = NULL;
  }

  return 0;
}

#ifdef RV1126_1109
static RKADK_VOID DoPullDemuxerVideoPacket(RKADK_VOID* pHandle) {
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;
  RKADK_S32 ret = 0;
  VDEC_STREAM_S stStream;
  MB_BLK buffer = RKADK_NULL;
  MB_EXT_CONFIG_S pstMbExtConfig;

  if (!pstPlayer->seekFlag || pstPlayer->seekFlag == RKADK_PLAYER_SEEK_SEND
      || (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT && pstDemuxerPacket->s8EofFlag)
      || (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_START
      && pstDemuxerPacket->s8SpecialFlag && pstDemuxerPacket->s64Pts >= pstPlayer->seekTimeStamp)) {

    if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_START) {
      pstPlayer->seekTimeStamp = pstDemuxerPacket->s64Pts;
      pstPlayer->seekFlag = RKADK_PLAYER_SEEK_SEND;
    }

    pstMbExtConfig.pFreeCB = BufferFree;
    pstMbExtConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    pstMbExtConfig.pu8VirAddr = (RK_U8 *)pstDemuxerPacket->s8PacketData;
    pstMbExtConfig.u64Size = pstDemuxerPacket->s32PacketSize;

    RK_MPI_SYS_CreateMB(&buffer, &pstMbExtConfig);

    stStream.u64PTS = pstDemuxerPacket->s64Pts;
    stStream.pMbBlk = buffer;
    stStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stStream.bEndOfStream = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bEndOfFrame = pstDemuxerPacket->s8EofFlag ? RK_TRUE : RK_FALSE;
    stStream.bBypassMbBlk = RK_TRUE;

__RETRY:
    ret = RK_MPI_VDEC_SendStream(pstPlayer->pstVdecCtx->chnIndex, &stStream, -1);
    if (ret < 0) {
      if (pstPlayer->bStopFlag) {
        RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
        return;
      }

      usleep(1000llu);
      goto  __RETRY;
    } else
      RK_MPI_MB_ReleaseMB(stStream.pMbBlk);
  } else if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_WAIT)
    return;

  return;
}
#endif

static RKADK_VOID DoPullDemuxerAudioPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_STREAM_S stAudioStream;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;

  if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_WAIT)
    return;
  else if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_START) {
    while(pstPlayer->seekFlag != RKADK_PLAYER_SEEK_SEND || !pstPlayer->seekFlag)
      usleep(1000);
  } else if (pstDemuxerPacket->s8EofFlag) {
    if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
      RK_MPI_ADEC_SendEndOfStream(pstPlayer->pstAdecCtx->chnIndex, RK_FALSE);
      pstPlayer->bAudioStopFlag = RKADK_TRUE;
      if (!pstPlayer->bStopFlag)
        RKADK_LOGI("read eos packet, now send eos packet!");
    }
  } else if (!pstPlayer->seekFlag || (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_SEND
             && pstDemuxerPacket->s64Pts >= pstPlayer->seekTimeStamp)) {
    stAudioStream.u32Len = pstDemuxerPacket->s32PacketSize;
    stAudioStream.u64TimeStamp = pstDemuxerPacket->s64Pts;
    stAudioStream.u32Seq = pstDemuxerPacket->s32Series;
    stAudioStream.bBypassMbBlk = RK_TRUE;
    MB_EXT_CONFIG_S extConfig = {0};
    extConfig.pFreeCB = BufferFree;
    extConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
    extConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
    extConfig.u64Size    = pstDemuxerPacket->s32PacketSize;
    RK_MPI_SYS_CreateMB(&(stAudioStream.pMbBlk), &extConfig);

__RETRY:
    ret = RK_MPI_ADEC_SendStream(pstPlayer->pstAdecCtx->chnIndex, &stAudioStream, pstPlayer->pstAdecCtx->bBlock);
    if (ret != RK_SUCCESS) {
      RKADK_LOGE("fail to send adec stream.");
      goto __RETRY;
    }

    RK_MPI_MB_ReleaseMB(stAudioStream.pMbBlk);
  }

  return;
}

static RKADK_VOID DoPullDemuxerWavPacket(RKADK_VOID* pHandle) {
  RKADK_S32 ret = 0;
  RKADK_S32 s32MilliSec = -1;
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  AUDIO_FRAME_S frame;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->ptr;
  pstPlayer->bGetPtsFlag = RKADK_TRUE;
  if (!pstPlayer->bStopFlag) {
    if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_WAIT)
        return;
    else if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_START) {
      while(pstPlayer->seekFlag != RKADK_PLAYER_SEEK_SEND || !pstPlayer->seekFlag)
        usleep(1000);
    } else if (!pstPlayer->seekFlag || pstDemuxerPacket->s8EofFlag || (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_SEND
               && (pstDemuxerPacket->s64Pts >= pstPlayer->seekTimeStamp))) {

      if (!pstDemuxerPacket->s8EofFlag)
        pstPlayer->positionTimeStamp = pstDemuxerPacket->s64Pts;

      frame.u32Len = pstDemuxerPacket->s32PacketSize;
      frame.u64TimeStamp = pstDemuxerPacket->s64Pts;
      frame.enBitWidth = FindBitWidth(pstPlayer->pstAoCtx->bitWidth);
      frame.enSoundMode = FindSoundMode(pstPlayer->pstDemuxerParam->audioChannels);
      frame.bBypassMbBlk = RK_TRUE;

      MB_EXT_CONFIG_S extConfig;
      memset(&extConfig, 0, sizeof(extConfig));
      extConfig.pFreeCB = BufferFree;
      extConfig.pOpaque = (RKADK_VOID *)pstDemuxerPacket->s8PacketData;
      extConfig.pu8VirAddr = (RK_U8*)pstDemuxerPacket->s8PacketData;
      extConfig.u64Size = pstDemuxerPacket->s32PacketSize;
      RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);

__RETRY:
      ret = RK_MPI_AO_SendFrame(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, &frame, s32MilliSec);
      if (ret < 0) {
        RK_LOGE("send frame fail, ret = %d, TimeStamp = %lld, s32MilliSec = %d",
                  ret, frame.u64TimeStamp, s32MilliSec);
        goto __RETRY;
      }

      RK_MPI_MB_ReleaseMB(frame.pMbBlk);
      if (pstDemuxerPacket->s32PacketSize <= 0) {
        if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
          pstPlayer->bWavEofFlag = RKADK_TRUE;
          RK_MPI_AO_WaitEos(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex, s32MilliSec);
          pthread_mutex_lock(&pstPlayer->WavMutex);
          if (pstPlayer->bAudioStopFlag == RKADK_TRUE) {
            if (!pstPlayer->bStopFlag)
              RKADK_LOGI("read eos packet, now send eos packet!");
            pthread_cond_signal(&pstPlayer->WavCond);
          } else
            pstPlayer->bAudioStopFlag = RKADK_TRUE;

          pthread_mutex_unlock(&pstPlayer->WavMutex);
        }
      }
    }
  }

  return;
}

RKADK_S32 RKADK_PLAYER_Create(RKADK_MW_PTR *pPlayer,
                              RKADK_PLAYER_CFG_S *pstPlayCfg) {
  RKADK_CHECK_POINTER(pstPlayCfg, RKADK_FAILURE);
  bool bSysInit = false;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  if (*pPlayer) {
    RKADK_LOGE("player has been created");
    return RKADK_FAILURE;
  }

  bSysInit = RKADK_MPI_SYS_CHECK();
  if (!bSysInit) {
    RKADK_LOGE("System is not initialized");
    return RKADK_FAILURE;
  }

  RKADK_LOGI("Create Player[%d, %d] Start...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)malloc(sizeof(RKADK_PLAYER_HANDLE_S));
  if (!pstPlayer) {
    RKADK_LOGE("malloc pstPlayer failed");
    return RKADK_FAILURE;
  }

  memset(pstPlayer, 0, sizeof(RKADK_PLAYER_HANDLE_S));
  pstPlayer->pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;
  if (pstPlayCfg->bEnableVideo == RKADK_FALSE && pstPlayCfg->bEnableAudio == RKADK_FALSE) {
    RKADK_LOGE("bEnableVideo and bEnableAudio are not enable");
    goto __FAILED;
  }

  #ifdef RV1126_1109
  pstPlayer->bEnableVideo = pstPlayCfg->bEnableVideo;
  #endif
  pstPlayer->bEnableAudio = pstPlayCfg->bEnableAudio;

  pstPlayer->pstDemuxerInput = (RKADK_DEMUXER_INPUT_S *)malloc(sizeof(RKADK_DEMUXER_INPUT_S));
  if (!pstPlayer->pstDemuxerInput) {
    RKADK_LOGE("malloc pDemuxerInput falied");
    goto __FAILED;
  }

  pstPlayer->pstDemuxerParam = (RKADK_DEMUXER_PARAM_S *)malloc(sizeof(RKADK_DEMUXER_PARAM_S));
  if (!pstPlayer->pstDemuxerParam) {
    RKADK_LOGE("malloc pDemuxerParam falied");
    goto __FAILED;
  }

  memset(pstPlayer->pstDemuxerInput, 0, sizeof(RKADK_DEMUXER_INPUT_S));
  pstPlayer->pstDemuxerInput->ptr = (RKADK_VOID *)pstPlayer;

  pstPlayer->pstDemuxerInput->readModeFlag = DEMUXER_TYPE_PASSIVE;

  #ifdef RV1126_1109
  pstPlayer->pstDemuxerInput->videoEnableFlag = pstPlayer->bEnableVideo;
  #endif

  pstPlayer->pstDemuxerInput->audioEnableFlag = pstPlayer->bEnableAudio;
  if (RKADK_DEMUXER_Create(&pstPlayer->pDemuxerCfg, pstPlayer->pstDemuxerInput)) {
    RKADK_LOGE("RKADK_DEMUXER_Create failed");
    goto __FAILED;
  }

  #ifdef RV1126_1109
  if (pstPlayer->bEnableVideo) {
    if (CreateVdecCtx(&pstPlayer->pstVdecCtx)) {
      RKADK_LOGE("Create VDEC ctx failed");
      goto __FAILED;
    }

    if (CreateVoCtx(&pstPlayer->pstVoCtx)) {
      RKADK_LOGE("Create VO ctx failed");
      goto __FAILED;
    }
  }
  #endif

  if (pstPlayer->bEnableAudio) {
    if (RKADK_AUDIO_DECODER_Register(RKADK_CODEC_TYPE_MP3)) {
      RKADK_LOGE("register mp3 failed");
      goto __FAILED;
    }

    if (CreateADECCtx(&pstPlayer->pstAdecCtx)) {
      RKADK_LOGE("Create ADEC ctx failed");
      goto __FAILED;
    }

    if(CreateAOCtx(&pstPlayer->pstAoCtx)) {
      RKADK_LOGE("Create AO ctx failed");
      goto __FAILED;
    }
  }

  pthread_mutex_init(&(pstPlayer->WavMutex), NULL);
  pthread_cond_init(&(pstPlayer->WavCond), NULL);

  #ifdef RV1126_1109
  pthread_mutex_init(&(pstPlayer->PauseVideoMutex), NULL);
  #endif

  pthread_mutex_init(&(pstPlayer->PauseAudioMutex), NULL);
  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);
  *pPlayer = (RKADK_MW_PTR)pstPlayer;
  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->pstDemuxerInput) {
    free(pstPlayer->pstDemuxerInput);
    pstPlayer->pstDemuxerInput = NULL;
  }

  if (pstPlayer->pstDemuxerParam) {
    free(pstPlayer->pstDemuxerParam);
    pstPlayer->pstDemuxerParam = NULL;
  }

  #ifdef RV1126_1109
  if (pstPlayer->pstVdecCtx) {
    free(pstPlayer->pstVdecCtx);
    pstPlayer->pstVdecCtx = NULL;
  }

  if (pstPlayer->pstVoCtx) {
    free(pstPlayer->pstVoCtx);
    pstPlayer->pstVoCtx = NULL;
  }
  #endif

  if (pstPlayer->pstAdecCtx) {
    free(pstPlayer->pstAdecCtx);
    pstPlayer->pstAdecCtx = NULL;
  }

  if (pstPlayer->pstAoCtx) {
    free(pstPlayer->pstAoCtx);
    pstPlayer->pstAoCtx = NULL;
  }

  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  if (pstPlayer)
    free(pstPlayer);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_LOGI("Destory Player Start...");
  if (pstPlayer->bStopFlag != RKADK_TRUE)
    RKADK_PLAYER_Stop(pPlayer);

  #ifdef RV1126_1109
  if (pstPlayer->bVideoExist == RKADK_TRUE) {
    if (VoStopDev(pstPlayer->pstVoCtx->VoDev))
      RKADK_LOGE("stop Vo dev failed");

    if (pstPlayer->pstVoCtx) {
      ret = DestroyVo(pstPlayer->pstVoCtx);
      if (ret)
        RKADK_LOGE("destroy Vo failed");
    }
  }

  if (pstPlayer->pstVdecCtx) {
    free(pstPlayer->pstVdecCtx);
    pstPlayer->pstVdecCtx = NULL;
  }

  if (pstPlayer->pstVoCtx) {
    free(pstPlayer->pstVoCtx);
    pstPlayer->pstVoCtx = NULL;
  }
  #endif

  if (RKADK_AUDIO_DECODER_UnRegister(RKADK_CODEC_TYPE_MP3))
    RKADK_LOGE("unregister mp3 failed");

  if (pstPlayer->pstAdecCtx) {
    free(pstPlayer->pstAdecCtx);
    pstPlayer->pstAdecCtx = NULL;
  }

  if (pstPlayer->pstAoCtx) {
    free(pstPlayer->pstAoCtx);
    pstPlayer->pstAoCtx = NULL;
  }

  if (pstPlayer->pDemuxerCfg)
    RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);

  if (pstPlayer->pstDemuxerInput) {
    free(pstPlayer->pstDemuxerInput);
    pstPlayer->pstDemuxerInput = NULL;
  }

  if (pstPlayer->pstDemuxerParam) {
    free(pstPlayer->pstDemuxerParam);
    pstPlayer->pstDemuxerParam = NULL;
  }

  pthread_mutex_destroy(&(pstPlayer->WavMutex));
  pthread_cond_destroy(&(pstPlayer->WavCond));

  #ifdef RV1126_1109
  pthread_mutex_destroy(&(pstPlayer->PauseVideoMutex));
  #endif

  pthread_mutex_destroy(&(pstPlayer->PauseAudioMutex));

  if (pstPlayer)
    free(pstPlayer);

  if (!ret)
    RKADK_LOGI("Destory Player End...");

  return ret;
}

RKADK_S32 RKADK_PLAYER_SetDataSource(RKADK_MW_PTR pPlayer,
                                     const RKADK_CHAR *pszfilePath) {
  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_CODEC_TYPE_E eAudioCodecType = RKADK_CODEC_TYPE_BUTT;
  if ((strlen(pszfilePath) <= 0) || (strlen(pszfilePath) >= 100)) {
    RKADK_LOGE("The length(%d) of the file name is unreasonable", strlen(pszfilePath));
    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    return RKADK_FALSE;
  }

  #ifdef RV1126_1109
  pstPlayer->bVideoExist = RKADK_FALSE;
  #endif

  pstPlayer->bStopFlag = RKADK_FALSE;
  pstPlayer->bAudioStopFlag = RKADK_FALSE;
  pstPlayer->bAudioExist = RKADK_FALSE;
  pstPlayer->pFilePath = pszfilePath;
  for(RKADK_S32 i = strlen(pszfilePath) - 1; i >= 0; i--) {
    if ('.' == pszfilePath[i]) {
#ifdef RV1126_1109
      if(!strcmp(pszfilePath + i + 1, "mp4")) {
        pstPlayer->demuxerFlag = MIX_VIDEO_FLAG;
        pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
        pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
        if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, pstPlayer->pstDemuxerParam)) {
          RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
          goto __FAILED;
        }

        if (pstPlayer->bEnableVideo == RKADK_TRUE) {
          if (pstPlayer->pstDemuxerParam->pVideoCodec != NULL) {
            pstPlayer->bVideoExist = RKADK_TRUE;

            if (strcmp(pstPlayer->pstDemuxerParam->pVideoCodec, "h264")) {
              RKADK_LOGE("Unsupported video format(%s)", pstPlayer->pstDemuxerParam->pVideoCodec);
              goto __FAILED;
            }

            if (pstPlayer->pstDemuxerParam->videoWidth <= 0 || pstPlayer->pstDemuxerParam->videoHeigh <= 0) {
              RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->pstDemuxerParam->videoWidth,
                          pstPlayer->pstDemuxerParam->videoHeigh);
              goto __FAILED;
            }

            pstPlayer->pstVdecCtx->srcWidth = pstPlayer->pstDemuxerParam->videoWidth;
            pstPlayer->pstVdecCtx->srcHeight = pstPlayer->pstDemuxerParam->videoHeigh;

            if (pstPlayer->pstDemuxerParam->VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
              pstPlayer->pstVdecCtx->outputPixFmt = RK_FMT_YUV420SP_10BIT;
            else
              pstPlayer->pstVdecCtx->outputPixFmt = RK_FMT_YUV420SP;

            if (VdecSetParam(pstPlayer->pstVdecCtx)) {
              RKADK_LOGE("Vdec set param failed");
              goto __FAILED;
            }
          } else if (pstPlayer->bEnableAudio == RKADK_FALSE) {
            RKADK_LOGE("Video does not exist and audio exists but cannot be played");
            goto __FAILED;
          }
        } else
          RKADK_LOGW("video is unable");

        if (pstPlayer->bEnableAudio == RKADK_TRUE) {
          if (pstPlayer->pstDemuxerParam->pAudioCodec != NULL) {
            if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "mp3"))
              eAudioCodecType = RKADK_CODEC_TYPE_MP3;
            else if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "wav"))
              eAudioCodecType = RKADK_CODEC_TYPE_PCM;
            else {
              RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->pstDemuxerParam->pAudioCodec);
              goto __FAILED;
            }

            pstPlayer->bAudioExist = RKADK_TRUE;
            pstPlayer->pstAdecCtx->sampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
            pstPlayer->pstAdecCtx->channel = pstPlayer->pstDemuxerParam->audioChannels;
            pstPlayer->pstAdecCtx->eCodecType = eAudioCodecType;
            pstPlayer->pstAoCtx->reSmpSampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
            if (eAudioCodecType != RKADK_CODEC_TYPE_PCM) {
              if (AdecSetParam(pstPlayer->pstAdecCtx)) {
                RKADK_LOGE("set Adec ctx failed");
                goto __FAILED;
              }
            }

            if (pstPlayer->pstDemuxerParam->audioFormat == 0)
              pstPlayer->pstAoCtx->bitWidth = 8;
            else if (pstPlayer->pstDemuxerParam->audioFormat == 1)
              pstPlayer->pstAoCtx->bitWidth = 16;
            else {
              RKADK_LOGE("AO create failed, audioFormat = %d", pstPlayer->pstDemuxerParam->audioFormat);
              goto __FAILED;
            }

            if (pstPlayer->pstDemuxerParam->audioChannels <= 0 ||
                pstPlayer->pstDemuxerParam->audioSampleRate <= 0) {
              RKADK_LOGE("AO create failed, channel = %d, reSmpSampleRate = %d",
                          pstPlayer->pstDemuxerParam->audioChannels, pstPlayer->pstDemuxerParam->audioSampleRate);
            }
          } else if (pstPlayer->bEnableVideo == RKADK_FALSE) {
            RKADK_LOGE("Audio does not exist, and video exists but cannot be played");
            goto __FAILED;
          }
        } else
          RKADK_LOGW("audio is unable");
      } else if (!strcmp(pszfilePath + i + 1, "h264")) {
        if (!pstPlayer->bEnableVideo) {
          RKADK_LOGE("video is unable");
          goto __FAILED;
        }

        pstPlayer->demuxerFlag = VIDEO_FLAG;
        if (!strcmp(pszfilePath + i + 1, "h264"))
          eAudioCodecType = RKADK_CODEC_TYPE_H264;

        pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
        pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
        if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, pstPlayer->pstDemuxerParam)) {
          RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
          goto __FAILED;
        }

        if (pstPlayer->pstDemuxerParam->pVideoCodec != NULL) {
          pstPlayer->bVideoExist = RKADK_TRUE;

          if (strcmp(pstPlayer->pstDemuxerParam->pVideoCodec, "h264")) {
            RKADK_LOGE("Unsupported video format(%s)", pstPlayer->pstDemuxerParam->pVideoCodec);
            goto __FAILED;
          }

          if (pstPlayer->pstDemuxerParam->videoWidth <= 0 || pstPlayer->pstDemuxerParam->videoHeigh <= 0) {
            RKADK_LOGE("error: videoWidth = %d\n, videoHeigh = %d\n", pstPlayer->pstDemuxerParam->videoWidth,
                        pstPlayer->pstDemuxerParam->videoHeigh);
            goto __FAILED;
          }

          pstPlayer->pstVdecCtx->srcWidth = pstPlayer->pstDemuxerParam->videoWidth;
          pstPlayer->pstVdecCtx->srcHeight = pstPlayer->pstDemuxerParam->videoHeigh;

          if (pstPlayer->pstDemuxerParam->VideoFormat == DEMUXER_VIDEO_YUV420SP_10BIT)
            pstPlayer->pstVdecCtx->outputPixFmt = RK_FMT_YUV420SP_10BIT;
          else
            pstPlayer->pstVdecCtx->outputPixFmt = RK_FMT_YUV420SP;

          if (VdecSetParam(pstPlayer->pstVdecCtx)) {
            RKADK_LOGE("Vdec set param failed");
            goto __FAILED;
          }
        } else if (pstPlayer->bEnableAudio == RKADK_FALSE) {
          RKADK_LOGE("Video does not exist and audio exists but cannot be played");
          goto __FAILED;
        }

        return RKADK_SUCCESS;
      } else if ((!strcmp(pszfilePath + i + 1, "mp3")) || (!strcmp(pszfilePath + i + 1, "wav"))) {
        if (!pstPlayer->bEnableAudio) {
          RKADK_LOGE("audio is unable");
          goto __FAILED;
        }

        pstPlayer->demuxerFlag = AUDIO_FLAG;

        pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
        if (!strcmp(pszfilePath + i + 1, "wav"))
          pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerWavPacket;
        else
          pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;

        if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, pstPlayer->pstDemuxerParam)) {
          RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
          goto __FAILED;
        }

        if (pstPlayer->pstDemuxerParam->pAudioCodec != NULL) {
          if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "mp3"))
            eAudioCodecType = RKADK_CODEC_TYPE_MP3;
          else if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "wav"))
            eAudioCodecType = RKADK_CODEC_TYPE_PCM;
          else {
            RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->pstDemuxerParam->pAudioCodec);
            goto __FAILED;
          }

          pstPlayer->bAudioExist = RKADK_TRUE;

          pstPlayer->pstAdecCtx->sampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
          pstPlayer->pstAdecCtx->channel = pstPlayer->pstDemuxerParam->audioChannels;
          pstPlayer->pstAdecCtx->eCodecType = eAudioCodecType;
          pstPlayer->pstAoCtx->reSmpSampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
          if (eAudioCodecType != RKADK_CODEC_TYPE_PCM) {
            if (AdecSetParam(pstPlayer->pstAdecCtx)) {
              RKADK_LOGE("set Adec ctx failed");
              goto __FAILED;
            }
          }

          if (pstPlayer->pstDemuxerParam->audioFormat == 0)
            pstPlayer->pstAoCtx->bitWidth = 8;
          else if (pstPlayer->pstDemuxerParam->audioFormat == 1)
            pstPlayer->pstAoCtx->bitWidth = 16;
          else {
            RKADK_LOGE("AO create failed, audioFormat = %d", pstPlayer->pstDemuxerParam->audioFormat);
            goto __FAILED;
          }

          if (pstPlayer->pstDemuxerParam->audioChannels <= 0 ||
              pstPlayer->pstDemuxerParam->audioSampleRate <= 0) {
            RKADK_LOGE("AO create failed, channel = %d, reSmpSampleRate = %d",
                        pstPlayer->pstDemuxerParam->audioChannels, pstPlayer->pstDemuxerParam->audioSampleRate);
          }
        }

        return RKADK_SUCCESS;
      }
#else
      if ((!strcmp(pszfilePath + i + 1, "mp3")) || (!strcmp(pszfilePath + i + 1, "wav"))) {
        if (!pstPlayer->bEnableAudio) {
          RKADK_LOGE("audio is unable");
          goto __FAILED;
        }

        pstPlayer->demuxerFlag = AUDIO_FLAG;
        if (!strcmp(pszfilePath + i + 1, "wav"))
          pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerWavPacket;
        else
          pstPlayer->pstDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;

        if (RKADK_DEMUXER_GetParam(pstPlayer->pDemuxerCfg, pszfilePath, pstPlayer->pstDemuxerParam)) {
          RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
          goto __FAILED;
        }

        if (pstPlayer->pstDemuxerParam->pAudioCodec != NULL) {
          if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "mp3"))
            eAudioCodecType = RKADK_CODEC_TYPE_MP3;
          else if (!strcmp(pstPlayer->pstDemuxerParam->pAudioCodec, "wav"))
            eAudioCodecType = RKADK_CODEC_TYPE_PCM;
          else {
            RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->pstDemuxerParam->pAudioCodec);
            goto __FAILED;
          }

          pstPlayer->bAudioExist = RKADK_TRUE;

          pstPlayer->pstAdecCtx->sampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
          pstPlayer->pstAdecCtx->channel = pstPlayer->pstDemuxerParam->audioChannels;
          pstPlayer->pstAdecCtx->eCodecType = eAudioCodecType;
          pstPlayer->pstAoCtx->reSmpSampleRate = pstPlayer->pstDemuxerParam->audioSampleRate;
          if (eAudioCodecType != RKADK_CODEC_TYPE_PCM) {
            if (AdecSetParam(pstPlayer->pstAdecCtx)) {
              RKADK_LOGE("set Adec ctx failed");
              goto __FAILED;
            }
          }

          if (pstPlayer->pstDemuxerParam->audioFormat == 0)
            pstPlayer->pstAoCtx->bitWidth = 8;
          else if (pstPlayer->pstDemuxerParam->audioFormat == 1)
            pstPlayer->pstAoCtx->bitWidth = 16;
          else {
            RKADK_LOGE("AO create failed, audioFormat = %d", pstPlayer->pstDemuxerParam->audioFormat);
            goto __FAILED;
          }

          if (pstPlayer->pstDemuxerParam->audioChannels <= 0 ||
              pstPlayer->pstDemuxerParam->audioSampleRate <= 0) {
            RKADK_LOGE("AO create failed, channel = %d, reSmpSampleRate = %d",
                        pstPlayer->pstDemuxerParam->audioChannels, pstPlayer->pstDemuxerParam->audioSampleRate);
          }
        }
      }
#endif
      else {
        RKADK_LOGE("Unsupported file format(%s)", pszfilePath);
        goto __FAILED;
      }
    }

    if (i < 0) {
      RKADK_LOGE("File suffix does not exist(%s)", pszfilePath);
      goto __FAILED;
    }
  }

  return RKADK_SUCCESS;

__FAILED:
  pstPlayer->bStopFlag = RKADK_FALSE;
  pstPlayer->bAudioStopFlag = RKADK_FALSE;
  pstPlayer->bAudioExist = RKADK_FALSE;
  pstPlayer->pFilePath = NULL;
  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Prepare(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_PREPARED, NULL);

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_SetVideoSink(RKADK_MW_PTR pPlayer,
                                    RKADK_PLAYER_FRAMEINFO_S *pstFrameInfo) {
#ifdef RV1126_1109
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  if (pstPlayer->bVideoExist == RKADK_TRUE) {
    if (VoSetParam(pstPlayer->pstVoCtx, pstFrameInfo, pstPlayer->pstDemuxerParam->videoAvgFrameRate)) {
      RKADK_LOGE("Vo set param failed");
      return RKADK_FAILURE;
    }

    if (VoDevStart(pstPlayer->pstVoCtx)) {
      RKADK_LOGE("Vo dev start failed");
      return RKADK_FAILURE;
    }
  } else
    RKADK_LOGE("fail to find video stream in input file");
#endif

  return RKADK_SUCCESS;
}

RKADK_VOID *EventEOF(RKADK_VOID *arg) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)arg;

  #ifdef RV1126_1109
  if (pstPlayer->bVideoExist || pstPlayer->demuxerFlag == VIDEO_FLAG)
    pthread_create(&pstPlayer->stThreadParam.tidVideoSend, RKADK_NULL, SendVideoDataThread, arg);
  #endif

  if (pstPlayer->bAudioExist) {
    if (pstPlayer->pstAdecCtx->eCodecType != RKADK_CODEC_TYPE_PCM)
      pthread_create(&pstPlayer->stThreadParam.tidAudioSend, RKADK_NULL, SendAudioDataThread, arg);

    pthread_create(&pstPlayer->stThreadParam.tidAudioCommand, RKADK_NULL, CommandThread, arg);
  }

  #ifdef RV1126_1109
  if (pstPlayer->stThreadParam.tidVideoSend)
    pthread_join(pstPlayer->stThreadParam.tidVideoSend, RKADK_NULL);
  #endif

  if (pstPlayer->stThreadParam.tidAudioSend)
    pthread_join(pstPlayer->stThreadParam.tidAudioSend, RKADK_NULL);

  if (pstPlayer->stThreadParam.tidAudioCommand)
    pthread_join(pstPlayer->stThreadParam.tidAudioCommand, RKADK_NULL);

  pthread_mutex_lock(&pstPlayer->WavMutex);
  if (!pstPlayer->bStopFlag && pstPlayer->bAudioExist
      && pstPlayer->pstAdecCtx->eCodecType == RKADK_CODEC_TYPE_PCM
      && pstPlayer->bAudioStopFlag == RKADK_FALSE) {
    pstPlayer->bAudioStopFlag = RKADK_TRUE;
    pthread_cond_wait(&pstPlayer->WavCond, &pstPlayer->WavMutex);
  }
  pthread_mutex_unlock(&pstPlayer->WavMutex);

  if (pstPlayer->pfnPlayerCallback != NULL && !pstPlayer->bStopFlag)
    pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_EOF, NULL);

  return NULL;
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_S32 ret = 0;

  if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT)
    pthread_mutex_lock(&pstPlayer->PauseAudioMutex);

  if (pstPlayer->pauseFlag == RKADK_PLAYER_PAUSE_START && pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
    pstPlayer->pauseFlag = RKADK_PLAYER_PAUSE_FALSE;

    if (pstPlayer->bAudioExist) {
      ret = RK_MPI_AO_ResumeChn(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
      if (ret != RKADK_SUCCESS) {
        RKADK_LOGE("RK_MPI_AO_ResumeChn failed, ret = %X\n", ret);
        return RKADK_FAILURE;
      }
    }
  } else {
  #ifdef RV1126_1109
    if (pstPlayer->demuxerFlag == MIX_VIDEO_FLAG) {
      if (pstPlayer->bVideoExist == RKADK_TRUE) {
        ret = RK_MPI_VDEC_StartRecvStream(pstPlayer->pstVdecCtx->chnIndex);
        if (ret != RKADK_SUCCESS) {
          RKADK_LOGE("start recv chn %d failed %X! ", pstPlayer->pstVdecCtx->chnIndex, ret);
          goto __FAILED;
        }

        if(VoStart(pstPlayer->pstVoCtx)) {
          RKADK_LOGE("start VO failed! ");
          goto __FAILED;
        }
      }

      if (pstPlayer->bAudioExist == RKADK_TRUE) {
        if (OpenDeviceAo(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          goto __FAILED;
        }

        pstPlayer->pstAoCtx->chnIndex = 0;
        if (USE_AO_MIXER)
          SetAoChannelMode(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);

        if (InitMpiAO(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          RKADK_LOGE("InitMpiAO failed");
          goto __FAILED;
        }
      }

      if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
        ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
        if (ret != 0) {
          RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
          goto __FAILED;
        }
      }
    } else if (pstPlayer->demuxerFlag == VIDEO_FLAG) {
      if (pstPlayer->bVideoExist == RKADK_TRUE) {
        ret = RK_MPI_VDEC_StartRecvStream(pstPlayer->pstVdecCtx->chnIndex);
        if (ret != RK_SUCCESS) {
          RKADK_LOGE("start recv chn %d failed %X! ", pstPlayer->pstVdecCtx->chnIndex, ret);
          goto __FAILED;
        }

        if(VoStart(pstPlayer->pstVoCtx)) {
          RKADK_LOGE("start VO failed! ");
          goto __FAILED;
        }

        if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
          ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
          if (ret != 0) {
            RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
            goto __FAILED;
          }
        }
      }
    } else if (pstPlayer->demuxerFlag == AUDIO_FLAG) {
      if (pstPlayer->bAudioExist == RKADK_TRUE) {
        if (OpenDeviceAo(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          goto __FAILED;
        }

        pstPlayer->pstAoCtx->chnIndex = 0;
        if (USE_AO_MIXER)
          SetAoChannelMode(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);

        if (InitMpiAO(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          RKADK_LOGE("InitMpiAO failed");
          goto __FAILED;
        }

        if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
          ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
          if (ret != 0) {
            RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
            goto __FAILED;
          }
        }
      }
    }
  #else
    if (pstPlayer->demuxerFlag == AUDIO_FLAG) {
      if (pstPlayer->bAudioExist == RKADK_TRUE) {
        if (OpenDeviceAo(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          goto __FAILED;
        }

        pstPlayer->pstAoCtx->chnIndex = 0;
        if (USE_AO_MIXER)
          SetAoChannelMode(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);

        if (InitMpiAO(pstPlayer->pstAoCtx) != RKADK_SUCCESS) {
          RKADK_LOGE("InitMpiAO failed");
          goto __FAILED;
        }

        if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
          ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
          if (ret != 0) {
            RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
            goto __FAILED;
          }
        }
      }
    }
  #endif

    if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT)
      pthread_create(&pstPlayer->stThreadParam.tidEof, 0, EventEOF, pPlayer);
  }

  if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT)
    pthread_mutex_unlock(&pstPlayer->PauseAudioMutex);

  if (pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT) {
    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_PLAY, NULL);
  }

  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Stop(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0, ret1 = 0, seekFlag = RKADK_PLAYER_SEEK_FALSE;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (pstPlayer->seekFlag == RKADK_PLAYER_SEEK_WAIT)
    seekFlag = RKADK_PLAYER_SEEK_WAIT;

  if (pstPlayer->bStopFlag != RKADK_TRUE) {
    pstPlayer->bStopFlag = RKADK_TRUE;
    pstPlayer->seekFlag = RKADK_PLAYER_SEEK_FALSE;
    pstPlayer->seekTimeStamp = 0;

    if (pstPlayer->pauseFlag == RKADK_PLAYER_PAUSE_START) {
      #ifdef RV1126_1109
      if (pstPlayer->bVideoExist) {
        for (RKADK_S32 i = 0; i < pstPlayer->pstVoCtx->windows; i++) {
          ret = RK_MPI_VO_ResumeChn(pstPlayer->pstVoCtx->VoLayer, i);
          if (ret != RKADK_SUCCESS) {
            RKADK_LOGE("RK_MPI_VO_ResumeChn failed, ret = %X\n", ret);
            return RKADK_FAILURE;
          }
        }
      }
      #endif

      if (pstPlayer->bAudioExist) {
        ret = RK_MPI_AO_ResumeChn(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
        if (ret) {
          RKADK_LOGE("RK_MPI_AO_ResumeChn failed(%X)", ret);
          ret1 |= RKADK_FAILURE;
        }
      }
    }

    pstPlayer->bGetPtsFlag = RKADK_FALSE;
    pstPlayer->positionTimeStamp = 0;
    pstPlayer->pauseFlag = RKADK_PLAYER_PAUSE_FALSE;
    RKADK_DEMUXER_ReadPacketStop(pstPlayer->pDemuxerCfg);
    if (pstPlayer->bAudioExist == RKADK_TRUE && pstPlayer->pstAdecCtx->eCodecType != RKADK_CODEC_TYPE_PCM
        && !pstPlayer->bAudioStopFlag)
          RK_MPI_ADEC_SendEndOfStream(pstPlayer->pstAdecCtx->chnIndex, RK_FALSE);

    if (pstPlayer->stThreadParam.tidEof) {
      if (pstPlayer->bWavEofFlag != RKADK_TRUE && pstPlayer->bAudioExist && pstPlayer->pstAdecCtx->eCodecType == RKADK_CODEC_TYPE_PCM) {
        pthread_mutex_lock(&pstPlayer->WavMutex);
        pthread_cond_signal(&pstPlayer->WavCond);
        pthread_mutex_unlock(&pstPlayer->WavMutex);
        pthread_join(pstPlayer->stThreadParam.tidEof, RKADK_NULL);
      } else {
        pthread_join(pstPlayer->stThreadParam.tidEof, RKADK_NULL);
      }
    }

    #ifdef RV1126_1109
    if (pstPlayer->bVideoExist == RKADK_TRUE || pstPlayer->demuxerFlag == VIDEO_FLAG) {
      if (pstPlayer->pstVdecCtx) {
        if (DestroyVdec(pstPlayer->pstVdecCtx))
          ret1 |= RKADK_FAILURE;
      }
    }
    #endif

    if (pstPlayer->bAudioExist == RKADK_TRUE) {
      if (pstPlayer->pstAdecCtx->eCodecType != RKADK_CODEC_TYPE_PCM) {
        ret = RK_MPI_ADEC_DestroyChn(pstPlayer->pstAdecCtx->chnIndex);
        if (ret) {
          RKADK_LOGE("destory Adec channel failed(%X)", ret);
          ret1 |= RKADK_FAILURE;
        }
      }

      if (DeInitMpiAO(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex,
                      &pstPlayer->pstAoCtx->bopenChannelFlag)) {
        RKADK_LOGE("destory Ao channel failed");
        ret1 |= RKADK_FAILURE;
      }

      if (CloseDeviceAO(pstPlayer->pstAoCtx)) {
        RKADK_LOGE("destory Ao failed");
        ret1 |= RKADK_FAILURE;
      }
    }
  }

  if (pstPlayer->pfnPlayerCallback != NULL) {
    if (!ret1 && seekFlag != RKADK_PLAYER_SEEK_WAIT)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);
    else if (ret1)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  }

  return ret1;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  pthread_mutex_lock(&pstPlayer->PauseAudioMutex);
  pstPlayer->pauseFlag = RKADK_PLAYER_PAUSE_START;

  if (pstPlayer->bAudioExist) {
    ret = RK_MPI_AO_PauseChn(pstPlayer->pstAoCtx->devId, pstPlayer->pstAoCtx->chnIndex);
    if (ret != RKADK_SUCCESS) {
      RKADK_LOGE("RK_MPI_AO_PauseChn failed, ret = %X\n", ret);
      return RKADK_FAILURE;
    }
  }
  pthread_mutex_unlock(&pstPlayer->PauseAudioMutex);

  if (pstPlayer->pfnPlayerCallback != NULL && pstPlayer->seekFlag != RKADK_PLAYER_SEEK_WAIT)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_PAUSED, NULL);

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0, tmpPauseFlag = RKADK_PLAYER_PAUSE_FALSE;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_S64 maxSeekTimeInMs = (RKADK_S64)pow(2, 63) / 1000;

  if (s64TimeInMs > maxSeekTimeInMs) {
    RKADK_LOGE("s64TimeInMs(%lld) is out of range", s64TimeInMs);
    goto __FAILED;
  }

  if (pstPlayer->pauseFlag == RKADK_PLAYER_PAUSE_START)
    tmpPauseFlag = RKADK_PLAYER_PAUSE_START;

  pstPlayer->seekFlag = RKADK_PLAYER_SEEK_WAIT;
  if (pstPlayer->bStopFlag != RKADK_TRUE)
    RKADK_PLAYER_Stop(pPlayer);

  pstPlayer->seekFlag = RKADK_PLAYER_SEEK_WAIT;
  RKADK_PLAYER_SetDataSource(pstPlayer, pstPlayer->pFilePath);
  pstPlayer->seekTimeStamp = s64TimeInMs * 1000;
  RKADK_PLAYER_Play(pstPlayer);

  pstPlayer->bStopFlag = RKADK_FALSE;
  ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
  pthread_create(&pstPlayer->stThreadParam.tidEof, 0, EventEOF, pPlayer);
  if (ret != 0) {
    RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
    goto __FAILED;
  }

  if (tmpPauseFlag == RKADK_PLAYER_PAUSE_START) {
    RKADK_PLAYER_Pause(pstPlayer);
  }

  if (pstPlayer->bVideoExist) {
    pstPlayer->seekFlag = RKADK_PLAYER_SEEK_START;
  }

  if (pstPlayer->bAudioExist) {
    if (!pstPlayer->bVideoExist)
      pstPlayer->seekFlag = RKADK_PLAYER_SEEK_SEND;
  }

  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_SEEK_END, NULL);

  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetPlayStatus(RKADK_MW_PTR pPlayer,
                                     RKADK_PLAYER_STATE_E *penState) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(penState, RKADK_FAILURE);
  RKADK_LOGE("GetPlayStatus unsupport");
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetDuration(RKADK_MW_PTR pPlayer, RKADK_U32 *pDuration) {
  RKADK_CHECK_POINTER(pDuration, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  void *demuxerCfg;
  RKADK_DEMUXER_INPUT_S demuxerInput;
  RKADK_DEMUXER_PARAM_S demuxerParam;
  RKADK_S32 ret = 0, vFrameCnt = 0, aFrameCnt = 0;
  RKADK_S64 vDuration = 0, vDuration1 = 0, aDuration = 0;
  DemuxerPacket outputPacket;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (!pstPlayer->pFilePath) {
    RKADK_LOGE("RKADK_PLAYER_GetDuration failed, file is NULL");
    goto __FAILED;
  }

  demuxerInput.ptr = RK_NULL;
  demuxerInput.readModeFlag = DEMUXER_TYPE_ACTIVE;
  #ifdef RV1126_1109
  demuxerInput.videoEnableFlag = pstPlayer->bEnableVideo;
  #else
  demuxerInput.videoEnableFlag = 0;
  #endif

  demuxerInput.audioEnableFlag = pstPlayer->bEnableAudio;
  demuxerParam.pstReadPacketCallback.pfnReadVideoPacketCallback = NULL;
  demuxerParam.pstReadPacketCallback.pfnReadAudioPacketCallback = NULL;

  if (RKADK_DEMUXER_Create(&demuxerCfg, &demuxerInput)) {
    RKADK_LOGE("RKADK_DEMUXER_Create failed");
    return RKADK_FAILURE;
  }

  if (RKADK_DEMUXER_GetParam(demuxerCfg, pstPlayer->pFilePath, &demuxerParam)) {
    RKADK_LOGE("RKADK_DEMUXER_GetParam failed");
    goto __FAILED;
  }

  #ifdef RV1126_1109
  if (pstPlayer->bVideoExist) {
    while(1) {
      vFrameCnt++;
      ret = RKADK_DEMUXER_ReadOneVideoPacket(demuxerCfg, &outputPacket);
      if (ret != 0) {
        RKADK_LOGE("read %d video packet failed", vFrameCnt);
        break;
      }

      if (outputPacket.s8PacketData) {
        if (outputPacket.s64Duration)
          vDuration = outputPacket.s64Pts + outputPacket.s64Duration;

        free(outputPacket.s8PacketData);
        outputPacket.s8PacketData = NULL;
      }

      if (outputPacket.s8EofFlag)
        break;
    }

    *pDuration = vDuration / 1000;
  }
  #endif

  if (!pstPlayer->bVideoExist && pstPlayer->bAudioExist) {
    while(1) {
      aFrameCnt++;
      ret = RKADK_DEMUXER_ReadOneAudioPacket(demuxerCfg, &outputPacket);
      if (ret != 0) {
          RKADK_LOGE("read %d audio packet failed", aFrameCnt);
          break;
      }

      if (outputPacket.s8PacketData) {
        if (outputPacket.s64Duration)
          aDuration = outputPacket.s64Pts + outputPacket.s64Duration;
        free(outputPacket.s8PacketData);
        outputPacket.s8PacketData = NULL;
      }

      if (outputPacket.s8EofFlag)
        break;
    }

    *pDuration = aDuration / 1000;
  }

  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_SUCCESS;

__FAILED:
  RKADK_DEMUXER_Destroy(&demuxerCfg);
  return RKADK_FAILURE;
}

RKADK_S64 RKADK_PLAYER_GetCurrentPosition(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_S64 duration = 0;
  if (pstPlayer->bGetPtsFlag) {
    duration = pstPlayer->positionTimeStamp / 1000;
    return duration;
  } else
    return RKADK_FAILURE;

}
