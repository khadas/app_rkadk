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

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "rkadk_storage.h"

#define SIZE_1MB (1024 * 1024)
#define MAX_CH 2
#define FILE_SIZE (64 * SIZE_1MB)

static bool quit = false;
static void SigtermHandler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

RKADK_S32 CreatFile(char *name, long size) {
  int fd;
  int ret;
  int wlLen;
  struct timeval tvAllBegin;
  struct timeval tvAllEnd;
  double timeCons;
  int bufLen = 4 * 1024;
  unsigned char buf[bufLen];

  RKADK_CHECK_POINTER(name, RKADK_FAILURE);
  RKADK_LOGI("Create file:%s size:%ld", name, size);
  gettimeofday(&tvAllBegin, NULL);

  fd = open(name, O_CREAT | O_RDWR);
  if (fd < 0) {
    RKADK_LOGE("Open file failed.");
    return -1;
  }

  gettimeofday(&tvAllEnd, NULL);
  timeCons = 1000 * (tvAllEnd.tv_sec - tvAllBegin.tv_sec) +
             ((tvAllEnd.tv_usec - tvAllBegin.tv_usec) / 1000.0);
  RKADK_LOGD("Open name:%s, timeCons = %fms", name, timeCons);

  if (fd) {
    while (size > 0 && !quit) {
      wlLen = (size > bufLen) ? bufLen : size;
      ret = write(fd, buf, wlLen);
      if (ret < 0) {
        RKADK_LOGE("Write failed.\n");
        close(fd);
        return -1;
      }
      size -= wlLen;
    }
    close(fd);
  }

  return 0;
}

static void *CreatFileThread(void *arg) {
  int ch = *(int *)arg;
  char name[400];
  time_t timep;
  struct tm *p;

  while (!quit) {
    usleep(100);
    time(&timep);
    p = gmtime(&timep);
    sprintf(name, "/mnt/sdcard/video%d/%d-%d-%d_%d-%d-%d.mp4", ch,
            (1900 + p->tm_year), (1 + p->tm_mon), p->tm_mday, p->tm_hour,
            p->tm_min, p->tm_sec);

    if (CreatFile(name, FILE_SIZE)) {
      RKADK_LOGE("Create file failed.");
      quit = true;
    }
  }

  return NULL;
}

RKADK_S32 CreatFileTest(RKADK_MW_PTR *ppHandle) {
  int ret;
  int i;
  int totalSize;
  int freeSize;
  pthread_t tid[MAX_CH];

  for (i = 0; i < MAX_CH; i++) {
    ret = pthread_create(&tid[i], NULL, CreatFileThread, (void *)(&i));

    if (ret) {
      RKADK_LOGE("pthread_create failed.");
      return -1;
    }
    usleep(500);
  }

  while (!quit) {
    usleep(5000);
    RKADK_LOGD("sync start");
    sync();
    RKADK_LOGD("sync end");
    RKADK_STORAGE_GetSdcardSize(ppHandle, &totalSize, &freeSize);
    RKADK_LOGI("sdcard totalSize: %d, freeSize: %d", totalSize, freeSize);

    if (RKADK_STORAGE_GetMountStatus(*ppHandle) == DISK_UNMOUNTED)
      quit = true;
  }

  for (i = 0; i < MAX_CH; i++) {
    ret = pthread_join(tid[i], NULL);

    if (ret) {
      RKADK_LOGE("pthread_join failed.");
      return -1;
    }
  }
  sync();

  return 0;
}

RKADK_S32 SetDevAttr(RKADK_STR_DEV_ATTR *pstDevAttr) {
  RKADK_S32 i;

  RKADK_CHECK_POINTER(pstDevAttr, RKADK_FAILURE);
  RKADK_LOGD("The DevAttr will be user-defined.");

  memset(pstDevAttr, 0, sizeof(RKADK_STR_DEV_ATTR));
  sprintf(pstDevAttr->cMountPath, "/mnt/sdcard");
  pstDevAttr->s32AutoDel = 1;
  pstDevAttr->s32FreeSizeDelMin = 1200;
  pstDevAttr->s32FreeSizeDelMax = 1800;
  pstDevAttr->s32FolderNum = 3;
  pstDevAttr->pstFolderAttr = (RKADK_STR_FOLDER_ATTR *)malloc(
      sizeof(RKADK_STR_FOLDER_ATTR) * pstDevAttr->s32FolderNum);

  if (!pstDevAttr->pstFolderAttr) {
    RKADK_LOGE("pstDevAttr->pstFolderAttr malloc failed.");
    return -1;
  }
  memset(pstDevAttr->pstFolderAttr, 0,
         sizeof(RKADK_STR_FOLDER_ATTR) * pstDevAttr->s32FolderNum);

  for (i = 0; i < pstDevAttr->s32FolderNum; i++) {
    pstDevAttr->pstFolderAttr[i].s32Limit = 33;
    sprintf(pstDevAttr->pstFolderAttr[i].cFolderPath, "/video%d/", i);
  }

  return 0;
}

RKADK_S32 FreeDevAttr(RKADK_STR_DEV_ATTR devAttr) {
  if (devAttr.pstFolderAttr) {
    free(devAttr.pstFolderAttr);
    devAttr.pstFolderAttr = NULL;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  RKADK_S32 i;
  RKADK_MW_PTR pHandle = NULL;
  RKADK_STR_DEV_ATTR stDevAttr;
  RKADK_FILE_LIST list;

  memset(&list, 0, sizeof(RKADK_FILE_LIST));
  sprintf(list.path, "/mnt/sdcard/video0/");

  if (argc > 0)
    RKADK_LOGI("%s run", argv[0]);

  if (SetDevAttr(&stDevAttr)) {
    RKADK_LOGE("Set devAttr failed.");
    return -1;
  }

  if (RKADK_STORAGE_Init(&pHandle, &stDevAttr)) {
    RKADK_LOGE("Storage init failed.");
    return -1;
  }

  signal(SIGINT, SigtermHandler);
  if (CreatFileTest(&pHandle))
    RKADK_LOGW("CreatFileTest failed.");

  while (!quit) {
    usleep(5000);
  }

  if (!RKADK_STORAGE_GetFileList(&list, pHandle)) {
    for (i = 0; i < list.s32FileNum; i++) {
      RKADK_LOGI("%s  %lld", list.file[i].filename, list.file[i].stSize);
    }
  }
  RKADK_STORAGE_FreeFileList(list);
  FreeDevAttr(stDevAttr);

  RKADK_STORAGE_Deinit(pHandle);
  RKADK_LOGD("%s out", argv[0]);

  return 0;
}
