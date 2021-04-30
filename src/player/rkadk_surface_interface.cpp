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

#include "rkadk_surface_interface.h"
#include "RTMediaBuffer.h"
#include "RTMediaData.h"
#include "rk_common.h"
#include "rk_mpi_vo.h"
#include "rkadk_common.h"
#include "rt_mem.h"
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

#define VOP_LAYER_CLUSTER_0 0
#define VOP_LAYER_CLUSTER_1 1

static RKADK_S32 RKADK_VO_GetPictureData(VO_FRAME_INFO_S *pstFrameInfo,
                                         RTMediaBuffer *buffer) {
  RKADK_U32 u32Size;

  RKADK_CHECK_POINTER(pstFrameInfo, RKADK_FAILURE);
  RKADK_CHECK_POINTER(buffer, RKADK_FAILURE);

  u32Size = buffer->getLength();
  memcpy(pstFrameInfo->pData, buffer->getData(), u32Size);

  return RKADK_SUCCESS;
}

static RKADK_S32 RKADK_VO_CreateGFXData(RKADK_U32 u32Width, RKADK_U32 u32Height,
                                        RKADK_U32 foramt, RKADK_U32 value,
                                        RKADK_VOID **pMblk,
                                        RTMediaBuffer *buffer) {
  VO_FRAME_INFO_S stFrameInfo;
  RKADK_U32 u32BuffSize;

  RKADK_CHECK_POINTER(pMblk, RKADK_FAILURE);
  RKADK_CHECK_POINTER(buffer, RKADK_FAILURE);

  u32BuffSize =
      RK_MPI_VO_CreateGraphicsFrameBuffer(u32Width, u32Height, foramt, pMblk);
  if (u32BuffSize == 0) {
    RKADK_LOGD("can not create gfx buffer");
    return RKADK_FAILURE;
  }

  RK_MPI_VO_GetFrameInfo(*pMblk, &stFrameInfo);
  RKADK_VO_GetPictureData(&stFrameInfo, buffer);

  return RKADK_SUCCESS;
}

static RKADK_S32
RKADK_VO_StartLayer(VO_LAYER voLayer,
                    const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr) {
  RKADK_S32 s32Ret = RKADK_SUCCESS;

  RKADK_CHECK_POINTER(pstLayerAttr, RKADK_FAILURE);

  s32Ret = RK_MPI_VO_SetLayerAttr(voLayer, pstLayerAttr);
  if (s32Ret) {
    RKADK_LOGD("Set layer attribute failed");
    return s32Ret;
  }

  s32Ret = RK_MPI_VO_EnableLayer(voLayer);
  if (s32Ret) {
    RKADK_LOGD("Enable layer failed");
    return s32Ret;
  }

  return s32Ret;
}

static RKADK_S32 RKADK_VO_StartDev(VO_DEV voDev, VO_PUB_ATTR_S *pstPubAttr) {
  RKADK_S32 s32Ret = RKADK_SUCCESS;

  RKADK_CHECK_POINTER(pstPubAttr, RKADK_FAILURE);

  s32Ret = RK_MPI_VO_SetPubAttr(voDev, pstPubAttr);
  if (s32Ret) {
    RKADK_LOGD("Set public attribute failed");
    return s32Ret;
  }

  s32Ret = RK_MPI_VO_Enable(voDev);
  if (s32Ret) {
    RKADK_LOGD("VO enable failed");
    return s32Ret;
  }

  return s32Ret;
}

static RKADK_S32 RKADK_VO_StopLayer(VO_LAYER voLayer) {
  return RK_MPI_VO_DisableLayer(voLayer);
}

static RKADK_S32 RKADK_VO_StopDev(VO_DEV voDev) {

  return RK_MPI_VO_Disable(voDev);
}

static PIXEL_FORMAT_E RKADK_FmtToRtfmt(RKADK_PLAYER_VO_FORMAT_E format) {
  PIXEL_FORMAT_E rtfmt;

  switch (format) {
  case VO_FORMAT_ARGB8888:
    rtfmt = RK_FMT_BGRA8888;
    break;
  case VO_FORMAT_ABGR8888:
    rtfmt = RK_FMT_RGBA8888;
    break;
  case VO_FORMAT_RGB888:
    rtfmt = RK_FMT_BGR888;
    break;
  case VO_FORMAT_BGR888:
    rtfmt = RK_FMT_RGB888;
    break;
  case VO_FORMAT_ARGB1555:
    rtfmt = RK_FMT_BGRA5551;
    break;
  case VO_FORMAT_ABGR1555:
    rtfmt = RK_FMT_RGBA5551;
    break;
  case VO_FORMAT_NV12:
    rtfmt = RK_FMT_YUV420SP;
    break;
  case VO_FORMAT_NV21:
    rtfmt = RK_FMT_YUV420SP_VU;
    break;
  default:
    RKADK_LOGW("invalid format: %d", format);
    rtfmt = RK_FMT_BUTT;
  }

  return rtfmt;
}

static RKADK_S32 RKADK_VO_SetRtSyncInfo(VO_SYNC_INFO_S *pstSyncInfo,
                                        VIDEO_FRAMEINFO_S *pstFrmInfo) {
  RKADK_CHECK_POINTER(pstSyncInfo, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstFrmInfo, RKADK_FAILURE);

  pstSyncInfo->bIdv = pstFrmInfo->stSyncInfo.bIdv;
  pstSyncInfo->bIhs = pstFrmInfo->stSyncInfo.bIhs;
  pstSyncInfo->bIvs = pstFrmInfo->stSyncInfo.bIvs;
  pstSyncInfo->bSynm = pstFrmInfo->stSyncInfo.bSynm;
  pstSyncInfo->bIop = pstFrmInfo->stSyncInfo.bIop;
  pstSyncInfo->u16FrameRate = (pstFrmInfo->stSyncInfo.u16FrameRate > 0)
                                  ? pstFrmInfo->stSyncInfo.u16FrameRate
                                  : 60;
  pstSyncInfo->u16PixClock = (pstFrmInfo->stSyncInfo.u16PixClock > 0)
                                 ? pstFrmInfo->stSyncInfo.u16PixClock
                                 : 65000;
  pstSyncInfo->u16Hact = (pstFrmInfo->stSyncInfo.u16Hact > 0)
                             ? pstFrmInfo->stSyncInfo.u16Hact
                             : 1200;
  pstSyncInfo->u16Hbb =
      (pstFrmInfo->stSyncInfo.u16Hbb > 0) ? pstFrmInfo->stSyncInfo.u16Hbb : 24;
  pstSyncInfo->u16Hfb =
      (pstFrmInfo->stSyncInfo.u16Hfb > 0) ? pstFrmInfo->stSyncInfo.u16Hfb : 240;
  pstSyncInfo->u16Hpw =
      (pstFrmInfo->stSyncInfo.u16Hpw > 0) ? pstFrmInfo->stSyncInfo.u16Hpw : 136;
  pstSyncInfo->u16Hmid =
      (pstFrmInfo->stSyncInfo.u16Hmid > 0) ? pstFrmInfo->stSyncInfo.u16Hmid : 0;
  pstSyncInfo->u16Vact = (pstFrmInfo->stSyncInfo.u16Vact > 0)
                             ? pstFrmInfo->stSyncInfo.u16Vact
                             : 1200;
  pstSyncInfo->u16Vbb =
      (pstFrmInfo->stSyncInfo.u16Vbb > 0) ? pstFrmInfo->stSyncInfo.u16Vbb : 200;
  pstSyncInfo->u16Vfb =
      (pstFrmInfo->stSyncInfo.u16Vfb > 0) ? pstFrmInfo->stSyncInfo.u16Vfb : 194;
  pstSyncInfo->u16Vpw =
      (pstFrmInfo->stSyncInfo.u16Vpw > 0) ? pstFrmInfo->stSyncInfo.u16Vpw : 6;

  return 0;
}

static RKADK_S32 RKADK_VO_SetDispRect(VO_VIDEO_LAYER_ATTR_S *pstLayerAttr,
                                      VIDEO_FRAMEINFO_S *pstFrmInfo) {
  RKADK_CHECK_POINTER(pstLayerAttr, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pstFrmInfo, RKADK_FAILURE);

  if (0 < pstFrmInfo->u32FrmInfoS32x)
    pstLayerAttr->stDispRect.s32X = pstFrmInfo->u32FrmInfoS32x;
  else
    RKADK_LOGD("positive x less than zero, use default value");

  if (0 < pstFrmInfo->u32FrmInfoS32y)
    pstLayerAttr->stDispRect.s32Y = pstFrmInfo->u32FrmInfoS32y;
  else
    RKADK_LOGD("positive y less than zero, use default value");

  if (0 < pstFrmInfo->u32DispWidth &&
      pstFrmInfo->u32DispWidth < pstFrmInfo->u32ImgWidth)
    pstLayerAttr->stDispRect.u32Width = pstFrmInfo->u32DispWidth;
  else
    RKADK_LOGD("DispWidth use default value");

  if (0 < pstFrmInfo->u32DispHeight &&
      pstFrmInfo->u32DispHeight < pstFrmInfo->u32ImgWidth)
    pstLayerAttr->stDispRect.u32Height = pstFrmInfo->u32DispHeight;
  else
    RKADK_LOGD("DispHeight use default value");

  if (0 < pstFrmInfo->u32ImgWidth)
    pstLayerAttr->stImageSize.u32Width = pstFrmInfo->u32ImgWidth;
  else
    RKADK_LOGD("ImgWidth, use default value");

  if (0 < pstFrmInfo->u32ImgWidth)
    pstLayerAttr->stImageSize.u32Width = pstFrmInfo->u32ImgWidth;
  else
    RKADK_LOGD("ImgHeight use default value");

  return 0;
}

RKADKSurfaceInterface::RKADKSurfaceInterface() {
  pstFrmInfo = rt_malloc(VIDEO_FRAMEINFO_S);
  rt_memset(pstFrmInfo, 0, sizeof(VIDEO_FRAMEINFO_S));
  s32Flag = 0;
}
RKADKSurfaceInterface::~RKADKSurfaceInterface() { rt_safe_free(pstFrmInfo); }

INT32 RKADKSurfaceInterface::dequeueBuffer(void **buf) {
  VIDEO_FRAME_INFO_S stVFrame;
  VO_LAYER voLayer;
  int ret;

  RKADK_CHECK_POINTER(*buf, RKADK_FAILURE);

  switch (pstFrmInfo->u32VoDev) {
  case VO_DEV_HD0:
    voLayer = VOP_LAYER_CLUSTER_0;
    break;
  case VO_DEV_HD1:
    voLayer = VOP_LAYER_CLUSTER_1;
    break;
  default:
    voLayer = VOP_LAYER_CLUSTER_0;
    break;
  }

  if (!s32Flag) {
    RTFrame frame = {0};
    VO_PUB_ATTR_S stVoPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    RKADK_VOID *pMblk = RKADK_NULL;

    rt_mediabuf_goto_frame(reinterpret_cast<RTMediaBuffer *>(*buf), &frame);

    ret = RKADK_VO_CreateGFXData(
        frame.mFrameW, frame.mFrameH, RKADK_FmtToRtfmt(pstFrmInfo->u32VoFormat),
        0, &pMblk, reinterpret_cast<RTMediaBuffer *>(*buf));
    if (ret == RKADK_FAILURE) {
      RKADK_LOGD("RKADK_VO_CreateGFXData failed(%d)", ret);
      return ret;
    }

    /* Bind Layer */
    VO_LAYER_MODE_E mode;
    switch (pstFrmInfo->u32VoLayerMode) {
    case 0:
      mode = VO_LAYER_MODE_CURSOR;
      break;
    case 1:
      mode = VO_LAYER_MODE_GRAPHIC;
      break;
    case 2:
      mode = VO_LAYER_MODE_VIDEO;
      break;
    default:
      mode = VO_LAYER_MODE_VIDEO;
    }
    ret = RK_MPI_VO_BindLayer(voLayer, pstFrmInfo->u32VoDev, mode);
    if (ret) {
      RKADK_LOGD("RK_MPI_VO_BindLayer failed(%d)", ret);
      return ret;
    }

    /* Enable VO Device */
    switch (pstFrmInfo->u32EnIntfType) {
    case DISPLAY_TYPE_HDMI:
      stVoPubAttr.enIntfType = VO_INTF_HDMI;
      break;
    case DISPLAY_TYPE_EDP:
      stVoPubAttr.enIntfType = VO_INTF_EDP;
      break;
    case DISPLAY_TYPE_VGA:
      stVoPubAttr.enIntfType = VO_INTF_VGA;
      break;
    case DISPLAY_TYPE_MIPI:
      stVoPubAttr.enIntfType = VO_INTF_MIPI;
      break;
    default:
      stVoPubAttr.enIntfType = VO_INTF_HDMI;
      RKADK_LOGD("option not set ,use HDMI default");
    }

    stVoPubAttr.enIntfSync = pstFrmInfo->enIntfSync;
    if (VO_OUTPUT_USER == stVoPubAttr.enIntfSync)
      RKADK_VO_SetRtSyncInfo(&stVoPubAttr.stSyncInfo, pstFrmInfo);

    ret = RKADK_VO_StartDev(pstFrmInfo->u32VoDev, &stVoPubAttr);
    if (ret) {
      RKADK_LOGD("RKADK_VO_StartDev failed(%d)", ret);
      return ret;
    }

    RKADK_LOGD("StartDev");

    /* Enable Layer */
    stLayerAttr.enPixFormat = RKADK_FmtToRtfmt(pstFrmInfo->u32VoFormat);
    stLayerAttr.stDispRect.s32X = 0;
    stLayerAttr.stDispRect.s32Y = 0;
    stLayerAttr.stDispRect.u32Width = frame.mFrameW / 2;
    stLayerAttr.stDispRect.u32Height = frame.mFrameH / 2;
    stLayerAttr.stImageSize.u32Width = frame.mFrameW;
    stLayerAttr.stImageSize.u32Height = frame.mFrameH;
    RKADK_VO_SetDispRect(&stLayerAttr, pstFrmInfo);

    stLayerAttr.u32DispFrmRt = pstFrmInfo->u32DispFrmRt;
    ret = RKADK_VO_StartLayer(voLayer, &stLayerAttr);
    if (ret) {
      RKADK_LOGD("RKADK_VO_StartLayer failed(%d)", ret);
      return ret;
    }

    /* Set Layer */
    stVFrame.stVFrame.pMbBlk = pMblk;
    pCbMblk = pMblk;
    ret = RK_MPI_VO_SendLayerFrame(voLayer, &stVFrame);
    if (ret) {
      RKADK_LOGD("RK_MPI_VO_SendLayerFrame failed(%d)", ret);
      return ret;
    }

    s32Flag = 1;
  } else {
    VO_FRAME_INFO_S stFrameInfo;

    memset(&stFrameInfo, 0, sizeof(VO_FRAME_INFO_S));
    ret = RK_MPI_VO_GetFrameInfo(pCbMblk, &stFrameInfo);
    if (ret) {
      RKADK_LOGD("RK_MPI_VO_GetFrameInfo failed(%d)", ret);
      return ret;
    }

    ret = RKADK_VO_GetPictureData(&stFrameInfo,
                                  reinterpret_cast<RTMediaBuffer *>(*buf));
    if (ret) {
      RKADK_LOGD("RKADK_VO_GetPictureData failed(%d)", ret);
      return ret;
    }

    stVFrame.stVFrame.pMbBlk = pCbMblk;
    ret = RK_MPI_VO_SendLayerFrame(voLayer, &stVFrame);
    if (ret) {
      RKADK_LOGD("RK_MPI_VO_SendLayerFrame failed(%d)", ret);
      return ret;
    }
  }

  return RKADK_SUCCESS;
}
