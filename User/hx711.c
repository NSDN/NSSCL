#include "hx711.h"

#include "delay.h"

static uint8_t sck_cnt;

#define SCK_H() GPIOB->BSHR = GPIO_Pin_3
#define SCK_L() GPIOB->BCR = GPIO_Pin_3
#define SDI()   ((GPIOB->INDR & GPIO_Pin_4) != 0 ? 1 : 0)

void HX711_Init(uint8_t mode) {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;  // SPD
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_WriteBit(GPIOA, GPIO_Pin_15, mode == HX711_HS ? Bit_SET : Bit_RESET);

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
        Delay_Ms(50);

    sck_cnt = ck;
    HX711_GetValue();
    Delay_Ms(50);
    for (uint8_t i = 0; i < 5; i++)
        HX711_GetValue();
}

uint8_t HX711_Ready() {
    if (SDI() == 0) {
        Delay_Us(1);
        return 1;
    }
    return 0;
}

int32_t HX711_GetValue() {
    uint32_t dat = 0;
    if (HX711_Ready()) {
        uint8_t count = 0;
        for (uint8_t i = 0; i < sck_cnt; i++) {
            SCK_H();
            Delay_Us(1);
            SCK_L();
            Delay_Us(1);

            if (count < 24) {
                dat <<= 1;
                dat |= SDI();
                count += 1;
            }
        }
    }
    return (int32_t) (dat << 8) / 0xFF;
}
