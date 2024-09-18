#include "oled.h"

#include "font.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static uint8_t buffer[OLED_WIDTH];

void IIC_Init(uint32_t freq)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    I2C_InitTypeDef  I2C_InitTSturcture = {0};

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    I2C_InitTSturcture.I2C_ClockSpeed = freq;
    I2C_InitTSturcture.I2C_Mode = I2C_Mode_I2C;
    I2C_InitTSturcture.I2C_DutyCycle = I2C_DutyCycle_2;
    I2C_InitTSturcture.I2C_OwnAddress1 = 0xFE;
    I2C_InitTSturcture.I2C_Ack = I2C_Ack_Enable;
    I2C_InitTSturcture.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_Init(I2C1, &I2C_InitTSturcture);

    I2C_Cmd(I2C1, ENABLE);
    I2C_AcknowledgeConfig(I2C1, ENABLE);
}

void cmd(uint8_t cmd) {
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);
    I2C_GenerateSTART(I2C1, ENABLE);

    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);

    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, 0x00);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, cmd);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}

void dat(uint8_t data) {
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);
    I2C_GenerateSTART(I2C1, ENABLE);

    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);

    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, 0x40);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(I2C1, data);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C1, ENABLE);
}

void dats(uint8_t* data, uint8_t len) {
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY) != RESET);
    I2C_GenerateSTART(I2C1, ENABLE);

    while(!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT));
    I2C_Send7bitAddress(I2C1, OLED_ADDR, I2C_Direction_Transmitter);

    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C1, 0x40);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    for (uint8_t i = 0; i < len; i++) {
        I2C_SendData(I2C1, data[i]);
        while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED));
    }

    I2C_GenerateSTOP(I2C1, ENABLE);
}

void pos(uint8_t x, uint8_t y) {
    cmd(0xB0 + y);
    cmd(((x & 0xF0) >> 4) | 0x10);
    cmd(x & 0x0F);
}

void OLED_Init() {
    IIC_Init(400000);

    cmd(0xAE);
    cmd(0xD5);
    cmd(0x80);
    cmd(0xA8);
    cmd(0x1F);
    cmd(0xAD);
    cmd(0x40);

    cmd(0xD3);
    cmd(0x30);
    cmd(0x20);  //-Set Page Addressing Mode (0x00/0x01/0x02)
    cmd(0x02);  //
    cmd(0xA1);  //--Set SEG/Column Mapping     0xa0左右反置 0xa1正常
    cmd(0xC0);  //Set COM/Row Scan Direction   0xc0上下反置 0xc8正常

    cmd(0x81);
    cmd(0x2B);
    cmd(0xD9);
    cmd(0x22);
    cmd(0xDB);
    cmd(0x20);
    cmd(0xA4);
    cmd(0x8D);  /*set charge pump enable*/
    cmd(0x72);  /* 0x12:7.5V; 0x52:8V;  0x72:9V;  0x92:10V */
    OLED_Clear();
    cmd(0xAF);
}

void OLED_Clear() {
    for (uint8_t m = 0; m < OLED_HEIGHT / 8; m++) {
        pos(0, m);
        memset(buffer, 0, OLED_WIDTH);
        dats(buffer, OLED_WIDTH);
    }
}

void OLED_Switch(uint8_t state) {
    if (state != 0) {
        cmd(0x8D);
        cmd(0x14);
        cmd(0xAF);
    } else {
        cmd(0x8D);
        cmd(0x10);
        cmd(0xAE);
    }
}

void OLED_Char(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, char c) {
    const uint8_t* font;
    uint8_t f_w, f_h, f_o, f_e;

    font = FONT_Get(fid);
    FONT_Size(font, &f_w, &f_h, &f_o, &f_e);
    if ((uint8_t) c > f_e)
        c = (char) f_e;
    c -= f_o;

    if (x > OLED_WIDTH - f_w) { x = 0; y += f_h / 8; }

    for (uint8_t seg = 0; seg < f_h / 8; seg++) {
        for (uint8_t col = 0; col < f_w; col++) {
            buffer[col] = font[c * f_w * f_h / 8 + f_w * seg + col];
            if (color == 0)
                buffer[col] = ~buffer[col];
        }
        pos(x, y + seg);
        dats(buffer, f_w);
    }
}

void OLED_Print(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, char* str) {
    const uint8_t* font;
    uint8_t f_w, f_h, pos;

    font = FONT_Get(fid);
    FONT_Size(font, &f_w, &f_h, &pos, &pos); // pos unused

    pos = 0;
    while (str[pos] != '\0') {
        if (y > (OLED_HEIGHT - f_h) / 8) { x = 0; y = 0; OLED_Clear(); }
        switch (str[pos]) {
            case '\n':
                x = 0; y += f_h / 8;
                break;
            default:
                OLED_Char(x, y, fid, color, str[pos]);
                x += f_w;
                if (x > OLED_WIDTH - f_w) {
                    x = 0;
                    y += f_h / 8;
                }
                break;
        }
        pos++;
    }
}

int OLED_Printf(uint8_t x, uint8_t y, uint8_t fid, uint8_t color, const char* format, ...) {
    char* iobuf = malloc(sizeof(char) * IOBUF_SIZE);
    va_list args;
    va_start(args, format);
    int result = vsprintf(iobuf, format, args);
    va_end(args);
    OLED_Print(x, y, fid, color, iobuf);
    free(iobuf);
    return result;
}

int OLED_Printfc(uint8_t y, uint8_t fid, uint8_t color, const char* format, ...) {
    uint8_t f_w, f_h, len;
    FONT_Size(FONT_Get(fid), &f_w, &f_h, &len, &len); // len unused

    char* iobuf = malloc(sizeof(char) * IOBUF_SIZE);
    va_list args;
    va_start(args, format);
    int result = vsprintf(iobuf, format, args);
    va_end(args);
    len = strlen(iobuf);
    OLED_Print(OLED_WIDTH / 2 - len * f_w / 2, y, fid, color, iobuf);
    free(iobuf);
    return result;
}

