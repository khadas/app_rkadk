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
#include "rkadk_player.h"
#include "rk_debug.h"
#include "rk_defines.h"
#include "rk_debug.h"
#include "rk_mpi_ao.h"
#include "rk_mpi_mb.h"
#include "rk_mpi_sys.h"
#include "audio_server.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define USE_AO_MIXER 0
#define audiobufsize 2048

typedef struct {
  RKADK_BOOL bEnableVideo;
  void *pListener;
} RKADK_PLAYER_HANDLE_S;

/* The attributes of a aio device */
typedef struct rkAUDIO_DEV_ATTR_S {
  RKADK_U32 u32PeriodSize;
  RKADK_U32 u32PeriodCount;
  RKADK_U32 u32StartDelay;
  RKADK_U32 u32StopDelay;
} AUDIO_DEV_ATTR_S;

typedef struct _rkMpiAOCtx {
  const char *srcFilePath;
  const char *dstFilePath;
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
  char  *chCardName;
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
} TEST_AO_CTX_S;

extern int pcmout_open_impl(struct playback_device *self, playback_device_cfg_t *cfg);
extern int pcmout_start_impl(struct playback_device *self);
extern int pcmout_write_impl(struct playback_device *self, const char *data, size_t data_len);
extern int pcmout_stop_impl(struct playback_device *self);
extern int pcmout_abort_impl(struct playback_device *self);
extern void pcmout_close_impl(struct playback_device *self);

#define PCM_DEVICE { \
  .open = pcmout_open_impl, \
  .start = pcmout_start_impl, \
  .write = pcmout_write_impl, \
  .stop = pcmout_stop_impl, \
  .abort = pcmout_abort_impl, \
  .close = pcmout_close_impl, \
}

void PlayerCallbackTest(player_handle_t self, play_info_t info, void *userdata);

static int playback_end = 0;
static player_handle_t player_test = NULL;
static play_cfg_t *cfg_test = NULL;
static player_cfg_t player_cfg =
{
  .preprocess_buf_size = 1024 * 40,
  .decode_buf_size = 1024 * 20,
  .preprocess_stack_size = 2048,
  .decoder_stack_size = 1024 * 12,
  .playback_stack_size = 2048,
  .tag = "one",
  .device = PCM_DEVICE,
  .listen = PlayerCallbackTest,
};

int firstframe = 0;
TEST_AO_CTX_S *ctx;
pthread_t tid;
TEST_AO_CTX_S params[AO_MAX_CHN_NUM];
pthread_t tidSend[AO_MAX_CHN_NUM];
pthread_t tidReceive[AO_MAX_CHN_NUM];
char *audiobuf = NULL;
FILE *fin = NULL;

static RKADK_PLAYER_EVENT_FN g_pfnPlayerCallback = NULL;

void PlayerCallbackTest(player_handle_t self, play_info_t info, void *userdata)
{
  printf("Playback end\n");
  playback_end = 1;
}

void QueryAoFlowGraphStat(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 ret = 0;
  AO_CHN_STATE_S pstStat;
  memset(&pstStat, 0, sizeof(AO_CHN_STATE_S));
  ret = RK_MPI_AO_QueryChnStat(aoDevId, aoChn, &pstStat);
  if (ret == RK_SUCCESS) {
    RK_LOGI("query ao flow status:");
    RK_LOGI("total number of channel buffer : %d", pstStat.u32ChnTotalNum);
    RK_LOGI("free number of channel buffer : %d", pstStat.u32ChnFreeNum);
    RK_LOGI("busy number of channel buffer : %d", pstStat.u32ChnBusyNum);
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
      RK_LOGE("channel = %d not support", ch);
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
      RK_LOGE("bitwidth(%d) not support", bit);
      return AUDIO_BIT_WIDTH_BUTT;
  }

  return bitWidth;
}

RK_S32 TestOpenDeviceAo(TEST_AO_CTX_S *ctx) {
  AUDIO_DEV aoDevId = ctx->s32DevId;
  AUDIO_SOUND_MODE_E soundMode;

  AIO_ATTR_S aoAttr;
  memset(&aoAttr, 0, sizeof(AIO_ATTR_S));

  if (ctx->chCardName) {
    snprintf((char *)(aoAttr.u8CardName),
              sizeof(aoAttr.u8CardName), "%s", ctx->chCardName);
  }

  aoAttr.soundCard.channels = ctx->s32DeviceChannel;
  aoAttr.soundCard.sampleRate = ctx->s32SampleRate;
  aoAttr.soundCard.bitWidth = AUDIO_BIT_WIDTH_16;

  AUDIO_BIT_WIDTH_E bitWidth = FindBitWidth(ctx->s32BitWidth);
  if (bitWidth == AUDIO_BIT_WIDTH_BUTT) {
    goto __FAILED;
  }
  aoAttr.enBitwidth = bitWidth;
  aoAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)ctx->s32ReSmpSampleRate;
  soundMode = FindSoundMode(ctx->s32Channel);
  if (soundMode == AUDIO_SOUND_MODE_BUTT) {
    goto __FAILED;
  }
  aoAttr.enSoundmode = soundMode;
  aoAttr.u32FrmNum = ctx->s32PeriodCount;
  aoAttr.u32PtNumPerFrm = ctx->s32PeriodSize;

  aoAttr.u32EXFlag = 0;
  aoAttr.u32ChnCnt = 2;

  RK_MPI_AO_SetPubAttr(aoDevId, &aoAttr);

  RK_MPI_AO_Enable(aoDevId);
  ctx->s32OpenFlag = 1;
  return RK_SUCCESS;
__FAILED:
  return RK_FAILURE;
}

RK_S32 TestInitMpiAo(TEST_AO_CTX_S *params) {
  RK_S32 result;

  result = RK_MPI_AO_EnableChn(params->s32DevId, params->s32ChnIndex);
  if (result != 0) {
    RK_LOGE("ao enable channel fail, aoChn = %d, reason = %x", params->s32ChnIndex, result);
    return RK_FAILURE;
  }

  // set sample rate of input data
  result = RK_MPI_AO_EnableReSmp(params->s32DevId, params->s32ChnIndex,
                                (AUDIO_SAMPLE_RATE_E)params->s32ReSmpSampleRate);
  if (result != 0) {
    RK_LOGE("ao enable channel fail, reason = %x, aoChn = %d", result, params->s32ChnIndex);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

RK_S32 DeinitMpiAo(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 result = RK_MPI_AO_DisableReSmp(aoDevId, aoChn);
  if (result != 0) {
    RK_LOGE("ao disable resample fail, reason = %d", result);
    return RK_FAILURE;
  }

  result = RK_MPI_AO_DisableChn(aoDevId, aoChn);
  if (result != 0) {
    RK_LOGE("ao disable channel fail, reason = %d", result);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

RK_S32 TestCloseDeviceAo(TEST_AO_CTX_S *ctx) {
  AUDIO_DEV aoDevId = ctx->s32DevId;
  if (ctx->s32OpenFlag == 1) {
    RK_S32 result = RK_MPI_AO_Disable(aoDevId);
    if (result != 0) {
      RK_LOGE("ao disable fail, reason = %X", result);
      return RK_FAILURE;
    }
    ctx->s32OpenFlag = 0;
  }

  return RK_SUCCESS;
}

RK_S32 TestSetAoChannelMode(AUDIO_DEV aoDevId, AO_CHN aoChn) {
  RK_S32 result = 0;
  AO_CHN_PARAM_S pstParams;
  memset(&pstParams, 0, sizeof(AO_CHN_PARAM_S));
  // for test : aoChn0 output left channel,  aoChn1 output right channel,
  if (aoChn == 0) {
    pstParams.enMode = AUDIO_CHN_MODE_LEFT;
  } else if (aoChn == 1) {
    pstParams.enMode = AUDIO_CHN_MODE_RIGHT;
  }

  result = RK_MPI_AO_SetChnParams(aoDevId, aoChn, &pstParams);
  if (result != RK_SUCCESS) {
    RK_LOGE("ao set channel params, aoChn = %d", aoChn);
    return RK_FAILURE;
  }

  return RK_SUCCESS;
}

void* SendDataThread(void * ptr) {
  char *buf = (char *)malloc(4096);
  uint32_t len = 4096;
  TEST_AO_CTX_S *params = (TEST_AO_CTX_S *)(ptr);
  MB_POOL_CONFIG_S pool_config;
  // set default value for struct
  RK_U8 *srcData = RK_NULL;
  AUDIO_FRAME_S frame;
  RK_U64 timeStamp = 0;
  RK_S32 s32MilliSec = -1;
  RK_S32 size = 0;
  RK_S32 result = 0;
  FILE *file = RK_NULL;
  pthread_join(tid, NULL);
  RK_LOGI("params->s32ChnIndex : %d", params->s32ChnIndex);

  srcData = (RK_U8 *)(calloc(len, sizeof(RK_U8)));
  memset(srcData, 0, len);
  while (1) {
    size= player_pull(player_test, buf, len);
    srcData = (RK_U8 *)buf;

    frame.u32Len = size;
    frame.u64TimeStamp = timeStamp++;
    frame.enBitWidth = FindBitWidth(params->s32BitWidth);
    frame.enSoundMode = FindSoundMode(params->s32Channel);
    frame.bBypassMbBlk = RK_FALSE;

    MB_EXT_CONFIG_S extConfig;
    memset(&extConfig, 0, sizeof(extConfig));
    extConfig.pOpaque = srcData;
    extConfig.pu8VirAddr = srcData;
    extConfig.u64Size = size;
    RK_MPI_SYS_CreateMB(&(frame.pMbBlk), &extConfig);

    result = RK_MPI_AO_SendFrame(params->s32DevId, params->s32ChnIndex, &frame, s32MilliSec);
    if (result < 0) {
        RK_LOGE("send frame fail, result = %d, TimeStamp = %lld, s32MilliSec = %d",
            result, frame.u64TimeStamp, s32MilliSec);
    }
    RK_MPI_MB_ReleaseMB(frame.pMbBlk);

    if (size <= 0) {
        RK_LOGI("eof");
        break;
    }
    usleep(10);
  }

__EXIT:
  RK_MPI_AO_WaitEos(params->s32DevId, params->s32ChnIndex, s32MilliSec);

  free(srcData);
  return RK_NULL;
}

void* CommandThread(void * ptr) {
  pthread_join(tid, NULL);
  TEST_AO_CTX_S *params = (TEST_AO_CTX_S *)(ptr);

  {
    AUDIO_FADE_S aFade;
    aFade.bFade = RK_FALSE;
    aFade.enFadeOutRate = (AUDIO_FADE_RATE_E)params->s32SetFadeRate;
    aFade.enFadeInRate = (AUDIO_FADE_RATE_E)params->s32SetFadeRate;
    RK_BOOL mute = (params->s32SetMute == 0) ? RK_FALSE : RK_TRUE;
    RK_LOGI("test info : mute = %d, volume = %d", mute, params->s32SetVolume);
    RK_MPI_AO_SetMute(params->s32DevId, mute, &aFade);
    RK_MPI_AO_SetVolume(params->s32DevId, params->s32SetVolume);
  }

  if (params->s32SetTrackMode) {
    RK_LOGI("test info : set track mode = %d", params->s32SetTrackMode);
    RK_MPI_AO_SetTrackMode(params->s32DevId, (AUDIO_TRACK_MODE_E)params->s32SetTrackMode);
    params->s32SetTrackMode = 0;
  }

  if (params->s32GetVolume) {
    RK_S32 volume = 0;
    RK_MPI_AO_GetVolume(params->s32DevId, &volume);
    RK_LOGI("test info : get volume = %d", volume);
    params->s32GetVolume = 0;
  }

  if (params->s32GetMute) {
    RK_BOOL mute = RK_FALSE;
    AUDIO_FADE_S fade;
    RK_MPI_AO_GetMute(params->s32DevId, &mute, &fade);
    RK_LOGI("test info : is mute = %d", mute);
    params->s32GetMute = 0;
  }

  if (params->s32GetTrackMode) {
    AUDIO_TRACK_MODE_E trackMode;
    RK_MPI_AO_GetTrackMode(params->s32DevId, &trackMode);
    RK_LOGI("test info : get track mode = %d", trackMode);
    params->s32GetTrackMode = 0;
  }

  if (params->s32QueryChnStat) {
    QueryAoFlowGraphStat(params->s32DevId, params->s32ChnIndex);
    params->s32QueryChnStat = 0;
  }

  if (params->s32SaveFile) {
    AUDIO_SAVE_FILE_INFO_S saveFile;
    memset(&saveFile, 0, sizeof(AUDIO_SAVE_FILE_INFO_S));
    if (params->dstFilePath) {
        saveFile.bCfg = RK_TRUE;
        saveFile.u32FileSize = 1024 * 1024;
        snprintf(saveFile.aFileName, sizeof(saveFile.aFileName), "%s", "ao_save_file.bin");
        snprintf(saveFile.aFilePath, sizeof(saveFile.aFilePath), "%s", params->dstFilePath);
    }
    RK_MPI_AO_SaveFile(params->s32DevId, params->s32ChnIndex, &saveFile);
    params->s32SaveFile = 0;
  }

  if (params->s32QueryFileStat) {
    AUDIO_FILE_STATUS_S fileStat;
    RK_MPI_AO_QueryFileStatus(params->s32DevId, params->s32ChnIndex, &fileStat);
    RK_LOGI("test info : query save file status = %d", fileStat.bSaving);
    params->s32QueryFileStat = 0;
  }

  if (params->s32PauseResumeChn) {
    usleep(500 * 1000);
    RK_MPI_AO_PauseChn(params->s32DevId, params->s32ChnIndex);
    RK_LOGI("pause test");
    usleep(1000 * 1000);
    RK_MPI_AO_ResumeChn(params->s32DevId, params->s32ChnIndex);
    RK_LOGI("resume test");
    params->s32PauseResumeChn = 0;
  }

  if (params->s32ClrChnBuf) {
    RK_MPI_AO_ClearChnBuf(params->s32DevId, params->s32ChnIndex);
    params->s32ClrChnBuf = 0;
  }

  if (params->s32ClrPubAttr) {
    RK_MPI_AO_ClrPubAttr(params->s32DevId);
    params->s32ClrPubAttr = 0;
  }

  if (params->s32GetPubAttr) {
    AIO_ATTR_S pstAttr;
    RK_MPI_AO_GetPubAttr(params->s32DevId, &pstAttr);
    RK_LOGI("input stream rate = %d", pstAttr.enSamplerate);
    RK_LOGI("input stream sound mode = %d", pstAttr.enSoundmode);
    RK_LOGI("open sound card rate = %d", pstAttr.soundCard.sampleRate);
    RK_LOGI("open sound card channel = %d", pstAttr.soundCard.channels);
    params->s32GetPubAttr = 0;
  }

  return RK_NULL;
}

void *DoPull(void *arg)
{
  struct audio_config config;

  player_audio_info(player_test, &config, -1);
  ctx->s32ReSmpSampleRate = config.sample_rate;
  ctx->s32Channel = config.channels;
  ctx->s32BitWidth = config.bits;

  if (ctx->s32Channel <= 0
  || ctx->s32ReSmpSampleRate <= 0)
  {
    printf("AO create failed\n");
    return NULL;
  }

  if(firstframe == 0)
  {
    firstframe++;
    int *ReSmpSampleRate = 0;

    if (TestOpenDeviceAo(ctx) != RK_SUCCESS) {
      return NULL;
    }

    for (int i = 0; i < ctx->s32ChnNum; i++) {
    memcpy(&(params[i]), ctx, sizeof(TEST_AO_CTX_S));
    params[i].s32ChnIndex = i;

    if (USE_AO_MIXER) {
      TestSetAoChannelMode(params[i].s32DevId, params[i].s32ChnIndex);
    }

    TestInitMpiAo(&params[i]);

    pthread_create(&tidSend[i], RK_NULL, SendDataThread, (void *)(&params[i]));
    pthread_create(&tidReceive[i], RK_NULL, CommandThread, (void *)(&params[i]));
    }
  }

  return NULL;
}

RKADK_S32 RKADK_PLAYER_Create(RKADK_MW_PTR *ppPlayer,
                              RKADK_PLAYER_CFG_S *pstPlayCfg) {
  int ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pstPlayCfg, RKADK_FAILURE);
  g_pfnPlayerCallback = pstPlayCfg->pfnPlayerCallback;

  if (*ppPlayer) {
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

  *ppPlayer = (RKADK_MW_PTR)pstPlayer;

  player_init();
  player_register_mp3dec();
  player_list_decoder();

  player_cfg.mode = AUDIO_CREATE_PULL | AUDIO_CREATE_PUSH;
  player_test = player_create(&player_cfg);
  playback_set_volume(100);

  ctx = (TEST_AO_CTX_S *)(malloc(sizeof(TEST_AO_CTX_S)));
  memset(ctx, 0, sizeof(TEST_AO_CTX_S));

  ctx->srcFilePath        = RK_NULL;
  ctx->dstFilePath        = RK_NULL;
  ctx->s32LoopCount       = 1;
  ctx->s32ChnNum          = 1;
  ctx->s32SampleRate      = AUDIO_SAMPLE_RATE;
  ctx->s32ReSmpSampleRate = 0;
  ctx->s32DeviceChannel   = AUDIO_CHANNEL;
  ctx->s32Channel         = 2;
  ctx->s32BitWidth        = AUDIO_BIT_WIDTH;
  ctx->s32PeriodCount     = 4;
  ctx->s32PeriodSize      = AUDIO_FRAME_COUNT;
  ctx->chCardName         = RK_NULL;
  ctx->chCardName         = AI_DEVICE_NAME;
  ctx->s32DevId           = 0;
  ctx->s32SetVolume       = 100;
  ctx->s32SetMute         = 0;
  ctx->s32SetTrackMode    = 0;
  ctx->s32SetFadeRate     = 0;
  ctx->s32GetVolume       = 0;
  ctx->s32GetMute         = 0;
  ctx->s32GetTrackMode    = 0;
  ctx->s32QueryChnStat    = 0;
  ctx->s32PauseResumeChn  = 0;
  ctx->s32SaveFile        = 0;
  ctx->s32QueryFileStat   = 0;
  ctx->s32ClrChnBuf       = 0;
  ctx->s32ClrPubAttr      = 0;
  ctx->s32GetPubAttr      = 0;

  RK_MPI_SYS_Init();

  RKADK_LOGI("Create Player[%d, %d] End...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);
  return 0;

failed:
  RKADK_LOGI("Create Player[%d, %d] failed...", pstPlayCfg->bEnableVideo,
             pstPlayCfg->bEnableAudio);

  if (pstPlayer)
    free(pstPlayer);

  return -1;
}

RKADK_S32 RKADK_PLAYER_Destroy(RKADK_MW_PTR pPlayer) {
  int ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  RKADK_LOGI("Destory Player Start...");
  player_push(player_test, audiobuf, 0);
  if(fin != NULL) {
    fclose(fin);
    fin = NULL;
  }

  ret = TestCloseDeviceAo(ctx);
  if (ret) {
    RKADK_LOGE("Ao destory failed(%d)", ret);
    return ret;
  }
  ret = player_stop(player_test);
  if (audiobuf) {
    free(audiobuf);
    audiobuf = NULL;
  }
  if (cfg_test) {
    free(cfg_test);
    cfg_test = NULL;
  }
  player_destroy(player_test);
  player_test = NULL;
  player_deinit();

  if (ctx) {
    free(ctx);
    ctx = RK_NULL;
  }

  if (ret) {
    RKADK_LOGE("Player destory failed(%d)", ret);
    return ret;
  }
  RK_MPI_SYS_Exit();

  RKADK_LOGI("Destory Player End...");
  return 0;
}

RKADK_S32 RKADK_PLAYER_SetDataSource(RKADK_MW_PTR pPlayer,
                                     const RKADK_CHAR *pszfilePath) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  cfg_test = malloc(sizeof(play_cfg_t));
  cfg_test->start_time = 0;

  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);

  cfg_test->target = (char *)pszfilePath;

  if(cfg_test->target != RKADK_NULL)
    return RKADK_SUCCESS;
  else {
    RKADK_LOGE("SetDataSource failed");
    return RKADK_FAILURE;
  }
}

RKADK_S32 RKADK_PLAYER_Prepare(RKADK_MW_PTR pPlayer) {
  cfg_test->preprocessor = (play_preprocessor_t)DEFAULT_FILE_PREPROCESSOR;
  cfg_test->freq_t = PLAY_FREQ_LOCALPLAY;
  cfg_test->need_free = 1;
  cfg_test->info_only = 0;
  playback_end = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_SetVideoSink(RKADK_MW_PTR pPlayer,
                                    RKADK_PLAYER_FRAMEINFO_S *pstFrameInfo) {

  RKADK_LOGE("SetVideoSink unsupport");
  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Play(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;
  int push_ret = 0;
  uint32_t len = 0;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  if (ctx->s32ChnNum > AO_MAX_CHN_NUM) {
    RKADK_LOGE("ao chn(%d) > max_chn(%d)", ctx->s32ChnNum, AO_MAX_CHN_NUM);
    return RKADK_FAILURE;
  }
  fin = fopen(cfg_test->target, "r");

  if(fin == NULL) {
    RKADK_LOGE("open %s failed, file %s is NULL", cfg_test->target, cfg_test->target);
    return RKADK_FAILURE;
  }
  audiobuf = (char *)malloc(audiobufsize);
  player_play(player_test, cfg_test);
  pthread_create(&tid, 0, DoPull, NULL);

  while (fin) {
    len = fread(audiobuf, 1, audiobufsize, fin);
    if (len <= 0) {
      player_push(player_test, audiobuf, 0);
      if(fin != NULL) {
        fclose(fin);
        fin = NULL;
      }
      break;
    }
    push_ret = player_push(player_test, audiobuf, len);

    if (push_ret <= 0)
      fseek(fin, -push_ret, SEEK_SET);
    usleep(10);
  }

  pthread_join(tid, NULL);

  for (int i = 0; i < ctx->s32ChnNum; i++) {
    pthread_join(tidSend[i], RK_NULL);
    pthread_join(tidReceive[i], RK_NULL);
    DeinitMpiAo(params[i].s32DevId, params[i].s32ChnIndex);
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_PLAYER_Stop(RKADK_MW_PTR pPlayer) {
  int ret;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  player_push(player_test, audiobuf, 0);
  if(fin != NULL) {
    fclose(fin);
    fin = NULL;
  }

  ret = TestCloseDeviceAo(ctx);
  firstframe = 0;
  ret = player_stop(player_test);
  if (audiobuf) {
    free(audiobuf);
    audiobuf = NULL;
  }
  if (cfg_test) {
    free(cfg_test);
    cfg_test = NULL;
  }

  if (ret) {
    RKADK_LOGE("Player stop failed(%d)", ret);
    return ret;
  }

  return ret;
}

RKADK_S32 RKADK_PLAYER_Pause(RKADK_MW_PTR pPlayer) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_Seek(RKADK_MW_PTR pPlayer, RKADK_S64 s64TimeInMs) {
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_PLAYER_GetPlayStatus(RKADK_MW_PTR pPlayer,
                                     RKADK_PLAYER_STATE_E *penState) {
  RKADK_U32 state;
  RKADK_PLAYER_STATE_E enState = RKADK_PLAYER_STATE_BUTT;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;
  RKADK_CHECK_POINTER(penState, RKADK_FAILURE);

  RKADK_LOGE("GetPlayStatus unsupport");
  *penState = enState;
  return 0;
}

RKADK_S32 RKADK_PLAYER_GetDuration(RKADK_MW_PTR pPlayer, RKADK_U32 *pDuration) {
  int ret = 0;
  RKADK_S64 duration = 0;
  RKADK_PLAYER_HANDLE_S *pstPlayer = NULL;

  RKADK_CHECK_POINTER(pDuration, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pPlayer, RKADK_FAILURE);
  pstPlayer = (RKADK_PLAYER_HANDLE_S *)pPlayer;

  ret = RKADK_FAILURE;

  RKADK_LOGE("GetDuration unsupport");
  return ret;
}
