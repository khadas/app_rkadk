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

#ifndef __RKADK_AUDIO_DECODER_H__
#define __RKADK_AUDIO_DECODER_H__

#define FILE_MODE 0
#define STREAM_MODE 1
#define AUDIO_BUF_SIZE 2048

#include "rkadk_common.h"

typedef struct {
  RKADK_S32 bitWidth;
  RKADK_S32 reSmpSampleRate;
  RKADK_S32 channel;
} AUDIO_DECODER_CTX_S;

/**
 * @brief  create the audio decoder
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_Create(RKADK_MW_PTR *pAudioDecoder);

/**
 * @brief  destroy the audio decoder
 * @param[in] decoderMode : mode of the decoder
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_Destroy(RKADK_S8 decoderMode, RKADK_MW_PTR *pAudioDecoder);

/**
 * @brief  create the audio decoder
 * @param[in] decoderMode : mode of the decoder
 * @param[in] eCodecType : codec type of the decoder
 * @param[in] pszfilePath : path of the audio file
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_SetParam(RKADK_S8 decoderMode, RKADK_CODEC_TYPE_E eCodecType,
                                       const RKADK_CHAR *pszfilePath, RKADK_MW_PTR pAudioDecoder);

/**
 * @brief  push the undecoded audio decoder mix data
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @param[in] pPacketData : data of the audio packet
 * @param[in] packetSize : size of the audio packet
 * @param[in] bEofFlag : eof flag of the audio packet
 * @param[in] packetSize : stop flag of the audio packet
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_StreamPush(RKADK_MW_PTR pAudioDecoder, RKADK_CHAR *pPacketData,
                                         RKADK_S32 packetSize, RKADK_BOOL bEofFlag, RKADK_BOOL bStopFlag);

/**
 * @brief  push the Undecoded audio decoder single data
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @param[in] bStopFlag : flag of the audio play end
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_FilePush(RKADK_MW_PTR pAudioDecoder, RKADK_BOOL bStopFlag);

/**
 * @brief  pull the audio decoding data
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_GetData(RKADK_MW_PTR pAudioDecoder, RKADK_CHAR *buf, RKADK_U32 len);

/**
 * @brief  start the audio decoder
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_Start(RKADK_MW_PTR pAudioDecoder);

/**
 * @brief  Stop the audio decoder
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_Stop(RKADK_MW_PTR pAudioDecoder);

/**
 * @brief  get the audio decoder info
 * @param[in] decoderMode : mode of the decoder
 * @param[in] pAudioDecoder : pointer of the audio decoder
 * @param[in] pCtx : struct of the audio decoder parameter
 * @retval 0 success, others failed
 */
RKADK_S32 RKADK_AUDIO_DECODER_GetInfo(RKADK_S8 decoderMode, RKADK_MW_PTR pAudioDecoder, AUDIO_DECODER_CTX_S *pCtx);

#endif
