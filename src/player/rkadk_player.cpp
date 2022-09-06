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
#include "rk_debug.h"
#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "audio_server.h"

extern RKADK_S32 pcmout_open_impl(struct playback_device *self, playback_device_cfg_t *cfg);
extern RKADK_S32 pcmout_start_impl(struct playback_device *self);
extern RKADK_S32 pcmout_write_impl(struct playback_device *self, const RKADK_CHAR *data, size_t data_len);
extern RKADK_S32 pcmout_stop_impl(struct playback_device *self);
extern RKADK_S32 pcmout_abort_impl(struct playback_device *self);
extern RKADK_VOID pcmout_close_impl(struct playback_device *self);

#define PCM_DEVICE { \
  .open = pcmout_open_impl, \
  .start = pcmout_start_impl, \
  .write = pcmout_write_impl, \
  .stop = pcmout_stop_impl, \
  .abort = pcmout_abort_impl, \
  .close = pcmout_close_impl, \
}

RKADK_VOID AudioPlayerCallback(player_handle_t self, play_info_t info, RKADK_VOID *userdata);

static player_cfg_t stPlayerCfg =
{
  .tag = "one",
  .device = PCM_DEVICE,
  .preprocess_buf_size = 1024 * 40,
  .decode_buf_size = 1024 * 20,
  .preprocess_stack_size = 2048,
  .decoder_stack_size = 1024 * 12,
  .playback_stack_size = 2048,
  .listen = AudioPlayerCallback,
};

#ifdef __cplusplus
}
#endif

#define USE_AO_MIXER 0
#define AUDIO_BUF_SIZE 2048

/* The attributes of a aio device */
typedef struct rkAUDIO_DEV_ATTR_S {
  RKADK_U32 u32PeriodSize;
  RKADK_U32 u32PeriodCount;
  RKADK_U32 u32StartDelay;
  RKADK_U32 u32StopDelay;
} AUDIO_DEV_ATTR_S;

typedef struct rkMpiAOCtx {
  const RKADK_CHAR *srcFilePath;
  const RKADK_CHAR *dstFilePath;
  RKADK_S32 s32LoopCount;
  RKADK_S32 s32ChnNum;
  RKADK_S32 s32SampleRate;
  RKADK_S32 s32ReSmpSampleRate;
  RKADK_S32 s32Channel;
  RKADK_S32 s32DeviceChannel;
  RKADK_S32 s32BitWidth;
  RKADK_S32 s32DevId;
  RKADK_S32 s32PeriodCount;
  RKADK_S32 s32PeriodSize;
  RKADK_CHAR *chCardName;
  RKADK_S32 s32ChnIndex;
  RKADK_S32 s32SetVolume;
  RKADK_S32 s32SetMute;
  RKADK_S32 s32SetFadeRate;
  RKADK_S32 s32SetTrackMode;
  RKADK_S32 s32GetVolume;
  RKADK_S32 s32GetMute;
  RKADK_S32 s32GetTrackMode;
  RKADK_S32 s32QueryChnStat;
  RKADK_S32 s32PauseResumeChn;
  RKADK_S32 s32SaveFile;
  RKADK_S32 s32QueryFileStat;
  RKADK_S32 s32ClrChnBuf;
  RKADK_S32 s32ClrPubAttr;
  RKADK_S32 s32GetPubAttr;
  RKADK_S32 s32OpenFlag;
} PLAYER_AO_CTX_S;

typedef struct _PLAYER_THREAD_PARAM_S {
  pthread_t tid;
  pthread_t tidEof;
  pthread_t tidSend;
  pthread_t tidReceive;
} PLAYER_THREAD_PARAM_S;

typedef struct {
  RKADK_BOOL bEnableVideo;
  RKADK_VOID *pListener;
  FILE *pFin;
  PLAYER_AO_CTX_S *pCtx;
  PLAYER_THREAD_PARAM_S stThreadParam;
  RKADK_CHAR *pAudioBuf;
  player_handle_t mAudioPlayer;
  play_cfg_t *pAudioPlayerCfg;
} RKADK_PLAYER_HANDLE_S;

static RKADK_PLAYER_EVENT_FN g_pfnPlayerCallback = NULL;

RKADK_VOID AudioPlayerCallback(player_handle_t self, play_info_t info, RKADK_VOID *userdata) {
  printf("MP3 playback end\n");
}

RKADK_VOID QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 ret = 0;
  AO_CHN_STATE_S pstStat;
  memset(&pstStat, 0, sizeof(AO_CHN_STATE_S));
  ret = RK_MPI_AO_QueryChnStat(aoDevId, aoChn, &pstStat);
  if (ret == RK_SUCCESS) {
    RKADK_LOGI("query ao flow status:");
    RKADK_LOGI("total number of channel buffer : %d", pstStat.u32ChnTotalNum);
    RKADK_LOGI("free number of channel buffer : %d", pstStat.u32ChnFreeNum);
    RKADK_LOGI("busy number of channel buffer : %d", pstStat.u32ChnBusyNum);
  }
}

static AUDIO_SOUND_MODE_E FindSoundMode(RK_S32 ch) {
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

static AUDIO_BIT_WIDTH_E FindBitWidth(RK_S32 bit) {
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

RK_S32 OpenDeviceAo(PLAYER_AO_CTX_S *ctx) {
  int bytes = 2; // if the requirement is 16bit
  AUDIO_DEV aoDevId = ctx->s32DevId;
  AUDIO_SOUND_MODE_E soundMode;

  AIO_ATTR_S aoAttr;
  memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

  if (ctx->chCardName) {
    snprintf((RKADK_CHAR *)(aoAttr.u8CardName),
              sizeof(aoAttr.u8CardName), "%s", ctx->chCardName);
  }

  aoAttr.soundCard.channels = AUDIO_DEVICE_CHANNEL;
  aoAttr.soundCard.sampleRate = ctx->s32SampleRate;
  aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

  AUDIO_BIT_WIDTH_E bitWidth = FindBitWidth(ctx->s32BitWidth);
  if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
    goto __FAILED;
  }

  bytes = ctx->s32BitWidth / 8;
  aoAttr.enBitwidth = bitWidth;
  aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->s32ReSmpSampleRate;
  soundMode = FindSoundMode(ctx->s32Channel);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    goto __FAILED;
  }
  aoAttr.enSoundmode = soundMode;
  aoAttr.u32FrmNum = ctx->s32PeriodCount;
  aoAttr.u32PtNumPerFrm = bytes * ctx->s32PeriodSize;

  aoAttr.u32EXFlag = 0;
  aoAttr.u32ChnCnt = 2;

  RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);

  RK_MPI_AO_Enable(aoDevId);
  ctx->s32OpenFlag = 1;
  return RK_SUCCESS;
__FAILED:
  return RK_FAILURE;
}

RK_S32 InitMpiAO(PLAYER_AO_CTX_S *params) {
  RK_S32 result;

  result = RK_MPI_AO_EnableChn(params->s32DevId, params->s32ChnIndex);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, aoChn = %d, reason = %x", params->s32ChnIndex, result);
    return RK_FAILURE;
  }

  // set sample rate of input data
  result = RK_MPI_AO_EnableReSmp(params->s32DevId, params->s32ChnIndex,
                                (AUDIO_SAMPLE_RATE_E)params->s32ReSmpSampleRate);
  if (result != 0) {
    RKADK_LOGE("ao enable channel fail, reason = %x, aoChn = %d", result, params->s32ChnIndex);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

RK_S32 DeInitMpiAO(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 result = RK_MPI_AO_DisableReSmp(aoDevId, aoChn);
  if (result != 0) {
    RKADK_LOGE("ao disable resample fail, reason = %d", result);
    return RK_FAILURE;
  }

  result = RK_MPI_AO_DisableChn(aoDevId, aoChn);
  if (result != 0) {
    RKADK_LOGE("ao disable channel fail, reason = %d", result);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

RK_S32 CloseDeviceAO(PLAYER_AO_CTX_S *ctx) {
  AUDIO_DEV aoDevId = ctx->s32DevId;
  if (ctx->s32OpenFlag == 1) {
    RK_S32 result = RK_MPI_AO_Disable(aoDevId);
    if (result != 0) {
      RKADK_LOGE("ao disable fail, reason = %X", result);
      return RK_FAILURE;
    }
    ctx->s32OpenFlag = 0;
  }

  return RK_SUCCESS;
}

RK_S32 SetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 result = 0;
  AO_CHN_PARAM_S pstParams;
  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));
  //aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0) {
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  } else if (aoChn == 1) {
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;
  }

  result = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (result != RK_SUCCESS) {
    RKADK_LOGE("ao set channel params, aoChn = %d", aoChn);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

RKADK_VOID* SendDataThread(RKADK_VOID * ptr) {
  RKADK_CHAR *buf = (RKADK_CHAR *)malloc(4096);
  uint32_t len = 4096;
  RKADK_PLAYER_HANDLE_S *pPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;
  MB_POOL_CONFIG_S pool_config;
  // set default value for struct
  RK_U8 *srcData = RK_NULL;
  AUDIO_FRAME_S frame;
  RK_U64 timeStamp = 0;
  RK_S32 s32MilliSec = -1;
  RK_S32 size = 0;
  RK_S32 result = 0;
  FILE *file = RK_NULL;
  RKADK_LOGI("s32ChnIndex : %d", pPlayer->pCtx->s32ChnIndex);

  srcData = (RK_U8 *)(calloc(len, sizeof(RK_U8)));
  memset(srcData, 0, len);
  while (1) {
    size = player_pull(pPlayer->mAudioPlayer, buf, len);
    srcData = (RK_U8 *)buf;

    frame.u32Len = size;
    frame.u64TimeStamp = timeStamp++;
    frame.enBitWidth = FindBitWidth(pPlayer->pCtx->s32BitWidth);
    frame.enSoundMode = FindSoundMode(pPlayer->pCtx->s32Channel);
    frame.bBypassMbBlk = RK_FALSE;

    MB_EXT_CONFIG_S extConfig;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.pOpaque = srcData;
    extConfig.pu8VirAddr = srcData;
    extConfig.u64Size = size;
    RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);

    result = RK_MPI_AO_SendFrame(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex, &frame, s32MilliSec);
    if (result < 0) {
        RKADK_LOGE("send frame fail, result = %d, TimeStamp = %lld, s32MilliSec = %d",
            result, frame.u64TimeStamp, s32MilliSec);
    }
    RK_MPI_MB_ReleaseMB(frame.pMbBlk);

    if (size <= 0) {
        RKADK_LOGI("eof");
        break;
    }
  }

  RK_MPI_AO_WaitEos(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex, s32MilliSec);
  free(srcData);
  return RK_NULL;
}

RKADK_VOID* CommandThread(RKADK_VOID * ptr) {
  RKADK_PLAYER_HANDLE_S *pPlayer = (RKADK_PLAYER_HANDLE_S *)ptr;

  {
    AUDIO_FADE_S aFade;
    aFade.bFade = RK_FALSE;
    aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)pPlayer->pCtx->s32SetFadeRate;
    aFade.enFadeInRate = (AUDIO_FADE_RATE_E)pPlayer->pCtx->s32SetFadeRate;
    RK_BOOL mute = (pPlayer->pCtx->s32SetMute == 0) ? RK_FALSE : RK_TRUE;
    RKADK_LOGI("info : mute = %d, volume = %d", mute, pPlayer->pCtx->s32SetVolume);
    RK_MPI_AO_SetMute(pPlayer->pCtx->s32DevId, mute, &aFade);
    RK_MPI_AO_SetVolume(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32SetVolume);
  }

  if (pPlayer->pCtx->s32SetTrackMode) {
    RKADK_LOGI("info : set track mode = %d", pPlayer->pCtx->s32SetTrackMode);
    RK_MPI_AO_SetTrackMode(pPlayer->pCtx->s32DevId, (AUDIO_TRACK_MODE_E)pPlayer->pCtx->s32SetTrackMode);
  }

  if (pPlayer->pCtx->s32GetVolume) {
    RK_S32 volume = 0;
    RK_MPI_AO_GetVolume(pPlayer->pCtx->s32DevId, &volume);
    RKADK_LOGI("info : get volume = %d", volume);
    pPlayer->pCtx->s32GetVolume = 0;
  }

  if (pPlayer->pCtx->s32GetMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(pPlayer->pCtx->s32DevId, &mute, &fade);
    RKADK_LOGI("info : is mute = %d", mute);
    pPlayer->pCtx->s32GetMute = 0;
  }

  if (pPlayer->pCtx->s32GetTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(pPlayer->pCtx->s32DevId, &trackMode);
    RKADK_LOGI("info : get track mode = %d", trackMode);
    pPlayer->pCtx->s32GetTrackMode = 0;
  }

  if (pPlayer->pCtx->s32QueryChnStat) {
    QueryAoFlowGraphStat(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
    pPlayer->pCtx->s32QueryChnStat = 0;
  }

  if (pPlayer->pCtx->s32SaveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (pPlayer->pCtx->dstFilePath) {
        saveFile.bCfg = RK_TRUE;
        saveFile.u32FileSize = 1024 * 1024;
        snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
        snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", pPlayer->pCtx->dstFilePath);
    }
    RK_MPI_AO_SaveFile(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex, &saveFile);
    pPlayer->pCtx->s32SaveFile = 0;
  }

  if (pPlayer->pCtx->s32QueryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex, &fileStat);
    RKADK_LOGI("info : query save file status = %d", fileStat.bSaving);
    pPlayer->pCtx->s32QueryFileStat = 0;
  }

  if (pPlayer->pCtx->s32PauseResumeChn) {
    usleep(500 * 1000);
    RK_MPI_AO_PauseChn(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
    RKADK_LOGI("AO pause");
    usleep(1000 * 1000);
    RK_MPI_AO_ResumeChn(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
    RKADK_LOGI("AO resume");
    pPlayer->pCtx->s32PauseResumeChn = 0;
  }

  if (pPlayer->pCtx->s32ClrChnBuf) {
    RK_MPI_AO_ClearChnBuf(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
    pPlayer->pCtx->s32ClrChnBuf = 0;
  }

  if (pPlayer->pCtx->s32ClrPubAttr) {
    RK_MPI_AO_ClrPubAttr(pPlayer->pCtx->s32DevId);
    pPlayer->pCtx->s32ClrPubAttr = 0;
  }

  if (pPlayer->pCtx->s32GetPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(pPlayer->pCtx->s32DevId, &pstAttr);
    RKADK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RKADK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RKADK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RKADK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    pPlayer->pCtx->s32GetPubAttr = 0;
  }

  return RK_NULL;
}

RKADK_VOID *DoPull(RKADK_VOID *arg) {
  struct audio_config config;
  RKADK_PLAYER_HANDLE_S *pPlayer = (RKADK_PLAYER_HANDLE_S *)arg;
  player_audio_info(pPlayer->mAudioPlayer, &config, -1);
  pPlayer->pCtx->s32ReSmpSampleRate = config.sample_rate;
  pPlayer->pCtx->s32Channel = AUDIO_DEVICE_CHANNEL;
  pPlayer->pCtx->s32BitWidth = config.bits;

  if (pPlayer->pCtx->s32Channel <= 0
  || pPlayer->pCtx->s32ReSmpSampleRate <= 0) {
    RKADK_LOGE("AO create failed");
    return NULL;
  }

  RKADK_S32 *ReSmpSampleRate = 0;
  if (OpenDeviceAo(pPlayer->pCtx) != RK_SUCCESS) {
    RKADK_LOGE("AO open failed");
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
    return NULL;
  }

  pPlayer->pCtx->s32ChnIndex = 0;

  if (USE_AO_MIXER) {
    SetAoChannelMode(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
  }

  if (InitMpiAO(pPlayer->pCtx) != RK_SUCCESS) {
    RKADK_LOGE("AO init failed");
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(arg, RKADK_PLAYER_EVENT_ERROR, NULL);
    return NULL;
  }

  pthread_create(&pPlayer->stThreadParam.tidSend, RK_NULL, SendDataThread, arg);
  pthread_create(&pPlayer->stThreadParam.tidReceive, RK_NULL, CommandThread, arg);

  return NULL;
}

RKADK_S32 RKADK_PLAYER_Create(RKADK_MW_PTR *pPlayer,
                              RKADK_PLAYER_CFG_S *pstPlayCfg) {
  RKADK_CHECK_POINTER(pstPlayCfg, RKADK_FAILURE);
  RKADK_S32 ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  g_pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;

  if (*pPlayer) {
    RKADK_LOGE("player has been created");
    return -1;
  }

  RKADK_LOGI("Create Player[%d, %d] Start...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  pstPlayer = (RKADK_PLAYER_HANDLE_S *)malloc(sizeof(RKADK_PLAYER_HANDLE_S));
  if (!pstPlayer) {
    RKADK_LOGE("malloc pstPlayer failed");
    return -1;
  }

  pstPlayer->bEnableVideo = pstPlayCfg->bEnableVideo;
  pstPlayer->pFin = NULL;
  pstPlayer->pAudioBuf = NULL;

  pstPlayer->pCtx = (PLAYER_AO_CTX_S *)(malloc(sizeof(PLAYER_AO_CTX_S)));
  memset(pstPlayer->pCtx, 0, sizeof(PLAYER_AO_CTX_S));

  pstPlayer->pCtx->srcFilePath        = RK_NULL;
  pstPlayer->pCtx->dstFilePath        = RK_NULL;
  pstPlayer->pCtx->s32LoopCount       = 1;
  pstPlayer->pCtx->s32ChnNum          = 1;
  pstPlayer->pCtx->s32SampleRate      = AUDIO_SAMPLE_RATE;
  pstPlayer->pCtx->s32ReSmpSampleRate = 0;
  pstPlayer->pCtx->s32DeviceChannel   = AUDIO_DEVICE_CHANNEL;
  pstPlayer->pCtx->s32Channel         = 2;
  pstPlayer->pCtx->s32BitWidth        = AUDIO_BIT_WIDTH;
  pstPlayer->pCtx->s32PeriodCount     = 2;
  pstPlayer->pCtx->s32PeriodSize      = AUDIO_FRAME_COUNT;
  pstPlayer->pCtx->chCardName         = RK_NULL;
  pstPlayer->pCtx->chCardName         = AI_DEVICE_NAME;
  pstPlayer->pCtx->s32DevId           = 0;
  pstPlayer->pCtx->s32SetVolume       = 100;
  pstPlayer->pCtx->s32SetMute         = 0;
  pstPlayer->pCtx->s32SetTrackMode    = 0;
  pstPlayer->pCtx->s32SetFadeRate     = 0;
  pstPlayer->pCtx->s32GetVolume       = 0;
  pstPlayer->pCtx->s32GetMute         = 0;
  pstPlayer->pCtx->s32GetTrackMode    = 0;
  pstPlayer->pCtx->s32QueryChnStat    = 0;
  pstPlayer->pCtx->s32PauseResumeChn  = 0;
  pstPlayer->pCtx->s32SaveFile        = 0;
  pstPlayer->pCtx->s32QueryFileStat   = 0;
  pstPlayer->pCtx->s32ClrChnBuf       = 0;
  pstPlayer->pCtx->s32ClrPubAttr      = 0;
  pstPlayer->pCtx->s32GetPubAttr      = 0;

  RKADK_MPI_SYS_Init();
  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);
  *pPlayer = (RKADK_MW_PTR)pstPlayer;
  return 0;

failed:
  RKADK_LOGI("Create Player[%d, %d] failed...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  if (pstPlayer)
    free(pstPlayer);

  return -1;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);

  RKADK_S32 ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_LOGI("Destory Player Start...");
  player_push(pstPlayer->mAudioPlayer, pstPlayer->pAudioBuf, 0);

  ret = CloseDeviceAO(pstPlayer->pCtx);
  if (ret) {
    RKADK_LOGE("Ao destory failed(%d)", ret);
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    return ret;
  }

  ret = player_stop(pstPlayer->mAudioPlayer);
  if (ret) {
    RKADK_LOGE("Player stop failed(%d)", ret);
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    return ret;
  }

  if (pstPlayer->pAudioPlayerCfg) {
    free(pstPlayer->pAudioPlayerCfg);
    pstPlayer->pAudioPlayerCfg = NULL;
  }

  player_destroy(pstPlayer->mAudioPlayer);
  pstPlayer->mAudioPlayer = NULL;
  player_deinit();

  if (pstPlayer->pFin != NULL) {
    fclose(pstPlayer->pFin);
    pstPlayer->pFin = NULL;
  }

  if (pstPlayer->pAudioBuf) {
    free(pstPlayer->pAudioBuf);
    pstPlayer->pAudioBuf = NULL;
  }


  if (pstPlayer->pCtx) {
    free(pstPlayer->pCtx);
    pstPlayer->pCtx = RK_NULL;
  }

  RKADK_MPI_SYS_Exit();
  RKADK_LOGI("Destory Player End...");
  if (g_pfnPlayerCallback != NULL)
    g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);
  return 0;
}

RKADK_S32 RKADK_PLAYER_SetDataSource(RKADK_MW_PTR pPlayer,
                                     const RKADK_CHAR *pszfilePath) {
  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  player_init();
  player_register_mp3dec();
  player_list_decoder();

  stPlayerCfg.mode = AUDIO_CREATE_PULL | AUDIO_CREATE_PUSH;
  pstPlayer->mAudioPlayer = player_create(&stPlayerCfg);
  playback_set_volume(100);
  pstPlayer->pAudioPlayerCfg = NULL;
  pstPlayer->pAudioPlayerCfg = (play_cfg_t *)malloc(sizeof(play_cfg_t));
  pstPlayer->pAudioPlayerCfg->start_time = 0;
  pstPlayer->pAudioPlayerCfg->target = (RKADK_CHAR *)pszfilePath;

  if (pstPlayer->pAudioPlayerCfg->target != RKADK_NULL) {
    pstPlayer->pFin = fopen(pstPlayer->pAudioPlayerCfg->target, "r");

    if (pstPlayer->pFin == NULL) {
      RKADK_LOGE("open %s failed, file %s is NULL", pstPlayer->pAudioPlayerCfg->target, pstPlayer->pAudioPlayerCfg->target);
      if (g_pfnPlayerCallback != NULL)
        g_pfnPlayerCallback(pstPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
      return RKADK_FAILURE;
    }
    pstPlayer->pAudioBuf = (RKADK_CHAR *)malloc(AUDIO_BUF_SIZE);
    return RKADK_SUCCESS;
  }
  else {
    RKADK_LOGE("SetDataSource failed");
    return RKADK_FAILURE;
  }
}

RKADK_S32 RKADK_PLAYER_Prepare(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  pstPlayer->pAudioPlayerCfg->preprocessor = (play_preprocessor_t)DEFAULT_FILE_PREPROCESSOR;
  pstPlayer->pAudioPlayerCfg->freq_t = PLAY_FREQ_LOCALPLAY;
  pstPlayer->pAudioPlayerCfg->need_free = 1;
  pstPlayer->pAudioPlayerCfg->info_only = 0;
  if (g_pfnPlayerCallback != NULL)
    g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_PREPARED, NULL);

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_SetVideoSink(RKADK_MW_PTR pPlayer,
                                    RKADK_PLAYER_FRAMEINFO_S *pstFrameInfo) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_LOGE("SetVideoSink unsupport");
  return RKADK_FAILURE;
}

RKADK_VOID *EventEOF(RKADK_VOID *arg) {
  RKADK_S32 push_ret = 0;
  uint32_t len = 0;
  RKADK_PLAYER_HANDLE_S *pPlayer = (RKADK_PLAYER_HANDLE_S *)arg;

  while (pPlayer->pFin) {
      len = fread(pPlayer->pAudioBuf, 1, AUDIO_BUF_SIZE, pPlayer->pFin);

    if (len <= 0) {
      player_push(pPlayer->mAudioPlayer, pPlayer->pAudioBuf, 0);
      if (pPlayer->pFin != NULL) {
        fclose(pPlayer->pFin);
        pPlayer->pFin = NULL;
      }
      break;
    }
    push_ret = player_push(pPlayer->mAudioPlayer, pPlayer->pAudioBuf, len);

    if (push_ret <= 0)
      fseek(pPlayer->pFin, -push_ret, SEEK_SET);
    usleep(10);
  }

  if (pPlayer->stThreadParam.tid)
    pthread_join(pPlayer->stThreadParam.tid, NULL);

  if (pPlayer->stThreadParam.tidSend)
    pthread_join(pPlayer->stThreadParam.tidSend, RK_NULL);

  if (pPlayer->stThreadParam.tidReceive)
    pthread_join(pPlayer->stThreadParam.tidReceive, RK_NULL);

  DeInitMpiAO(pPlayer->pCtx->s32DevId, pPlayer->pCtx->s32ChnIndex);
  if (g_pfnPlayerCallback != NULL)
    g_pfnPlayerCallback((RKADK_VOID *)pPlayer, RKADK_PLAYER_EVENT_EOF, NULL);
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  if (g_pfnPlayerCallback != NULL)
    g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STARTED, NULL);

  player_play(pstPlayer->mAudioPlayer, pstPlayer->pAudioPlayerCfg);
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
  RKADK_S32 ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  player_push(pstPlayer->mAudioPlayer, pstPlayer->pAudioBuf, 0);
  ret = CloseDeviceAO(pstPlayer->pCtx);
  if (ret) {
    RKADK_LOGE("Ao destory failed(%d)", ret);
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    return ret;
  }

  ret = player_stop(pstPlayer->mAudioPlayer);
  if (ret) {
    if (g_pfnPlayerCallback != NULL)
      g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_ERROR, NULL);
    RKADK_LOGE("Player stop failed(%d)", ret);
    return ret;
  }

  if (pstPlayer->pAudioPlayerCfg) {
    free(pstPlayer->pAudioPlayerCfg);
    pstPlayer->pAudioPlayerCfg = NULL;
  }

  player_destroy(pstPlayer->mAudioPlayer);
  pstPlayer->mAudioPlayer = NULL;
  player_deinit();

  if (pstPlayer->pFin != NULL) {
    fclose(pstPlayer->pFin);
    pstPlayer->pFin = NULL;
  }

  if (pstPlayer->pAudioBuf) {
    free(pstPlayer->pAudioBuf);
    pstPlayer->pAudioBuf = NULL;
  }

  if (g_pfnPlayerCallback != NULL)
    g_pfnPlayerCallback(pPlayer, RKADK_PLAYER_EVENT_STOPPED, NULL);

  return ret;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetPlayStatus(RKADK_MW_PTR pPlayer,
                                     RKADK_PLAYER_STATE_E *penState) {
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_CHECK_POINTER(penState, RKADK_FAILURE);
  RKADK_U32 state;
  RKADK_PLAYER_STATE_E enState = RKADK_PLAYER_STATE_BUTT;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_LOGE("GetPlayStatus unsupport");
  *penState = enState;
  return 0;
}

RKADK_S32 RKADK_PLAYER_GetDuration(RKADK_MW_PTR pPlayer, RKADK_U32 *pDuration) {
  RKADK_CHECK_POINTER(pDuration, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  RKADK_S32 ret = 0;
  RKADK_S64 duration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  ret = RKADK_FAILURE;
  RKADK_LOGE("GetDuration unsupport");
  return ret;
}
