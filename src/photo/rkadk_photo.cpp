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

#include "rkadk_photo.h"
#include "rkadk_log.h"
#include "rkadk_media_comm.h"
#include "rkadk_param.h"
#include "rkmedia_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  bool init;
  RKADK_PHOTO_DATA_RECV_FN_PTR pDataRecvFn;
} PHOTO_INFO_S;

static PHOTO_INFO_S g_stPhotoInfo[RKADK_MAX_SENSOR_CNT] = {0};

static void RKADK_PHOTO_VencOutCb0(MEDIA_BUFFER mb) {
  if (!g_stPhotoInfo[0].pDataRecvFn) {
    RKADK_LOGW("RKADK_PHOTO_PACKET_RECV_FN_PTR unregistered");
    RK_MPI_MB_ReleaseBuffer(mb);
    return;
  }

  g_stPhotoInfo[0].pDataRecvFn((RKADK_U8 *)RK_MPI_MB_GetPtr(mb),
                               RK_MPI_MB_GetSize(mb));
  RK_MPI_MB_ReleaseBuffer(mb);
}

static void RKADK_PHOTO_VencOutCb1(MEDIA_BUFFER mb) {
  if (!g_stPhotoInfo[1].pDataRecvFn) {
    RKADK_LOGW("RKADK_PHOTO_PACKET_RECV_FN_PTR unregistered");
    RK_MPI_MB_ReleaseBuffer(mb);
    return;
  }

  g_stPhotoInfo[1].pDataRecvFn((RKADK_U8 *)RK_MPI_MB_GetPtr(mb),
                               RK_MPI_MB_GetSize(mb));
  RK_MPI_MB_ReleaseBuffer(mb);
}

static void RKADK_PHOTO_SetVencAttr(RKADK_U32 u32CamID,
                                    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg,
                                    VENC_CHN_ATTR_S *pstVencAttr) {
  memset(pstVencAttr, 0, sizeof(VENC_CHN_ATTR_S));
  pstVencAttr->stVencAttr.enType = RK_CODEC_TYPE_JPEG;
  pstVencAttr->stVencAttr.imageType = pstPhotoCfg->vi_attr.stChnAttr.enPixFmt;
  pstVencAttr->stVencAttr.u32PicWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32PicHeight = pstPhotoCfg->image_height;
  pstVencAttr->stVencAttr.u32VirWidth = pstPhotoCfg->image_width;
  pstVencAttr->stVencAttr.u32VirHeight = pstPhotoCfg->image_height;
  pstVencAttr->stVencAttr.stAttrJpege.u32ZoomWidth = 0;
  pstVencAttr->stVencAttr.stAttrJpege.u32ZoomHeight = 0;
  pstVencAttr->stVencAttr.stAttrJpege.u32ZoomVirWidth = 0;
  pstVencAttr->stVencAttr.stAttrJpege.u32ZoomVirHeight = 0;
}

static void RKADK_PHOTO_SetChn(RKADK_U32 u32CamID,
                               RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg,
                               MPP_CHN_S *pstViChn, MPP_CHN_S *pstVencChn) {
  pstViChn->enModId = RK_ID_VI;
  pstViChn->s32DevId = 0;
  pstViChn->s32ChnId = pstPhotoCfg->vi_attr.u32ViChn;

  pstVencChn->enModId = RK_ID_VENC;
  pstVencChn->s32DevId = 0;
  pstVencChn->s32ChnId = pstPhotoCfg->venc_chn;
}

RKADK_S32 RKADK_PHOTO_Init(RKADK_PHOTO_ATTR_S *pstPhotoAttr) {
  int ret;
  MPP_CHN_S stViChn;
  MPP_CHN_S stVencChn;
  VENC_CHN_ATTR_S stVencAttr;

  RKADK_CHECK_POINTER(pstPhotoAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstPhotoAttr->u32CamID, RKADK_FAILURE);

  PHOTO_INFO_S *pstPhotoInfo = &g_stPhotoInfo[pstPhotoAttr->u32CamID];
  if (pstPhotoInfo->init) {
    RKADK_LOGI("photo: camera[%d] has been init", pstPhotoAttr->u32CamID);
    return 0;
  }

  pstPhotoInfo->pDataRecvFn = pstPhotoAttr->pfnPhotoDataProc;

  RKADK_PARAM_Init();
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      RKADK_PARAM_GetPhotoCfg(pstPhotoAttr->u32CamID);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  RK_MPI_SYS_Init();

  RKADK_PHOTO_SetChn(pstPhotoAttr->u32CamID, pstPhotoCfg, &stViChn, &stVencChn);

  // Create VI
  ret = RKADK_MPI_VI_Init(pstPhotoAttr->u32CamID, stViChn.s32ChnId,
                          &(pstPhotoCfg->vi_attr.stChnAttr));
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_Init failed, ret = %d", ret);
    return ret;
  }

  // Create VENC
  RKADK_PHOTO_SetVencAttr(pstPhotoAttr->u32CamID, pstPhotoCfg, &stVencAttr);
  ret = RK_MPI_VENC_CreateChn(stVencChn.s32ChnId, &stVencAttr);
  if (ret) {
    RKADK_LOGE("Create Venc failed! ret=%d", ret);
    goto failed;
  }

  if (pstPhotoAttr->u32CamID == 0)
    ret = RK_MPI_SYS_RegisterOutCb(&stVencChn, RKADK_PHOTO_VencOutCb0);
  else
    ret = RK_MPI_SYS_RegisterOutCb(&stVencChn, RKADK_PHOTO_VencOutCb1);
  if (ret) {
    RKADK_LOGE("Register Output callback failed! ret=%d", ret);
    goto failed;
  }

  // The encoder defaults to continuously receiving frames from the previous
  // stage. Before performing the bind operation, set s32RecvPicNum to 0 to
  // make the encoding enter the pause state.
  VENC_RECV_PIC_PARAM_S stRecvParam;
  stRecvParam.s32RecvPicNum = 0;
  ret = RK_MPI_VENC_StartRecvFrame(stVencChn.s32ChnId, &stRecvParam);
  if (ret) {
    RKADK_LOGE("RK_MPI_VENC_StartRecvFrame failed = %d", ret);
    goto failed;
  }

  // Bind
  ret = RK_MPI_SYS_Bind(&stViChn, &stVencChn);
  if (ret) {
    RKADK_LOGE("Bind VI[%d] to VENC[%d]::JPEG failed! ret=%d", stViChn.s32ChnId,
               stVencChn.s32ChnId, ret);
    goto failed;
  }

  pstPhotoInfo->init = true;
  return 0;

failed:
  RKADK_LOGE("failed");
  RK_MPI_VENC_DestroyChn(stVencChn.s32ChnId);
  RKADK_MPI_VI_DeInit(pstPhotoAttr->u32CamID, stViChn.s32ChnId);
  return ret;
}

RKADK_S32 RKADK_PHOTO_DeInit(RKADK_U32 u32CamID) {
  int ret;
  MPP_CHN_S stViChn;
  MPP_CHN_S stVencChn;

  RKADK_CHECK_CAMERAID(u32CamID, RKADK_FAILURE);

  PHOTO_INFO_S *pstPhotoInfo = &g_stPhotoInfo[u32CamID];
  if (!pstPhotoInfo->init) {
    RKADK_LOGI("photo: camera[%d] has been deinit", u32CamID);
    return 0;
  }

  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamID);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  RKADK_PHOTO_SetChn(u32CamID, pstPhotoCfg, &stViChn, &stVencChn);

  // UnBind
  ret = RK_MPI_SYS_UnBind(&stViChn, &stVencChn);
  if (ret) {
    RKADK_LOGE("UnBind VI[%d] to VENC[%d]::JPEG failed! ret=%d",
               stViChn.s32ChnId, stVencChn.s32ChnId, ret);
    return -1;
  }

  // Destory VENC
  ret = RK_MPI_VENC_DestroyChn(stVencChn.s32ChnId);
  if (ret) {
    RKADK_LOGE("Destory VENC[%d] failed, ret=%d", stVencChn.s32ChnId, ret);
    return -1;
  }

  // Destory VI
  ret = RKADK_MPI_VI_DeInit(u32CamID, stViChn.s32ChnId);
  if (ret) {
    RKADK_LOGE("RKADK_MPI_VI_DeInit failed, ret=%d", ret);
    return -1;
  }

  pstPhotoInfo->pDataRecvFn = NULL;
  pstPhotoInfo->init = false;
  return 0;
}

RKADK_S32 RKADK_PHOTO_TakePhoto(RKADK_PHOTO_ATTR_S *pstPhotoAttr) {
  VENC_RECV_PIC_PARAM_S stRecvParam;

  RKADK_CHECK_POINTER(pstPhotoAttr, RKADK_FAILURE);
  RKADK_CHECK_CAMERAID(pstPhotoAttr->u32CamID, RKADK_FAILURE);

  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      RKADK_PARAM_GetPhotoCfg(pstPhotoAttr->u32CamID);
  if (!pstPhotoCfg) {
    RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    return -1;
  }

  if (pstPhotoAttr->enPhotoType == RKADK_PHOTO_TYPE_LAPSE) {
    // TODO
    RKADK_LOGI("nonsupport photo type = %d", pstPhotoAttr->enPhotoType);
    return -1;
  }

  PHOTO_INFO_S *pstPhotoInfo = &g_stPhotoInfo[pstPhotoAttr->u32CamID];
  if (!pstPhotoInfo->init) {
    RKADK_LOGI("photo: camera[%d] isn't init", pstPhotoAttr->u32CamID);
    return -1;
  }

  if (pstPhotoAttr->enPhotoType == RKADK_PHOTO_TYPE_SINGLE)
    stRecvParam.s32RecvPicNum = 1;
  else
    stRecvParam.s32RecvPicNum =
        pstPhotoAttr->unPhotoTypeAttr.stMultipleAttr.s32Count;

  return RK_MPI_VENC_StartRecvFrame(pstPhotoCfg->venc_chn, &stRecvParam);
}
