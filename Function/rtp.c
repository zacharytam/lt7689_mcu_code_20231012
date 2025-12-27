/************************************************************************
  * Copyright(c) 2023 Levetop Semiconductor Co.,Led. All rights reserved.
  * @file     rtp.c
  * @author   UartTFT Application Team
  * @version  V1.0.0
  * @date     2023-02-24
  * @brief     
 *************************************************************************/

#include "rtp.h"
#include "tp_drv.h"
#include "i2c_drv.h"
#include "uart_drv.h"
#include "iomacros.h"
uint8_t Rtp_Cor = 0;
void LT_TpDelay(void)
{
	uint8_t t = 1;
	while (t--)
	{
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
		__NOP();
	}
}

#define CMD_RDX 0XD0
#define CMD_RDY 0X90

// SPI write data
void TP_Write_Byte(uint8_t num)
{
	uint8_t count = 0;
	for (count = 0; count < 8; count++)
	{
		if (num & 0x80)
			TDIN_SET;
		else
			TDIN_RESET;
		num <<= 1;
		TCLK_RESET;
		DelayUS(8);
		TCLK_SET;
		DelayUS(8);
	}
}

// SPI read data
// Read adc value from touch screen IC
// CMD:instruct
// Return value: read data
int16_t TP_Read_AD(CMD)
{
	uint8_t count = 0;
	uint16_t Num = 0;
	TCLK_RESET; // Lower the clock first
	TDIN_RESET; // Pull down the data cable

	TCS_RESET; // Select touch screen IC

	TP_Write_Byte(CMD);

	DelayUS(8); // The maximum conversion time of ADS7846 is 6us
	TCLK_RESET;
	DelayUS(8);
	TCLK_SET;
	DelayUS(8);
	TCLK_RESET;
	DelayUS(8);

	for (count = 0; count < 16; count++) // Read 16-bit data, only the upper 12 bits are valid
	{
		Num <<= 1;
		TCLK_RESET; // Falling edge effective
		DelayUS(8);
		TCLK_SET;
		DelayUS(8);

		if (GET_SDO_Bit)
			Num++;
		// printf("eport=%d\r\n",EPORT_ReadGpioData(EPORT_PIN3));
	}
	Num >>= 4;

	// Only the upper 12 bits are valid.
	TCS_SET; // Release the selection
	return Num;
}

//Read a coordinate value (x or y)
//Read READ continuously_ TIMES secondary data is arranged in ascending order,
//Then remove the lowest and highest LOST_ Number of VALs, take the average value
//Xy: Instruction (CMD_RDX/CMD_RDY)
//Return value: read data
#define READ_TIMES 8 // Reads
#define LOST_VAL 2	 // Discard value
uint16_t TP_Read_XOY(uint8_t xy)
{
	uint16_t i, j;
	uint16_t buf[READ_TIMES];
	uint16_t sum = 0;
	uint16_t temp;
	for (i = 0; i < READ_TIMES; i++)
		buf[i] = TP_Read_AD(xy);
	for (i = 0; i < READ_TIMES - 1; i++) // sort
	{
		for (j = i + 1; j < READ_TIMES; j++)
		{
			if (buf[i] > buf[j]) // Ascending
			{
				temp = buf[i];
				buf[i] = buf[j];
				buf[j] = temp;
			}
		}
	}
	sum = 0;
	for (i = LOST_VAL; i < READ_TIMES - LOST_VAL; i++)
		sum += buf[i];
	temp = sum / (READ_TIMES - 2 * LOST_VAL);
	return temp;
}

/********************************************************************************
* Function Name	: TP_Read_XY
* Description  	: Read the touch screen IC for two consecutive times, and the deviation between the two times cannot exceed.
* Input        	: - x,y: Read coordinate value
* Output       	: None
* Return       	: 0:error  1:ok
*********************************************************************************/

uint8_t TP_Read_XY(uint16_t *x, uint16_t *y)
{
	*x = TP_Read_XOY(CMD_RDX);
	*y = TP_Read_XOY(CMD_RDY);
	if (*x > 4095 || *y > 4095)
		return 0; // Reading failed
	//	printf("%d %d\r\n",*x,*y);
	return 1;
}

/********************************************************************************
* Function Name	: TP_Read_XY2
* Description  	: Read the touch screen IC for two consecutive times, and the deviation between the two times cannot exceed.
* Input        	: - x,y: Read coordinate value
* Output       	: None
* Return       	: 0:error  1:ok
*********************************************************************************/

#define ERR_RANGE 50 // Error range
uint8_t TP_Read_XY2(uint16_t *x, uint16_t *y)
{
	uint16_t x1, y1;
	uint16_t x2, y2;
	uint8_t flag;
	flag = TP_Read_XY(&x1, &y1);
	if (flag == 0)
		return (0);
	flag = TP_Read_XY(&x2, &y2);
	if (flag == 0)
		return (0);
	if (((x2 <= x1 && x1 < x2 + ERR_RANGE) || (x1 <= x2 && x2 < x1 + ERR_RANGE)) // 前后两次采样在+-50内
		&& ((y2 <= y1 && y1 < y2 + ERR_RANGE) || (y1 <= y2 && y2 < y1 + ERR_RANGE)))
	{
		*x = (x1 + x2) / 2;
		*y = (y1 + y2) / 2;
		return 1;
	}
	else
	{
		return 0;
	}
}

// Display correction point
void LT_TpDrawTouchPoint(uint16_t x, uint16_t y, uint32_t color)
{
	LT768_DrawLine(x - 12, y, x + 13, y, color); // Horizontal line
	LT768_DrawLine(x, y - 12, x, y + 13, color); // Vertical line
	LT768_DrawCircle(x, y, 6, color);			 // Draw center circle
}

/********************************************************************************
* Function Name	: TP_Read_XY2
* Description  	: touch scan.
* Input        	: None
* Output       	: None
* Return       	: 0:have touch event  1:no touch event
*********************************************************************************/
uint8_t LT_TpScan(void)
{
	if ((EPORT_ReadGpioData(EPORT_PIN4) == 0))
	{
		if (TP_Read_XY2(&gTpInfo.px, &gTpInfo.py))
		{
			Backlight_count = 0;
			//Backlight_touch = 1;

			if ((Backlight_flag == 0 || Backlight_touch == 2 ) && var[2 * 0x700e + 1] == 1 )
			{
				if (buzzer && Backlight_flag == 0)
					touch_buzzer();
				
				Set_Timer1_Compare_Buffer(gDutyBuf[var[0x7001 * 2 + 1]]);
				
				if (var[0x700f * 2 + 1] == 0)
					Start_PWM1();
				
				Backlight_touch = 2;
				gTpInfo.sta = 0;
				First_press = 0;
				Backlight_flag = 1;
			}
			else
			{
				gTpInfo.x[0] = gTpInfo.xfac * gTpInfo.px + gTpInfo.xoff;
				gTpInfo.y[0] = gTpInfo.yfac * gTpInfo.py + gTpInfo.yoff;

				if (gTpInfo.x[0] > LCD_XSIZE_TFT)
					gTpInfo.x[0] = LCD_XSIZE_TFT;
				if (gTpInfo.y[0] > LCD_YSIZE_TFT)
					gTpInfo.y[0] = LCD_YSIZE_TFT;
				
				if (First_press == 0)
				{
					First_press = 1;
					First_pressX = gTpInfo.x[0];
					First_pressY = gTpInfo.y[0];
				}

				/* debug
					printf("X=%d Y=%d \r\n",gTpInfo.x[0],gTpInfo.y[0]);
				*/
				
				if (gTpInfo.sta == 0)
					gTpInfo.sta = 1;
				return 1;
			}
		}
		else
		{
			return 0;
		}
	}
	else
	{
		
		gTpInfo.sta = 0;
		if(Backlight_flag == 1) Backlight_touch = 0;
		/* debug
			printf("x=%d y=%d\r\n",gTpInfo.x[0],gTpInfo.y[0]);
			printf("gTpInfo.sta=%d \r\n",gTpInfo.sta);
		*/
		return 0;
	}
}

uint8_t LT_TpAdjustScan(void)
{
	
	if (EPORT_ReadGpioData(EPORT_PIN4) == 0) //&& Rtp_temp==1)
	{
		if (TP_Read_XY2(&gTpInfo.px, &gTpInfo.py))
		{
			gTpInfo.x[0] = gTpInfo.xfac * gTpInfo.px + gTpInfo.xoff;
			gTpInfo.y[0] = gTpInfo.yfac * gTpInfo.py + gTpInfo.yoff;
		}
		if (gTpInfo.x[0] > LCD_XSIZE_TFT)
			gTpInfo.x[0] = LCD_XSIZE_TFT;
		if (gTpInfo.y[0] > LCD_YSIZE_TFT)
			gTpInfo.y[0] = LCD_YSIZE_TFT;
		if (gTpInfo.sta == 0)
			gTpInfo.sta = 1;
		return 1;
	}
	else
	{
		gTpInfo.sta = 0;
		/* debug
			printf("x=%d    y=%d\r\n",gTpInfo.x[0],gTpInfo.y[0]);
		*/
		return 0;
	}
}
// Prompt string
uint8_t *const TP_REMIND_MSG_TBL = (uint8_t *)"Please correct the resistance screen";

// Touch screen calibration
// Get four calibration parameters
uint8_t LT_TpAdjust(void)
{
	uint8_t cnt = 0;
	uint16_t outTime = 0;
	uint16_t d1, d2;
	uint32_t temp1, temp2;
	uint16_t posTemp[2][4] = {0};
	float fac;

	Canvas_Image_Start_address(0);
	Canvas_image_width(LCD_XSIZE_TFT);
	LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
	LT768_Select_Internal_Font_Init(32, 1, 1, 0, 0);
	LT768_Print_Internal_Font_String(40, 40, Red, White, (char *)TP_REMIND_MSG_TBL);

	LT_TpDrawTouchPoint(20, 20, Red);

	while (1)
	{                                                      
		
		WDT_FeedDog(); //Clear watchdog
		LT_TpAdjustScan();
		
		if (gTpInfo.sta == 1)
		{
			outTime = 0;
			gTpInfo.sta = 2;

			posTemp[0][cnt] = gTpInfo.px;
			posTemp[1][cnt] = gTpInfo.py;
			cnt++;
			switch (cnt)
			{
			case 1:
				LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
				LT_TpDrawTouchPoint(LCD_XSIZE_TFT - 20, 20, Red);
				break;

			case 2:
				LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
				LT_TpDrawTouchPoint(20, LCD_YSIZE_TFT - 20, Red);
				break;

			case 3:
				LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
				LT_TpDrawTouchPoint(LCD_XSIZE_TFT - 20, LCD_YSIZE_TFT - 20, Red);
				break;

			case 4: 
				temp1 = abs(posTemp[0][0] - posTemp[0][1]);
				temp2 = abs(posTemp[1][0] - posTemp[1][1]);
				temp1 *= temp1;
				temp2 *= temp2;
				d1 = sqrt(temp1 + temp2);

				temp1 = abs(posTemp[0][2] - posTemp[0][3]);
				temp2 = abs(posTemp[1][2] - posTemp[1][3]);
				temp1 *= temp1;
				temp2 *= temp2;
				d2 = sqrt(temp1 + temp2);
				fac = (float)d1 / d2;

				if (fac < 0.95f || fac > 1.05f || d1 == 0 || d2 == 0)
				{
					cnt = 0;
					LT_TpDrawTouchPoint(LCD_XSIZE_TFT - 20, LCD_YSIZE_TFT - 20, White);
					LT_TpDrawTouchPoint(20, 20, Red);
					LT768_Print_Internal_Font_String(200, 180, Red, White, "Check Failure,Please recheck!");
					continue;
				}

				// Calculate the ratio of the left and right sides
				temp1 = abs(posTemp[0][0] - posTemp[0][2]);
				temp2 = abs(posTemp[1][0] - posTemp[1][2]);
				temp1 *= temp1;
				temp2 *= temp2;
				d1 = sqrt(temp1 + temp2);

				temp1 = abs(posTemp[0][1] - posTemp[0][3]);
				temp2 = abs(posTemp[1][1] - posTemp[1][3]);
				temp1 *= temp1;
				temp2 *= temp2;
				d2 = sqrt(temp1 + temp2);

				if (fac < 0.95f || fac > 1.05f || d1 == 0 || d2 == 0)
				{
					cnt = 0;
					LT_TpDrawTouchPoint(LCD_XSIZE_TFT - 20, LCD_YSIZE_TFT - 20, White);
					LT_TpDrawTouchPoint(20, 20, Red);
					LT768_Print_Internal_Font_String(200, 180, Red, White, "Check Failure,Please recheck!");
					continue;
				}

				// 计算相对侧的长度比例
				temp1 = abs(posTemp[0][0] - posTemp[0][3]);
				temp2 = abs(posTemp[1][0] - posTemp[1][3]);
				temp1 *= temp1;
				temp2 *= temp2;
				d1 = sqrt(temp1 + temp2);

				temp1 = abs(posTemp[0][1] - posTemp[0][2]);
				temp2 = abs(posTemp[1][1] - posTemp[1][2]);
				temp1 *= temp1;
				temp2 *= temp2;
				d2 = sqrt(temp1 + temp2);

				fac = (float)d1 / d2;

				if (fac < 0.95f || fac > 1.05f || d1 == 0 || d2 == 0) 
				{
					cnt = 0;
					LT_TpDrawTouchPoint(LCD_XSIZE_TFT - 20, LCD_YSIZE_TFT - 20, White);
					LT_TpDrawTouchPoint(20, 20, Red);
					LT768_Print_Internal_Font_String(200, 180, Red, White, "Check Failure,Please recheck!");
					continue;
				}

				/* If the ratio of the upper and lower sides, the ratio of the left and right sides, 
					and the ratio of the diagonal side are all within the range, the four correction points meet the requirements*/
				gTpInfo.xfac = (float)(LCD_XSIZE_TFT - 40) / (posTemp[0][1] - posTemp[0][0]);
				gTpInfo.xoff = (LCD_XSIZE_TFT - gTpInfo.xfac * (posTemp[0][1] + posTemp[0][0])) / 2;
				gTpInfo.yfac = (float)(LCD_YSIZE_TFT - 40) / (posTemp[1][2] - posTemp[1][0]);
				gTpInfo.yoff = (LCD_YSIZE_TFT - gTpInfo.yfac * (posTemp[1][2] + posTemp[1][0])) / 2;
				gTpxfac.xfac = gTpInfo.xfac;
				gTpxoff.xoff = gTpInfo.xoff;
				gTpyfac.yfac = gTpInfo.yfac;
				gTpyoff.yoff = gTpInfo.yoff;
				LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
				LT768_Print_Internal_Font_String(200, 180, Red, White, "Touch Screen Adjust OK!"); // Calibration successful
				DelayMS(1000);

				//								printf("%f",gTpInfo.xfac);
				//								printf("%f",gTpInfo.xoff);
				//								printf("%f",gTpInfo.yfac);
				//								printf("%f",gTpInfo.yoff);

				// Save 4 parameters
				LT_TpSaveAdjdata();
				return 1;
			}
		}
		DelayMS(10);
		outTime++;
		if (outTime > 4000)
		{
			// Get the 4 parameters previously saved
			LT_TpGetAdjdata();
			LT768_DrawSquare_Fill(0, 0, LCD_XSIZE_TFT, LCD_YSIZE_TFT, White);
			LT768_Print_Internal_Font_String(200, 180, Red, White, "Touch Screen Adjust Failure!"); // Calibration failed
			return 1;
		}
	}
}

uint8_t LT_TpInit(void)
{
	RTP_io_Init();
	DelayMS(10);

	if (LT_TpGetAdjdata() != 1)
	{
		Display_ON();
		DelayMS(100);
		LT768_PWM1_Init(1, 0, 5, 800, 450);

		Rtp_Cor = 1;
		LT_TpAdjust();
	}
	Rtp_Cor = 0;
	TP_Read_XY2(&gTpInfo.px, &gTpInfo.py);

	gTpInfo.scan = LT_TpScan;
	return 1;
}
