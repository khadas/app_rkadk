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

#ifndef __RKADK_PHOTO_H__
#define __RKADK_PHOTO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rkadk_common.h"

/** photo type enum */
typedef enum {
  RKADK_PHOTO_TYPE_SINGLE = 0,
  RKADK_PHOTO_TYPE_MULTIPLE,
  RKADK_PHOTO_TYPE_LAPSE, // TODO
  RKADK_PHOTO_TYPE_BUTT
} RKADK_PHOTO_TYPE_E;

/** single photo attr */
typedef struct {
  // TODO
  /* s32TimeSec is 0 that means photo immediately, larger than 0 that means
   * photo delay s32TimeSec second */
  RKADK_S32 s32Time_sec;
} RKADK_PHOTO_SINGLE_ATTR_S;

/** lapse photo attr */
typedef struct {
  RKADK_S32 s32Interval_ms; /* unit: millisecond */
} RKADK_PHOTO_LAPSE_ATTR_S;

/** burst photo attr */
typedef struct {
  /* s32Count is -1 that means continuous photo, larger than 0 that meas photo
   * number */
  RKADK_S32 s32Count;
} RKADK_PHOTO_MULTIPLE_ATTR_S;

/* photo data recv callback */
typedef void (*RKADK_PHOTO_DATA_RECV_FN_PTR)(RKADK_U8 *pu8DataBuf,
                                             RKADK_U32 u32DataLen);

typedef struct {
  RKADK_U32 u32CamID; /** cam id, 0--front，1--rear */
  RKADK_PHOTO_TYPE_E enPhotoType;
  union tagPhotoTypeAttr {
    RKADK_PHOTO_SINGLE_ATTR_S stSingleAttr;
    RKADK_PHOTO_LAPSE_ATTR_S stLapseAttr; // TODO
    RKADK_PHOTO_MULTIPLE_ATTR_S stMultipleAttr;
  } unPhotoTypeAttr;
  RKADK_PHOTO_DATA_RECV_FN_PTR pfnPhotoDataProc;
} RKADK_PHOTO_ATTR_S;

/****************************************************************************/
/*                            Interface Definition                          */
/****************************************************************************/
/**
* @brief init photo, it should be called first
* @param[in] pstPhotoAttr: photo attribute
* @return 0 success, non-zero error code.
*/
RKADK_S32 RKADK_PHOTO_Init(RKADK_PHOTO_ATTR_S *pstPhotoAttr);

/**
* @brief deinit photo
* @param[in] u32CamID: camera id
* @return 0 success, non-zero error code.
*/
RKADK_S32 RKADK_PHOTO_DeInit(RKADK_U32 u32CamID);

/**
 * @brief take photo
 * @param[in] pstPhotoAttr: photo attribute
 * @return 0 success, non-zero error code.
 */
RKADK_S32 RKADK_PHOTO_TakePhoto(RKADK_PHOTO_ATTR_S *pstPhotoAttr);

#ifdef __cplusplus
}
#endif
#endif
