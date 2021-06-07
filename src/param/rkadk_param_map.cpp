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

#include "rkadk_param_map.h"

static RKADK_MAP_TABLE_CFG_S
    g_sensorMapTableList[RKADK_MAX_SENSOR_CNT][RKADK_PARAM_MAP_BUTT] = {
        {{sizeof(g_stSensorCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stSensorCfgMapTable_0},
         {sizeof(g_stRecCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stRecCfgMapTable_0},
         {sizeof(g_stRecCfgMapTable_0_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stRecCfgMapTable_0_0},
         {sizeof(g_stRecParamMapTable_0_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stRecParamMapTable_0_0},
         {sizeof(g_stRecCfgMapTable_0_1) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stRecCfgMapTable_0_1},
         {sizeof(g_stRecParamMapTable_0_1) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stRecParamMapTable_0_1},
         {sizeof(g_stStreamCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stStreamCfgMapTable_0},
         {sizeof(g_stStreamParamMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stStreamParamMapTable_0},
         {sizeof(g_stPhotoCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stPhotoCfgMapTable_0},
         {sizeof(g_stViCfgMapTable_0) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stViCfgMapTable_0},
         {sizeof(g_stViCfgMapTable_1) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stViCfgMapTable_1},
         {sizeof(g_stViCfgMapTable_2) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stViCfgMapTable_2},
         {sizeof(g_stViCfgMapTable_3) / sizeof(RKADK_SI_CONFIG_MAP_S),
          g_stViCfgMapTable_3}}};

RKADK_MAP_TABLE_CFG_S *
RKADK_PARAM_GetMapTable(RKADK_U32 u32Camid, RKADK_PARAM_MAP_TYPE_E eMapTable) {
  RKADK_CHECK_CAMERAID(u32Camid, NULL);

  if (eMapTable >= RKADK_PARAM_MAP_BUTT) {
    RKADK_LOGE("invaild u32Camid[%d] eMapTable: %d", u32Camid, eMapTable);
    return NULL;
  }

  return &(g_sensorMapTableList[u32Camid][eMapTable]);
}
