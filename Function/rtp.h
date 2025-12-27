/************************************************************************
  * Copyright(c) 2023 Levetop Semiconductor Co.,Led. All rights reserved.
  * @file     rtp.h
  * @author   UartTFT Application Team
  * @version  V1.0.0
  * @date     2023-02-24
  * @brief     
 *************************************************************************/
#ifndef _rtp_h
#define _rtp_h

#include "LT768_Lib.h"

#include <math.h>
#include <stdlib.h>
#include "delay.h"
#include "data.h"

uint8_t LT_TpInit(void);
uint8_t LT_TpScan(void);
uint8_t LT_TpAdjust(void);
void LT_TpSaveAdjdata(void);
uint8_t LT_TpGetAdjdata(void);
uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y);
uint8_t LT_TpInit(void);
#endif
