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
#include "rk_comm_vdec.h"
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

typedef struct _RKADK_PLAYER_AO_CTX_S {
  const RKADK_CHAR *srcFilePath;
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
  RKADK_CHAR *cardName;
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

typedef struct _RKADK_PLAYER_THREAD_PARAM_S {
  pthread_t tid;
  pthread_t tidEof;
  pthread_t tidSend;
  pthread_t tidReceive;
} RKADK_PLAYER_THREAD_PARAM_S;

typedef struct _RKADK_PLAYER_HANDLE_S {
  RKADK_BOOL bEnableVideo;
  RKADK_BOOL bVideoExist;
  RKADK_BOOL bEnableAudio;
  RKADK_BOOL bAudioExist;
  RKADK_BOOL bStopFlag;
  RKADK_VOID *pListener;
  RKADK_VOID *pDemuxerCfg;
  RKADK_VOID *pVideoDecoder;
  RKADK_VOID *pAudioDecoder;
  RKADK_S8 demuxerFlag;
  RKADK_S8 audioDecoderMode;
  RKADK_DEMUXER_PARAM_S *pDemuxerParam;
  RKADK_PLAYER_AO_CTX_S *pAOCtx;
  RKADK_PLAYER_THREAD_PARAM_S stThreadParam;
  RKADK_PLAYER_EVENT_FN pfnPlayerCallback;
} RKADK_PLAYER_HANDLE_S;

RKADK_VOID QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
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

RKADK_S32 OpenDeviceAo(RKADK_PLAYER_AO_CTX_S *ctx) {
  int bytes = 2; // if the requirement is 16bit
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
  RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);
  RK_MPI_AO_Enable(aoDevId);
  ctx->openFlag = 1;
  return RK_SUCCESS;
}

RKADK_S32 InitMpiAO(RKADK_PLAYER_AO_CTX_S *params) {
  RKADK_S32 result;

  result = RK_MPI_AO_EnableChn(params->devId, params->chnIndex);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, aoChn = %d, reason = %x", params->chnIndex, result);
    return RKADK_FAILURE;
  }

  // set sample rate of input data
  result = RK_MPI_AO_EnableReSmp(params->devId, params->chnIndex,
                                (AUDIO_SAMPLE_RATE_E)params->reSmpSampleRate);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, reason = %x, aoChn = %d", result, params->chnIndex);
    return RKADK_FAILURE;
  }

  params->bopenChannelFlag = RKADK_TRUE;
  return RKADK_SUCCESS;
}

RKADK_S32 DeInitMpiAO(AUDIO_DEV aoDevId, AO_CHN aoChn, RKADK_BOOL *openFlag) {
  if (*openFlag) {
    RKADK_S32 result = RK_MPI_AO_DisableReSmp(aoDevId, aoChn);
    if (result != 0) {
      RKADK_LOGE("ao disable resample fail, reason = %x", result);
      return RKADK_FAILURE;
    }

    result = RK_MPI_AO_DisableChn(aoDevId, aoChn);
    if (result != 0) {
      RKADK_LOGE("ao disable channel fail, reason = %x", result);
      return RKADK_FAILURE;
    }

    *openFlag = RKADK_FALSE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 CloseDeviceAO(RKADK_PLAYER_AO_CTX_S *ctx) {
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

RKADK_S32 SetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RKADK_S32 result = 0;
  AO_CHN_PARAM_S pstParams;
  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));
  //aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0) {
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  } else if (aoChn == 1) {
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;
  }

  result = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (result != RKADK_SUCCESS) {
    RKADK_LOGE("ao set channel params, aoChn = %d", aoChn);
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_VOID* SendDataThread(RKADK_VOID *ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  uint32_t len = 4096;
  RKADK_CHAR *buf = (RKADK_CHAR *)calloc(len, sizeof(RKADK_CHAR));
  if (!buf) {
    RKADK_LOGE("malloc AO buf failed, bufsize = %d", len);
    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(ptr, RKADK_PLAYER_EVENT_ERROR, NULL);
    return RK_NULL;
  }

  AUDIO_FRAME_S frame;
  RKADK_U64 timeStamp = 0;
  RKADK_S32 s32MilliSec = -1;
  RKADK_S32 size = 0;
  RKADK_S32 result = 0;
  RKADK_LOGI("chnIndex : %d", pstPlayer->pAOCtx->chnIndex);

  while (1) {
    size = RKADK_AUDIO_DECODER_GetData(pstPlayer->pAudioDecoder, buf, len);

    frame.u32Len = size;
    frame.u64TimeStamp = timeStamp++;
    frame.enBitWidth = FindBitWidth(pstPlayer->pAOCtx->bitWidth);
    frame.enSoundMode = FindSoundMode(pstPlayer->pAOCtx->channel);
    frame.bBypassMbBlk = RK_FALSE;

    MB_EXT_CONFIG_S extConfig;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.pOpaque = (RK_U8 *)buf;
    extConfig.pu8VirAddr = (RK_U8 *)buf;
    extConfig.u64Size = size;
    RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);
    if (!pstPlayer->bStopFlag) {
      result = RK_MPI_AO_SendFrame(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex, &frame, s32MilliSec);
      if (result < 0) {
          RKADK_LOGE("send frame fail, result = %x, TimeStamp = %lld, s32MilliSec = %d",
              result, frame.u64TimeStamp, s32MilliSec);
      }
    }

    RK_MPI_MB_ReleaseMB(frame.pMbBlk);
    if (size <= 0) {
        RKADK_LOGI("eof");
        break;
    }
  }

  if (!pstPlayer->bStopFlag)
    RK_MPI_AO_WaitEos(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex, s32MilliSec);

  if (buf) {
    free(buf);
    buf = NULL;
  }

  return RK_NULL;
}

RKADK_VOID* CommandThread(RKADK_VOID * ptr) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;

  {
    AUDIO_FADE_S aFade;
    aFade.bFade = RK_FALSE;
    aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)pstPlayer->pAOCtx->setFadeRate;
    aFade.enFadeInRate = (AUDIO_FADE_RATE_E)pstPlayer->pAOCtx->setFadeRate;
    RK_BOOL mute = (pstPlayer->pAOCtx->setMute == 0) ? RK_FALSE : RK_TRUE;
    RK_MPI_AO_SetMute(pstPlayer->pAOCtx->devId, mute, &aFade);
    RK_MPI_AO_SetVolume(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->setVolume);
  }

  if (pstPlayer->pAOCtx->setTrackMode) {
    RKADK_LOGI("info : set track mode = %d", pstPlayer->pAOCtx->setTrackMode);
    RK_MPI_AO_SetTrackMode(pstPlayer->pAOCtx->devId, (AUDIO_TRACK_MODE_E)pstPlayer->pAOCtx->setTrackMode);
  }

  if (pstPlayer->pAOCtx->getVolume) {
    RKADK_S32 volume = 0;
    RK_MPI_AO_GetVolume(pstPlayer->pAOCtx->devId, &volume);
    RKADK_LOGI("info : get volume = %d", volume);
    pstPlayer->pAOCtx->getVolume = 0;
  }

  if (pstPlayer->pAOCtx->getMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(pstPlayer->pAOCtx->devId, &mute, &fade);
    RKADK_LOGI("info : is mute = %d", mute);
    pstPlayer->pAOCtx->getMute = 0;
  }

  if (pstPlayer->pAOCtx->getTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(pstPlayer->pAOCtx->devId, &trackMode);
    RKADK_LOGI("info : get track mode = %d", trackMode);
    pstPlayer->pAOCtx->getTrackMode = 0;
  }

  if (pstPlayer->pAOCtx->queryChnStat) {
    QueryAoFlowGraphStat(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
    pstPlayer->pAOCtx->queryChnStat = 0;
  }

  if (pstPlayer->pAOCtx->saveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pstPlayer->pAOCtx->dstFilePath) {
        saveFile.bCfg = RK_TRUE;
        saveFile.u32FileSize = 1024 * 1024;
        snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
        snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", pstPlayer->pAOCtx->dstFilePath);
    }
    RK_MPI_AO_SaveFile(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex, &saveFile);
    pstPlayer->pAOCtx->saveFile = 0;
  }

  if (pstPlayer->pAOCtx->queryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex, &fileStat);
    RKADK_LOGI("info : query save file status = %d", fileStat.bSaving);
    pstPlayer->pAOCtx->queryFileStat = 0;
  }

  if (pstPlayer->pAOCtx->pauseResumeChn) {
    usleep(500 * 1000);
    RK_MPI_AO_PauseChn(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
    RKADK_LOGI("AO pause");
    usleep(1000 * 1000);
    RK_MPI_AO_ResumeChn(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
    RKADK_LOGI("AO resume");
    pstPlayer->pAOCtx->pauseResumeChn = 0;
  }

  if (pstPlayer->pAOCtx->clrChnBuf) {
    RK_MPI_AO_ClearChnBuf(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
    pstPlayer->pAOCtx->clrChnBuf = 0;
  }

  if (pstPlayer->pAOCtx->clrPubAttr) {
    RK_MPI_AO_ClrPubAttr(pstPlayer->pAOCtx->devId);
    pstPlayer->pAOCtx->clrPubAttr = 0;
  }

  if (pstPlayer->pAOCtx->getPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(pstPlayer->pAOCtx->devId, &pstAttr);
    RKADK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RKADK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RKADK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RKADK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    pstPlayer->pAOCtx->getPubAttr = 0;
  }

  return RK_NULL;
}

void DoPullDemuxerVideoPacket(void* pHandle) {
  return;
}


void DoPullDemuxerAudioPacket(void* pHandle) {
  DemuxerPacket *pstDemuxerPacket = (DemuxerPacket *)pHandle;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pstDemuxerPacket->pPlayer;
  RKADK_AUDIO_DECODER_StreamPush(pstPlayer->pAudioDecoder, (char *)pstDemuxerPacket->s8PacketData, pstDemuxerPacket->s32PacketSize);

  return;
}

RKADK_VOID *DoPull(RKADK_VOID *arg) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)arg;
  if (pstPlayer->demuxerFlag == MIX_VIDEO_FLAG) {
    AUDIO_DECODER_CTX_S decoderCtx;
    if (RKADK_AUDIO_DECODER_GetInfo(pstPlayer->audioDecoderMode, pstPlayer->pAudioDecoder, &decoderCtx) != RKADK_SUCCESS) {
      RKADK_LOGE("RKADK_AUDIO_DECODER_GetInfo failed");
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }
  }

  if (pstPlayer->demuxerFlag == AUDIO_FLAG) {
    AUDIO_DECODER_CTX_S decoderCtx;
    memset(&decoderCtx, 0, sizeof(AUDIO_DECODER_CTX_S));
    if (RKADK_AUDIO_DECODER_GetInfo(pstPlayer->audioDecoderMode, pstPlayer->pAudioDecoder, &decoderCtx) != RKADK_SUCCESS) {
      RKADK_LOGE("RKADK_AUDIO_DECODER_GetInfo failed");
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }

    pstPlayer->pAOCtx->bitWidth = decoderCtx.bitWidth;
    pstPlayer->pAOCtx->channel = decoderCtx.channel;
    pstPlayer->pAOCtx->reSmpSampleRate = decoderCtx.reSmpSampleRate;

    if (pstPlayer->pAOCtx->channel <= 0
        || pstPlayer->pAOCtx->reSmpSampleRate <= 0) {
      RKADK_LOGE("AO create failed");
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }

    if (OpenDeviceAo(pstPlayer->pAOCtx) != RKADK_SUCCESS) {
      RKADK_LOGE("AO open failed");
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }

    pstPlayer->pAOCtx->chnIndex = 0;

    if (USE_AO_MIXER) {
      SetAoChannelMode(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
    }

    if (InitMpiAO(pstPlayer->pAOCtx) != RKADK_SUCCESS) {
      RKADK_LOGE("AO init failed");
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }
  }

  if (pstPlayer->bAudioExist == RKADK_TRUE || pstPlayer->demuxerFlag == AUDIO_FLAG) {
    pthread_create(&pstPlayer->stThreadParam.tidSend, RK_NULL, SendDataThread, arg);
    pthread_create(&pstPlayer->stThreadParam.tidReceive, RK_NULL, CommandThread, arg);
  }

  return NULL;
}

RKADK_S32 CreateAOCtx(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  pstPlayer->pAOCtx = (RKADK_PLAYER_AO_CTX_S *)malloc(sizeof(RKADK_PLAYER_AO_CTX_S));
  if (!pstPlayer->pAOCtx) {
    RKADK_LOGE("malloc pAOCtx falied");
    return RKADK_FAILURE;
  }

  memset(pstPlayer->pAOCtx, 0, sizeof(RKADK_PLAYER_AO_CTX_S));

  pstPlayer->pAOCtx->srcFilePath        = RK_NULL;
  pstPlayer->pAOCtx->dstFilePath        = RK_NULL;
  pstPlayer->pAOCtx->chnNum          = 1;
  pstPlayer->pAOCtx->sampleRate      = AUDIO_SAMPLE_RATE;
  pstPlayer->pAOCtx->reSmpSampleRate = 0;
  pstPlayer->pAOCtx->deviceChannel   = AUDIO_DEVICE_CHANNEL;
  pstPlayer->pAOCtx->channel         = 2;
  pstPlayer->pAOCtx->bitWidth        = AUDIO_BIT_WIDTH;
  pstPlayer->pAOCtx->periodCount     = 2;
  pstPlayer->pAOCtx->periodSize      = AUDIO_FRAME_COUNT;
  pstPlayer->pAOCtx->cardName        = RK_NULL;
  pstPlayer->pAOCtx->cardName        = (RKADK_CHAR *)AI_DEVICE_NAME;
  pstPlayer->pAOCtx->devId           = 0;
  pstPlayer->pAOCtx->setVolume       = 100;
  pstPlayer->pAOCtx->setMute         = 0;
  pstPlayer->pAOCtx->setTrackMode    = 0;
  pstPlayer->pAOCtx->setFadeRate     = 0;
  pstPlayer->pAOCtx->getVolume       = 0;
  pstPlayer->pAOCtx->getMute         = 0;
  pstPlayer->pAOCtx->getTrackMode    = 0;
  pstPlayer->pAOCtx->queryChnStat    = 0;
  pstPlayer->pAOCtx->pauseResumeChn  = 0;
  pstPlayer->pAOCtx->saveFile        = 0;
  pstPlayer->pAOCtx->queryFileStat   = 0;
  pstPlayer->pAOCtx->clrChnBuf       = 0;
  pstPlayer->pAOCtx->clrPubAttr      = 0;
  pstPlayer->pAOCtx->getPubAttr      = 0;
  return RKADK_SUCCESS;
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
    return RKADK_FAILURE;
  }

  pstPlayer->bEnableVideo = pstPlayCfg->bEnableVideo;
  pstPlayer->bEnableAudio = pstPlayCfg->bEnableAudio;

  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);
  *pPlayer = (RKADK_MW_PTR)pstPlayer;
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_LOGI("Destory Player Start...");
  RKADK_PLAYER_Stop(pPlayer);

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
  RKADK_S32 ret = 0;
  RKADK_CODEC_TYPE_E eAudioCodecType = RKADK_CODEC_TYPE_BUTT;
  if ((strlen(pszfilePath) <= 0) || (strlen(pszfilePath) >= 100)) {
    RKADK_LOGE("The length(%d) of the file name is unreasonable", strlen(pszfilePath));
    if (pstPlayer->pfnPlayerCallback != NULL)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    return RKADK_FALSE;
  }

  pstPlayer->bStopFlag = RKADK_FALSE;
  for(RKADK_S32 i = strlen(pszfilePath) - 1; i >= 0; i--) {
    if ('.' == pszfilePath[i]) {
      if(!strcmp(pszfilePath + i + 1, "mp4")) {
        pstPlayer->demuxerFlag = MIX_VIDEO_FLAG;
        pstPlayer->pDemuxerParam = (RKADK_DEMUXER_PARAM_S *)malloc(sizeof(RKADK_DEMUXER_PARAM_S));
        if (!pstPlayer->pDemuxerParam) {
          RKADK_LOGE("malloc pDemuxerParam falied");
          return RKADK_FALSE;
        }

        memset(pstPlayer->pDemuxerParam, 0, sizeof(RKADK_DEMUXER_PARAM_S));
        pstPlayer->pDemuxerParam->pPlayer = pPlayer;
        pstPlayer->pDemuxerParam->pstReadPacketCallback.pfnReadVideoPacketCallback = DoPullDemuxerVideoPacket;
        pstPlayer->pDemuxerParam->pstReadPacketCallback.pfnReadAudioPacketCallback = DoPullDemuxerAudioPacket;
        ret = RKADK_DEMUXER_Create(&pstPlayer->pDemuxerCfg, pszfilePath, pstPlayer->pDemuxerParam);
        if (ret != 0) {
            RKADK_LOGE("RKADK_PLAYER_SetDataSource failed");
            goto __FAILED;
        }

        if (pstPlayer->bEnableVideo == RKADK_TRUE) {
          if (pstPlayer->pDemuxerParam->pVideoCodec != NULL) {
            pstPlayer->bVideoExist = RKADK_TRUE;
            if (!strcmp(pstPlayer->pDemuxerParam->pAudioCodec, "h264"))
              eAudioCodecType = RKADK_CODEC_TYPE_H264;
            else if (!strcmp(pstPlayer->pDemuxerParam->pAudioCodec, "h265"))
              eAudioCodecType = RKADK_CODEC_TYPE_H264;
            else {
              RKADK_LOGE("Unsupported video format(%s)", pstPlayer->pDemuxerParam->pVideoCodec);
              goto __FAILED;
            }

          } else if (pstPlayer->bEnableAudio == RKADK_FALSE) {
            RKADK_LOGE("Video does not exist and audio exists but cannot be played");
            goto __FAILED;
          }
        }

        if (pstPlayer->bEnableAudio == RKADK_TRUE) {
          if (pstPlayer->pDemuxerParam->pAudioCodec != NULL) {
            if (!strcmp(pstPlayer->pDemuxerParam->pAudioCodec, "mp3"))
              eAudioCodecType = RKADK_CODEC_TYPE_MP3;
            else if (!strcmp(pstPlayer->pDemuxerParam->pAudioCodec, "wav"))
              eAudioCodecType = RKADK_CODEC_TYPE_PCM;
            else {
              RKADK_LOGE("Unsupported audio format(%s)", pstPlayer->pDemuxerParam->pAudioCodec);
              goto __FAILED;
            }

            pstPlayer->bAudioExist = RKADK_TRUE;
            if(CreateAOCtx(pPlayer)) {
              RKADK_LOGE("Create AO Ctx failed");
              goto __FAILED;
            }

            pstPlayer->pAOCtx->reSmpSampleRate = pstPlayer->pDemuxerParam->audioSampleRate;
            if (pstPlayer->pDemuxerParam->audioFormat == 0)
              pstPlayer->pAOCtx->bitWidth = 8;
            else if (pstPlayer->pDemuxerParam->audioFormat == 1)
              pstPlayer->pAOCtx->bitWidth = 16;
            else {
              RKADK_LOGE("AO create failed, audioFormat = %d", pstPlayer->pDemuxerParam->audioFormat);
              goto __FAILED;
            }

            if (pstPlayer->pDemuxerParam->audioChannels <= 0
            || pstPlayer->pDemuxerParam->audioSampleRate <= 0) {
              RKADK_LOGE("AO create failed, channel = %d, reSmpSampleRate = %d",
                          pstPlayer->pDemuxerParam->audioChannels, pstPlayer->pDemuxerParam->audioSampleRate);

            }

            pstPlayer->audioDecoderMode = STREAM_MODE;
            ret = RKADK_AUDIO_DECODER_Create(pstPlayer->audioDecoderMode, eAudioCodecType, pszfilePath, &pstPlayer->pAudioDecoder);
            if (ret) {
              RKADK_LOGE("RKADK_AUDIO_DECODER_Create failed(%d)", ret);
              goto __FAILED;
            }

          } else if (pstPlayer->bEnableVideo == RKADK_FALSE) {
            RKADK_LOGE("Audio does not exist, and video exists but cannot be played");
            goto __FAILED;
          }
        }
      } else if (!strcmp(pszfilePath + i + 1, "h264")) {
        if (pstPlayer->bEnableAudio == RKADK_TRUE) {
          pstPlayer->demuxerFlag = VIDEO_FLAG;

          if (!strcmp(pszfilePath + i + 1, "h264"))
            eAudioCodecType = RKADK_CODEC_TYPE_H264;
        }

        return RKADK_SUCCESS;
      } else if ((!strcmp(pszfilePath + i + 1, "mp3")) || (!strcmp(pszfilePath + i + 1, "wav")) ||
                 (!strcmp(pszfilePath + i + 1, "pcm"))) {
        if (pstPlayer->bEnableAudio == RKADK_TRUE) {
          pstPlayer->demuxerFlag = AUDIO_FLAG;
          pstPlayer->audioDecoderMode = FILE_MODE;
          if(CreateAOCtx(pPlayer)) {
              RKADK_LOGE("Create AO Ctx failed");
              goto __FAILED;
          }

          if (!strcmp(pszfilePath + i + 1, "mp3"))
            eAudioCodecType = RKADK_CODEC_TYPE_MP3;
          ret = RKADK_AUDIO_DECODER_Create(pstPlayer->audioDecoderMode, eAudioCodecType, pszfilePath, &pstPlayer->pAudioDecoder);
          if (ret) {
            RKADK_LOGE("RKADK_AUDIO_DECODER_Create failed(%d)", ret);
            goto __FAILED;
          }
        }

        return RKADK_SUCCESS;
      } else {
        RKADK_LOGE("Unsupported file format(%s)", pszfilePath);
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
        return RKADK_FALSE;
      }
        break;
    }

    if (i < 0) {
      RKADK_LOGE("File suffix does not exist(%s)", pszfilePath);
      goto __FAILED;
    }
  }

  return RKADK_SUCCESS;

__FAILED:
  if (pstPlayer->pAOCtx) {
    free(pstPlayer->pAOCtx);
    pstPlayer->pAOCtx = NULL;
  }

  if (pstPlayer->pDemuxerParam) {
    free(pstPlayer->pDemuxerParam);
    pstPlayer->pDemuxerParam = NULL;
  }

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
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_LOGE("SetVideoSink unsupport");
  return RKADK_FAILURE;
}

RKADK_VOID *EventEOF(RKADK_VOID *arg) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)arg;
  if (pstPlayer->demuxerFlag == AUDIO_FLAG) {
    RKADK_S32 ret = RKADK_AUDIO_DECODER_FilePush(pstPlayer->pAudioDecoder, pstPlayer->bStopFlag);
    if (ret) {
      RKADK_LOGE("RKADK_AUDIO_DECODER_FilePush failed(%d)", ret);
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
      return NULL;
    }
  }

  if (pstPlayer->stThreadParam.tid)
    pthread_join(pstPlayer->stThreadParam.tid, NULL);

  if (pstPlayer->stThreadParam.tidSend)
    pthread_join(pstPlayer->stThreadParam.tidSend, RK_NULL);

  if (pstPlayer->stThreadParam.tidReceive)
    pthread_join(pstPlayer->stThreadParam.tidReceive, RK_NULL);

  if (pstPlayer->pfnPlayerCallback != NULL && !pstPlayer->bStopFlag)
    pstPlayer->pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_EOF, NULL);

  return NULL;
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_S32 ret = 0;
  if (pstPlayer->pfnPlayerCallback != NULL)
    pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STARTED, NULL);

  if (pstPlayer->demuxerFlag == MIX_VIDEO_FLAG) {
    if (pstPlayer->bAudioExist == RKADK_TRUE) {
      ret = RKADK_AUDIO_DECODER_Start(pstPlayer->pAudioDecoder);
      if (ret) {
        RKADK_LOGE("AUDIO_DECODER_Play_Mix failed(%d)", ret);
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
        return RKADK_FAILURE;
      }

      if (OpenDeviceAo(pstPlayer->pAOCtx) != RKADK_SUCCESS) {
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
        return RKADK_FAILURE;
      }

      pstPlayer->pAOCtx->chnIndex = 0;
      if (USE_AO_MIXER) {
        SetAoChannelMode(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex);
      }

      if (InitMpiAO(pstPlayer->pAOCtx) != RKADK_SUCCESS) {
        RKADK_LOGE("InitMpiAO failed");
        if (pstPlayer->pfnPlayerCallback != NULL)
          pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
        return RKADK_FAILURE;
      }

      ret = RKADK_DEMUXER_ReadPacketStart(pstPlayer->pDemuxerCfg);
      if (ret != 0) {
          RKADK_LOGE("RKADK_DEMUXER_ReadPacketStart failed");
          if (pstPlayer->pfnPlayerCallback != NULL)
            pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
          return RKADK_FAILURE;
      }
    }
  } else if (pstPlayer->demuxerFlag == AUDIO_FLAG) {
    ret = RKADK_AUDIO_DECODER_Start(pstPlayer->pAudioDecoder);
    if (ret) {
      RKADK_LOGE("AUDIO_DECODER_Play_Mix failed(%d)", ret);
      if (pstPlayer->pfnPlayerCallback != NULL)
        pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
      return RKADK_FAILURE;
    }
  }

  pthread_create(&pstPlayer->stThreadParam.tid, 0, DoPull, pPlayer);
  pthread_attr_t tidAttr;
  pthread_attr_init(&tidAttr);
  pthread_attr_setdetachstate(&tidAttr, PTHREAD_CREATE_DETACHED);
  pthread_create(&pstPlayer->stThreadParam.tidEof, &tidAttr, &EventEOF, pPlayer);
  pthread_attr_destroy (&tidAttr);
  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Stop(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  if (pstPlayer->bStopFlag != RKADK_TRUE) {
    pstPlayer->bStopFlag = RKADK_TRUE;
    if (pstPlayer->demuxerFlag == MIX_VIDEO_FLAG) {
      if (pstPlayer->pDemuxerCfg != NULL) {
        RKADK_DEMUXER_Destroy(&pstPlayer->pDemuxerCfg);
      }

      if (pstPlayer->pDemuxerParam != NULL) {
        free(pstPlayer->pDemuxerParam);
        pstPlayer->pDemuxerParam = NULL;
      }
    }

    if (pstPlayer->bAudioExist == RKADK_TRUE || pstPlayer->demuxerFlag == AUDIO_FLAG) {
      if (RKADK_AUDIO_DECODER_Stop(pstPlayer->pAudioDecoder)) {
        RKADK_LOGE("RKADK_AUDIO_DECODER_Stop failed(%d)", ret);
        ret = RKADK_FAILURE;
      }

      if (RKADK_AUDIO_DECODER_Destroy(pstPlayer->demuxerFlag, &(pstPlayer->pAudioDecoder))) {
        RKADK_LOGE("RKADK_AUDIO_DECODER_Destroy failed(%d)", ret);
        ret = RKADK_FAILURE;
      }

      if (DeInitMpiAO(pstPlayer->pAOCtx->devId, pstPlayer->pAOCtx->chnIndex,
                      &pstPlayer->pAOCtx->bopenChannelFlag)) {
        RKADK_LOGE("Ao destory failed(%d)", ret);
        ret = RKADK_FAILURE;
      }

      if (CloseDeviceAO(pstPlayer->pAOCtx)) {
        RKADK_LOGE("Ao destory failed(%d)", ret);
        ret = RKADK_FAILURE;
      }

      if (pstPlayer->pAOCtx) {
        free(pstPlayer->pAOCtx);
        pstPlayer->pAOCtx = NULL;
      }
    }
  }

  if (pstPlayer->pfnPlayerCallback != NULL) {
    if (!ret)
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);
    else
      pstPlayer->pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
  }

  return ret;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
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
  RKADK_LOGE("GetDuration unsupport");
  return RKADK_FAILURE;
}
