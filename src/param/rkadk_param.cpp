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

#include "rkadk_param.h"
#include "rkadk_param_map.h"

typedef enum {
  RKADK_RECORD_0 = 0,
  RKADK_RECORD_1,
  RKADK_STREAM,
  RKADK_PHOTO,
} RKADK_PARAM_WODR_MODE;

/** parameter context */
static RKADK_PARAM_CONTEXT_S g_stPARAMCtx = {
  .bInit = false,
  .mutexLock = PTHREAD_MUTEX_INITIALIZER,
  .stCfg = {0}
};

static RKADK_S32 RKADK_PARAM_SaveViCfg(const char *path, RKADK_U32 viIndex,
                                       RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_PARAM_MEDIA_CFG_S *pstMediaCfg = NULL;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);
  pstMediaCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId];

  switch (viIndex) {
  case 0:
    ret = RKADK_Struct2Ini(
        path, &pstMediaCfg->stViCfg[viIndex], g_stViCfgMapTable_0,
        sizeof(g_stViCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
    break;

  case 1:
    ret = RKADK_Struct2Ini(
        path, &pstMediaCfg->stViCfg[viIndex], g_stViCfgMapTable_1,
        sizeof(g_stViCfgMapTable_1) / sizeof(RKADK_SI_CONFIG_MAP_S));
    break;

  case 2:
    ret = RKADK_Struct2Ini(
        path, &pstMediaCfg->stViCfg[viIndex], g_stViCfgMapTable_2,
        sizeof(g_stViCfgMapTable_2) / sizeof(RKADK_SI_CONFIG_MAP_S));
    break;

  case 3:
    ret = RKADK_Struct2Ini(
        path, &pstMediaCfg->stViCfg[viIndex], g_stViCfgMapTable_3,
        sizeof(g_stViCfgMapTable_3) / sizeof(RKADK_SI_CONFIG_MAP_S));
    break;

  default:
    RKADK_LOGE("Invaild vi index = %d", viIndex);
    return -1;
  }

  if (ret)
    RKADK_LOGE("save vi[%d] param failed", viIndex);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveCommCfg(const char *path) {
  int ret = 0;
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  ret = RKADK_Struct2Ini(path, pstCommCfg, g_stCommCfgMapTable,
                         sizeof(g_stCommCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save common param failed");

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveSensorCfg(const char *path,
                                           RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];

  ret = RKADK_Struct2Ini(path, pstSensorCfg, g_stSensorCfgMapTable_0,
                         sizeof(g_stSensorCfgMapTable_0) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save sensor[%d] param failed", u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SavePhotoCfg(const char *path,
                                          RKADK_U32 u32CamId) {
  int ret;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstPhotoCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;

  ret = RKADK_Struct2Ini(path, pstPhotoCfg, g_stPhotoCfgMapTable_0,
                         sizeof(g_stPhotoCfgMapTable_0) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save sensor[%d] photo param failed", u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveStreamCfg(const char *path,
                                           RKADK_U32 u32CamId) {
  int ret = 0;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstStreamCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg;

  ret = RKADK_Struct2Ini(
      path, &pstStreamCfg->attribute, g_stStreamCfgMapTable_0,
      sizeof(g_stStreamCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret)
    RKADK_LOGE("save sensor[%d] stream param failed", u32CamId);

  return ret;
}

static RKADK_S32 RKADK_PARAM_SaveRecCfg(const char *path, RKADK_U32 u32CamId) {
  int i, ret = 0;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;

  ret = RKADK_Struct2Ini(path, pstRecCfg, g_stRecCfgMapTable_0,
                         sizeof(g_stRecCfgMapTable_0) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("save sensor[%d] record param failed", u32CamId);
    return ret;
  }

  for (i = 0; i < (int)pstRecCfg->file_num; i++) {
    if (i == 0) {
      ret = RKADK_Struct2Ini(
          path, &pstRecCfg->attribute[i], g_stRecCfgMapTable_0_0,
          sizeof(g_stRecCfgMapTable_0_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
    } else {
      ret = RKADK_Struct2Ini(
          path, &pstRecCfg->attribute[i], g_stRecCfgMapTable_0_1,
          sizeof(g_stRecCfgMapTable_0_1) / sizeof(RKADK_SI_CONFIG_MAP_S));
    }

    if (ret) {
      RKADK_LOGE("save sensor[%d] record attribute[%d] param failed", u32CamId,
                 i);
      return ret;
    }
  }

  return 0;
}

static void RKADK_PARAM_Dump() {
  int i, j;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  printf("Common Config\n");
  printf("\tsensor_count: %d\n", pstCfg->stCommCfg.sensor_count);
  printf("\trec_unmute: %d\n", pstCfg->stCommCfg.rec_unmute);
  printf("\tenable_speaker: %d\n", pstCfg->stCommCfg.enable_speaker);
  printf("\tspeaker_volume: %d\n", pstCfg->stCommCfg.speaker_volume);
  printf("\tmic_unmute: %d\n", pstCfg->stCommCfg.mic_unmute);
  printf("\tmic_volume: %d\n", pstCfg->stCommCfg.mic_volume);
  printf("\tosd_time_format: %d\n", pstCfg->stCommCfg.osd_time_format);
  printf("\tshow osd: %d\n", pstCfg->stCommCfg.osd);
  printf("\tboot_sound: %d\n", pstCfg->stCommCfg.boot_sound);

  printf("Audio Config\n");
  printf("\taudio_node: %s\n", pstCfg->stAudioCfg.audio_node);
  printf("\tsample_format: %d\n", pstCfg->stAudioCfg.sample_format);
  printf("\tchannels: %d\n", pstCfg->stAudioCfg.channels);
  printf("\tsamplerate: %d\n", pstCfg->stAudioCfg.samplerate);
  printf("\tsamples_per_frame: %d\n", pstCfg->stAudioCfg.samples_per_frame);
  printf("\tbitrate: %d\n", pstCfg->stAudioCfg.bitrate);

  printf("Thumb Config\n");
  printf("\tthumb_width: %d\n", pstCfg->stThumbCfg.thumb_width);
  printf("\tthumb_height: %d\n", pstCfg->stThumbCfg.thumb_height);
  printf("\tvenc_chn: %d\n", pstCfg->stThumbCfg.venc_chn);

  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    printf("Sensor[%d] Config\n", i);
    printf("\tmax_width[%d] framerate: %d\n", i,
           pstCfg->stSensorCfg[i].max_width);
    printf("\tmax_height[%d] framerate: %d\n", i,
           pstCfg->stSensorCfg[i].max_height);
    printf("\tsensor[%d] framerate: %d\n", i, pstCfg->stSensorCfg[i].framerate);
    printf("\tsensor[%d] enable_record: %d\n", i,
           pstCfg->stSensorCfg[i].enable_record);
    printf("\tsensor[%d] enable_photo: %d\n", i,
           pstCfg->stSensorCfg[i].enable_photo);
    printf("\tsensor[%d] flip: %d\n", i, pstCfg->stSensorCfg[i].flip);
    printf("\tsensor[%d] mirror: %d\n", i, pstCfg->stSensorCfg[i].mirror);
    printf("\tsensor[%d] wdr: %d\n", i, pstCfg->stSensorCfg[i].wdr);
    printf("\tsensor[%d] hdr: %d\n", i, pstCfg->stSensorCfg[i].hdr);
    printf("\tsensor[%d] ldc: %d\n", i, pstCfg->stSensorCfg[i].ldc);
    printf("\tsensor[%d] antifog: %d\n", i, pstCfg->stSensorCfg[i].antifog);

    printf("\tVI Config\n");
    for (j = 0; j < RKADK_ISPP_VI_NODE_CNT; j++) {
      printf("\t\tsensor[%d] VI[%d] device_name: %s\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].device_name);
      printf("\t\tsensor[%d] VI[%d] chn_id: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].chn_id);
      printf("\t\tsensor[%d] VI[%d] buf_cnt: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].buf_cnt);
      printf("\t\tsensor[%d] VI[%d] pix_fmt: %s\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].pix_fmt);
      printf("\t\tsensor[%d] VI[%d] width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].width);
      printf("\t\tsensor[%d] VI[%d] height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stViCfg[j].height);
    }

    printf("\tRecord Config\n");
    printf("\t\tsensor[%d] stRecCfg record_type: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.record_type);
    printf("\t\tsensor[%d] stRecCfg record_time: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.record_time);
    printf("\t\tsensor[%d] stRecCfg splite_time: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.splite_time);
    printf("\t\tsensor[%d] stRecCfg pre_record_time: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.pre_record_time);
    printf("\t\tsensor[%d] stRecCfg lapse_interval: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.lapse_interval);
    printf("\t\tsensor[%d] stRecCfg lapse_multiple: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.lapse_multiple);
    printf("\t\tsensor[%d] stRecCfg file_num: %d\n", i,
           pstCfg->stMediaCfg[i].stRecCfg.file_num);

    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      printf("\t\tsensor[%d] stRecCfg[%d] width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].width);
      printf("\t\tsensor[%d] stRecCfg[%d] height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].height);
      printf("\t\tsensor[%d] stRecCfg[%d] bitrate: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].bitrate);
      printf("\t\tsensor[%d] stRecCfg[%d] gop: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].gop);
      printf("\t\tsensor[%d] stRecCfg[%d] profile: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].profile);
      printf("\t\tsensor[%d] stRecCfg[%d] venc_chn: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_chn);
      printf("\t\tsensor[%d] stRecCfg[%d] codec_type: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].codec_type);
      printf("\t\tsensor[%d] stRecCfg[%d] rc_mode: %s\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].rc_mode);
      printf("\t\tsensor[%d] stRecCfg[%d] first_frame_qp: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.first_frame_qp);
      printf("\t\tsensor[%d] stRecCfg[%d] qp_step: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.qp_step);
      printf("\t\tsensor[%d] stRecCfg[%d] max_qp: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.max_qp);
      printf("\t\tsensor[%d] stRecCfg[%d] min_qp: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.min_qp);
      printf("\t\tsensor[%d] stRecCfg[%d] row_qp_delta_i: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.row_qp_delta_i);
      printf("\t\tsensor[%d] stRecCfg[%d] row_qp_delta_p: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param.row_qp_delta_p);
    }

    printf("\tPhoto Config\n");
    printf("\t\tsensor[%d] stPhotoCfg image_width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.image_width);
    printf("\t\tsensor[%d] stPhotoCfg image_height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.image_height);
    printf("\t\tsensor[%d] stPhotoCfg snap_num: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.snap_num);
    printf("\t\tsensor[%d] stPhotoCfg venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.venc_chn);

    printf("\tStream Config\n");
    printf("\t\tsensor[%d] stStreamCfg width: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.width);
    printf("\t\tsensor[%d] stStreamCfg height: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.height);
    printf("\t\tsensor[%d] stStreamCfg bitrate: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.bitrate);
    printf("\t\tsensor[%d] stStreamCfg gop: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.gop);
    printf("\t\tsensor[%d] stStreamCfg profile: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.profile);
    printf("\t\tsensor[%d] stStreamCfg venc_chn: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_chn);
    printf("\t\tsensor[%d] stStreamCfg codec_type: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.codec_type);
    printf("\t\tsensor[%d] stStreamCfg rc_mode: %s\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.rc_mode);
    printf(
        "\t\tsensor[%d] stStreamCfg first_frame_qp: %d\n", i,
        pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.first_frame_qp);
    printf("\t\tsensor[%d] stStreamCfg qp_step: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.qp_step);
    printf("\t\tsensor[%d] stStreamCfg max_qp: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.max_qp);
    printf("\t\tsensor[%d] stStreamCfg min_qp: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.min_qp);
    printf(
        "\t\tsensor[%d] stStreamCfg row_qp_delta_i: %d\n", i,
        pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.row_qp_delta_i);
    printf(
        "\t\tsensor[%d] stStreamCfg row_qp_delta_p: %d\n", i,
        pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param.row_qp_delta_p);
  }
}

static void RKADK_PARAM_DumpAttr() {
  int i, j;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      printf("\tRec VI Attribute\n");
      printf("\t\tsensor[%d] stRecCfg[%d] pcVideoNode: %s\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.pcVideoNode);
      printf("\t\tsensor[%d] stRecCfg[%d] u32ViChn: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].u32ViChn);
      printf("\t\tsensor[%d] stRecCfg[%d] u32Width: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.u32Width);
      printf("\t\tsensor[%d] stRecCfg[%d] u32Height: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.u32Height);
      printf("\t\tsensor[%d] stRecCfg[%d] u32BufCnt: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.u32BufCnt);
      printf("\t\tsensor[%d] stRecCfg[%d] enPixFmt: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.enPixFmt);
      printf("\t\tsensor[%d] stRecCfg[%d] enBufType: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.enBufType);
      printf("\t\tsensor[%d] stRecCfg[%d] enWorkMode: %d\n", i, j,
             pstCfg->stMediaCfg[i].stRecCfg.vi_attr[j].stChnAttr.enWorkMode);
    }

    printf("\tPhoto VI Attribute\n");
    printf("\t\tsensor[%d] stPhotoCfg pcVideoNode: %s\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.pcVideoNode);
    printf("\t\tsensor[%d] stPhotoCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stPhotoCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.u32Width);
    printf("\t\tsensor[%d] stPhotoCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.u32Height);
    printf("\t\tsensor[%d] stPhotoCfg u32BufCnt: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.u32BufCnt);
    printf("\t\tsensor[%d] stPhotoCfg enPixFmt: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.enPixFmt);
    printf("\t\tsensor[%d] stPhotoCfg enBufType: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.enBufType);
    printf("\t\tsensor[%d] stPhotoCfg enWorkMode: %d\n", i,
           pstCfg->stMediaCfg[i].stPhotoCfg.vi_attr.stChnAttr.enWorkMode);

    printf("\tStream VI Attribute\n");
    printf("\t\tsensor[%d] stStreamCfg pcVideoNode: %s\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.pcVideoNode);
    printf("\t\tsensor[%d] stStreamCfg u32ViChn: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.u32ViChn);
    printf("\t\tsensor[%d] stStreamCfg u32Width: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.u32Width);
    printf("\t\tsensor[%d] stStreamCfg u32Height: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.u32Height);
    printf("\t\tsensor[%d] stStreamCfg u32BufCnt: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.u32BufCnt);
    printf("\t\tsensor[%d] stStreamCfg enPixFmt: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.enPixFmt);
    printf("\t\tsensor[%d] stStreamCfg enBufType: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.enBufType);
    printf("\t\tsensor[%d] stStreamCfg enWorkMode: %d\n", i,
           pstCfg->stMediaCfg[i].stStreamCfg.vi_attr.stChnAttr.enWorkMode);
  }
}

static RKADK_S32 RKADK_PARAM_LoadParam(const char *path) {
  int i, j, ret = 0;
  RKADK_PARAM_CFG_S *pstCfg = &g_stPARAMCtx.stCfg;

  // load common config
  memset(&pstCfg->stCommCfg, 0, sizeof(RKADK_PARAM_COMM_CFG_S));
  ret = RKADK_Ini2Struct(path, &pstCfg->stCommCfg, g_stCommCfgMapTable,
                         sizeof(g_stCommCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("load %s failed", path);
    return ret;
  }

  if (pstCfg->stCommCfg.sensor_count > RKADK_MAX_SENSOR_CNT) {
    RKADK_LOGW("Sensor count(%d) > RKADK_MAX_SENSOR_CNT",
               pstCfg->stCommCfg.sensor_count);
    pstCfg->stCommCfg.sensor_count = RKADK_MAX_SENSOR_CNT;
  }

  // load audio config
  memset(&pstCfg->stAudioCfg, 0, sizeof(RKADK_PARAM_AUDIO_CFG_S));
  ret = RKADK_Ini2Struct(path, &pstCfg->stAudioCfg, g_stAudioCfgMapTable,
                         sizeof(g_stAudioCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("load audio param failed");
    return ret;
  }

  // load thumb config
  memset(&pstCfg->stThumbCfg, 0, sizeof(RKADK_PARAM_THUMB_CFG_S));
  ret = RKADK_Ini2Struct(path, &pstCfg->stThumbCfg, g_stThumbCfgMapTable,
                         sizeof(g_stThumbCfgMapTable) /
                             sizeof(RKADK_SI_CONFIG_MAP_S));
  if (ret) {
    RKADK_LOGE("load thumb param failed");
    return ret;
  }

  // load sensor config
  for (i = 0; i < (int)pstCfg->stCommCfg.sensor_count; i++) {
    memset(&pstCfg->stSensorCfg[i], 0, sizeof(RKADK_PARAM_SENSOR_CFG_S));

    if (i == 0) {
      ret = RKADK_Ini2Struct(
          path, &pstCfg->stSensorCfg[i], g_stSensorCfgMapTable_0,
          sizeof(g_stSensorCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
      if (ret) {
        RKADK_LOGE("load sensor[%d] param failed", i);
        return ret;
      }
    } else {
      RKADK_LOGD("nonsupport sensor(%d)", i);
      return 0;
    }

    for (j = 0; j < RKADK_ISPP_VI_NODE_CNT; j++) {
      memset(&pstCfg->stMediaCfg[i].stViCfg[j], 0,
             sizeof(RKADK_PARAM_VI_CFG_S));
      if (j == 0) {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stViCfg[j], g_stViCfgMapTable_0,
            sizeof(g_stViCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
      } else if (j == 1) {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stViCfg[j], g_stViCfgMapTable_1,
            sizeof(g_stViCfgMapTable_1) / sizeof(RKADK_SI_CONFIG_MAP_S));
      } else if (j == 2) {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stViCfg[j], g_stViCfgMapTable_2,
            sizeof(g_stViCfgMapTable_2) / sizeof(RKADK_SI_CONFIG_MAP_S));
      } else {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stViCfg[j], g_stViCfgMapTable_3,
            sizeof(g_stViCfgMapTable_3) / sizeof(RKADK_SI_CONFIG_MAP_S));
      }

      if (ret) {
        RKADK_LOGE("load sensor[%d] vi[%d] param failed", i, j);
        return ret;
      }
    }

    // load record config
    memset(&pstCfg->stMediaCfg[i].stRecCfg, 0, sizeof(RKADK_PARAM_REC_CFG_S));
    ret = RKADK_Ini2Struct(
        path, &pstCfg->stMediaCfg[i].stRecCfg, g_stRecCfgMapTable_0,
        sizeof(g_stRecCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
    if (ret) {
      RKADK_LOGE("load sensor[%d] record param failed", i);
      return ret;
    }

    if (pstCfg->stMediaCfg[i].stRecCfg.file_num > RECORD_FILE_NUM_MAX) {
      RKADK_LOGW("Sensor[%d] rec file num(%d) > RECORD_FILE_NUM_MAX", i,
                 pstCfg->stMediaCfg[i].stRecCfg.file_num);
      pstCfg->stMediaCfg[i].stRecCfg.file_num = RECORD_FILE_NUM_MAX;
    }

    for (j = 0; j < (int)pstCfg->stMediaCfg[i].stRecCfg.file_num; j++) {
      memset(&pstCfg->stMediaCfg[i].stRecCfg.attribute[j], 0,
             sizeof(RKADK_PARAM_VENC_ATTR_S));
      if (j == 0) {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stRecCfg.attribute[j],
            g_stRecCfgMapTable_0_0,
            sizeof(g_stRecCfgMapTable_0_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
        ret |= RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param,
            g_stRecParamMapTable_0_0,
            sizeof(g_stRecParamMapTable_0_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
      } else {
        ret = RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stRecCfg.attribute[j],
            g_stRecCfgMapTable_0_1,
            sizeof(g_stRecCfgMapTable_0_1) / sizeof(RKADK_SI_CONFIG_MAP_S));
        ret |= RKADK_Ini2Struct(
            path, &pstCfg->stMediaCfg[i].stRecCfg.attribute[j].venc_param,
            g_stRecParamMapTable_0_1,
            sizeof(g_stRecParamMapTable_0_1) / sizeof(RKADK_SI_CONFIG_MAP_S));
      }

      if (ret) {
        RKADK_LOGE("load sensor[%d] record attribute[%d] param failed", i, j);
        return ret;
      }
    }

    // load stream config
    memset(&pstCfg->stMediaCfg[i].stStreamCfg.attribute, 0,
           sizeof(RKADK_PARAM_VENC_ATTR_S));
    ret = RKADK_Ini2Struct(path, &pstCfg->stMediaCfg[i].stStreamCfg.attribute,
                           g_stStreamCfgMapTable_0,
                           sizeof(g_stStreamCfgMapTable_0) /
                               sizeof(RKADK_SI_CONFIG_MAP_S));
    ret |= RKADK_Ini2Struct(
        path, &pstCfg->stMediaCfg[i].stStreamCfg.attribute.venc_param,
        g_stStreamParamMapTable_0,
        sizeof(g_stStreamParamMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
    if (ret) {
      RKADK_LOGE("load sensor[%d] stream param failed", i);
      return ret;
    }

    // load photo config
    memset(&pstCfg->stMediaCfg[i].stPhotoCfg, 0,
           sizeof(RKADK_PARAM_PHOTO_CFG_S));
    ret = RKADK_Ini2Struct(
        path, &pstCfg->stMediaCfg[i].stPhotoCfg, g_stPhotoCfgMapTable_0,
        sizeof(g_stPhotoCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S));
    if (ret) {
      RKADK_LOGE("load sensor[%d] photo param failed", i);
      return ret;
    }
  }

  return 0;
}

static void RKADK_PARAM_UseDefault() {
  // default common config
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  pstCommCfg->sensor_count = 1;
  pstCommCfg->rec_unmute = true;
  pstCommCfg->enable_speaker = true;
  pstCommCfg->speaker_volume = 50;
  pstCommCfg->mic_unmute = true;
  pstCommCfg->mic_volume = 50;
  pstCommCfg->osd_time_format = 0;
  pstCommCfg->osd = true;
  pstCommCfg->boot_sound = true;

  // default audio config
  RKADK_PARAM_AUDIO_CFG_S *pstAudioCfg = &g_stPARAMCtx.stCfg.stAudioCfg;
  memcpy(pstAudioCfg->audio_node, AI_DEVICE_NAME, RKADK_BUFFER_LEN);
  pstAudioCfg->sample_format = AUDIO_SAMPLE_FORMAT;
  pstAudioCfg->channels = AUDIO_CHANNEL;
  pstAudioCfg->samplerate = AUDIO_SAMPLE_RATE;
  pstAudioCfg->samples_per_frame = AUDIO_FRAME_COUNT;
  pstAudioCfg->bitrate = AUDIO_BIT_REAT;

  // default thumb config
  RKADK_PARAM_THUMB_CFG_S *pstThumbCfg = &g_stPARAMCtx.stCfg.stThumbCfg;
  pstThumbCfg->thumb_width = THUMB_WIDTH;
  pstThumbCfg->thumb_height = THUMB_HEIGHT;
  pstThumbCfg->venc_chn = THUMB_VENC_CHN;

  // default sensor.0 config
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg = &g_stPARAMCtx.stCfg.stSensorCfg[0];
  pstSensorCfg->max_width = SENSOR_MAX_WIDTH;
  pstSensorCfg->max_height = SENSOR_MAX_HEIGHT;
  pstSensorCfg->framerate = VIDEO_FRAME_RATE;
  pstSensorCfg->enable_record = true;
  pstSensorCfg->enable_photo = true;
  pstSensorCfg->flip = false;
  pstSensorCfg->mirror = false;
  pstSensorCfg->ldc = 0;
  pstSensorCfg->wdr = 0;
  pstSensorCfg->hdr = 0;
  pstSensorCfg->antifog = 0;

  // default sensor.0.rec config
  RKADK_PARAM_REC_CFG_S *pstRecCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stRecCfg;
  pstRecCfg->record_type = RKADK_REC_TYPE_NORMAL;
  pstRecCfg->record_time = 60;
  pstRecCfg->splite_time = 60;
  pstRecCfg->pre_record_time = 0;
  pstRecCfg->lapse_interval = 60;
  pstRecCfg->lapse_multiple = 30;
  pstRecCfg->file_num = 1;

  // default sensor.0.rec.0 config
  pstRecCfg->attribute[0].width = RECORD_VIDEO_WIDTH;
  pstRecCfg->attribute[0].height = RECORD_VIDEO_HEIGHT;
  pstRecCfg->attribute[0].bitrate = RECORD_VIDEO_WIDTH * RECORD_VIDEO_HEIGHT;
  pstRecCfg->attribute[0].gop = VIDEO_GOP;
  pstRecCfg->attribute[0].profile = VIDEO_PROFILE;
  pstRecCfg->attribute[0].venc_chn = 0;
  pstRecCfg->attribute[0].codec_type = RKADK_CODEC_TYPE_H264;
  memcpy(pstRecCfg->attribute[0].rc_mode, "VBR", RKADK_RC_MODE_LEN);
  pstRecCfg->attribute[0].venc_param.max_qp = 48;
  pstRecCfg->attribute[0].venc_param.min_qp = 8;

  // default sensor.0.photo config
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[0].stPhotoCfg;
  pstPhotoCfg->image_width = PHOTO_VIDEO_WIDTH;
  pstPhotoCfg->image_height = PHOTO_VIDEO_HEIGHT;
  pstPhotoCfg->snap_num = 1;
  pstPhotoCfg->venc_chn = 2;

  // default sensor.0.stream config
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[0].stStreamCfg;
  pstStreamCfg->attribute.width = STREAM_VIDEO_WIDTH;
  pstStreamCfg->attribute.height = STREAM_VIDEO_HEIGHT;
  pstStreamCfg->attribute.bitrate = STREAM_VIDEO_WIDTH * STREAM_VIDEO_HEIGHT;
  pstStreamCfg->attribute.gop = VIDEO_GOP;
  pstStreamCfg->attribute.profile = VIDEO_PROFILE;
  pstStreamCfg->attribute.venc_chn = 3;
  pstStreamCfg->attribute.codec_type = RKADK_CODEC_TYPE_H264;
  memcpy(pstStreamCfg->attribute.rc_mode, "VBR", RKADK_RC_MODE_LEN);
  pstStreamCfg->attribute.venc_param.max_qp = 48;
  pstStreamCfg->attribute.venc_param.min_qp = 8;

  // default vi config
  RKADK_PARAM_VI_CFG_S *pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stViCfg[0];
  pstViCfg->chn_id = 0;
  memcpy(pstViCfg->device_name, "rkispp_m_bypass", RKADK_BUFFER_LEN);
  pstViCfg->buf_cnt = 4;
  pstViCfg->width = RECORD_VIDEO_WIDTH;
  pstViCfg->height = RECORD_VIDEO_HEIGHT;
  memcpy(pstViCfg->pix_fmt, "FBC0", RKADK_VI_PIX_FMT_LEN);

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stViCfg[1];
  pstViCfg->chn_id = 1;
  memcpy(pstViCfg->device_name, "rkispp_scale0", RKADK_BUFFER_LEN);
  pstViCfg->buf_cnt = 4;
  memcpy(pstViCfg->pix_fmt, "NV12", RKADK_VI_PIX_FMT_LEN);

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stViCfg[2];
  pstViCfg->chn_id = 2;
  memcpy(pstViCfg->device_name, "rkispp_scale1", RKADK_BUFFER_LEN);
  pstViCfg->buf_cnt = 2;
  memcpy(pstViCfg->pix_fmt, "NV12", RKADK_VI_PIX_FMT_LEN);

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[0].stViCfg[3];
  pstViCfg->chn_id = 3;
  memcpy(pstViCfg->device_name, "rkispp_scale2", RKADK_BUFFER_LEN);
  pstViCfg->buf_cnt = 4;
  pstViCfg->width = STREAM_VIDEO_WIDTH;
  pstViCfg->height = STREAM_VIDEO_HEIGHT;
  memcpy(pstViCfg->pix_fmt, "NV12", RKADK_VI_PIX_FMT_LEN);
}

static RKADK_S32 RKADK_PARAM_LoadDefault() {
  int ret;
  char buffer[RKADK_BUFFER_LEN];

  ret = RKADK_PARAM_LoadParam(RKADK_DEFPARAM_PATH);
  if (ret) {
    RKADK_LOGE("load default param failed");
    return ret;
  }

  memset(buffer, 0, RKADK_BUFFER_LEN);
  sprintf(buffer, "cp %s %s", RKADK_DEFPARAM_PATH, RKADK_PARAM_PATH);
  system(buffer);

  return 0;
}

static RKADK_S32 RKADK_PARAM_FindViIndex(RKADK_PARAM_WODR_MODE mode,
                                         RKADK_S32 s32CamID, RKADK_U32 width,
                                         RKADK_U32 height) {
  int index = -1;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamID];
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);

  for (index = 0; index < RKADK_ISPP_VI_NODE_CNT; index++) {
    pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stViCfg[index];
    if (pstViCfg->width == width && pstViCfg->height == height)
      return index;
  }

  switch (mode) {
  case RKADK_RECORD_0:
    RKADK_LOGD("Sensor(%d) rec[0](%d*%d) not find matched VI", s32CamID, width,
               height);
    if ((width == pstSensorCfg->max_width) &&
        (width == pstSensorCfg->max_height)) {
      RKADK_LOGD("rec[0] default VI[0]");
      index = 0;
    } else {
      RKADK_LOGD("rec[0] default VI[1]");
      index = 1;
    }
    break;

  case RKADK_RECORD_1:
    RKADK_LOGD("Sensor(%d) rec[1](%d*%d) not find matched VI, default VI[2]",
               s32CamID, width, height);
    index = 2;
    break;

  case RKADK_PHOTO: {
    RKADK_PARAM_VENC_ATTR_S *pstRecAttr =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stRecCfg.attribute[0];
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
        &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stPhotoCfg;

    RKADK_LOGD("Sensor(%d) photo(%d*%d) not find matched VI", s32CamID, width,
               height);
    RKADK_LOGW("Force photo resolution = rec[0] resolution(%d*%d)",
               pstRecAttr->width, pstRecAttr->height);

    pstPhotoCfg->image_width = pstRecAttr->width;
    pstPhotoCfg->image_height = pstRecAttr->height;
    RKADK_PARAM_SavePhotoCfg(RKADK_PARAM_PATH, s32CamID);

    width = pstRecAttr->width;
    height = pstRecAttr->height;
    if ((pstRecAttr->width == pstSensorCfg->max_width) &&
        (pstRecAttr->height == pstSensorCfg->max_height))
      index = 0;
    else
      index = 1;
    break;
  }

  case RKADK_STREAM:
    RKADK_LOGD("Sensor(%d) stream(%d*%d) not find matched VI, default VI[3]",
               s32CamID, width, height);
    index = 3;
    break;

  default:
    RKADK_LOGE("Invaild mode = %d", mode);
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stViCfg[index];
  pstViCfg->width = width;
  pstViCfg->height = height;

  if (!strcmp(pstViCfg->device_name, "rkispp_scale0")) {
    if ((pstViCfg->width != pstSensorCfg->max_width) &&
        (pstViCfg->width > 2000)) {
      RKADK_LOGW("rkispp_scale0 resolution(%d * %d) > 2K, default NV16",
                 pstViCfg->width, pstViCfg->height);
      memcpy(pstViCfg->pix_fmt, "NV16", RKADK_VI_PIX_FMT_LEN);
    }
  }

  RKADK_PARAM_SaveViCfg(RKADK_PARAM_PATH, index, s32CamID);
  return index;
}

static IMAGE_TYPE_E RKADK_PARAM_GetPixFmt(char *pixFmt) {
  IMAGE_TYPE_E enPixFmt = IMAGE_TYPE_UNKNOW;

  RKADK_CHECK_POINTER(pixFmt, IMAGE_TYPE_UNKNOW);

  if (!strcmp(pixFmt, "NV12"))
    enPixFmt = IMAGE_TYPE_NV12;
  else if (!strcmp(pixFmt, "NV16"))
    enPixFmt = IMAGE_TYPE_NV16;
  else if (!strcmp(pixFmt, "YUYV"))
    enPixFmt = IMAGE_TYPE_YUYV422;
  else if (!strcmp(pixFmt, "FBC0"))
    enPixFmt = IMAGE_TYPE_FBC0;
  else if (!strcmp(pixFmt, "FBC2"))
    enPixFmt = IMAGE_TYPE_FBC2;

  return enPixFmt;
}

static RKADK_S32 RKADK_PARAM_SetStreamAttr(RKADK_S32 s32CamID) {
  int index;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stStreamCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);

  index = RKADK_PARAM_FindViIndex(RKADK_STREAM, s32CamID,
                                  pstStreamCfg->attribute.width,
                                  pstStreamCfg->attribute.height);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stViCfg[index];
  pstStreamCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  pstStreamCfg->vi_attr.stChnAttr.pcVideoNode = pstViCfg->device_name;
  pstStreamCfg->vi_attr.stChnAttr.u32Width = pstViCfg->width;
  pstStreamCfg->vi_attr.stChnAttr.u32Height = pstViCfg->height;
  pstStreamCfg->vi_attr.stChnAttr.u32BufCnt = pstViCfg->buf_cnt;
  pstStreamCfg->vi_attr.stChnAttr.enPixFmt =
      RKADK_PARAM_GetPixFmt(pstViCfg->pix_fmt);
  pstStreamCfg->vi_attr.stChnAttr.enBufType = VI_CHN_BUF_TYPE_MMAP;
  pstStreamCfg->vi_attr.stChnAttr.enWorkMode = VI_WORK_MODE_NORMAL;

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetPhotoAttr(RKADK_S32 s32CamID) {
  int index;
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stPhotoCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);

  index =
      RKADK_PARAM_FindViIndex(RKADK_PHOTO, s32CamID, pstPhotoCfg->image_width,
                              pstPhotoCfg->image_height);
  if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
    RKADK_LOGE("not find match vi index");
    return -1;
  }

  pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stViCfg[index];
  pstPhotoCfg->vi_attr.u32ViChn = pstViCfg->chn_id;
  pstPhotoCfg->vi_attr.stChnAttr.pcVideoNode = pstViCfg->device_name;
  pstPhotoCfg->vi_attr.stChnAttr.u32Width = pstViCfg->width;
  pstPhotoCfg->vi_attr.stChnAttr.u32Height = pstViCfg->height;
  pstPhotoCfg->vi_attr.stChnAttr.u32BufCnt = pstViCfg->buf_cnt;
  pstPhotoCfg->vi_attr.stChnAttr.enPixFmt =
      RKADK_PARAM_GetPixFmt(pstViCfg->pix_fmt);
  pstPhotoCfg->vi_attr.stChnAttr.enBufType = VI_CHN_BUF_TYPE_MMAP;
  pstPhotoCfg->vi_attr.stChnAttr.enWorkMode = VI_WORK_MODE_NORMAL;

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetRecAttr(RKADK_S32 s32CamID) {
  int i, index;
  RKADK_PARAM_WODR_MODE mode;
  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stRecCfg;
  RKADK_PARAM_VI_CFG_S *pstViCfg = NULL;

  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);

  for (i = 0; i < (int)pstRecCfg->file_num; i++) {
    if (i == 0)
      mode = RKADK_RECORD_0;
    else
      mode = RKADK_RECORD_1;

    index =
        RKADK_PARAM_FindViIndex(mode, s32CamID, pstRecCfg->attribute[i].width,
                                pstRecCfg->attribute[i].height);
    if (index < 0 || index >= RKADK_ISPP_VI_NODE_CNT) {
      RKADK_LOGE("not find match vi index");
      return -1;
    }

    pstViCfg = &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stViCfg[index];
    pstRecCfg->vi_attr[i].u32ViChn = pstViCfg->chn_id;
    pstRecCfg->vi_attr[i].stChnAttr.pcVideoNode = pstViCfg->device_name;
    pstRecCfg->vi_attr[i].stChnAttr.u32Width = pstViCfg->width;
    pstRecCfg->vi_attr[i].stChnAttr.u32Height = pstViCfg->height;
    pstRecCfg->vi_attr[i].stChnAttr.u32BufCnt = pstViCfg->buf_cnt;
    pstRecCfg->vi_attr[i].stChnAttr.enPixFmt =
        RKADK_PARAM_GetPixFmt(pstViCfg->pix_fmt);
    pstRecCfg->vi_attr[i].stChnAttr.enBufType = VI_CHN_BUF_TYPE_MMAP;
    pstRecCfg->vi_attr[i].stChnAttr.enWorkMode = VI_WORK_MODE_NORMAL;
  }

  return 0;
}

static RKADK_S32 RKADK_PARAM_SetMediaAttr() {
  int i, ret = 0;

  for (i = 0; i < (int)g_stPARAMCtx.stCfg.stCommCfg.sensor_count; i++) {
    // Must be called before setRecattr
    ret = RKADK_PARAM_SetStreamAttr(i);
    if (ret)
      break;

    // Must be called before SetPhotoAttr
    ret = RKADK_PARAM_SetRecAttr(i);
    if (ret)
      break;

    ret = RKADK_PARAM_SetPhotoAttr(i);
    if (ret)
      break;
  }

#ifdef RKADK_DUMP_CONFIG
  RKADK_PARAM_DumpAttr();
#endif

  return ret;
}

static void RKADK_PARAM_SetMicVolume(RKADK_U32 volume) {
  char buffer[RKADK_BUFFER_LEN];

  memset(buffer, 0, RKADK_BUFFER_LEN);
  sprintf(buffer, "amixer sset MasterC %d%%", volume);
  system(buffer);
}

static void RKADK_PARAM_SetSpeakerVolume(RKADK_U32 volume) {
  char buffer[RKADK_BUFFER_LEN];

  memset(buffer, 0, RKADK_BUFFER_LEN);
  sprintf(buffer, "amixer sset MasterP %d%%", volume);
  system(buffer);
}

static void RKADK_PARAM_MicMute(bool mute) {
  char buffer[RKADK_BUFFER_LEN];

  memset(buffer, 0, RKADK_BUFFER_LEN);
  if (mute)
    sprintf(buffer, "amixer sset 'ADC SDP MUTE' on");
  else
    sprintf(buffer, "amixer sset 'ADC SDP MUTE' off");

  system(buffer);
}

static void RKADK_PARAM_SetVolume() {
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;

  RKADK_PARAM_MicMute(!pstCommCfg->mic_unmute);
  RKADK_PARAM_SetMicVolume(pstCommCfg->mic_volume);

  if (!pstCommCfg->enable_speaker)
    RKADK_PARAM_SetSpeakerVolume(0);
  else
    RKADK_PARAM_SetSpeakerVolume(pstCommCfg->speaker_volume);
}

VENC_RC_MODE_E RKADK_PARAM_GetRcMode(char *rcMode,
                                     RKADK_CODEC_TYPE_E enCodecType) {
  VENC_RC_MODE_E enRcMode = VENC_RC_MODE_BUTT;

  RKADK_CHECK_POINTER(rcMode, VENC_RC_MODE_BUTT);

  switch (enCodecType) {
  case RKADK_CODEC_TYPE_H264:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_H264CBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_H264VBR;
    else if (!strcmp(rcMode, "AVBR"))
      enRcMode = VENC_RC_MODE_H264AVBR;
    break;
  case RKADK_CODEC_TYPE_H265:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_H265CBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_H265VBR;
    else if (!strcmp(rcMode, "AVBR"))
      enRcMode = VENC_RC_MODE_H265AVBR;
    break;
  case RKADK_CODEC_TYPE_MJPEG:
    if (!strcmp(rcMode, "CBR"))
      enRcMode = VENC_RC_MODE_MJPEGCBR;
    else if (!strcmp(rcMode, "VBR"))
      enRcMode = VENC_RC_MODE_MJPEGVBR;
    break;
  default:
    RKADK_LOGE("Nonsupport codec type: %d", enCodecType);
    break;
  }

  return enRcMode;
}

RKADK_S32 RKADK_PARAM_GetRcParam(RKADK_PARAM_VENC_ATTR_S stVencAttr,
                                 VENC_RC_PARAM_S *pstRcParam) {
  RKADK_S32 s32FirstFrameStartQp;
  RKADK_U32 u32RowQpDeltaI;
  RKADK_U32 u32RowQpDeltaP;
  RKADK_U32 u32StepQp;

  RKADK_CHECK_POINTER(pstRcParam, RKADK_FAILURE);
  memset(pstRcParam, 0, sizeof(VENC_RC_PARAM_S));

  s32FirstFrameStartQp = stVencAttr.venc_param.first_frame_qp > 0 ?
                             stVencAttr.venc_param.first_frame_qp : -1;

  u32StepQp = stVencAttr.venc_param.qp_step > 0 ?
                  stVencAttr.venc_param.qp_step : 2;

  u32RowQpDeltaI = stVencAttr.venc_param.row_qp_delta_i > 0 ?
                       stVencAttr.venc_param.row_qp_delta_i : 1;

  u32RowQpDeltaP = stVencAttr.venc_param.row_qp_delta_p > 0 ?
                       stVencAttr.venc_param.row_qp_delta_p : 2;

  pstRcParam->s32FirstFrameStartQp = s32FirstFrameStartQp;
  pstRcParam->u32RowQpDeltaI = u32RowQpDeltaI;
  pstRcParam->u32RowQpDeltaP = u32RowQpDeltaP;
  switch (stVencAttr.codec_type) {
  case RKADK_CODEC_TYPE_H264:
    pstRcParam->stParamH264.u32StepQp = u32StepQp;
    pstRcParam->stParamH264.u32MaxQp = stVencAttr.venc_param.max_qp;
    pstRcParam->stParamH264.u32MinQp = stVencAttr.venc_param.min_qp;
    pstRcParam->stParamH264.u32MaxIQp = stVencAttr.venc_param.max_qp;
    pstRcParam->stParamH264.u32MinIQp = stVencAttr.venc_param.min_qp;
    break;
  case RKADK_CODEC_TYPE_H265:
    pstRcParam->stParamH265.u32StepQp = u32StepQp;
    pstRcParam->stParamH265.u32MaxQp = stVencAttr.venc_param.max_qp;
    pstRcParam->stParamH265.u32MinQp = stVencAttr.venc_param.min_qp;
    pstRcParam->stParamH265.u32MaxIQp = stVencAttr.venc_param.max_qp;
    pstRcParam->stParamH265.u32MinIQp = stVencAttr.venc_param.min_qp;
    break;
  case RKADK_CODEC_TYPE_MJPEG:
    break;
  default:
    RKADK_LOGE("Nonsupport codec type: %d", stVencAttr.codec_type);
    return -1;
  }

  return 0;
}

RKADK_PARAM_CONTEXT_S *RKADK_PARAM_GetCtx() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx;
}

RKADK_PARAM_REC_CFG_S *RKADK_PARAM_GetRecCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stRecCfg;
}

RKADK_PARAM_SENSOR_CFG_S *RKADK_PARAM_GetSensorCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stSensorCfg[u32CamId];
}

RKADK_PARAM_STREAM_CFG_S *RKADK_PARAM_GetStreamCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stStreamCfg;
}

RKADK_PARAM_PHOTO_CFG_S *RKADK_PARAM_GetPhotoCfg(RKADK_U32 u32CamId) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  RKADK_CHECK_CAMERAID(u32CamId, NULL);
  return &g_stPARAMCtx.stCfg.stMediaCfg[u32CamId].stPhotoCfg;
}

RKADK_PARAM_AUDIO_CFG_S *RKADK_PARAM_GetAudioCfg() {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx.stCfg.stAudioCfg;
}

RKADK_PARAM_THUMB_CFG_S *RKADK_PARAM_GetThumbCfg(RKADK_VOID) {
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, NULL);
  return &g_stPARAMCtx.stCfg.stThumbCfg;
}

RKADK_S32 RKADK_PARAM_GetVencChnId(RKADK_U32 u32CamId,
                                   RKADK_STREAM_TYPE_E enStrmType) {
  RKADK_S32 s32VencChnId = -1;

  RKADK_CHECK_CAMERAID(u32CamId, RKADK_FAILURE);

  switch (enStrmType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg)
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    else
      s32VencChnId = pstRecCfg->attribute[0].venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_VIDEO_SUB: {
    RKADK_PARAM_REC_CFG_S *pstRecCfg = RKADK_PARAM_GetRecCfg(u32CamId);
    if (!pstRecCfg)
      RKADK_LOGE("RKADK_PARAM_GetRecCfg failed");
    else
      s32VencChnId = pstRecCfg->attribute[1].venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_SNAP: {
    RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg = RKADK_PARAM_GetPhotoCfg(u32CamId);
    if (!pstPhotoCfg)
      RKADK_LOGE("RKADK_PARAM_GetPhotoCfg failed");
    else
      s32VencChnId = pstPhotoCfg->venc_chn;
    break;
  }
  case RKADK_STREAM_TYPE_USER: {
    RKADK_PARAM_STREAM_CFG_S *pstStreamCfg = RKADK_PARAM_GetStreamCfg(u32CamId);
    if (!pstStreamCfg)
      RKADK_LOGE("RKADK_PARAM_GetStreamCfg failed");
    else
      s32VencChnId = pstStreamCfg->attribute.venc_chn;
    break;
  }
  default:
    RKADK_LOGE("Unsupport stream type: %d", enStrmType);
    break;
  }

  return s32VencChnId;
}

RKADK_PARAM_RES_E RKADK_PARAM_GetResType(RKADK_U32 width, RKADK_U32 height) {
  RKADK_PARAM_RES_E type = RKADK_RES_BUTT;

  if (width == RKADK_WIDTH_720P && height == RKADK_HEIGHT_720P)
    type = RKADK_RES_720P;
  else if (width == RKADK_WIDTH_1080P && height == RKADK_HEIGHT_1080P)
    type = RKADK_RES_1080P;
  else if (width == RKADK_WIDTH_1296P && height == RKADK_HEIGHT_1296P)
    type = RKADK_RES_1296P;
  else if (width == RKADK_WIDTH_1440P && height == RKADK_HEIGHT_1440P)
    type = RKADK_RES_1440P;
  else if (width == RKADK_WIDTH_1520P && height == RKADK_HEIGHT_1520P)
    type = RKADK_RES_1520P;
  else if (width == RKADK_WIDTH_1600P && height == RKADK_HEIGHT_1600P)
    type = RKADK_RES_1600P;
  else if (width == RKADK_WIDTH_1620P && height == RKADK_HEIGHT_1620P)
    type = RKADK_RES_1620P;
  else if (width == RKADK_WIDTH_1944P && height == RKADK_HEIGHT_1944P)
    type = RKADK_RES_1944P;
  else if (width == RKADK_WIDTH_3840P && height == RKADK_HEIGHT_2160P)
    type = RKADK_RES_2160P;
  else
    RKADK_LOGE("Unsupport resolution(%d*%d)", width, height);

  return type;
}

RKADK_S32 RKADK_PARAM_GetResolution(RKADK_PARAM_RES_E type, RKADK_U32 *width,
                                    RKADK_U32 *height) {
  switch (type) {
  case RKADK_RES_720P:
    *width = RKADK_WIDTH_720P;
    *height = RKADK_HEIGHT_720P;
    break;
  case RKADK_RES_1080P:
    *width = RKADK_WIDTH_1080P;
    *height = RKADK_HEIGHT_1080P;
    break;
  case RKADK_RES_1296P:
    *width = RKADK_WIDTH_1296P;
    *height = RKADK_HEIGHT_1296P;
    break;
  case RKADK_RES_1440P:
    *width = RKADK_WIDTH_1440P;
    *height = RKADK_HEIGHT_1440P;
    break;
  case RKADK_RES_1520P:
    *width = RKADK_WIDTH_1520P;
    *height = RKADK_HEIGHT_1520P;
    break;
  case RKADK_RES_1600P:
    *width = RKADK_WIDTH_1600P;
    *height = RKADK_HEIGHT_1600P;
    break;
  case RKADK_RES_1620P:
    *width = RKADK_WIDTH_1620P;
    *height = RKADK_HEIGHT_1620P;
    break;
  case RKADK_RES_1944P:
    *width = RKADK_WIDTH_1944P;
    *height = RKADK_HEIGHT_1944P;
    break;
  case RKADK_RES_2160P:
    *width = RKADK_WIDTH_3840P;
    *height = RKADK_HEIGHT_2160P;
    break;
  default:
    RKADK_LOGE("Unsupport resolution type: %d, set to 1080P", type);
    *width = RKADK_WIDTH_1080P;
    *height = RKADK_HEIGHT_1080P;
    break;
  }

  return 0;
}

static RKADK_CODEC_TYPE_E RKADK_PARAM_GetCodecType(RKADK_S32 s32CamId,
                          RKADK_STREAM_TYPE_E enStreamType) {
  RKADK_CODEC_TYPE_E enCodecType = RKADK_CODEC_TYPE_BUTT;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    enCodecType = pstRecCfg->attribute[0].codec_type;
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    enCodecType = pstRecCfg->attribute[1].codec_type;
    break;

  case RKADK_STREAM_TYPE_USER:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId);
    enCodecType = pstStreamCfg->attribute.codec_type;
    break;
  default:
    RKADK_LOGE("Unsupport enStreamType: %d", enStreamType);
    break;
  }

  return enCodecType;
}

static RKADK_S32 RKADK_PARAM_SetCodecType(RKADK_S32 s32CamId,
                         RKADK_PARAM_CODEC_CFG_S *pstCodecCfg) {
  RKADK_S32 ret;
  RKADK_PARAM_REC_CFG_S *pstRecCfg;
  RKADK_PARAM_STREAM_CFG_S *pstStreamCfg;

  switch (pstCodecCfg->enStreamType) {
  case RKADK_STREAM_TYPE_VIDEO_MAIN:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[0].codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstRecCfg->attribute[0].codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveRecCfg(RKADK_PARAM_PATH, s32CamId);
    break;

  case RKADK_STREAM_TYPE_VIDEO_SUB:
    pstRecCfg = RKADK_PARAM_GetRecCfg(s32CamId);
    if (pstRecCfg->attribute[1].codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstRecCfg->attribute[1].codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveRecCfg(RKADK_PARAM_PATH, s32CamId);
    break;

  case RKADK_STREAM_TYPE_USER:
    pstStreamCfg = RKADK_PARAM_GetStreamCfg(s32CamId);
    if (pstStreamCfg->attribute.codec_type == pstCodecCfg->enCodecType)
      return 0;

    pstStreamCfg->attribute.codec_type = pstCodecCfg->enCodecType;
    ret = RKADK_PARAM_SaveStreamCfg(RKADK_PARAM_PATH, s32CamId);
    break;

  default:
    RKADK_LOGE("Unsupport enStreamType: %d", pstCodecCfg->enStreamType);
    break;
  }

  return ret;
}

RKADK_S32 RKADK_PARAM_GetCamParam(RKADK_S32 s32CamID,
                                  RKADK_PARAM_TYPE_E enParamType,
                                  RKADK_VOID *pvParam) {
  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);

  // RKADK_LOGD("s32CamID: %d, enParamType: %d, u32_pvParam: %d, b_pvParam: %d",
  // s32CamID, enParamType, *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stRecCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamID];
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stPhotoCfg;

  switch (enParamType) {
  case RKADK_PARAM_TYPE_FPS:
    *(RKADK_U32 *)pvParam = pstSensorCfg->framerate;
    break;
  case RKADK_PARAM_TYPE_FLIP:
    *(bool *)pvParam = pstSensorCfg->flip;
    break;
  case RKADK_PARAM_TYPE_MIRROR:
    *(bool *)pvParam = pstSensorCfg->mirror;
    break;
  case RKADK_PARAM_TYPE_LDC:
    *(RKADK_U32 *)pvParam = pstSensorCfg->ldc;
    break;
  case RKADK_PARAM_TYPE_ANTIFOG:
    *(RKADK_U32 *)pvParam = pstSensorCfg->antifog;
    break;
  case RKADK_PARAM_TYPE_WDR:
    *(RKADK_U32 *)pvParam = pstSensorCfg->wdr;
    break;
  case RKADK_PARAM_TYPE_HDR:
    *(RKADK_U32 *)pvParam = pstSensorCfg->hdr;
    break;
  case RKADK_PARAM_TYPE_REC:
    *(bool *)pvParam = pstSensorCfg->enable_record;
    break;
  case RKADK_PARAM_TYPE_PHOTO_ENABLE:
    *(bool *)pvParam = pstSensorCfg->enable_photo;
    break;
  case RKADK_PARAM_TYPE_RES:
    *(RKADK_PARAM_RES_E *)pvParam = RKADK_PARAM_GetResType(
        pstRecCfg->attribute[0].width, pstRecCfg->attribute[0].height);
    break;
  case RKADK_PARAM_TYPE_CODEC_TYPE: {
    RKADK_PARAM_CODEC_CFG_S *pstCodecCfg;

    pstCodecCfg = (RKADK_PARAM_CODEC_CFG_S *)pvParam;
    pstCodecCfg->enCodecType =
        RKADK_PARAM_GetCodecType(s32CamID, pstCodecCfg->enStreamType);
    break;
  }
  case RKADK_PARAM_TYPE_SPLITTIME:
    *(RKADK_U32 *)pvParam = pstRecCfg->splite_time;
    break;
  case RKADK_PARAM_TYPE_RECORD_TYPE:
    *(RKADK_REC_TYPE_E *)pvParam = pstRecCfg->record_type;
    break;
  case RKADK_PARAM_TYPE_FILE_CNT:
    *(RKADK_U32 *)pvParam = pstRecCfg->file_num;
    break;
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL:
    *(RKADK_U32 *)pvParam = pstRecCfg->lapse_interval;
    break;
  case RKADK_PARAM_TYPE_LAPSE_MULTIPLE:
    *(RKADK_U32 *)pvParam = pstRecCfg->lapse_multiple;
    break;
  case RKADK_PARAM_TYPE_RECORD_TIME:
    *(RKADK_U32 *)pvParam = pstRecCfg->record_time;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_TIME:
    *(RKADK_U32 *)pvParam = pstRecCfg->pre_record_time;
    break;
  case RKADK_PARAM_TYPE_PHOTO_RES:
    *(RKADK_PARAM_RES_E *)pvParam = RKADK_PARAM_GetResType(
        pstPhotoCfg->image_width, pstPhotoCfg->image_height);
    break;
  case RKADK_PARAM_TYPE_SNAP_NUM:
    *(RKADK_U32 *)pvParam = pstPhotoCfg->snap_num;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetCamParam(RKADK_S32 s32CamID,
                                  RKADK_PARAM_TYPE_E enParamType,
                                  const RKADK_VOID *pvParam) {
  RKADK_S32 ret;
  bool bSaveRecCfg = false;
  bool bSavePhotoCfg = false;
  bool bSaveSensorCfg = false;
  RKADK_PARAM_RES_E type = RKADK_RES_BUTT;

  // RKADK_LOGD("s32CamID: %d, enParamType: %d, u32_pvParam: %d, b_pvParam: %d",
  // s32CamID, enParamType, *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_CHECK_CAMERAID(s32CamID, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  RKADK_PARAM_REC_CFG_S *pstRecCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stRecCfg;
  RKADK_PARAM_SENSOR_CFG_S *pstSensorCfg =
      &g_stPARAMCtx.stCfg.stSensorCfg[s32CamID];
  RKADK_PARAM_PHOTO_CFG_S *pstPhotoCfg =
      &g_stPARAMCtx.stCfg.stMediaCfg[s32CamID].stPhotoCfg;

  switch (enParamType) {
  case RKADK_PARAM_TYPE_FPS:
    RKADK_CHECK_EQUAL(pstSensorCfg->flip, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->framerate = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_FLIP:
    RKADK_CHECK_EQUAL(pstSensorCfg->flip, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->flip = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_MIRROR:
    RKADK_CHECK_EQUAL(pstSensorCfg->mirror, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->mirror = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_LDC:
    RKADK_CHECK_EQUAL(pstSensorCfg->ldc, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->ldc = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_ANTIFOG:
    RKADK_CHECK_EQUAL(pstSensorCfg->antifog, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->antifog = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_WDR:
    RKADK_CHECK_EQUAL(pstSensorCfg->wdr, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->wdr = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_HDR:
    RKADK_CHECK_EQUAL(pstSensorCfg->hdr, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->hdr = *(RKADK_U32 *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_REC:
    RKADK_CHECK_EQUAL(pstSensorCfg->enable_record, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->enable_record = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_PHOTO_ENABLE:
    RKADK_CHECK_EQUAL(pstSensorCfg->enable_photo, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstSensorCfg->enable_photo = *(bool *)pvParam;
    bSaveSensorCfg = true;
    break;
  case RKADK_PARAM_TYPE_RES:
    type = *(RKADK_PARAM_RES_E *)pvParam;
    RKADK_PARAM_GetResolution(type, &(pstRecCfg->attribute[0].width),
                              &(pstRecCfg->attribute[0].height));
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_CODEC_TYPE:
    ret = RKADK_PARAM_SetCodecType(s32CamID, (RKADK_PARAM_CODEC_CFG_S *)pvParam);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return ret;
  case RKADK_PARAM_TYPE_SPLITTIME:
    RKADK_CHECK_EQUAL(pstRecCfg->splite_time, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->splite_time = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_RECORD_TYPE:
    RKADK_CHECK_EQUAL(pstRecCfg->record_type, *(RKADK_REC_TYPE_E *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->record_type = *(RKADK_REC_TYPE_E *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_FILE_CNT:
    RKADK_CHECK_EQUAL(pstRecCfg->file_num, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->file_num = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_LAPSE_INTERVAL:
    RKADK_CHECK_EQUAL(pstRecCfg->lapse_interval, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->lapse_interval = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_LAPSE_MULTIPLE:
    RKADK_CHECK_EQUAL(pstRecCfg->lapse_multiple, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->lapse_multiple = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_RECORD_TIME:
    RKADK_CHECK_EQUAL(pstRecCfg->record_time, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->record_time = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_PRE_RECORD_TIME:
    RKADK_CHECK_EQUAL(pstRecCfg->pre_record_time, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstRecCfg->pre_record_time = *(RKADK_U32 *)pvParam;
    bSaveRecCfg = true;
    break;
  case RKADK_PARAM_TYPE_PHOTO_RES:
    type = *(RKADK_PARAM_RES_E *)pvParam;
    RKADK_PARAM_GetResolution(type, &(pstPhotoCfg->image_width),
                              &(pstPhotoCfg->image_height));
    bSavePhotoCfg = true;
    break;
  case RKADK_PARAM_TYPE_SNAP_NUM:
    RKADK_CHECK_EQUAL(pstPhotoCfg->snap_num, *(RKADK_U32 *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);
    pstPhotoCfg->snap_num = *(RKADK_U32 *)pvParam;
    bSavePhotoCfg = true;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  if (bSaveSensorCfg)
    RKADK_PARAM_SaveSensorCfg(RKADK_PARAM_PATH, s32CamID);

  if (bSaveRecCfg) {
    RKADK_PARAM_SaveRecCfg(RKADK_PARAM_PATH, s32CamID);
    RKADK_PARAM_SetRecAttr(s32CamID);
  }

  if (bSavePhotoCfg) {
    RKADK_PARAM_SavePhotoCfg(RKADK_PARAM_PATH, s32CamID);
    RKADK_PARAM_SetPhotoAttr(s32CamID);
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetCommParam(RKADK_PARAM_TYPE_E enParamType,
                                   const RKADK_VOID *pvParam) {
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);

  // RKADK_LOGD("enParamType: %d, u32_pvParam: %d, b_pvParam: %d", enParamType,
  // *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  switch (enParamType) {
  case RKADK_PARAM_TYPE_REC_UNMUTE:
    RKADK_CHECK_EQUAL(pstCommCfg->rec_unmute, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->rec_unmute = *(bool *)pvParam;
    break;
  case RKADK_PARAM_TYPE_AUDIO:
    RKADK_CHECK_EQUAL(pstCommCfg->enable_speaker, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->enable_speaker = *(bool *)pvParam;
    if (!pstCommCfg->enable_speaker)
      RKADK_PARAM_SetSpeakerVolume(0);
    else
      RKADK_PARAM_SetSpeakerVolume(pstCommCfg->speaker_volume);
    break;
  case RKADK_PARAM_TYPE_VOLUME:
    RKADK_CHECK_EQUAL(pstCommCfg->speaker_volume, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->speaker_volume = *(RKADK_U32 *)pvParam;
    RKADK_PARAM_SetSpeakerVolume(pstCommCfg->speaker_volume);
    break;
  case RKADK_PARAM_TYPE_MIC_UNMUTE:
    RKADK_CHECK_EQUAL(pstCommCfg->mic_unmute, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->mic_unmute = *(bool *)pvParam;
    RKADK_PARAM_MicMute(!pstCommCfg->mic_unmute);
    break;
  case RKADK_PARAM_TYPE_MIC_VOLUME:
    RKADK_CHECK_EQUAL(pstCommCfg->mic_volume, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->mic_volume = *(RKADK_U32 *)pvParam;
    RKADK_PARAM_SetMicVolume(pstCommCfg->mic_volume);
    break;
  case RKADK_PARAM_TYPE_OSD_TIME_FORMAT:
    RKADK_CHECK_EQUAL(pstCommCfg->osd_time_format, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->osd_time_format = *(RKADK_U32 *)pvParam;
    break;
  case RKADK_PARAM_TYPE_OSD:
    RKADK_CHECK_EQUAL(pstCommCfg->osd, *(bool *)pvParam, g_stPARAMCtx.mutexLock,
                      RKADK_SUCCESS);

    pstCommCfg->osd = *(bool *)pvParam;
    break;
  case RKADK_PARAM_TYPE_BOOTSOUND:
    RKADK_CHECK_EQUAL(pstCommCfg->boot_sound, *(bool *)pvParam,
                      g_stPARAMCtx.mutexLock, RKADK_SUCCESS);

    pstCommCfg->boot_sound = *(bool *)pvParam;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_PARAM_SaveCommCfg(RKADK_PARAM_PATH);
  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_GetCommParam(RKADK_PARAM_TYPE_E enParamType,
                                   RKADK_VOID *pvParam) {
  RKADK_CHECK_POINTER(pvParam, RKADK_FAILURE);
  RKADK_CHECK_INIT(g_stPARAMCtx.bInit, RKADK_FAILURE);

  // RKADK_LOGD("enParamType: %d, u32_pvParam: %d, b_pvParam: %d", enParamType,
  // *(RKADK_U32 *)pvParam, *(bool *)pvParam);

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);
  RKADK_PARAM_COMM_CFG_S *pstCommCfg = &g_stPARAMCtx.stCfg.stCommCfg;
  switch (enParamType) {
  case RKADK_PARAM_TYPE_REC_UNMUTE:
    *(bool *)pvParam = pstCommCfg->rec_unmute;
    break;
  case RKADK_PARAM_TYPE_AUDIO:
    *(bool *)pvParam = pstCommCfg->enable_speaker;
    break;
  case RKADK_PARAM_TYPE_VOLUME:
    *(RKADK_U32 *)pvParam = pstCommCfg->speaker_volume;
    break;
  case RKADK_PARAM_TYPE_MIC_UNMUTE:
    *(bool *)pvParam = pstCommCfg->mic_unmute;
    break;
  case RKADK_PARAM_TYPE_MIC_VOLUME:
    *(RKADK_U32 *)pvParam = pstCommCfg->mic_volume;
    break;
  case RKADK_PARAM_TYPE_OSD_TIME_FORMAT:
    *(RKADK_U32 *)pvParam = pstCommCfg->osd_time_format;
    break;
  case RKADK_PARAM_TYPE_OSD:
    *(bool *)pvParam = pstCommCfg->osd;
    break;
  case RKADK_PARAM_TYPE_BOOTSOUND:
    *(bool *)pvParam = pstCommCfg->boot_sound;
    break;
  default:
    RKADK_LOGE("Unsupport enParamType(%d)", enParamType);
    RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
    return -1;
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return 0;
}

RKADK_S32 RKADK_PARAM_SetDefault() {
  RKADK_S32 ret = RKADK_SUCCESS;

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  memset(&g_stPARAMCtx.stCfg, 0, sizeof(RKADK_PARAM_CFG_S));
  ret = RKADK_PARAM_LoadDefault();
  if (ret) {
    RKADK_LOGE("load default param failed");
  } else {
    RKADK_PARAM_SetMediaAttr();
    RKADK_PARAM_SetVolume();

#ifdef RKADK_DUMP_CONFIG
    RKADK_PARAM_Dump();
#endif
  }

  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return ret;
}

RKADK_S32 RKADK_PARAM_Init() {
  RKADK_S32 ret = RKADK_SUCCESS;

  /* Check Module Init Status */
  if (g_stPARAMCtx.bInit)
    return RKADK_SUCCESS;

  RKADK_MUTEX_LOCK(g_stPARAMCtx.mutexLock);

  memset(&g_stPARAMCtx.stCfg, 0, sizeof(RKADK_PARAM_CFG_S));
  ret = RKADK_PARAM_LoadParam(RKADK_PARAM_PATH);
  if (ret) {
    RKADK_LOGE("load param failed, load default");
    ret = RKADK_PARAM_LoadDefault();
    if (ret) {
      RKADK_LOGE("load default param failed, use default");
      RKADK_PARAM_UseDefault();
    }
  }

  ret = RKADK_PARAM_SetMediaAttr();
  if (ret) {
    RKADK_LOGE("set media attribute failed");
    goto end;
  }

  RKADK_PARAM_SetVolume();

#ifdef RKADK_DUMP_CONFIG
  RKADK_PARAM_Dump();
#endif

  g_stPARAMCtx.bInit = true;

end:
  RKADK_MUTEX_UNLOCK(g_stPARAMCtx.mutexLock);
  return ret;
}

RKADK_S32 RKADK_PARAM_Deinit() {
  // reserved
  return 0;
}
