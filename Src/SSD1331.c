#include "stm32f3xx.h"
#include "stm32f3xx_hal_gpio.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_hal_spi.h"

#include "stdio.h"
#include "main.h"

#include "SSD1331.h"

static unsigned char CHR_X, CHR_Y;

void _sendCmd(uint8_t cmd)
{
    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_RESET); // DC = 0 (komenda)
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET); // CS = 0
    HAL_SPI_Transmit(&hspi3, &cmd, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);   // CS = 1
}

void _sendData(uint8_t data)
{
    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin, GPIO_PIN_SET);   // DC = 1 (dane)
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_RESET); // CS = 0
    HAL_SPI_Transmit(&hspi3, &data, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);   // CS = 1
}

void SSD1331_init(void)
{
    // Reset ekranu
    HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    // Inicjalizacja SSD1331
    _sendCmd(CMD_DISPLAY_OFF);
    _sendCmd(CMD_SET_CONTRAST_A); _sendCmd(0x91);
    _sendCmd(CMD_SET_CONTRAST_B); _sendCmd(0x50);
    _sendCmd(CMD_SET_CONTRAST_C); _sendCmd(0x7D);
    _sendCmd(CMD_MASTER_CURRENT_CONTROL); _sendCmd(0x06);
    _sendCmd(CMD_SET_PRECHARGE_SPEED_A); _sendCmd(0x64);
    _sendCmd(CMD_SET_PRECHARGE_SPEED_B); _sendCmd(0x78);
    _sendCmd(CMD_SET_PRECHARGE_SPEED_C); _sendCmd(0x64);
    _sendCmd(CMD_SET_REMAP); _sendCmd(0x72);
    _sendCmd(CMD_SET_DISPLAY_START_LINE); _sendCmd(0x0);
    _sendCmd(CMD_SET_DISPLAY_OFFSET); _sendCmd(0x0);
    _sendCmd(CMD_NORMAL_DISPLAY);
    _sendCmd(CMD_SET_MULTIPLEX_RATIO); _sendCmd(0x3F);
    _sendCmd(CMD_SET_MASTER_CONFIGURE); _sendCmd(0x8E);
    _sendCmd(CMD_POWER_SAVE_MODE); _sendCmd(0x00);
    _sendCmd(CMD_PHASE_PERIOD_ADJUSTMENT); _sendCmd(0x31);
    _sendCmd(CMD_DISPLAY_CLOCK_DIV); _sendCmd(0xF0);
    _sendCmd(CMD_SET_PRECHARGE_VOLTAGE); _sendCmd(0x3A);
    _sendCmd(CMD_SET_V_VOLTAGE); _sendCmd(0x3E);
    _sendCmd(CMD_DEACTIVE_SCROLLING);
    _sendCmd(CMD_NORMAL_BRIGHTNESS_DISPLAY_ON);
}
void SSD1331_drawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x < 0) || (x >= RGB_OLED_WIDTH) || (y < 0) || (y >= RGB_OLED_HEIGHT))
        return;
    //set column point
    _sendCmd(CMD_SET_COLUMN_ADDRESS);
    _sendCmd(x);
    _sendCmd(RGB_OLED_WIDTH-1);
    //set row point
    _sendCmd(CMD_SET_ROW_ADDRESS);
    _sendCmd(y);
    _sendCmd(RGB_OLED_HEIGHT-1);

    HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin,GPIO_PIN_SET);; //cs
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_RESET);; //cs

	_sendData(color >> 8);
	_sendData(color);

	HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_SET);; //cs

}

void SSD1331_drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if((x0 < 0) || (y0 < 0) || (x1 < 0) || (y1 < 0))
        return;

    if (x0 >= RGB_OLED_WIDTH)  x0 = RGB_OLED_WIDTH - 1;
    if (y0 >= RGB_OLED_HEIGHT) y0 = RGB_OLED_HEIGHT - 1;
    if (x1 >= RGB_OLED_WIDTH)  x1 = RGB_OLED_WIDTH - 1;
    if (y1 >= RGB_OLED_HEIGHT) y1 = RGB_OLED_HEIGHT - 1;

    _sendCmd(CMD_DRAW_LINE);//draw line
    _sendCmd(x0);//start column
    _sendCmd(y0);//start row
    _sendCmd(x1);//end column
    _sendCmd(y1);//end row
    _sendCmd((uint8_t)((color>>11)&0x1F));//R
    _sendCmd((uint8_t)((color>>5)&0x3F));//G
    _sendCmd((uint8_t)(color&0x1F));//B
}

void SSD1331_drawFrame(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t outColor, uint16_t fillColor)
{
    if((x0 < 0) || (y0 < 0) || (x1 < 0) || (y1 < 0))
        return;

    if (x0 >= RGB_OLED_WIDTH)  x0 = RGB_OLED_WIDTH - 1;
    if (y0 >= RGB_OLED_HEIGHT) y0 = RGB_OLED_HEIGHT - 1;
    if (x1 >= RGB_OLED_WIDTH)  x1 = RGB_OLED_WIDTH - 1;
    if (y1 >= RGB_OLED_HEIGHT) y1 = RGB_OLED_HEIGHT - 1;

    _sendCmd(CMD_FILL_WINDOW);//fill window
    _sendCmd(ENABLE_FILL);
    _sendCmd(CMD_DRAW_RECTANGLE);//draw rectangle
    _sendCmd(x0);//start column
    _sendCmd(y0);//start row
    _sendCmd(x1);//end column
    _sendCmd(y1);//end row
    _sendCmd((uint8_t)((outColor>>11)&0x1F));//R
    _sendCmd((uint8_t)((outColor>>5)&0x3F));//G
    _sendCmd((uint8_t)(outColor&0x1F));//B
    _sendCmd((uint8_t)((fillColor>>11)&0x1F));//R
    _sendCmd((uint8_t)((fillColor>>5)&0x3F));//G
    _sendCmd((uint8_t)(fillColor&0x1F));//B
}

void SSD1331_drawCircle(uint16_t x, uint16_t y, uint16_t radius, uint16_t color) {
	signed char xc = 0;
	signed char yc = 0;
	signed char p = 0;

    // Out of range
    if (x >= RGB_OLED_WIDTH || y >= RGB_OLED_HEIGHT)
        return;

    yc = radius;
    p = 3 - (radius<<1);
    while (xc <= yc)
    {
    	SSD1331_drawPixel(x + xc, y + yc, color);
    	SSD1331_drawPixel(x + xc, y - yc, color);
    	SSD1331_drawPixel(x - xc, y + yc, color);
    	SSD1331_drawPixel(x - xc, y - yc, color);
    	SSD1331_drawPixel(x + yc, y + xc, color);
    	SSD1331_drawPixel(x + yc, y - xc, color);
    	SSD1331_drawPixel(x - yc, y + xc, color);
    	SSD1331_drawPixel(x - yc, y - xc, color);
        if (p < 0) p += (xc++ << 2) + 6;
            else p += ((xc++ - yc--)<<2) + 10;
    }

}

// Set current position in cache
void SSD1331_SetXY(unsigned char x, unsigned char y) {
	CHR_X = x;
	CHR_Y = y;
}

void SSD1331_XY_INK(LcdFontSize size) {
	CHR_X += 6*size;
	if (CHR_X + 6*size > RGB_OLED_WIDTH) {
		CHR_X = 0;
		CHR_Y += 8*size;
		if (CHR_Y + 8*size > RGB_OLED_HEIGHT) {
			CHR_Y = 0;
		}
	}
}

void SSD1331_Chr(LcdFontSize size, unsigned char ch, uint16_t chr_color, uint16_t bg_color) {
	unsigned char y, x, sx, sy;
	uint16_t color;
	/////uint16_t cx=CHR_X*6*size;
	uint16_t cx=CHR_X;
	/////uint16_t cy=CHR_Y*8*size;
	uint16_t cy=CHR_Y;

	if ( (cx + 6*size > RGB_OLED_WIDTH) || (cy + 8*size > RGB_OLED_HEIGHT) ) {
		return;
	}

	// CHR
    if ( (ch >= 0x20) && (ch <= 0x7F) )
    {
        // offset in symbols table ASCII[0x20-0x7F]
        ch -= 32;
    }
    else if (ch >= 0xC0)
    {
        // offset in symbols table CP1251[0xC0-0xFF] (Cyrillic)
        ch -= 96;
    }
    else
    {
        // Ignore unknown symbols
        ch = 95;
    }

    if ((size > FONT_1X) & (ch > 15) & (ch < 26)) {
        ch -= 16;
    	for (sy = 0; sy<size; sy++) {
    	for (y = 0; y<8; y++ ) {
    		//set column point
    		_sendCmd(CMD_SET_COLUMN_ADDRESS);
    		_sendCmd(cx);
    		_sendCmd(RGB_OLED_WIDTH-1);
    		//set row point
    		_sendCmd(CMD_SET_ROW_ADDRESS);
    		_sendCmd(y + cy + sy*8);
    		_sendCmd(RGB_OLED_HEIGHT-1);
    		HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin,GPIO_PIN_SET);//cs
    		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_RESET); //cs
    		for (x = 0; x < 5*size; x++ ) {
    			if ( (((BigNumbers[ch][x+sy*10] >> y) & 0x01 ) & (size == FONT_2X)) |
    				 (((LargeNumbers[ch][x+sy*20] >> y) & 0x01 ) & (size == FONT_4X))

    				) {
    				color = chr_color;
    			}
    			else {
    				color = bg_color;
    			}
				_sendData(color >> 8);
				_sendData(color);
    		}
    	}
    	}
    }
    else {
    	for (y = 0; y<8; y++ ) {
    		for (sy = 0; sy<size; sy++ ) {
    			//set column point
    			_sendCmd(CMD_SET_COLUMN_ADDRESS);
    			_sendCmd(cx);
    			_sendCmd(RGB_OLED_WIDTH-1);
    			//set row point
    			_sendCmd(CMD_SET_ROW_ADDRESS);
    			_sendCmd(y*size + sy + cy);
    			_sendCmd(RGB_OLED_HEIGHT-1);
        		HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin,GPIO_PIN_SET);; //cs
        		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_RESET);; //cs
    			for (x = 0; x<5; x++ ) {
    				if ((FontLookup[ch][x] >> y) & 0x01) {
    					color = chr_color;
    				}
    				else {
    					color = bg_color;
    				}
    				//SSD1331_drawPixel(x+cx, y+cy, color);
    				for (sx = 0; sx<size; sx++ ) {
    					_sendData(color >> 8);
    					_sendData(color);
    				}
    			}
    			_sendData(bg_color >> 8);
    			_sendData(bg_color);
        		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_SET);; //cs

    		}
    	}
    }

    /////CHR_X++;
    //CHR_X += 6*size;
}

// Print a string to display
void SSD1331_Str(LcdFontSize size, unsigned char dataArray[], uint16_t chr_color, uint16_t bg_color) {
    unsigned char tmpIdx=0;

    while( dataArray[ tmpIdx ] != '\0' )
    {
        /*/////
    	if (CHR_X > 15) {
        	CHR_X = 0;
        	CHR_Y++;
        	if (CHR_Y > 7) {
        		CHR_Y = 0;
        	}
        }
        */
    	/*
    	if (CHR_X + 6*size > RGB_OLED_WIDTH) {
        	CHR_X = 0;
        	CHR_Y += 8*size;
        	if (CHR_Y + 8*size > RGB_OLED_HEIGHT) {
        		CHR_Y = 0;
        	}
        }*/

        SSD1331_Chr(size, dataArray[ tmpIdx ], chr_color, bg_color);
        SSD1331_XY_INK(size);
        tmpIdx++;
    }
}

// Print a string from the Flash to display
void SSD1331_FStr(LcdFontSize size, const unsigned char *dataPtr, uint16_t chr_color, uint16_t bg_color) {
    unsigned char c;
    for (c = *( dataPtr ); c; ++dataPtr, c = *( dataPtr ))
    {
        /*
    	if (CHR_X > 15) {
        	CHR_X = 0;
        	CHR_Y++;
        	if (CHR_Y > 7) {
        		CHR_Y = 0;
        	}
        }
        */

        SSD1331_Chr(size, c, chr_color, bg_color);
        SSD1331_XY_INK(size);
    }
}

void SSD1331_IMG(const unsigned char *img, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
	uint16_t xx, yy;

	if ( (x + width > RGB_OLED_WIDTH) | (y+height > RGB_OLED_HEIGHT) ){
		return;
	}

	for (yy=0; yy<height; yy++) {
		//set column point
		_sendCmd(CMD_SET_COLUMN_ADDRESS);
		_sendCmd(x);
		_sendCmd(RGB_OLED_WIDTH-1);
		//set row point
		_sendCmd(CMD_SET_ROW_ADDRESS);
		_sendCmd(y + yy);
		_sendCmd(RGB_OLED_HEIGHT-1);
		HAL_GPIO_WritePin(DC_GPIO_Port, DC_Pin,GPIO_PIN_SET);; //cs
		HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin,GPIO_PIN_RESET);; //cs
		for (xx=0; xx<width*2; xx++) {
			_sendData(img[yy*width*2 + xx]);
		}
	}
}

void SSD1331_copyWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,uint16_t x2, uint16_t y2)
{
    _sendCmd(CMD_COPY_WINDOW);//copy window
    _sendCmd(x0);//start column
    _sendCmd(y0);//start row
    _sendCmd(x1);//end column
    _sendCmd(y1);//end row
    _sendCmd(x2);//new column
    _sendCmd(y2);//new row
}

void SSD1331_dimWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    _sendCmd(CMD_DIM_WINDOW);//copy area
    _sendCmd(x0);//start column
    _sendCmd(y0);//start row
    _sendCmd(x1);//end column
    _sendCmd(y1);//end row
}

void SSD1331_clearWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    _sendCmd(CMD_CLEAR_WINDOW);//clear window
    _sendCmd(x0);//start column
    _sendCmd(y0);//start row
    _sendCmd(x1);//end column
    _sendCmd(y1);//end row
}

void SSD1331_setScrolling(ScollingDirection direction, uint8_t rowAddr, uint8_t rowNum, uint8_t timeInterval)
{
    uint8_t scolling_horizontal = 0x0;
    uint8_t scolling_vertical = 0x0;
    switch(direction){
        case Horizontal:
            scolling_horizontal = 0x01;
            scolling_vertical = 0x00;
            break;
        case Vertical:
            scolling_horizontal = 0x00;
            scolling_vertical = 0x01;
            break;
        case Diagonal:
            scolling_horizontal = 0x01;
            scolling_vertical = 0x01;
            break;
        default:
            break;
    }
    _sendCmd(CMD_CONTINUOUS_SCROLLING_SETUP);
    _sendCmd(scolling_horizontal);
    _sendCmd(rowAddr);
    _sendCmd(rowNum);
    _sendCmd(scolling_vertical);
    _sendCmd(timeInterval);
    _sendCmd(CMD_ACTIVE_SCROLLING);
}

void SSD1331_enableScrolling(bool enable)
{
    if(enable)
        _sendCmd(CMD_ACTIVE_SCROLLING);
    else
        _sendCmd(CMD_DEACTIVE_SCROLLING);
}

void SSD1331_setDisplayMode(DisplayMode mode)
{
    _sendCmd(mode);
}

void SSD1331_setDisplayPower(DisplayPower power)
{
    _sendCmd(power);
}

void SSD1331_DisplayON(void)
{
	_sendCmd(CMD_NORMAL_BRIGHTNESS_DISPLAY_ON);
}
void SSD1331_DisplayOFF(void){
	_sendCmd(CMD_DISPLAY_OFF);
}
