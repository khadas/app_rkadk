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

#include "rkadk_audio_decoder.h"
#include "rkadk_param.h"
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
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

typedef struct {
  FILE *pFin;
  RKADK_CHAR *pAudioBuf;
  player_handle_t audioPlayer;
  play_cfg_t *pAudioPlayerCfg;
  RKADK_CODEC_TYPE_E eCodecType;
} AUDIO_DECODER_PARAM_S;

RKADK_VOID AudioPlayerCallback(player_handle_t self, play_info_t info, RKADK_VOID *userdata) {
  printf("MP3 playback end\n");
}

RKADK_S32 RKADK_AUDIO_DECODER_Create(RKADK_S8 decoderMode, RKADK_CODEC_TYPE_E eCodecType,
                                     const RKADK_CHAR *pszfilePath, RKADK_MW_PTR *pAudioDecoder) {
  RKADK_CHECK_POINTER(pszfilePath, RKADK_FAILURE);

  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)malloc(sizeof(AUDIO_DECODER_PARAM_S));
  if (!pstAudioDecoder) {
    RKADK_LOGE("malloc pAudioDecoder falied");
    return RKADK_FAILURE;
  }

  memset(pstAudioDecoder, 0, sizeof(AUDIO_DECODER_PARAM_S));
  pstAudioDecoder->eCodecType = eCodecType;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    player_init();
    if (eCodecType == RKADK_CODEC_TYPE_MP3)
      player_register_mp3dec();

    //player_list_decoder();

    stPlayerCfg.mode = AUDIO_CREATE_PULL | AUDIO_CREATE_PUSH;
    pstAudioDecoder->audioPlayer = player_create(&stPlayerCfg);
    playback_set_volume(100);
    pstAudioDecoder->pAudioPlayerCfg = (play_cfg_t *)malloc(sizeof(play_cfg_t));
    if (!pstAudioDecoder->pAudioPlayerCfg) {
      RKADK_LOGE("malloc pAudioPlayerCfg falied");
      goto __FAILED;
    }

    memset(pstAudioDecoder->pAudioPlayerCfg, 0, sizeof(play_cfg_t));
    pstAudioDecoder->pAudioPlayerCfg->start_time = 0;
    pstAudioDecoder->pAudioPlayerCfg->target = (RKADK_CHAR *)pszfilePath;

    if (decoderMode == FILE_MODE) {
      pstAudioDecoder->pFin = fopen(pstAudioDecoder->pAudioPlayerCfg->target, "r");

      if (pstAudioDecoder->pFin == NULL) {
        RKADK_LOGE("open %s failed, file %s is NULL", pstAudioDecoder->pAudioPlayerCfg->target, pstAudioDecoder->pAudioPlayerCfg->target);
        goto __FAILED;
      }

      pstAudioDecoder->pAudioBuf = (RKADK_CHAR *)malloc(AUDIO_BUF_SIZE);
      if (!pstAudioDecoder->pAudioBuf) {
        RKADK_LOGE("malloc pAudioBuf falied");
        goto __FAILED;
      }
    }

    pstAudioDecoder->pAudioPlayerCfg->preprocessor = (play_preprocessor_t)DEFAULT_FILE_PREPROCESSOR;
    pstAudioDecoder->pAudioPlayerCfg->freq_t = PLAY_FREQ_LOCALPLAY;
    pstAudioDecoder->pAudioPlayerCfg->need_free = 1;
    pstAudioDecoder->pAudioPlayerCfg->info_only = 0;

    (*pAudioDecoder) = (RKADK_MW_PTR)pstAudioDecoder;

    return RKADK_SUCCESS;
  } else {
    RKADK_LOGE("Cannot register unsupported audio decoding format");
    return RKADK_FAILURE;
  }

__FAILED:
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    if (pstAudioDecoder->pAudioPlayerCfg) {
      free(pstAudioDecoder->pAudioPlayerCfg);
      pstAudioDecoder->pAudioPlayerCfg = NULL;
    }

    if (pstAudioDecoder) {
      free(pstAudioDecoder);
      pstAudioDecoder = NULL;
    }
  }

  return RKADK_FAILURE;
}

RKADK_S32 RKADK_AUDIO_DECODER_Destroy(RKADK_S8 decoderMode, RKADK_MW_PTR *pAudioDecoder) {
  RKADK_CHECK_POINTER(*pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)(*pAudioDecoder);
  RKADK_CHECK_POINTER(pstAudioDecoder->audioPlayer, RKADK_FAILURE);

  RKADK_AUDIO_DECODER_Stop(*pAudioDecoder);
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    if (pstAudioDecoder->pAudioPlayerCfg) {
      free(pstAudioDecoder->pAudioPlayerCfg);
      pstAudioDecoder->pAudioPlayerCfg = NULL;
    }

    player_destroy(pstAudioDecoder->audioPlayer);
    pstAudioDecoder->audioPlayer = NULL;
    player_deinit();

    if (decoderMode == FILE_MODE) {
      if (pstAudioDecoder->pFin != NULL) {
        fclose(pstAudioDecoder->pFin);
        pstAudioDecoder->pFin = NULL;
      }

      if (pstAudioDecoder->pAudioBuf) {
        free(pstAudioDecoder->pAudioBuf);
        pstAudioDecoder->pAudioBuf = NULL;
      }
    }

    if ((*pAudioDecoder) != NULL) {
      free(*pAudioDecoder);
      (*pAudioDecoder) = NULL;
    }
  } else {
    RKADK_LOGE("Cannot unregister unsupported audio decoding format");
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_AUDIO_DECODER_StreamPush(RKADK_MW_PTR pAudioDecoder, RKADK_CHAR *pPacketData, RKADK_S32 packetSize) {
  int ret = 0;

  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    if (packetSize <= 0) {
      player_push(pstAudioDecoder->audioPlayer, pPacketData, 0);
    } else {
      player_push(pstAudioDecoder->audioPlayer, pPacketData, packetSize);
    }
  } else {
    RKADK_LOGE("Cannot push unsupported audio decoding format data");
    return RKADK_FAILURE;
  }

  return ret;
}


RKADK_S32 RKADK_AUDIO_DECODER_FilePush(RKADK_MW_PTR pAudioDecoder, RKADK_BOOL bStopFlag) {
  RKADK_U32 len = 0;
  RKADK_S32 push_ret = 0;

  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  RKADK_CHECK_POINTER(pstAudioDecoder->pFin, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstAudioDecoder->pAudioBuf, RKADK_FAILURE);

  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    while (pstAudioDecoder->pFin && bStopFlag == RKADK_FALSE) {
      len = fread(pstAudioDecoder->pAudioBuf, 1, AUDIO_BUF_SIZE, pstAudioDecoder->pFin);
      if (len <= 0) {
        player_push(pstAudioDecoder->audioPlayer, pstAudioDecoder->pAudioBuf, 0);
        if (pstAudioDecoder->pFin != NULL) {
          fclose(pstAudioDecoder->pFin);
          pstAudioDecoder->pFin = NULL;
        }
        break;
      }

      push_ret = player_push(pstAudioDecoder->audioPlayer, pstAudioDecoder->pAudioBuf, len);
      if (push_ret <= 0)
        fseek(pstAudioDecoder->pFin, -push_ret, SEEK_SET);

      usleep(10);
    }
  } else {
    RKADK_LOGE("Cannot push unsupported audio decoding format data");
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_AUDIO_DECODER_GetData(RKADK_MW_PTR pAudioDecoder, RKADK_CHAR *buf, RKADK_U32 len) {
  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    return player_pull(pstAudioDecoder->audioPlayer, buf, len);
  } else {
    RKADK_LOGE("Cannot pull unsupported audio decoding format data");
    return RKADK_FAILURE;
  }
}

RKADK_S32 RKADK_AUDIO_DECODER_Start(RKADK_MW_PTR pAudioDecoder) {
  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    player_play(pstAudioDecoder->audioPlayer, pstAudioDecoder->pAudioPlayerCfg);
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_AUDIO_DECODER_Stop(RKADK_MW_PTR pAudioDecoder) {
  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    player_push(pstAudioDecoder->audioPlayer, pstAudioDecoder->pAudioBuf, 0);

    int ret = player_stop(pstAudioDecoder->audioPlayer);
    if (ret) {
      RKADK_LOGE("Player stop failed(%d)", ret);
      return ret;
    }
  } else {
    RKADK_LOGE("Cannot stop unsupported audio decoding format");
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}

RKADK_S32 RKADK_AUDIO_DECODER_GetInfo(RKADK_S8 decoderMode, RKADK_MW_PTR pAudioDecoder, AUDIO_DECODER_CTX_S *pCtx) {
  RKADK_CHECK_POINTER(pAudioDecoder, RKADK_FAILURE);
  AUDIO_DECODER_PARAM_S *pstAudioDecoder = (AUDIO_DECODER_PARAM_S *)pAudioDecoder;
  if (pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_MP3
      || pstAudioDecoder->eCodecType == RKADK_CODEC_TYPE_PCM) {
    struct audio_config config;
    if (player_audio_info(pstAudioDecoder->audioPlayer, &config, -1) != RKADK_SUCCESS) {
      return RKADK_FAILURE;
    }

    if (decoderMode == FILE_MODE) {
      pCtx->bitWidth = config.bits;
      pCtx->reSmpSampleRate = config.sample_rate;
      pCtx->channel = AUDIO_DEVICE_CHANNEL;
    }
  } else {
    RKADK_LOGE("Cannot get unsupported audio decoding format info");
    return RKADK_FAILURE;
  }

  return RKADK_SUCCESS;
}
