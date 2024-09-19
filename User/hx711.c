#include "hx711.h"

#include "los_task.h"

static uint8_t sck_cnt;

#define SCK_H() GPIOB->BSHR = GPIO_Pin_3
#define SCK_L() GPIOB->BCR = GPIO_Pin_3
#define SDI()   ((GPIOB->INDR & GPIO_Pin_4) != 0 ? 1 : 0)

static uint8_t speed_mode = HX711_LS;
void HX711_Init(uint8_t mode) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;  // SPD
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_WriteBit(GPIOA, GPIO_Pin_15, mode == HX711_HS ? Bit_SET : Bit_RESET);
    speed_mode = mode;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;   // SCK
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_ResetBits(GPIOB, GPIO_Pin_3);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;   // SDI
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    sck_cnt = HX711_A_128;
}

void HX711_Switch(uint8_t ck) {
    if (sck_cnt == ck)
        return;

    if (!HX711_Ready())
        LOS_TaskDelay(speed_mode == HX711_HS ? 50 : 400);

    sck_cnt = ck;
    HX711_GetValue();
    LOS_TaskDelay(speed_mode == HX711_HS ? 50 : 400);
}

extern uint32_t SystemCoreClock;

void delay_us(uint32_t t) {
    uint32_t c_us = SystemCoreClock / 1000000;
    uint32_t n = c_us * t;
    while (n--) {
        __asm volatile ("nop");
    }
}

uint8_t HX711_Ready() {
    LOS_TaskLock();
    if (SDI() == 0) {
        delay_us(1);
        LOS_TaskUnlock();
        return 1;
    }
    LOS_TaskUnlock();
    return 0;
}

int32_t HX711_GetValue() {
    uint32_t dat = 0;
    if (HX711_Ready()) {
        LOS_TaskLock();
        uint8_t count = 0;
        for (uint8_t i = 0; i < sck_cnt; i++) {
            SCK_H();
            delay_us(1);
            SCK_L();
            delay_us(1);

            if (count < 24) {
                dat <<= 1;
                dat |= SDI();
                count += 1;
            }
        }
        LOS_TaskUnlock();
    }
    return (int32_t) (dat << 8) / 0xFF;
}
