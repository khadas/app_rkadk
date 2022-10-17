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

#include "rkadk_muxer.h"
#include "rkadk_media_comm.h"
#include "rkadk_thumb_comm.h"
#include "rkadk_param.h"
#include "linux_list.h"
#include "rkadk_log.h"
#include "rkadk_signal.h"
#include "rkadk_thread.h"
#include "rkmuxer.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(_a) (sizeof(_a) / sizeof((_a)[0]))
#endif // ARRAY_SIZE

/** Stream Count Check */
#define RKADK_CHECK_STREAM_CNT(cnt)                                            \
  do {                                                                         \
    if ((cnt > RKADK_MUXER_STREAM_MAX_CNT) || (cnt == 0)) {                    \
      RKADK_LOGE("stream count[%d] > RKADK_MUXER_STREAM_MAX_CNT or == 0",      \
                 cnt);                                                         \
      return -1;                                                               \
    }                                                                          \
  } while (0)

typedef struct {
  struct list_head mark;
  unsigned char *buf;
  RKADK_U32 size;
  int isKeyFrame;
  int64_t pts;
  struct list_head *pool;
} MUXER_BUF_CELL_S;

typedef struct {
  RKADK_MW_PTR ptr;
  RKADK_U32 vChnId; // venc channel id
  int muxerId;
  char cFileName[RKADK_MAX_FILE_PATH_LEN];
  const char *cOutputFmt;
  VideoParam stVideo;
  AudioParam stAudio;
  int32_t gop;
  int32_t duration;     // s
  int32_t realDuration; // ms
  int64_t startTime;    // us
  int frameCnt;
  bool bGetThumb;
  bool bRequestThumb;
  bool bEnableStream;
  bool bMuxering;
  RKADK_MUXER_REQUEST_FILE_NAME_CB pcbRequestFileNames;
  RKADK_MUXER_EVENT_CALLBACK_FN pfnEventCallback;
  void *pThread;
  pthread_mutex_t mutex;
  void *pSignal;

  // list param
  MUXER_BUF_CELL_S stVCell[20]; // video list cache size
  MUXER_BUF_CELL_S stACell[20]; // audio list cache size
  struct list_head stVFree;     // video list remain size
  struct list_head stAFree;     // audio list remain size
  struct list_head stProcList;  // process list

  // thumbnail list param;
  bool bEnableThumb;
  MUXER_BUF_CELL_S stThumbCell[2]; // thumbnail list cache size
  struct list_head stThumbFree;     // thumbnail list remain size
  struct list_head stThumbProcList; // thumbnail process list
} MUXER_HANDLE_S;

static void RKADK_MUXER_ListInit(MUXER_HANDLE_S *pstMuxerHandle) {
  INIT_LIST_HEAD(&pstMuxerHandle->stVFree);
  INIT_LIST_HEAD(&pstMuxerHandle->stAFree);
  INIT_LIST_HEAD(&pstMuxerHandle->stProcList);

  for (unsigned int i = 0; i < ARRAY_SIZE(pstMuxerHandle->stVCell); i++) {
    INIT_LIST_HEAD(&pstMuxerHandle->stVCell[i].mark);
    pstMuxerHandle->stVCell[i].pool = &pstMuxerHandle->stVFree;
    list_add_tail(&pstMuxerHandle->stVCell[i].mark, &pstMuxerHandle->stVFree);
  }

  for (unsigned int i = 0; i < ARRAY_SIZE(pstMuxerHandle->stACell); i++) {
    INIT_LIST_HEAD(&pstMuxerHandle->stACell[i].mark);
    pstMuxerHandle->stACell[i].pool = &pstMuxerHandle->stAFree;
    list_add_tail(&pstMuxerHandle->stACell[i].mark, &pstMuxerHandle->stAFree);
  }
}

static void RKADK_MUXER_CellFree(MUXER_HANDLE_S *pstMuxerHandle,
                                 MUXER_BUF_CELL_S *cell) {
  struct list_head *pool = cell->pool;

  list_del_init(&cell->mark);
  if (cell->buf) {
    free(cell->buf);
    cell->buf = NULL;
  }

  memset(cell, 0, sizeof(MUXER_BUF_CELL_S));
  INIT_LIST_HEAD(&cell->mark);
  cell->pool = pool;
  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  list_add_tail(&cell->mark, cell->pool);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
}

static void RKADK_MUXER_ListRelease(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;
  int ret = 0;

  list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stProcList, mark) {
    ret = 1;
    list_del_init(&cell->mark);
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    break;
  }

  if (ret)
    RKADK_LOGI("lose frame");
}

static MUXER_BUF_CELL_S *RKADK_MUXER_CellGet(MUXER_HANDLE_S *pstMuxerHandle,
                                             struct list_head *head) {
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  list_for_each_entry_safe(cell, cell_n, head, mark) {
    list_del_init(&cell->mark);
    RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
    return cell;
  }
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  return NULL;
}

static void RKADK_MUXER_CellPush(MUXER_HANDLE_S *pstMuxerHandle,
                                 MUXER_BUF_CELL_S *one) {
  int ret = -1;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stProcList, mark) {
      if (cell->pts > one->pts) {
        list_add_tail(&one->mark, &cell->mark);
        ret = 0;
        break;
      }
    }
    if (ret) {
      list_add_tail(&one->mark, &pstMuxerHandle->stProcList);
    }
    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
}

static MUXER_BUF_CELL_S *RKADK_MUXER_CellPop(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *rst = NULL;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stProcList, mark) {
      list_del_init(&cell->mark);
      rst = cell;
      break;
    }
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  return rst;
}

void RKADK_MUXER_ListDropPFrame(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *rst = NULL;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stProcList, mark) {
    if (cell->isKeyFrame == 0 && cell->pool == &pstMuxerHandle->stVFree) {
      rst = cell;
    }
  }
  if (rst) {
    list_del_init(&rst->mark);
  }
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);

  if (rst)
    RKADK_MUXER_CellFree(pstMuxerHandle, rst);
  else
    RKADK_LOGE("drop pframe fail");
}

// thumbnail list
static void RKADK_MUXER_ThumbListInit(MUXER_HANDLE_S *pstMuxerHandle) {
  INIT_LIST_HEAD(&pstMuxerHandle->stThumbFree);
  INIT_LIST_HEAD(&pstMuxerHandle->stThumbProcList);

  for (unsigned int i = 0; i < ARRAY_SIZE(pstMuxerHandle->stThumbCell); i++) {
    INIT_LIST_HEAD(&pstMuxerHandle->stThumbCell[i].mark);
    pstMuxerHandle->stThumbCell[i].pool = &pstMuxerHandle->stThumbFree;
    list_add_tail(&pstMuxerHandle->stThumbCell[i].mark, &pstMuxerHandle->stThumbFree);
  }
  pstMuxerHandle->bEnableThumb = true;
}

static void RKADK_MUXER_ThumbListRelease(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;
  int ret = 0;

  list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stThumbProcList, mark) {
    ret = 1;
    list_del_init(&cell->mark);
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    break;
  }
  pstMuxerHandle->bEnableThumb = false;
  if (ret)
    RKADK_LOGI("lose frame");
}

static void RKADK_MUXER_ThumbCellPush(MUXER_HANDLE_S *pstMuxerHandle,
                                 MUXER_BUF_CELL_S *one) {
  int ret = -1;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stThumbProcList, mark) {
      if (cell->pts > one->pts) {
        list_add_tail(&one->mark, &cell->mark);
        ret = 0;
        break;
      }
    }
    if (ret) {
      list_add_tail(&one->mark, &pstMuxerHandle->stThumbProcList);
    }
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
}

static MUXER_BUF_CELL_S *RKADK_MUXER_ThumbCellPop(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *rst = NULL;
  MUXER_BUF_CELL_S *cell = NULL;
  MUXER_BUF_CELL_S *cell_n = NULL;

  RKADK_MUTEX_LOCK(pstMuxerHandle->mutex);
  do {
    list_for_each_entry_safe(cell, cell_n, &pstMuxerHandle->stThumbProcList, mark) {
      list_del_init(&cell->mark);
      rst = cell;
      break;
    }
  } while (0);
  RKADK_MUTEX_UNLOCK(pstMuxerHandle->mutex);
  return rst;
}

static MUXER_HANDLE_S *RKADK_MUXER_FindHandle(RKADK_MUXER_HANDLE_S *pstMuxer,
                                              RKADK_U32 chnId) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (pstMuxerHandle && pstMuxerHandle->vChnId == chnId)
      break;
  }

  return pstMuxerHandle;
}

int RKADK_MUXER_WriteVideoFrame(RKADK_U32 chnId, RKADK_CHAR *buf,
                                RKADK_U32 size, int64_t pts, int isKeyFrame,
                                void *handle) {
  int cnt = 0;
  RKADK_CHECK_POINTER(handle, RKADK_FAILURE);

  RKADK_MUXER_HANDLE_S *pstMuxer = (RKADK_MUXER_HANDLE_S *)handle;
  MUXER_HANDLE_S *pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, chnId);
  if (!pstMuxerHandle) {
    // RKADK_LOGE("don't find muxer handle");
    return -1;
  }

  if (!pstMuxerHandle->bEnableStream) {
    //RKADK_LOGI("Muxer is not enable stream");
    return 0;
  }

  MUXER_BUF_CELL_S *cell;

  while ((cell = RKADK_MUXER_CellGet(pstMuxerHandle, &pstMuxerHandle->stVFree)) == NULL) {
      if (cnt % 100 == 0)
        RKADK_LOGI("Stream[%d] get video cell fail, retry, cnt = %d",chnId, cnt);
      cnt++;
      usleep(10000);
  }

  cell->buf = (unsigned char *)malloc(size);
  if (NULL == cell->buf) {
    RKADK_LOGE("malloc video cell buf failed");
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    return -1;
  }

  memcpy(cell->buf, buf, size);
  cell->isKeyFrame = isKeyFrame;
  cell->pts = pts;
  cell->size = size;
  RKADK_MUXER_CellPush(pstMuxerHandle, cell);
  return 0;
}

int RKADK_MUXER_WriteAudioFrame(RKADK_CHAR *buf, RKADK_U32 size, int64_t pts,
                                void *handle) {
  int cnt = 0;
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  int headerSize = 0; // aenc header size

  RKADK_CHECK_POINTER(handle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)handle;
  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle || !pstMuxerHandle->bEnableStream)
      continue;

    MUXER_BUF_CELL_S *cell;
    while ((cell = RKADK_MUXER_CellGet(pstMuxerHandle, &pstMuxerHandle->stAFree)) == NULL) {
      if (cnt % 100 == 0)
        RKADK_LOGI("Stream[%d] get audio cell fail, retry, cnt = %d",pstMuxerHandle->vChnId, cnt);
      cnt++;
      usleep(10000);
    }

    cell->size = size - headerSize;
    cell->buf = (unsigned char *)malloc(cell->size);
    if (NULL == cell->buf) {
      RKADK_LOGE("malloc audio cell buf failed");
      return -1;
    }
    memcpy(cell->buf, (buf + headerSize), cell->size);
    cell->isKeyFrame = 0;
    cell->pts = pts;
    RKADK_MUXER_CellPush(pstMuxerHandle, cell);
  }

  return 0;
}

void RKADK_MUXER_ProcessEvent(MUXER_HANDLE_S *pstMuxerHandle,
                              RKADK_MUXER_EVENT_E enEventType, int64_t value) {
  RKADK_MUXER_EVENT_INFO_S stEventInfo;
  memset(&stEventInfo, 0, sizeof(RKADK_MUXER_EVENT_INFO_S));

  if (!pstMuxerHandle->pfnEventCallback) {
    RKADK_LOGD("Unregistered event callback");
    return;
  }

  stEventInfo.enEvent = enEventType;
  stEventInfo.unEventInfo.stFileInfo.u32Duration = value;
  memcpy(stEventInfo.unEventInfo.stFileInfo.asFileName,
         pstMuxerHandle->cFileName, strlen(pstMuxerHandle->cFileName));
  pstMuxerHandle->pfnEventCallback(pstMuxerHandle, &stEventInfo);
}

static FILE *g_output_file = NULL;

static void RKADK_MUXER_Close(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *thumbCell = NULL;
  RKADK_THUMB_ATTR_S stThumbAttr;
  int ret;
  if (!pstMuxerHandle->bMuxering)
    return;

  // Stop muxer
  rkmuxer_deinit(pstMuxerHandle->muxerId);

  // thumbnail
  if (pstMuxerHandle->bEnableThumb) {
    thumbCell = RKADK_MUXER_ThumbCellPop(pstMuxerHandle);
    if (thumbCell) {
      memset(&stThumbAttr, 0, sizeof(RKADK_THUMB_ATTR_S));
      RKADK_PARAM_THUMB_CFG_S *ptsThumbCfg =
        RKADK_PARAM_GetThumbCfg();
      if (!ptsThumbCfg) {
        RKADK_LOGE("RKADK_PARAM_GetThumbCfg failed");
        return;
      }
      stThumbAttr.enType = RKADK_THUMB_TYPE_JPEG;
      stThumbAttr.u32Width = ptsThumbCfg->thumb_width;
      stThumbAttr.u32Height = ptsThumbCfg->thumb_height;
      stThumbAttr.u32VirWidth = ptsThumbCfg->thumb_width;
      stThumbAttr.u32VirHeight = ptsThumbCfg->thumb_height;
      stThumbAttr.pu8Buf = (RKADK_U8 *)thumbCell->buf;
      stThumbAttr.u32BufSize = thumbCell->size;
      RKADK_LOGI("FileName = %s, buf = %p, buf_size = %d",
      pstMuxerHandle->cFileName, stThumbAttr.pu8Buf, stThumbAttr.u32BufSize);
      ThumbnailBuildIn(pstMuxerHandle->cFileName,
                              &stThumbAttr);
      RKADK_MUXER_CellFree(pstMuxerHandle, thumbCell);
      if (pstMuxerHandle->vChnId == 0)
        ThumbnailRenameFile(pstMuxerHandle->cFileName);
    }
  }

  if (pstMuxerHandle->realDuration <= 0) {
    pstMuxerHandle->realDuration =
      pstMuxerHandle->frameCnt * (1000 / pstMuxerHandle->stVideo.frame_rate_num);
    RKADK_LOGI("The revised Duration = %d, frameCnt = %d",
      pstMuxerHandle->realDuration, pstMuxerHandle->frameCnt);
  }

  RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_END,
                           pstMuxerHandle->realDuration);

  // Reset muxer
  pstMuxerHandle->realDuration = 0;
  pstMuxerHandle->startTime = 0;
  pstMuxerHandle->bMuxering = 0;
  pstMuxerHandle->frameCnt = 0;
  pstMuxerHandle->bGetThumb = false;
  pstMuxerHandle->bRequestThumb = false;

  if (g_output_file)
    fclose(g_output_file);
}

static bool RKADK_MUXER_SaveThumb(MUXER_HANDLE_S *pstMuxerHandle) {
  MUXER_BUF_CELL_S *thumbCell = NULL;

  if (pstMuxerHandle->bEnableThumb) {
    thumbCell = RKADK_MUXER_ThumbCellPop(pstMuxerHandle);
    if (thumbCell) {
      ThumbnailSaveFile(pstMuxerHandle->cFileName, thumbCell->buf,
                              thumbCell->size);
      RKADK_MUXER_ThumbCellPush(pstMuxerHandle, thumbCell);
      return false;
    } else {
      return true;
    }
  }
}

static bool RKADK_MUXER_Proc(void *params) {
  int ret = 0;
  MUXER_BUF_CELL_S *cell = NULL;

  if (!params) {
    RKADK_LOGE("Invalid param");
    return false;
  }

  MUXER_HANDLE_S *pstMuxerHandle = (MUXER_HANDLE_S *)params;
  RKADK_SIGNAL_Wait(pstMuxerHandle->pSignal, pstMuxerHandle->duration * 1000);

  cell = RKADK_MUXER_CellPop(pstMuxerHandle);
  while (cell) {
    // Create muxer
    if (pstMuxerHandle->bEnableStream) {
      if (!pstMuxerHandle->bMuxering && cell->isKeyFrame) {
        ret = pstMuxerHandle->pcbRequestFileNames(pstMuxerHandle->ptr,
                                                  pstMuxerHandle->cFileName,
                                                  pstMuxerHandle->muxerId);
        if (ret) {
          RKADK_LOGE("request file name failed");
        } else {
          RKADK_LOGI("Ready to recod new video file path:[%s]",
                    pstMuxerHandle->cFileName);
          RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_FILE_BEGIN,
                                  pstMuxerHandle->duration);
          ret = rkmuxer_init(pstMuxerHandle->muxerId,
                            (char *)pstMuxerHandle->cOutputFmt,
                            pstMuxerHandle->cFileName, &pstMuxerHandle->stVideo,
                            &pstMuxerHandle->stAudio);
          if (ret) {
            RKADK_LOGE("rkmuxer_init failed[%d]", ret);
          } else {
            pstMuxerHandle->bMuxering = true;
            pstMuxerHandle->startTime = cell->pts;
            pstMuxerHandle->frameCnt = 1;
            pstMuxerHandle->bGetThumb = true;
            pstMuxerHandle->bRequestThumb = true;
          }
        }
      } else if (!pstMuxerHandle->bMuxering){
        RKADK_LOGI("Stream [%d] wait I frame!", pstMuxerHandle->vChnId);
        RK_MPI_VENC_RequestIDR(pstMuxerHandle->vChnId, RK_FALSE);
      }

      if (pstMuxerHandle->bGetThumb && pstMuxerHandle->vChnId == 0) {
        pstMuxerHandle->bGetThumb = RKADK_MUXER_SaveThumb(pstMuxerHandle);
      }

      // Process
      if (pstMuxerHandle->bMuxering) {
        // Check close
        if (cell->isKeyFrame && ((cell->pts - pstMuxerHandle->startTime >=
                                  (pstMuxerHandle->duration * 1000000 -
                                  1000000 / pstMuxerHandle->stVideo.frame_rate_num)))) {
          RKADK_LOGI("File switch: chn = %d, frameCnt = %d", pstMuxerHandle->vChnId, pstMuxerHandle->frameCnt);
          RKADK_MUXER_Close(pstMuxerHandle);
          continue;
        }

        // Write
        if (cell->pool == &pstMuxerHandle->stVFree) {
  #if 0
          if(!g_output_file) {
            g_output_file = fopen("/data/venc.h264", "w");
            if (!g_output_file)
              RKADK_LOGE("open /data/venc.h264 failed");
          }

          if (g_output_file)
            fwrite(cell->buf, 1, cell->size, g_output_file);
  #endif
          rkmuxer_write_video_frame(pstMuxerHandle->muxerId, cell->buf,
                                    cell->size, cell->pts, cell->isKeyFrame);
          if (cell->pts < pstMuxerHandle->startTime)
            RKADK_LOGE("Stream [%d] muxer pts err pts = %lld, startTime = %lld",
                        pstMuxerHandle->vChnId, cell->pts, pstMuxerHandle->startTime);
          pstMuxerHandle->realDuration =
              (cell->pts - pstMuxerHandle->startTime) / 1000;
          pstMuxerHandle->frameCnt++;
          //requst thumbnai
          if (pstMuxerHandle->vChnId == 0 &&
              pstMuxerHandle->frameCnt > ((pstMuxerHandle->duration - pstMuxerHandle->gop / pstMuxerHandle->stVideo.frame_rate_num)
              * pstMuxerHandle->stVideo.frame_rate_num) && cell->isKeyFrame && pstMuxerHandle->bRequestThumb) {
            RKADK_LOGI("Request thumbnail frameCnt = %d", pstMuxerHandle->frameCnt);
            ret = RK_MPI_VENC_ThumbnailRequest(pstMuxerHandle->vChnId);
            if (ret)
              RKADK_LOGE("RK_MPI_VENC_ThumbnailRequest fail %x", ret);
            pstMuxerHandle->bRequestThumb = false;
          }
        } else if (cell->pool == &pstMuxerHandle->stAFree) {
          rkmuxer_write_audio_frame(pstMuxerHandle->muxerId, cell->buf,
                                    cell->size, cell->pts);
        } else {
          RKADK_LOGE("unknow pool");
        }
      }
    }

    // free and next
    RKADK_MUXER_CellFree(pstMuxerHandle, cell);
    cell = RKADK_MUXER_CellPop(pstMuxerHandle);
  }

  // Check exit
  if (!pstMuxerHandle->bEnableStream)
    RKADK_MUXER_Close(pstMuxerHandle);

  return pstMuxerHandle->bEnableStream;
}

static RKADK_S32 RKADK_MUXER_Enable(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                                    RKADK_MUXER_HANDLE_S *pstMuxer) {
  int i, j;
  char name[256];
  MUXER_HANDLE_S *pMuxerHandle = NULL;
  RKADK_MUXER_STREAM_ATTR_S *pstSrcStreamAttr = NULL;
  RKADK_MUXER_TRACK_SOURCE_S *pstTrackSource = NULL;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;

  pstRecCfg = RKADK_PARAM_GetRecCfg(pstMuxerAttr->u32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return -1;
  }

  for (i = 0; i < (int)pstMuxerAttr->u32StreamCnt; i++) {
    pMuxerHandle = (MUXER_HANDLE_S *)malloc(sizeof(MUXER_HANDLE_S));
    if (!pMuxerHandle) {
      RKADK_LOGE("malloc muxer handle failed");
      return -1;
    }
    memset(pMuxerHandle, 0, sizeof(MUXER_HANDLE_S));

    pMuxerHandle->muxerId =
        i + (pstMuxerAttr->u32CamId * RKADK_MUXER_STREAM_MAX_CNT);
    pMuxerHandle->bEnableStream = true;
    pMuxerHandle->pcbRequestFileNames = pstMuxerAttr->pcbRequestFileNames;
    pMuxerHandle->pfnEventCallback = pstMuxerAttr->pfnEventCallback;

    pstSrcStreamAttr = &(pstMuxerAttr->astStreamAttr[i]);
    pMuxerHandle->duration = pstSrcStreamAttr->u32TimeLenSec;
    pMuxerHandle->gop = pstRecCfg->attribute[i].gop;

    switch (pstSrcStreamAttr->enType) {
    case RKADK_MUXER_TYPE_MP4:
      pMuxerHandle->cOutputFmt = "mp4";
      break;
    case RKADK_MUXER_TYPE_MPEGTS:
      pMuxerHandle->cOutputFmt = "mpegts";
      break;
    case RKADK_MUXER_TYPE_FLV:
      pMuxerHandle->cOutputFmt = "flv";
      break;
    default:
      RKADK_LOGE("not support type: %d", pstSrcStreamAttr->enType);
      return -1;
    }

    for (j = 0; j < (int)pstSrcStreamAttr->u32TrackCnt; j++) {
      pstTrackSource = &(pstSrcStreamAttr->aHTrackSrcHandle[j]);

      if (pstTrackSource->enTrackType == RKADK_TRACK_SOURCE_TYPE_VIDEO) {
        RKADK_TRACK_VIDEO_SOURCE_INFO_S *videoInfo =
            &(pstTrackSource->unTrackSourceAttr.stVideoInfo);

        pMuxerHandle->vChnId = pstTrackSource->u32ChnId;
        pMuxerHandle->stVideo.width = videoInfo->u32Width;
        pMuxerHandle->stVideo.height = videoInfo->u32Height;
        pMuxerHandle->stVideo.bit_rate = videoInfo->u32BitRate;
        pMuxerHandle->stVideo.frame_rate_den = 1;
        pMuxerHandle->stVideo.frame_rate_num = videoInfo->u32FrameRate;
        pMuxerHandle->stVideo.profile = videoInfo->u16Profile;
        pMuxerHandle->stVideo.level = videoInfo->u16Level;

        switch (videoInfo->enCodecType) {
        case RKADK_CODEC_TYPE_H264:
          memcpy(pMuxerHandle->stVideo.codec, "H.264", strlen("H.264"));
          break;
        case RKADK_CODEC_TYPE_H265:
          memcpy(pMuxerHandle->stVideo.codec, "H.265", strlen("H.265"));
          break;
        default:
          RKADK_LOGE("not support enCodecType: %d", videoInfo->enCodecType);
          return -1;
        }
      } else if (pstTrackSource->enTrackType == RKADK_TRACK_SOURCE_TYPE_AUDIO) {
        RKADK_TRACK_AUDIO_SOURCE_INFO_S *audioInfo =
            &(pstTrackSource->unTrackSourceAttr.stAudioInfo);

        pMuxerHandle->stAudio.channels = audioInfo->u32ChnCnt;
        pMuxerHandle->stAudio.frame_size = audioInfo->u32SamplesPerFrame;
        pMuxerHandle->stAudio.sample_rate = audioInfo->u32SampleRate;

        switch (audioInfo->u32BitWidth) {
        case 16:
          memcpy(pMuxerHandle->stAudio.format, "S16", strlen("S16"));
          break;
        case 32:
          memcpy(pMuxerHandle->stAudio.format, "S32", strlen("S32"));
          break;
        default:
          RKADK_LOGE("not support u32BitWidth: %d", audioInfo->u32BitWidth);
          return -1;
        }

        switch (audioInfo->enCodecType) {
        case RKADK_CODEC_TYPE_MP3:
          memcpy(pMuxerHandle->stAudio.codec, "MP2", strlen("MP2"));
          break;
        case RKADK_CODEC_TYPE_MP2:
          memcpy(pMuxerHandle->stAudio.codec, "MP2", strlen("MP2"));
          break;
        default:
          RKADK_LOGE("not support enCodecType: %d", audioInfo->enCodecType);
          return -1;
        }
      }
    }

    // Init List
    RKADK_MUXER_ListInit(pMuxerHandle);

    // Create signal
    pMuxerHandle->pSignal = RKADK_SIGNAL_Create(0, 1);
    if (!pMuxerHandle->pSignal) {
      RKADK_LOGE("RKADK_SIGNAL_Create failed");
      return -1;
    }
    snprintf(name, sizeof(name), "Muxer_%d", pMuxerHandle->vChnId);
    pMuxerHandle->ptr = (RKADK_MW_PTR)pstMuxer;
    pMuxerHandle->mutex = PTHREAD_MUTEX_INITIALIZER;
    pMuxerHandle->pThread = RKADK_THREAD_Create(RKADK_MUXER_Proc, pMuxerHandle, name);
    if (!pMuxerHandle->pThread) {
      RKADK_LOGE("RKADK_THREAD_Create failed");
      return -1;
    }
    pstMuxer->pMuxerHandle[i] = (RKADK_MW_PTR)pMuxerHandle;
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Create(RKADK_MUXER_ATTR_S *pstMuxerAttr,
                             RKADK_MW_PTR *ppHandle) {
  int i, ret = 0;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_LOGI("Create Record[%d] Start...", pstMuxerAttr->u32CamId);

  RKADK_CHECK_POINTER(pstMuxerAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstMuxerAttr->u32CamId, RKADK_FAILURE);

  if (*ppHandle) {
    RKADK_LOGE("Muxer Handle has been created");
    return -1;
  }

  pstMuxer = (RKADK_MUXER_HANDLE_S *)malloc(sizeof(RKADK_MUXER_HANDLE_S));
  if (!pstMuxer) {
    RKADK_LOGE("malloc pstMuxer failed");
    return -1;
  }
  memset(pstMuxer, 0, sizeof(RKADK_MUXER_HANDLE_S));

  pstMuxer->u32CamId = pstMuxerAttr->u32CamId;
  pstMuxer->u32StreamCnt = pstMuxerAttr->u32StreamCnt;
  pstMuxer->bLapseRecord = pstMuxerAttr->bLapseRecord;

  ret = RKADK_MUXER_Enable(pstMuxerAttr, pstMuxer);
  if (ret) {
    RKADK_LOGE("RKADK_MUXER_Enable failed");
    goto failed;
  }

  *ppHandle = (RKADK_MW_PTR)pstMuxer;
  RKADK_LOGI("Create Record[%d] Stop...", pstMuxerAttr->u32CamId);
  return 0;

failed:
  for (i = 0; i < (int)pstMuxerAttr->u32StreamCnt; i++) {
    if (pstMuxer->pMuxerHandle[i]) {
      free(pstMuxer->pMuxerHandle[i]);
      pstMuxer->pMuxerHandle[i] = NULL;
    }
  }

  if (pstMuxer) {
    free(pstMuxer);
    pstMuxer = NULL;
  }

  return ret;
}

RKADK_S32 RKADK_MUXER_Destroy(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  RKADK_LOGI("Destory Muxer[%d] Start...", pstMuxer->u32CamId);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    // Set flag off
    pstMuxerHandle->bEnableStream = false;

    // exit thread
    RKADK_THREAD_SetExit(pstMuxerHandle->pThread);

    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);

    // Destroy thread
    RKADK_THREAD_Destory(pstMuxerHandle->pThread);
    pstMuxerHandle->pThread = NULL;

    // Destroy signal
    RKADK_SIGNAL_Destroy(pstMuxerHandle->pSignal);

    // Release list
    RKADK_MUXER_ListRelease(pstMuxerHandle);

    // Release thu list
    if (pstMuxerHandle->bEnableThumb)
      RKADK_MUXER_ThumbListRelease(pstMuxerHandle);

    free(pstMuxer->pMuxerHandle[i]);
    pstMuxer->pMuxerHandle[i] = NULL;
  }

  RKADK_LOGI("Destory Muxer[%d] Stop...", pstMuxer->u32CamId);

  if (pstMuxer) {
    free(pstMuxer);
    pstMuxer = NULL;
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Start(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    pstMuxerHandle->bEnableStream = true;
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_START, 0);
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Stop(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle) {
      RKADK_LOGD("Muxer Handle[%d] is NULL", i);
      continue;
    }

    pstMuxerHandle->bEnableStream = false;
    RKADK_MUXER_ProcessEvent(pstMuxerHandle, RKADK_MUXER_EVENT_STREAM_STOP, 0);
    RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_SetFrameRate(RKADK_MW_PTR pHandle,
                                   RKADK_MUXER_FPS_ATTR_S stFpsAttr) {
  return 0;
}

RKADK_S32
RKADK_MUXER_ManualSplit(RKADK_MW_PTR pHandle,
                        RKADK_MUXER_MANUAL_SPLIT_ATTR_S *pstSplitAttr) {
  return 0;
}

bool RKADK_MUXER_EnableAudio(RKADK_S32 s32CamId) {
  bool bEnable = false;
  RKADK_PARAM_REC_CFG_S *pstRecCfg = NULL;
  RKADK_PARAM_AUDIO_CFG_S *pstAudioCfg = NULL;

  pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
  if (!pstRecCfg) {
    RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    return false;
  }

  pstAudioCfg = RKADK_PARAM_GetAudioCfg();
  if (!pstAudioCfg) {
    RKADK_LOGE("RKADK_PARAM_GetAudioCfg failed");
    return false;
  }

  switch (pstAudioCfg->codec_type) {
  case RKADK_CODEC_TYPE_MP3:
    bEnable = true;
    break;

  case RKADK_CODEC_TYPE_MP2:
    if (pstRecCfg->file_type == RKADK_MUXER_TYPE_FLV)
      bEnable = false;
    else
      bEnable = true;
    break;

  default:
    bEnable = false;
    break;
  }

  return bEnable;
}

RKADK_S32 RKADK_MUXER_SendThumbData(RKADK_MW_PTR pHandle, RKADK_CHAR *buf, RKADK_U32 size,
                                  int64_t pts) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];

    if (!pstMuxerHandle || !pstMuxerHandle->bEnableThumb)
      continue;

    MUXER_BUF_CELL_S *cell =
        RKADK_MUXER_CellGet(pstMuxerHandle, &pstMuxerHandle->stThumbFree);
    if (NULL == cell) {
      RKADK_LOGI("Lose thumbnail data");
      continue;
    }

    cell->size = size;
    cell->buf = (unsigned char *)malloc(cell->size);
    if (NULL == cell->buf) {
      RKADK_LOGE("Malloc thumbnail cell buf failed");
      return -1;
    }
    memcpy(cell->buf, buf, cell->size);
    cell->pts = pts;
    RKADK_MUXER_ThumbCellPush(pstMuxerHandle, cell);
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_CreateThumbList(RKADK_MW_PTR pHandle) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  for (int i = 0; i < (int)pstMuxer->u32StreamCnt; i++) {
    pstMuxerHandle = (MUXER_HANDLE_S *)pstMuxer->pMuxerHandle[i];
    RKADK_MUXER_ThumbListInit(pstMuxerHandle);
  }

  RKADK_LOGI("Create thumbnail list success");
  return 0;
}

RKADK_S32 RKADK_MUXER_ConfigVideoParam(RKADK_U32 chnId, RKADK_MW_PTR pHandle,
                             RKADK_TRACK_VIDEO_SOURCE_INFO_S *pstVideoInfo) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, chnId);
  if (!pstMuxerHandle) {
    // RKADK_LOGE("don't find muxer handle");
    return -1;
  }

  pstMuxerHandle->stVideo.width = pstVideoInfo->u32Width;
  pstMuxerHandle->stVideo.height = pstVideoInfo->u32Height;
  pstMuxerHandle->stVideo.bit_rate = pstVideoInfo->u32BitRate;
  pstMuxerHandle->stVideo.profile = pstVideoInfo->u16Profile;
  pstMuxerHandle->stVideo.level = pstVideoInfo->u16Level;

  switch (pstVideoInfo->enCodecType) {
  case RKADK_CODEC_TYPE_H264:
    memcpy(pstMuxerHandle->stVideo.codec, "H.264", strlen("H.264"));
    break;
  case RKADK_CODEC_TYPE_H265:
    memcpy(pstMuxerHandle->stVideo.codec, "H.265", strlen("H.265"));
    break;
  default:
    RKADK_LOGE("not support enCodecType: %d", pstVideoInfo->enCodecType);
    return -1;
  }

  return 0;
}

RKADK_S32 RKADK_MUXER_Reset(RKADK_MW_PTR pHandle, RKADK_U32 chnId) {
  MUXER_HANDLE_S *pstMuxerHandle = NULL;
  RKADK_MUXER_HANDLE_S *pstMuxer = NULL;
  char name[256];

  RKADK_CHECK_POINTER(pHandle, RKADK_FAILURE);

  pstMuxer = (RKADK_MUXER_HANDLE_S *)pHandle;
  RKADK_CHECK_STREAM_CNT(pstMuxer->u32StreamCnt);

  RKADK_LOGI("Reset Muxer[%d] Start...", chnId);

  pstMuxerHandle = RKADK_MUXER_FindHandle(pstMuxer, chnId);

  if (!pstMuxerHandle) {
    RKADK_LOGD("Muxer Handle is NULL");
    return -1;
  }

  RKADK_THREAD_SetExit(pstMuxerHandle->pThread);

  RKADK_SIGNAL_Give(pstMuxerHandle->pSignal);

  // Destroy thread
  RKADK_THREAD_Destory(pstMuxerHandle->pThread);
  pstMuxerHandle->pThread = NULL;

  snprintf(name, sizeof(name), "Muxer_%d", pstMuxerHandle->vChnId);
  pstMuxerHandle->pThread = RKADK_THREAD_Create(RKADK_MUXER_Proc, pstMuxerHandle, name);
  if (!pstMuxerHandle->pThread) {
    RKADK_LOGE("RKADK_THREAD_Create failed");
    return -1;
  }

  RKADK_LOGI("Reset Muxer[%d] End...", chnId);

  return 0;
}
