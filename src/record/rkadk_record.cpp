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

#include "rkadk_record.h"
#include "rkadk_log.h"
#include "rkadk_media_comm.h"
#include "rkadk_param.h"
#include "rkadk_audio_mp3.h"
#include <deque>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static RKADK_REC_REQUEST_FILE_NAMES_FN
    g_pfnRequestFileNames[RKADK_MAX_SENSOR_CNT] = {NULL};
static std::deque<char *>
    g_fileNameDeque[RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT];
static pthread_mutex_t g_fileNameMutexLock = PTHREAD_MUTEX_INITIALIZER;

static int GetRecordFileName(RKADK_VOID *pHandle, RKADK_CHAR *pcFileName,
                             RKADK_U32 muxerId) {
  int index, ret;
  RKADK_MUXER_HANDLE_S *pstRecorder;

  RKADK_MUTEX_LOCK(g_fileNameMutexLock);

  if (muxerId >= RKADK_MUXER_STREAM_MAX_CNT * RKADK_MAX_SENSOR_CNT) {
    RKADK_LOGE("Incorrect file index: %d", muxerId);
    RKADK_MUTEX_UNLOCK(g_fileNameMutexLock);
    return -1;
  }

  pstRecorder = (RKADK_MUXER_HANDLE_S *)pHandle;
  if (!pstRecorder) {
    RKADK_LOGE("pstRecorder is null");
    RKADK_MUTEX_UNLOCK(g_fileNameMutexLock);
    return -1;
  }

  if (!g_pfnRequestFileNames[pstRecorder->u32CamId]) {
    RKADK_LOGE("Not Registered request name callback");
    RKADK_MUTEX_UNLOCK(g_fileNameMutexLock);
    return -1;
  }

  if (g_fileNameDeque[muxerId].empty()) {
    ARRAY_FILE_NAME fileName;
    fileName = (ARRAY_FILE_NAME)malloc(pstRecorder->u32StreamCnt *
                                       RKADK_MAX_FILE_PATH_LEN);
    ret = g_pfnRequestFileNames[pstRecorder->u32CamId](
        pHandle, pstRecorder->u32StreamCnt, fileName);
    if (ret) {
      RKADK_LOGE("get file name failed(%d)", ret);
      RKADK_MUTEX_UNLOCK(g_fileNameMutexLock);
      return -1;
    }

    char *newName[pstRecorder->u32StreamCnt];
    for (int i = 0; i < (int)pstRecorder->u32StreamCnt; i++) {
      newName[i] = strdup(fileName[i]);
      index = i + (RKADK_MUXER_STREAM_MAX_CNT * pstRecorder->u32CamId);
      g_fileNameDeque[index].push_back(newName[i]);
    }
    free(fileName);
  }

  auto front = g_fileNameDeque[muxerId].front();
  memcpy(pcFileName, front, strlen(front));
  g_fileNameDeque[muxerId].pop_front();
  free(front);

  RKADK_MUTEX_UNLOCK(g_fileNameMutexLock);
  return 0;
}

static void RKADK_RECORD_SetVideoChn(int index, RKADK_U32 u32CamId,
                                     RKADK_PARAM_REC_CFG_S *pstRecCfg,
                                     MPP_CHN_S *pstViChn, MPP_CHN_S *pstVencChn,
                                     MPP_CHN_S *pstRgaChn) {
  pstViChn->enModId = RK_ID_VI;
  pstViChn->s32DevId = u32CamId;
  pstViChn->s32ChnId = pstRecCfg->vi_attr[index].u32ViChn;

#ifdef RKADK_ENABLE_RGA
  pstRgaChn->enModId = RK_ID_RGA;
  pstRgaChn->s32DevId = u32CamId;
  pstRgaChn->s32ChnId = pstRecCfg->attribute[index].rga_chn;
#endif

  pstVencChn->enModId = RK_ID_VENC;
  pstVencChn->s32DevId = u32CamId;
  pstVencChn->s32ChnId = pstRecCfg->attribute[index].venc_chn;
}

static void RKADK_RECORD_SetAudioChn(MPP_CHN_S *pstAiChn,
                                     MPP_CHN_S *pstAencChn) {
  pstAiChn->enModId = RK_ID_AI;
  pstAiChn->s32DevId = 0;
  pstAiChn->s32ChnId = RECORD_AI_CHN;

  pstAencChn->enModId = RK_ID_AENC;
  pstAencChn->s32DevId = 0;
  pstAencChn->s32ChnId = RECORD_AENC_CHN;
}

static int RKADK_RECORD_SetVideoAttr(int index, RKADK_U32 u32CamId,
                                     RKADK_PARAM_REC_CFG_S *pstRecCfg,
                                     VENC_CHN_ATTR_S *pstVencAttr) {
  int ret;
  RKADK_U32 u32Gop;
  RKADK_U32 u32DstFrameRateNum = 0;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  RKADK_U32 bitrate;

  RKADK_CHECK_POINTER(pstVencAttr, RKADK_FAILURE);

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  memset(pstVencAttr, 0, sizeof(VENC_CHN_ATTR_S));

  if (pstRecCfg->record_type == RKADK_REC_TYPE_LAPSE) {
    bitrate = pstRecCfg->attribute[index].bitrate / pstRecCfg->lapse_multiple;
    u32DstFrameRateNum = pstSensorCfg->framerate / pstRecCfg->lapse_multiple;
    if (u32DstFrameRateNum < 1)
      u32DstFrameRateNum = 1;
    else if (u32DstFrameRateNum > pstSensorCfg->framerate)
      u32DstFrameRateNum = pstSensorCfg->framerate;
  } else {
    bitrate = pstRecCfg->attribute[index].bitrate;
    u32DstFrameRateNum = pstSensorCfg->framerate;
  }
  u32Gop = pstRecCfg->attribute[index].gop;

  pstVencAttr->stRcAttr.enRcMode =
      RKADK_PARAM_GetRcMode(pstRecCfg->attribute[index].rc_mode,
                            pstRecCfg->attribute[index].codec_type);

  ret = RKADK_MEDIA_SetRcAttr(&pstVencAttr->stRcAttr, u32Gop, bitrate,
                              pstSensorCfg->framerate, u32DstFrameRateNum);
  if (ret) {
    RKADK_LOGE("RKADK_MEDIA_SetRcAttr failed");
    return -1;
  }

  pstVencAttr->stVencAttr.enType =
      RKADK_MEDIA_GetRkCodecType(pstRecCfg->attribute[index].codec_type);
  pstVencAttr->stVencAttr.enPixelFormat =
      pstRecCfg->vi_attr[index].stChnAttr.enPixelFormat;
  pstVencAttr->stVencAttr.u32PicWidth = pstRecCfg->attribute[index].width;
  pstVencAttr->stVencAttr.u32PicHeight = pstRecCfg->attribute[index].height;
  pstVencAttr->stVencAttr.u32VirWidth = pstRecCfg->attribute[index].width;
  pstVencAttr->stVencAttr.u32VirHeight = pstRecCfg->attribute[index].height;
  pstVencAttr->stVencAttr.u32Profile = pstRecCfg->attribute[index].profile;
  pstVencAttr->stVencAttr.u32StreamBufCnt = 3; // 5
  pstVencAttr->stVencAttr.u32BufSize = pstRecCfg->attribute[index].width *
                                       pstRecCfg->attribute[index].height * 3 /
                                       2;

  return 0;
}

#ifdef RKADK_ENABLE_RGA
static bool RKADK_RECORD_IsUseRga(int index, RKADK_PARAM_REC_CFG_S *pstRecCfg) {
  RKADK_U32 u32SrcWidth = pstRecCfg->vi_attr[index].stChnAttr.u32Width;
  RKADK_U32 u32SrcHeight = pstRecCfg->vi_attr[index].stChnAttr.u32Height;
  RKADK_U32 u32DstWidth = pstRecCfg->attribute[index].width;
  RKADK_U32 u32DstHeight = pstRecCfg->attribute[index].height;

  if (u32DstWidth == u32SrcWidth && u32DstHeight == u32SrcHeight) {
    return false;
  } else {
    RKADK_LOGD("In[%d, %d], Out[%d, %d]", u32SrcWidth, u32SrcHeight,
               u32DstWidth, u32DstHeight);
    return true;
  }
}
#endif

static int RKADK_RECORD_CreateVideoChn(RKADK_U32 u32CamId) {
  int ret;
  VENC_CHN_ATTR_S stVencChnAttr;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

#ifdef RKADK_ENABLE_RGA
  bool bUseRga = false;
  RGA_ATTR_S stRgaAttr;
#endif

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  for (int i = 0; i < (int)pstRecCfg->file_num; i++) {
    ret = RKADK_RECORD_SetVideoAttr(i, u32CamId, pstRecCfg, &stVencChnAttr);
    if (ret) {
      RKADK_LOGE("RKADK_RECORD_SetVideoAttr(%d) failed", i);
      return ret;
    }

    // Create VI
    ret = RKADK_MPI_VI_Init(u32CamId, pstRecCfg->vi_attr[i].u32ViChn,
                            &(pstRecCfg->vi_attr[i].stChnAttr));
    if (ret) {
      RKADK_LOGE("RKADK_MPI_VI_Init faile, ret = %d", ret);
      return ret;
    }

#ifdef RKADK_ENABLE_RGA
    // Create RGA
    bUseRga = RKADK_RECORD_IsUseRga(i, pstRecCfg);
    if (bUseRga) {
      memset(&stRgaAttr, 0, sizeof(stRgaAttr));
      stRgaAttr.bEnBufPool = RK_TRUE;
      stRgaAttr.u16BufPoolCnt = 3;
      stRgaAttr.stImgIn.imgType = pstRecCfg->vi_attr[i].stChnAttr.enPixFmt;
      stRgaAttr.stImgIn.u32Width = pstRecCfg->vi_attr[i].stChnAttr.u32Width;
      stRgaAttr.stImgIn.u32Height = pstRecCfg->vi_attr[i].stChnAttr.u32Height;
      stRgaAttr.stImgIn.u32HorStride = pstRecCfg->vi_attr[i].stChnAttr.u32Width;
      stRgaAttr.stImgIn.u32VirStride =
          pstRecCfg->vi_attr[i].stChnAttr.u32Height;
      stRgaAttr.stImgOut.imgType = stRgaAttr.stImgIn.imgType;
      stRgaAttr.stImgOut.u32Width = pstRecCfg->attribute[i].width;
      stRgaAttr.stImgOut.u32Height = pstRecCfg->attribute[i].height;
      stRgaAttr.stImgOut.u32HorStride = pstRecCfg->attribute[i].width;
      stRgaAttr.stImgOut.u32VirStride = pstRecCfg->attribute[i].height;
      ret = RKADK_MPI_RGA_Init(pstRecCfg->attribute[i].rga_chn, &stRgaAttr);
      if (ret) {
        RKADK_LOGE("Init Rga[%d] falied[%d]", pstRecCfg->attribute[i].rga_chn,
                   ret);
        RKADK_MPI_VI_DeInit(u32CamId, pstRecCfg->vi_attr[i].u32ViChn);
        return ret;
      }
    }
#endif

    // Create VENC
    ret = RKADK_MPI_VENC_Init(pstRecCfg->attribute[i].venc_chn, &stVencChnAttr);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_VENC_Init failed(%d)", ret);

#ifdef RKADK_ENABLE_RGA
      if (bUseRga)
        RKADK_MPI_RGA_DeInit(pstRecCfg->attribute[i].rga_chn);
#endif

      RKADK_MPI_VI_DeInit(u32CamId, pstRecCfg->vi_attr[i].u32ViChn);
      return ret;
    }
  }

  return 0;
}

static int RKADK_RECORD_DestoryVideoChn(RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

#ifdef RKADK_ENABLE_RGA
  bool bUseRga = false;
#endif

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  for (int i = 0; i < (int)pstRecCfg->file_num; i++) {
    // Destroy VENC
    ret = RKADK_MPI_VENC_DeInit(pstRecCfg->attribute[i].venc_chn);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_VENC_DeInit failed[%x]", ret);
      return ret;
    }

#ifdef RKADK_ENABLE_RGA
    bUseRga = RKADK_RECORD_IsUseRga(i, pstRecCfg);
    if (bUseRga) {
      ret = RKADK_MPI_RGA_DeInit(pstRecCfg->attribute[i].rga_chn);
      if (ret) {
        RKADK_LOGE("RKADK_MPI_RGA_DeInit failed[%x]", ret);
        return ret;
      }
    }
#endif

    // Destroy VI
    ret = RKADK_MPI_VI_DeInit(u32CamId, pstRecCfg->vi_attr[i].u32ViChn);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_VI_DeInit failed[%x]", ret);
      return ret;
    }
  }

  return 0;
}

static int RKADK_RECORD_CreateAudioChn() {
  int ret;
  AUDIO_SOUND_MODE_E soundMode;
  AIO_ATTR_S stAiAttr;
  AENC_CHN_ATTR_S stAencAttr;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = NULL;

  pstAudioParam = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioParam) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return -1;
  }

  if (pstAudioParam->codec_type == RKADK_CODEC_TYPE_MP3){
    ret = RegisterAencMp3();
    if (ret) {
      RKADK_LOGE("Register Mp3 encoder failed(%d)", ret);
      return ret;
    }
  }

  // Create AI
  memset(&stAiAttr, 0, sizeof(AIO_ATTR_S));
  memcpy(stAiAttr.u8CardName, pstAudioParam->audio_node,
         strlen(pstAudioParam->audio_node));
  stAiAttr.soundCard.channels = AUDIO_DEVICE_CHANNEL;
  stAiAttr.soundCard.sampleRate = pstAudioParam->samplerate;
  stAiAttr.soundCard.bitWidth = pstAudioParam->bit_width;

  stAiAttr.enBitwidth = pstAudioParam->bit_width;
  stAiAttr.enSamplerate = (AUDIO_SAMPLE_RATE_E)pstAudioParam->samplerate;
  soundMode = RKADK_AI_GetSoundMode(pstAudioParam->channels);
  if (soundMode == AUDIO_SOUND_MODE_BUTT)
    return -1;

  stAiAttr.enSoundmode = soundMode;
  stAiAttr.u32FrmNum = 2;
  stAiAttr.u32PtNumPerFrm = pstAudioParam->samples_per_frame;
  stAiAttr.u32EXFlag = 0;
  stAiAttr.u32ChnCnt = pstAudioParam->channels;
  ret = RKADK_MPI_AI_Init(0, RECORD_AI_CHN, &stAiAttr, pstAudioParam->vqe_mode,
                          pstAudioParam->mic_type);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_AI_Init failed(%d)", ret);
    return ret;
  }

  // Create AENC
  memset(&stAencAttr, 0, sizeof(AENC_CHN_ATTR_S));
  stAencAttr.enType = RKADK_MEDIA_GetRkCodecType(pstAudioParam->codec_type);
  stAencAttr.u32BufCount = 4;
  stAencAttr.stCodecAttr.enType = stAencAttr.enType;
  stAencAttr.stCodecAttr.u32Channels = pstAudioParam->channels;
  stAencAttr.stCodecAttr.u32SampleRate = pstAudioParam->samplerate;
  stAencAttr.stCodecAttr.enBitwidth = pstAudioParam->bit_width;
  stAencAttr.stCodecAttr.pstResv = RK_NULL;

  if (pstAudioParam->codec_type == RKADK_CODEC_TYPE_MP3){
    stAencAttr.stCodecAttr.u32Resv[0] = pstAudioParam->samples_per_frame;
    stAencAttr.stCodecAttr.u32Resv[1] = pstAudioParam->bitrate;
  }

  ret = RKADK_MPI_AENC_Init(RECORD_AENC_CHN, &stAencAttr);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_AENC_Init failed(%d)", ret);
    RKADK_MPI_AI_DeInit(0, RECORD_AI_CHN, pstAudioParam->vqe_mode);
    return -1;
  }

  return 0;
}

static int RKADK_RECORD_DestoryAudioChn() {
  int ret;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = NULL;

  pstAudioParam = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioParam) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return -1;
  }

  ret = RKADK_MPI_AENC_DeInit(RECORD_AENC_CHN);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_AENC_DeInit failed(%d)", ret);
    return ret;
  }

  ret = RKADK_MPI_AI_DeInit(0, RECORD_AI_CHN, pstAudioParam->vqe_mode);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_AI_DeInit failed(%d)", ret);
    return ret;
  }

  if (pstAudioParam->codec_type == RKADK_CODEC_TYPE_MP3){
    ret = UnRegisterAencMp3();
    if (ret) {
      RKADK_LOGE("UnRegister Mp3 encoder failed(%d)", ret);
      return ret;
    }
  }
  return 0;
}

static int64_t fakeTime = 0;
static void RKADK_RECORD_AencOutCb(AUDIO_STREAM_S stFrame,
                                   RKADK_VOID *pHandle) {
  // current rockit audio timestamp inaccurate, use fake time
  //fakeTime += 62560;

  RKADK_MUXER_WriteAudioFrame(
      (RKADK_CHAR *)RK_MPI_MB_Handle2VirAddr(stFrame.pMbBlk), stFrame.u32Len,
    /* fakeTime  */ stFrame.u64TimeStamp, pHandle);
}

static void RKADK_RECORD_VencOutCb(RKADK_MEDIA_VENC_DATA_S stData,
                                   RKADK_VOID *pHandle) {
  RKADK_CHECK_POINTER_N(pHandle);
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;
  RKADK_U64 u64LapsePts;

  RKADK_MUXER_HANDLE_S *pstMuxer =
    (RKADK_MUXER_HANDLE_S *)pHandle;

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(pstMuxer->u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return;
  }

  pstRecCfg = RKADK_PARAM_GetRecCfg(pstMuxer->u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return;
  }

  RKADK_CHAR *data =
      (RKADK_CHAR *)RK_MPI_MB_Handle2VirAddr(stData.stFrame.pstPack->pMbBlk);

  if ((stData.stFrame.pstPack->DataType.enH264EType == H264E_NALU_ISLICE ||
      stData.stFrame.pstPack->DataType.enH264EType == H264E_NALU_IDRSLICE) ||
      (stData.stFrame.pstPack->DataType.enH265EType == H265E_NALU_ISLICE ||
      stData.stFrame.pstPack->DataType.enH265EType == H265E_NALU_IDRSLICE)) {
    //RKADK_LOGD("write I frame);
    if (pstRecCfg->record_type == RKADK_REC_TYPE_LAPSE) {
      RKADK_U64 u64LapsePts =
        stData.stFrame.pstPack->u64PTS / pstSensorCfg->framerate;
      RKADK_MUXER_WriteVideoFrame(stData.u32ChnId, data,
                                stData.stFrame.pstPack->u32Len,
                                u64LapsePts, 1, pHandle);
    } else {
      RKADK_MUXER_WriteVideoFrame(stData.u32ChnId, data,
                                stData.stFrame.pstPack->u32Len,
                                stData.stFrame.pstPack->u64PTS, 1, pHandle);
    }
  } else {
    // RKADK_LOGD("write P frame");
    if (pstRecCfg->record_type == RKADK_REC_TYPE_LAPSE) {
      RKADK_U64 u64LapsePts =
        stData.stFrame.pstPack->u64PTS / pstSensorCfg->framerate;
      RKADK_MUXER_WriteVideoFrame(stData.u32ChnId, data,
                                stData.stFrame.pstPack->u32Len,
                                u64LapsePts, 0, pHandle);
    } else {
      RKADK_MUXER_WriteVideoFrame(stData.u32ChnId, data,
                                  stData.stFrame.pstPack->u32Len,
                                  stData.stFrame.pstPack->u64PTS, 0, pHandle);
    }
  }
}

static RKADK_S32 RKADK_RECORD_VencGetData(RKADK_U32 u32CamId,
                                          MPP_CHN_S *pstVencChn,
                                          RKADK_MW_PTR pRecorder) {
  int ret = 0;

  VENC_RECV_PIC_PARAM_S stRecvParam;
  stRecvParam.s32RecvPicNum = -1;
  ret = RK_MPI_VENC_StartRecvFrame(pstVencChn->s32ChnId, &stRecvParam);
  if (ret) {
    RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed[%x]", ret);
    return ret;
  }

  ret =
      RKADK_MEDIA_GetVencBuffer(pstVencChn, RKADK_RECORD_VencOutCb, pRecorder);
  if (ret) {
    RKADK_LOGE("RKADK_MEDIA_GetVencBuffer failed[%x]", ret);
    return ret;
  }

  return ret;
}

static int RKADK_RECORD_BindChn(RKADK_U32 u32CamId, RKADK_MW_PTR pRecorder) {
  int ret;
  MPP_CHN_S stSrcChn, stDestChn, stRgaChn;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

#ifdef RKADK_ENABLE_RGA
  bool bUseRga;
#endif

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  if (pstRecCfg->record_type != RKADK_REC_TYPE_LAPSE &&
      RKADK_MUXER_EnableAudio(u32CamId)) {
    RKADK_RECORD_SetAudioChn(&stSrcChn, &stDestChn);

    // Get aenc data
    ret = RKADK_MEDIA_GetAencBuffer(&stDestChn, RKADK_RECORD_AencOutCb,
                                    pRecorder);
    if (ret) {
      RKADK_LOGE("RKADK_MEDIA_GetAencBuffer failed[%x]", ret);
      return ret;
    }

    // Bind AI to AENC
    ret = RKADK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_SYS_Bind failed(%d)", ret);
      return ret;
    }
  }

  for (int i = 0; i < (int)pstRecCfg->file_num; i++) {
    RKADK_RECORD_SetVideoChn(i, u32CamId, pstRecCfg, &stSrcChn, &stDestChn,
                             &stRgaChn);

    // Get venc data
    if (RKADK_RECORD_VencGetData(u32CamId, &stDestChn, pRecorder))
      return -1;

#ifdef RKADK_ENABLE_RGA
    bUseRga = RKADK_RECORD_IsUseRga(i, pstRecCfg);
    if (bUseRga) {
      // RGA Bind VENC
      ret = RKADK_MPI_SYS_Bind(&stRgaChn, &stDestChn);
      if (ret) {
        RKADK_LOGE("Bind RGA[%d] to VENC[%d] failed[%d]", stRgaChn.s32ChnId,
                   stDestChn.s32ChnId, ret);
        return ret;
      }

      // VI Bind RGA
      ret = RKADK_MPI_SYS_Bind(&stSrcChn, &stRgaChn);
      if (ret) {
        RKADK_LOGE("Bind VI[%d] to RGA[%d] failed[%d]", stSrcChn.s32ChnId,
                   stRgaChn.s32ChnId, ret);
        return ret;
      }
    } else
#endif
    {
      // Bind VI to VENC
      ret = RKADK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
      if (ret) {
        RKADK_LOGE("RKADK_MPI_SYS_Bind failed(%d)", ret);
        return ret;
      }
    }
  }

  return 0;
}

static int RKADK_RECORD_UnBindChn(RKADK_U32 u32CamId) {
  int ret;
  MPP_CHN_S stSrcChn, stDestChn, stRgaChn;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

#ifdef RKADK_ENABLE_RGA
  bool bUseRga = false;
#endif

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  if (pstRecCfg->record_type != RKADK_REC_TYPE_LAPSE &&
      RKADK_MUXER_EnableAudio(u32CamId)) {
    RKADK_RECORD_SetAudioChn(&stSrcChn, &stDestChn);

    // Stop get aenc data
    RKADK_MEDIA_StopGetAencBuffer(&stDestChn, RKADK_RECORD_AencOutCb);

    // UnBind AI to AENC
    ret = RKADK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (ret) {
      RKADK_LOGE("RKADK_MPI_SYS_UnBind failed(%d)", ret);
      return ret;
    }
  }

  for (int i = 0; i < (int)pstRecCfg->file_num; i++) {
    RKADK_RECORD_SetVideoChn(i, u32CamId, pstRecCfg, &stSrcChn, &stDestChn,
                             &stRgaChn);

    // Stop get venc data
    RKADK_MEDIA_StopGetVencBuffer(&stDestChn, RKADK_RECORD_VencOutCb);

#ifdef RKADK_ENABLE_RGA
    bUseRga = RKADK_RECORD_IsUseRga(i, pstRecCfg);
    if (bUseRga) {
      // RGA UnBind VENC
      ret = RKADK_MPI_SYS_UnBind(&stRgaChn, &stDestChn);
      if (ret) {
        RKADK_LOGE("UnBind RGA[%d] to VENC[%d] failed[%d]", stRgaChn.s32ChnId,
                   stDestChn.s32ChnId, ret);
        return ret;
      }

      // VI UnBind RGA
      ret = RKADK_MPI_SYS_UnBind(&stSrcChn, &stRgaChn);
      if (ret) {
        RKADK_LOGE("UnBind VI[%d] to RGA[%d] failed[%d]", stSrcChn.s32ChnId,
                   stRgaChn.s32ChnId, ret);
        return ret;
      }
    } else
#endif
    {
      // UnBind VI to VENC
      ret = RKADK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
      if (ret) {
        RKADK_LOGE("RKADK_MPI_SYS_UnBind failed(%d)", ret);
        return ret;
      }
    }
  }

  return 0;
}

static RKADK_S32 RKADK_RECORD_SetMuxerAttr(RKADK_U32 u32CamId,
                                           RKADK_MUXER_ATTR_S *pstMuxerAttr) {
  bool bEnableAudio = false;
  RKADK_U32 u32Integer = 0, u32Remainder = 0;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioParam = NULL;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstAudioParam = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioParam) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return -1;
  }

  pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  bEnableAudio = RKADK_MUXER_EnableAudio(u32CamId);

  pstSensorCfg = RKADK_PARAM_GetSensorCfg(u32CamId);
  if (!pstSensorCfg) {
    RKADK_LOGE("RKADK_PARAM_GetSensorCfg failed");
    return -1;
  }

  memset(pstMuxerAttr, 0, sizeof(RKADK_MUXER_ATTR_S));

  u32Integer = pstRecCfg->attribute[0].gop / pstSensorCfg->framerate;
  u32Remainder = pstRecCfg->attribute[0].gop % pstSensorCfg->framerate;
  pstMuxerAttr->stPreRecordAttr.u32PreRecCacheTime =
      pstRecCfg->pre_record_time + u32Integer;
  if (u32Remainder)
    pstMuxerAttr->stPreRecordAttr.u32PreRecCacheTime += 1;

  if (pstRecCfg->record_type == RKADK_REC_TYPE_LAPSE)
    pstMuxerAttr->bLapseRecord = RK_TRUE;
  else
    pstMuxerAttr->bLapseRecord = RK_FALSE;

  pstMuxerAttr->u32CamId = u32CamId;
  pstMuxerAttr->u32StreamCnt = pstRecCfg->file_num;
  pstMuxerAttr->stPreRecordAttr.u32PreRecTimeSec = pstRecCfg->pre_record_time;
  pstMuxerAttr->stPreRecordAttr.enPreRecordMode = pstRecCfg->pre_record_mode;
  pstMuxerAttr->pcbRequestFileNames = GetRecordFileName;

  for (int i = 0; i < (int)pstMuxerAttr->u32StreamCnt; i++) {
    pstMuxerAttr->astStreamAttr[i].enType = pstRecCfg->file_type;
    if (pstMuxerAttr->bLapseRecord) {
      pstMuxerAttr->astStreamAttr[i].u32TimeLenSec =
          pstRecCfg->record_time_cfg[i].lapse_interval;
      pstMuxerAttr->astStreamAttr[i].u32TrackCnt = 1; // only video track
    } else {
      pstMuxerAttr->astStreamAttr[i].u32TimeLenSec =
          pstRecCfg->record_time_cfg[i].record_time;
      pstMuxerAttr->astStreamAttr[i].u32TrackCnt = RKADK_MUXER_TRACK_MAX_CNT;
    }

    if (pstMuxerAttr->astStreamAttr[i].u32TrackCnt == RKADK_MUXER_TRACK_MAX_CNT)
      if (!bEnableAudio)
        pstMuxerAttr->astStreamAttr[i].u32TrackCnt = 1;

    // video track
    RKADK_MUXER_TRACK_SOURCE_S *aHTrackSrcHandle =
        &(pstMuxerAttr->astStreamAttr[i].aHTrackSrcHandle[0]);
    aHTrackSrcHandle->enTrackType = RKADK_TRACK_SOURCE_TYPE_VIDEO;
    aHTrackSrcHandle->u32ChnId = pstRecCfg->attribute[i].venc_chn;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.enCodecType =
        pstRecCfg->attribute[i].codec_type;
    if (RKADK_MEDIA_GetPixelFormat(
            pstRecCfg->vi_attr[i].stChnAttr.enPixelFormat,
            aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.cPixFmt))
      return -1;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32FrameRate =
        pstSensorCfg->framerate;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u16Level = 41;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u16Profile =
        pstRecCfg->attribute[i].profile;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32BitRate =
        pstRecCfg->attribute[i].bitrate;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Width =
        pstRecCfg->attribute[i].width;
    aHTrackSrcHandle->unTrackSourceAttr.stVideoInfo.u32Height =
        pstRecCfg->attribute[i].height;

    if (pstMuxerAttr->bLapseRecord || !bEnableAudio)
      continue;

    // audio track
    aHTrackSrcHandle = &(pstMuxerAttr->astStreamAttr[i].aHTrackSrcHandle[1]);
    aHTrackSrcHandle->enTrackType = RKADK_TRACK_SOURCE_TYPE_AUDIO;
    aHTrackSrcHandle->u32ChnId = RECORD_AENC_CHN;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.enCodecType =
        pstAudioParam->codec_type;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32BitWidth =
        RKADK_MEDIA_GetAudioBitWidth(pstAudioParam->bit_width);
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32ChnCnt =
        pstAudioParam->channels;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32SamplesPerFrame =
        pstAudioParam->samples_per_frame;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32SampleRate =
        pstAudioParam->samplerate;
    aHTrackSrcHandle->unTrackSourceAttr.stAudioInfo.u32Bitrate =
        pstAudioParam->bitrate;
  }

  return 0;
}

RKADK_S32 RKADK_RECORD_Create(RKADK_RECORD_ATTR_S *pstRecAttr,
                              RKADK_MW_PTR *ppRecorder) {
  int ret;
  bool bEnableAudio = false;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

  RKADK_CHECK_POINTER(pstRecAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstRecAttr->s32CamID, RKADK_FAILURE);

  pstRecCfg = RKADK_PARAM_GetRecCfg(pstRecAttr->s32CamID);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  RKADK_LOGI("Create Record[%d, %d] Start...", pstRecAttr->s32CamID,
             pstRecCfg->record_type);

  RKADK_MPI_SYS_Init();
  RKADK_PARAM_Init(NULL, NULL);

  if (RKADK_RECORD_CreateVideoChn(pstRecAttr->s32CamID))
    return -1;

  bEnableAudio = RKADK_MUXER_EnableAudio(pstRecAttr->s32CamID);
  if (pstRecCfg->record_type != RKADK_REC_TYPE_LAPSE && bEnableAudio) {
    if (RKADK_RECORD_CreateAudioChn()) {
      RKADK_RECORD_DestoryVideoChn(pstRecAttr->s32CamID);
      return -1;
    }
  }

  g_pfnRequestFileNames[pstRecAttr->s32CamID] = pstRecAttr->pfnRequestFileNames;

  RKADK_MUXER_ATTR_S stMuxerAttr;
  ret = RKADK_RECORD_SetMuxerAttr(pstRecAttr->s32CamID, &stMuxerAttr);
  if (ret) {
    RKADK_LOGE("RKADK_RECORD_SetMuxerAttr failed");
    goto failed;
  }

  stMuxerAttr.pfnEventCallback = pstRecAttr->pfnEventCallback;
  if (RKADK_MUXER_Create(&stMuxerAttr, ppRecorder))
    goto failed;

  if (RKADK_RECORD_BindChn(pstRecAttr->s32CamID, *ppRecorder))
    goto failed;

  RKADK_LOGI("Create Record[%d, %d] End...", pstRecAttr->s32CamID,
             pstRecCfg->record_type);
  return 0;

failed:
  RKADK_LOGE("Create Record[%d, %d] failed", pstRecAttr->s32CamID,
             pstRecCfg->record_type);
  RKADK_RECORD_DestoryVideoChn(pstRecAttr->s32CamID);

  if (pstRecCfg->record_type != RKADK_REC_TYPE_LAPSE && bEnableAudio)
    RKADK_RECORD_DestoryAudioChn();

  return -1;
}

RKADK_S32 RKADK_RECORD_Destroy(RKADK_MW_PTR pRecorder) {
  RKADK_S32 ret, index;
  RKADK_U32 u32CamId;
  RKADK_REC_TYPE_E enRecType = RKADK_REC_TYPE_NORMAL;
  RKADK_MUXER_HANDLE_S *stRecorder = NULL;

  RKADK_CHECK_POINTER(pRecorder, RKADK_FAILURE);
  stRecorder = (RKADK_MUXER_HANDLE_S *)pRecorder;
  if (!stRecorder) {
    RKADK_LOGE("stRecorder is null");
    return -1;
  }

  if (stRecorder->bLapseRecord)
    enRecType = RKADK_REC_TYPE_LAPSE;

  RKADK_LOGI("Destory Record[%d, %d] Start...", stRecorder->u32CamId,
             enRecType);

  u32CamId = stRecorder->u32CamId;
  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  for (int i = 0; i < (int)stRecorder->u32StreamCnt; i++) {
    index = i + (RKADK_MUXER_STREAM_MAX_CNT * u32CamId);
    while (!g_fileNameDeque[index].empty()) {
      RKADK_LOGI("clear file name deque[%d]", index);
      auto front = g_fileNameDeque[index].front();
      g_fileNameDeque[index].pop_front();
      free(front);
    }
    g_fileNameDeque[index].clear();
  }

  ret = RKADK_MUXER_Destroy(pRecorder);
  if (ret) {
    RKADK_LOGE("RK_REC_Destroy failed, ret = %d", ret);
    return ret;
  }

  ret = RKADK_RECORD_UnBindChn(u32CamId);
  if (ret) {
    RKADK_LOGE("RKADK_RECORD_UnBindChn failed, ret = %d", ret);
    return ret;
  }

  ret = RKADK_RECORD_DestoryVideoChn(u32CamId);
  if (ret) {
    RKADK_LOGE("RKADK_RECORD_DestoryVideoChn failed[%x]", ret);
    return ret;
  }

  if (enRecType != RKADK_REC_TYPE_LAPSE && RKADK_MUXER_EnableAudio(u32CamId)) {
    ret = RKADK_RECORD_DestoryAudioChn();
    if (ret) {
      RKADK_LOGE("RKADK_RECORD_DestoryAudioChn failed, ret = %d", ret);
      return ret;
    }
  }

  g_pfnRequestFileNames[u32CamId] = NULL;
  RKADK_MPI_SYS_Exit();
  RKADK_LOGI("Destory Record[%d, %d] End...", u32CamId, enRecType);
  return 0;
}

RKADK_S32 RKADK_RECORD_Start(RKADK_MW_PTR pRecorder) {
  return RKADK_MUXER_Start(pRecorder);
}

RKADK_S32 RKADK_RECORD_Stop(RKADK_MW_PTR pRecorder) {
  return RKADK_MUXER_Stop(pRecorder);
}

RKADK_S32 RKADK_RECORD_SetFrameRate(RKADK_MW_PTR pRecorder,
                                    RKADK_RECORD_FPS_ATTR_S stFpsAttr) {
  RKADK_LOGE("Settings are not supported");
  return -1;
}

RKADK_S32
RKADK_RECORD_ManualSplit(RKADK_MW_PTR pRecorder,
                         RKADK_REC_MANUAL_SPLIT_ATTR_S *pstSplitAttr) {
  return RKADK_MUXER_ManualSplit(pRecorder, pstSplitAttr);
}

RKADK_S32 RKADK_RECORD_GetAencChn() { return RECORD_AENC_CHN; }

RKADK_S32 RKADK_RECORD_RegisterEventCallback(
    RKADK_MW_PTR pRecorder, RKADK_REC_EVENT_CALLBACK_FN pfnEventCallback) {
  RKADK_LOGE("unsupport toogle mirror");
  return -1;

}

RKADK_S32 RKADK_RECORD_ToogleMirror(RKADK_MW_PTR pRecorder,
                                    RKADK_STREAM_TYPE_E enStrmType,
                                    int mirror) {
  RKADK_LOGE("unsupport toogle mirror");
  return -1;
}

RKADK_S32 RKADK_RECORD_ToogleFlip(RKADK_MW_PTR pRecorder,
                                  RKADK_STREAM_TYPE_E enStrmType,
                                  int flip) {
  RKADK_LOGE("unsupport toogle flip");
  return -1;
}