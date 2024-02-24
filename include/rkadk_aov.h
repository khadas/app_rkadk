/*
 * Copyright (c) 2023 Rockchip, Inc. All Rights Reserved.
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

#ifndef __RKADK_AOV_H__
#define __RKADK_AOV_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

int RKADK_AOV_Init();
int RKADK_AOV_DeInit();
int RKADK_AOV_EnterSleep();
int RKADK_AOV_SetSuspendTime(int u32WakeupSuspendTime);
int RKADK_AOV_DisableNonBootCPUs();
int RKADK_AOV_EnableNonBootCPUs();
//void RKADK_AOV_DumpPtsToTMP(uint32_t seq, uint64_t pts, int max_dump_pts_count);

#ifdef __cplusplus
}
#endif
#endif
