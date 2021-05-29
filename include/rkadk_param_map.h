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

#ifndef __RKADK_PARAM_MAP_H__
#define __RKADK_PARAM_MAP_H__

#include "rkadk_param.h"
#include "rkadk_struct2ini.h"

#ifdef __cplusplus
extern "C" {
#endif

/* common map table */
static RKADK_SI_CONFIG_MAP_S g_stCommCfgMapTable[] = {
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, int_e, sensor_count),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, bool_e, rec_unmute),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, bool_e, enable_speaker),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, int_e, speaker_volume),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, bool_e, mic_unmute),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, int_e, mic_volume),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, int_e, osd_time_format),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, bool_e, osd),
    DEFINE_MAP(common, tagRKADK_PARAM_COMM_CFG_S, bool_e, boot_sound),
};

/* audio map table */
static RKADK_SI_CONFIG_MAP_S g_stAudioCfgMapTable[] = {
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, string_e, audio_node),
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, int_e, sample_format),
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, int_e, channels),
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, int_e, samplerate),
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, int_e, samples_per_frame),
    DEFINE_MAP(audio, tagRKADK_PARAM_AUDIO_CFG_S, int_e, bitrate),
};

/* thumb map table */
static RKADK_SI_CONFIG_MAP_S g_stThumbCfgMapTable[] = {
    DEFINE_MAP(thumb, tagRKADK_PARAM_THUMB_CFG_S, int_e, thumb_width),
    DEFINE_MAP(thumb, tagRKADK_PARAM_THUMB_CFG_S, int_e, thumb_height),
    DEFINE_MAP(thumb, tagRKADK_PARAM_THUMB_CFG_S, int_e, venc_chn),
};

/* sensor map table */
static RKADK_SI_CONFIG_MAP_S g_stSensorCfgMapTable_0[] = {
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, max_width),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, max_height),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, framerate),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, bool_e, enable_record),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, bool_e, enable_photo),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, bool_e, flip),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, bool_e, mirror),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, ldc),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, wdr),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, hdr),
    DEFINE_MAP(sensor.0, tagRKADK_PARAM_SENSOR_CFG_S, int_e, antifog),

};

/* record map table */
static RKADK_SI_CONFIG_MAP_S g_stRecCfgMapTable_0[] = {
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, record_type),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, record_time),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, splite_time),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, pre_record_time),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, lapse_interval),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, lapse_multiple),
    DEFINE_MAP(sensor.0.rec, tagRKADK_PARAM_REC_CFG_S, int_e, file_num),
};

static RKADK_SI_CONFIG_MAP_S g_stRecCfgMapTable_0_0[] = {
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, width),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, height),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, bitrate),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, gop),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, profile),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, codec_type),
    DEFINE_MAP(sensor.0.rec.0, tagRKADK_PARAM_VENC_ATTR_S, int_e, venc_chn),
};

static RKADK_SI_CONFIG_MAP_S g_stRecCfgMapTable_0_1[] = {
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, width),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, height),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, bitrate),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, gop),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, profile),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, codec_type),
    DEFINE_MAP(sensor.0.rec.1, tagRKADK_PARAM_VENC_ATTR_S, int_e, venc_chn),
};

/* stream map table */
static RKADK_SI_CONFIG_MAP_S g_stStreamCfgMapTable_0[] = {
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, width),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, height),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, bitrate),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, gop),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, profile),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, codec_type),
    DEFINE_MAP(sensor.0.stream, tagRKADK_PARAM_VENC_ATTR_S, int_e, venc_chn),
};

/* photo map table */
static RKADK_SI_CONFIG_MAP_S g_stPhotoCfgMapTable_0[] = {
    DEFINE_MAP(sensor.0.photo, tagRKADK_PARAM_PHOTO_CFG_S, int_e, image_width),
    DEFINE_MAP(sensor.0.photo, tagRKADK_PARAM_PHOTO_CFG_S, int_e, image_height),
    DEFINE_MAP(sensor.0.photo, tagRKADK_PARAM_PHOTO_CFG_S, int_e, snap_num),
    DEFINE_MAP(sensor.0.photo, tagRKADK_PARAM_PHOTO_CFG_S, int_e, venc_chn),
};

/* VI[0] ~ VI[3] map table */
static RKADK_SI_CONFIG_MAP_S g_stViCfgMapTable_0[] = {
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, int_e, chn_id),
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, string_e, device_name),
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, int_e, buf_cnt),
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, int_e, width),
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, int_e, height),
    DEFINE_MAP(sensor.0.vi.0, tagRKADK_PARAM_VI_CFG_S, string_e, pix_fmt),
};

static RKADK_SI_CONFIG_MAP_S g_stViCfgMapTable_1[] = {
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, int_e, chn_id),
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, string_e, device_name),
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, int_e, buf_cnt),
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, int_e, width),
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, int_e, height),
    DEFINE_MAP(sensor.0.vi.1, tagRKADK_PARAM_VI_CFG_S, string_e, pix_fmt),
};

static RKADK_SI_CONFIG_MAP_S g_stViCfgMapTable_2[] = {
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, int_e, chn_id),
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, string_e, device_name),
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, int_e, buf_cnt),
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, int_e, width),
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, int_e, height),
    DEFINE_MAP(sensor.0.vi.2, tagRKADK_PARAM_VI_CFG_S, string_e, pix_fmt),
};

static RKADK_SI_CONFIG_MAP_S g_stViCfgMapTable_3[] = {
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, int_e, chn_id),
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, string_e, device_name),
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, int_e, buf_cnt),
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, int_e, width),
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, int_e, height),
    DEFINE_MAP(sensor.0.vi.3, tagRKADK_PARAM_VI_CFG_S, string_e, pix_fmt),
};

#ifdef __cplusplus
}
#endif
#endif
