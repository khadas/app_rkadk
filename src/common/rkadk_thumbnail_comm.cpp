#include "rkadk_media_comm.h"
#include "rkadk_param.h"
#include "rkadk_thumbnail_comm.h"
#include "rkadk_log.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <utime.h>

#define THM_BOX_HEADER_LEN 8 /* size: 4byte, type: 4byte */
#define RGA_ZOOM_MAX 16

typedef struct {
  uint16_t tag_no; // tag number
  uint16_t format; // data format
  uint32_t length; // data len
  uint32_t offset; // data or data
  union IFD_VALUE {
    uint8_t ubv;     // format = 1, 1 byte
    const char *asv; // format = 2,7 1 byte
    uint16_t uwv;    // format = 3, 2 bytes
    uint32_t udwv;   // format = 4, 4 bytes
    uint64_t uddwv;  // format = 5, 8 bytes
    int8_t bv;       // format = 6, 1 byte
    int16_t wv;      // format = 8, 2 bytes
    int32_t dwv;     // format = 9, 4 bytes
    int64_t ddwv;    // format = 10, 8 bytes
    float fv;        // format = 11, 4 bytes
    double dv;       // format = 12, 8 bytes
  } value[10];
} IFD;

static IFD stIfd0[] = {
    {0x010e, 2, 32, 0x00, {{.asv = "thumbnail_test"}}},      // picture info
    {0x010f, 2, 32, 0x00, {{.asv = "rockchip"}}},            // manufact info
    {0x0110, 2, 32, 0x00, {{.asv = "rockchip IP Camrea"}}},  // module info
    {0x0131, 2, 32, 0x00, {{.asv = "rkadk v1.3.2"}}},        // software version
    {0x0132, 2, 32, 0x00, {{.asv = "2022:06:24 17:10:50"}}}, // date
};

static IFD stIfd1[] = {
    {0x0100, 3, 1, 320, {{.uwv = 320}}}, // ImageWidth
    {0x0101, 3, 1, 180, {{.uwv = 180}}}, // ImageLength
    {0x0202, 4, 1, 0, {{.udwv = 0}}},    // JpegIFByteCount
    {0x0201, 4, 1, 0, {{.udwv = 0}}},    // JpegIFOffset
};

static char exif_header[8] = {0x00, 0x00, 0x45, 0x78, 0x69, 0x66, 0x00, 0x00};
static char tiff_header[8] = {0x49, 0x49, 0x2A, 0x00, 0x08, 0x00, 0x00, 0x00};
static int format_2_len[13] = {0, 1, 1, 2, 4, 8, 1, 1, 2, 4, 8, 4, 8};

static int PackageApp1(IFD ifd0[], IFD ifd1[], int ifd0_len, int ifd1_len,
                char *app1_buf, char *thumbnail_buf, int thumbnail_len) {
  int dir_start = 0;
  int data_start = 0;
  int data_len = 0;
  int total_len = 0;
  app1_buf[0] = 0xff;
  app1_buf[1] = 0xe1;
  memcpy(app1_buf + 2, exif_header, 8);
  app1_buf += 10;
  memcpy(app1_buf + dir_start, tiff_header, 8);
  dir_start += 8;
  // package ifd0
  *(short *)(app1_buf + dir_start) = ifd0_len;
  dir_start += 2;
  data_start = dir_start + ifd0_len * 12 + 4;
  for (int i = 0; i < ifd0_len; i++, dir_start += 12) {
    int value_len = ifd0[i].length * format_2_len[ifd0[i].format];
    if (value_len > 4) {
      if (ifd0[i].format == 2) {
        memcpy(app1_buf + data_start, ifd0[i].value[0].asv,
               strlen(ifd0[i].value[0].asv));
      } else {
        memcpy(app1_buf + data_start, (char *)ifd0[i].value, value_len);
      }
      ifd0[i].offset = data_start;
      data_start += value_len;
      data_len += value_len;
    }
    memcpy(app1_buf + dir_start, (char *)&ifd0[i], 12);
  }
  *(int *)(app1_buf + dir_start) = data_start;
  dir_start = data_start;
  // package ifd1
  *(short *)(app1_buf + dir_start) = ifd1_len;
  dir_start += 2;
  data_len = 0;
  data_start = dir_start + ifd1_len * 12 + 4;
  for (int i = 0; i < ifd1_len; i++, dir_start += 12) {
    int value_len = ifd1[i].length * format_2_len[ifd1[i].format];
    if (ifd1[i].tag_no == 0x0201) {
      ifd1[i].offset = ((data_start + 0x200) / 0x200) * 0x200;
      memcpy(app1_buf + ifd1[i].offset, thumbnail_buf, thumbnail_len);
      total_len = ifd1[i].offset + thumbnail_len;
    } else if (ifd1[i].tag_no == 0x0202) {
      ifd1[i].offset = thumbnail_len;
    } else if (value_len > 4) {
      if (ifd1[i].format == 2) {
        memcpy(app1_buf + data_start, ifd1[i].value[0].asv,
               strlen(ifd1[i].value[0].asv));
      } else {
        memcpy(app1_buf + data_start, (char *)ifd1[i].value, value_len);
      }
      ifd1[i].offset = data_start;
      data_start += value_len;
      data_len += value_len;
    }
    memcpy(app1_buf + dir_start, (char *)&ifd1[i], 12);
  }
  *(int *)(app1_buf + dir_start) = 0;
  app1_buf -= 10;
  total_len += 10;
  app1_buf[2] = ((total_len - 2) >> 8) & 0xff;
  app1_buf[3] = (total_len - 2) & 0xff;
  return total_len;
}

static int RKADK_Thumbnail_Vi(RKADK_S32 u32CamId, RKADK_S32 ChnId,
                              RKADK_U32 thumb_width,
                              RKADK_U32 thumb_height) {
  int ret = 0;
  VI_CHN_ATTR_S stChnAttr;
  memset(&stChnAttr, 0, sizeof(stChnAttr));
  stChnAttr.stIspOpt.u32BufCount = 5;
  stChnAttr.stIspOpt.enMemoryType = VI_V4L2_MEMORY_TYPE_DMABUF;
  stChnAttr.stIspOpt.bNoUseLibV4L2 = RK_TRUE;
  stChnAttr.u32Depth = 0;
  stChnAttr.enPixelFormat = RK_FMT_YUV420SP;
  stChnAttr.stFrameRate.s32SrcFrameRate = -1;
  stChnAttr.stFrameRate.s32DstFrameRate = -1;
  stChnAttr.stSize.u32Width  = thumb_width;
  stChnAttr.stSize.u32Height = thumb_height;
  stChnAttr.u32Depth         = 1;
  stChnAttr.bMirror          = RK_FALSE;
  stChnAttr.bFlip            = RK_FALSE;
  stChnAttr.stIspOpt.stMaxSize.u32Width  = thumb_width;
  stChnAttr.stIspOpt.stMaxSize.u32Height = thumb_height;

  ret = RKADK_MPI_VI_Init(u32CamId, ChnId, &stChnAttr);
  if (ret != 0) {
    RKADK_LOGE("RKADK_MPI_VI_Init failed, ret = %d", ret);
    return ret;
  }
  return 0;
}

static int RKADK_Thumbnail_Venc(RKADK_S32 ChnId,
                                RKADK_U32 thumb_width,
                                RKADK_U32 thumb_height) {
  int ret = 0;
  VENC_RECV_PIC_PARAM_S stRecvParam;
  VENC_CHN_REF_BUF_SHARE_S stVencChnRefBufShare;
  VENC_CHN_ATTR_S stAttr;

  memset(&stAttr, 0, sizeof(stAttr));
  stAttr.stVencAttr.enType = RK_VIDEO_ID_JPEG;
  stAttr.stVencAttr.enPixelFormat = RK_FMT_YUV420SP;
  stAttr.stVencAttr.u32MaxPicWidth = thumb_width;
  stAttr.stVencAttr.u32MaxPicHeight = thumb_height;
  stAttr.stVencAttr.u32PicWidth = thumb_width;
  stAttr.stVencAttr.u32PicHeight = thumb_height;
  stAttr.stVencAttr.u32VirWidth = thumb_width;
  stAttr.stVencAttr.u32VirHeight = thumb_height;
  stAttr.stVencAttr.u32StreamBufCnt = 5;
  stAttr.stVencAttr.u32BufSize = thumb_width
        * thumb_height * 3 / 2;

  stIfd1[0] = {0x0100, 3, 1, thumb_width, {{.uwv = thumb_width}}}; // ImageWidth
  stIfd1[1] = {0x0100, 3, 1, thumb_height, {{.uwv = thumb_height}}}; // ImageLength

  ret = RKADK_MPI_VENC_Init(ChnId, &stAttr);
  if (ret != 0) {
    RKADK_LOGE("RKADK_MPI_VENC_Init failed, ret = %d", ret);
    return ret;
  }

  memset(&stVencChnRefBufShare, 0, sizeof(VENC_CHN_REF_BUF_SHARE_S));
  stVencChnRefBufShare.bEnable = RK_TRUE;

  RK_MPI_VENC_SetChnRefBufShareAttr(ChnId, &stVencChnRefBufShare);
  RK_MPI_VENC_EnableThumbnail(ChnId);

  stRecvParam.s32RecvPicNum = -1;
  RK_MPI_VENC_StartRecvFrame(ChnId, &stRecvParam);

  return 0;
}

RKADK_S32 ThumbnailInit(RKADK_U32 u32CamId, RKADK_U32 thumb_width,
                               RKADK_U32 thumb_height, RKADK_U32 venc_chn,
                               RKADK_U32 vi_chn) {
  int ret = 0;
  MPP_CHN_S stViChnThu, stVencChnThu;
  memset(&stViChnThu, 0, sizeof(MPP_CHN_S));
  memset(&stVencChnThu, 0, sizeof(MPP_CHN_S));

  stViChnThu.enModId = RK_ID_VI;
  stViChnThu.s32DevId = u32CamId;
  stViChnThu.s32ChnId = vi_chn;

  stVencChnThu.enModId = RK_ID_VENC;
  stVencChnThu.s32DevId = u32CamId;
  stVencChnThu.s32ChnId = venc_chn;
  ret = RKADK_Thumbnail_Vi(u32CamId, stViChnThu.s32ChnId,
                           thumb_width, thumb_height);
  if (ret != 0) {
    RKADK_LOGE("RKADK_PHOTO_ViThumBnail failed, ret = %d", ret);
    goto exit;
  }

  ret = RKADK_Thumbnail_Venc(stVencChnThu.s32ChnId,
                             thumb_width, thumb_height);
  if (ret) {
    RKADK_LOGE("RKADK_PHOTO_VencThumBnail failed, ret = %d", ret);
    goto exit;
  }

  ret = RK_MPI_SYS_Bind(&stViChnThu, &stVencChnThu);
    if (ret != RK_SUCCESS) {
        RKADK_LOGE("bind %d ch venc failed", stVencChnThu.s32ChnId);
        goto exit;
    }

  return ret;

exit:
  return -1;
}

RKADK_S32 ThumbnailDeInit(RKADK_U32 u32CamId,
                                 RKADK_U32 venc_chn,
                                 RKADK_U32 vi_chn) {
  int ret = 0;
  MPP_CHN_S stViChnThu, stVencChnThu;
  memset(&stViChnThu, 0, sizeof(MPP_CHN_S));
  memset(&stVencChnThu, 0, sizeof(MPP_CHN_S));

  stViChnThu.enModId = RK_ID_VI;
  stViChnThu.s32DevId = u32CamId;
  stViChnThu.s32ChnId = vi_chn;

  stVencChnThu.enModId = RK_ID_VENC;
  stVencChnThu.s32DevId = u32CamId;
  stVencChnThu.s32ChnId = venc_chn;

  ret = RK_MPI_SYS_UnBind(&stViChnThu, &stVencChnThu);
  if (ret) {
      RKADK_LOGE("unbind %d ch venc failed", venc_chn);
  }

  ret = RKADK_MPI_VI_DeInit(u32CamId, vi_chn);
  if (ret) {
      RKADK_LOGE("RK_MPI_VI_DisableChn %x", ret);
  }

  ret = RK_MPI_VENC_StopRecvFrame(venc_chn);
  if (ret) {
      RKADK_LOGE("stop venc thumbnail fail %x", ret);
  }
  ret = RKADK_MPI_VENC_DeInit(venc_chn);
  if (ret) {
      RKADK_LOGE("RK_MPI_VDEC_DestroyChn fail %x", ret);
  }

  return ret;
}

RKADK_S32 ThumbnailPhotoData(RKADK_U8 *pJpegdata, RKADK_U32 JpegLen,
                               VENC_STREAM_S stThuFrame,
                               RKADK_U8 *pNewPhoto) {
  int ret = 0;
  char *thumb_data;
  int thumb_len;
  int app0_len;
  int off_set;
  int userdata_len;

  //thumbnail
  thumb_data = (char *)RK_MPI_MB_Handle2VirAddr(stThuFrame.pstPack->pMbBlk);
  thumb_len = stThuFrame.pstPack->u32Len;
  RKADK_LOGI("Thumbnail seq = %d, data %p, size = %d", stThuFrame.u32Seq,
              thumb_data, thumb_len);

  app0_len = pJpegdata[5];
  off_set = app0_len + 4;
  memcpy(pNewPhoto, pJpegdata, off_set);
  pNewPhoto += off_set;
  userdata_len = PackageApp1(stIfd0, stIfd1, sizeof(stIfd0) / sizeof(IFD),
                sizeof(stIfd1) / sizeof(IFD), (char *)pNewPhoto, thumb_data, thumb_len);
  pNewPhoto += userdata_len;
  memcpy(pNewPhoto, pJpegdata + off_set, JpegLen - off_set);
  pNewPhoto -= (off_set + userdata_len);

  return JpegLen + userdata_len;
}

RKADK_S32 ThumbnailChnBind(RKADK_U32 u32VencChn, RKADK_U32 u32VencChnTb) {
  int ret;

  ret = RK_MPI_VENC_ThumbnailBind(u32VencChn, u32VencChnTb);
  if (ret != RK_SUCCESS) {
    RK_LOGE("thumbnail bind %d ch venc failed", u32VencChn);
    return ret;
  }

  return 0;
}

RKADK_S32 ThumbnailBuildIn(RKADK_CHAR *pszFileName,
                            RKADK_THUMB_ATTR_S *pstThumbAttr) {
  FILE *fd = NULL;
  int ret = -1;
  bool bBuildInThm = false;
  RKADK_U32 u32BoxSize;
  char boxHeader[THM_BOX_HEADER_LEN] = {0};
  char largeSize[THM_BOX_HEADER_LEN] = {0};

  fd = fopen(pszFileName, "r+");
  if (!fd) {
    RKADK_LOGE("open %s failed", pszFileName);
    return -1;
  }

  while (!feof(fd)) {
    if (fread(boxHeader, THM_BOX_HEADER_LEN, 1, fd) != 1) {
      if (feof(fd)) {
        RKADK_LOGD("EOF");
        bBuildInThm = true;
      } else {
        RKADK_LOGE("Can't read box header");
      }
      break;
    }

    u32BoxSize = boxHeader[0] << 24 | boxHeader[1] << 16 | boxHeader[2] << 8 |
                 boxHeader[3];

    if (!u32BoxSize) {
      if (!boxHeader[4] && !boxHeader[5] && !boxHeader[6] && !boxHeader[7]) {
        RKADK_LOGE("invalid data, cover!!!");

        if (fseek(fd, -THM_BOX_HEADER_LEN, SEEK_CUR))
          RKADK_LOGE("seek failed");
        else
          bBuildInThm = true;
      } else {
        RKADK_LOGE("u32BoxSize = %d, invalid data", u32BoxSize);
      }

      break;
    } else if (u32BoxSize == 1) {
      if (fread(largeSize, THM_BOX_HEADER_LEN, 1, fd) != 1) {
        RKADK_LOGE("read largeSize failed");
        break;
      }

      u32BoxSize = (RKADK_U64)largeSize[0] << 56 |
                   (RKADK_U64)largeSize[1] << 48 |
                   (RKADK_U64)largeSize[2] << 40 |
                   (RKADK_U64)largeSize[3] << 32 | largeSize[4] << 24 |
                   largeSize[5] << 16 | largeSize[6] << 8 | largeSize[7];

      if (fseek(fd, u32BoxSize - (THM_BOX_HEADER_LEN * 2), SEEK_CUR)) {
        RKADK_LOGE("largeSize seek failed");
        break;
      }
    } else {
      if (fseek(fd, u32BoxSize - THM_BOX_HEADER_LEN, SEEK_CUR)) {
        RKADK_LOGE("seek failed");
        break;
      }
    }
  }

  if (!bBuildInThm)
    goto exit;

  // 16: 4bytes width + 4bytes height + 4bytes VirWidth + 4bytes VirHeight
  u32BoxSize = pstThumbAttr->u32BufSize + THM_BOX_HEADER_LEN + 16;
  boxHeader[0] = u32BoxSize >> 24;
  boxHeader[1] = (u32BoxSize & 0x00FF0000) >> 16;
  boxHeader[2] = (u32BoxSize & 0x0000FF00) >> 8;
  boxHeader[3] = u32BoxSize & 0x000000FF;
  boxHeader[4] = 't';
  boxHeader[5] = 'h';
  boxHeader[6] = 'm';
  boxHeader[7] = pstThumbAttr->enType;

  if (fwrite(boxHeader, THM_BOX_HEADER_LEN, 1, fd) != 1) {
    RKADK_LOGE("write thm box header failed");
    goto exit;
  }

  if (fwrite(&(pstThumbAttr->u32Width), 4, 1, fd) != 1) {
    RKADK_LOGE("write thm width failed");
    goto exit;
  }

  if (fwrite(&(pstThumbAttr->u32Height), 4, 1, fd) != 1) {
    RKADK_LOGE("write thm height failed");
    goto exit;
  }

  if (fwrite(&(pstThumbAttr->u32VirWidth), 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual width failed");
    goto exit;
  }

  if (fwrite(&(pstThumbAttr->u32VirHeight), 4, 1, fd) != 1) {
    RKADK_LOGE("write thm virtual height failed");
    goto exit;
  }

  if (fwrite(pstThumbAttr->pu8Buf, pstThumbAttr->u32BufSize, 1, fd) != 1) {
    RKADK_LOGE("write thm box body failed");
    goto exit;
  }

  ret = 0;
  RKADK_LOGD("done!");

exit:
  if (fd)
    fclose(fd);

  return ret;
}