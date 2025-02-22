#include "ch32v20x.h"
#include "delay.h"

#include <string.h>
#include <stdbool.h>

#define abs(v) ((v) < 0 ? -(v) : (v))

#include "oled.h"
#include "hx711.h"

void GPIOx_Init() {
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;  // TPA
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;  // TPB
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void TIMx_Init() {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_CounterModeConfig(TIM1, TIM_CounterMode_Down);
    TIM_CounterModeConfig(TIM2, TIM_CounterMode_Up);
    TIM_SetAutoreload(TIM1, 1000 - 1); // 1s
    TIM_PrescalerConfig(TIM1, SystemCoreClock / 1000 - 1, TIM_PSCReloadMode_Immediate); // 1kHz
    TIM_SelectOutputTrigger(TIM1, TIM_TRGOSource_Update);
    TIM_SelectInputTrigger(TIM2, TIM_TS_ITR0);
    TIM_SelectSlaveMode(TIM2, TIM_SlaveMode_External1);
    TIM_Cmd(TIM1, ENABLE);
}

bool tpa_down = false, tpb_down = false;
#define TPA() ((GPIOA->INDR & GPIO_Pin_0) != 0)
#define TPB() ((GPIOA->INDR & GPIO_Pin_1) != 0)

#define VBAT_AVG_SIZE 20
int32_t vbatBuf[VBAT_AVG_SIZE] = { 0 };

uint16_t VBAT_Get() {
    HX711_Switch(HX711_B_32);

    if (HX711_Ready()) {
        memmove(&vbatBuf[0], &vbatBuf[1], sizeof(int32_t) * (VBAT_AVG_SIZE - 1));
        vbatBuf[VBAT_AVG_SIZE - 1] = HX711_GetValue();
    }

    float vbat = 0;
    for (uint8_t i = 0; i < VBAT_AVG_SIZE; i++)
        vbat += (float) vbatBuf[i] / 0x7FFFFF * 80 * 69 * 1.11f;
    vbat /= VBAT_AVG_SIZE;

    return (int) vbat; // mV
}

void VBAT_WaitStable() {
    for (uint8_t i = 0; i < VBAT_AVG_SIZE; i++) {
        VBAT_Get();
        Delay_Ms(10);
    }
}

#define SCL_AVG_SIZE 20
int32_t sclBuf[SCL_AVG_SIZE] = { 0 };

uint16_t SCL_Get() {
    HX711_Switch(HX711_A_128);

    if (HX711_Ready()) {
        memmove(&sclBuf[0], &sclBuf[1], sizeof(int32_t) * (SCL_AVG_SIZE - 1));
        sclBuf[SCL_AVG_SIZE - 1] = HX711_GetValue();
    }

    float scl = 0;
    for (uint8_t i = 0; i < SCL_AVG_SIZE; i++)
        scl += (float) sclBuf[i] / 0x7FFFFF * 20;
    scl /= SCL_AVG_SIZE;

    return (int) (scl * 2000); // 0.5uV
}

void SCL_WaitStable() {
    for (uint8_t i = 0; i < SCL_AVG_SIZE; i++) {
        SCL_Get();
        Delay_Ms(10);
    }
}

float SCL_GetGram() {
    float f = SCL_Get();
    return f * 0.1013f - 34.24;
}

int main(void) {
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Delay_Init();
    GPIOx_Init();
    TIMx_Init();

    HX711_Init(HX711_HS); // 100Hz
    OLED_Init();

    OLED_Printfc(1, 1, 1, "NyaSama Scale");
    OLED_Printfc(2, 1, 1, "v1.0");

    VBAT_WaitStable();
    if (VBAT_Get() < 3200) {
        OLED_Printfc(0, 2, 1, "LOBAT");
        OLED_Printfc(3, 0, 1, "%4dmV", VBAT_Get());
        while (1);
    }

    uint8_t gui_index = 0;
    uint8_t t8 = 0;
    uint16_t t16 = 0;

    Delay_Ms(1000);

    SCL_WaitStable();
    float zero = SCL_GetGram();
    float mass = zero;

    OLED_Clear();

    float dm = 0, pm = 0;

    while (1) {
        if (TPA() && !tpa_down) {
            tpa_down = true;
            OLED_Clear();
            OLED_Printfc(0, 2, 1, "NEXT");
        } else if (!TPA() && tpa_down) {
            tpa_down = false;
            gui_index += 1;
            OLED_Clear();

            pm = mass = 0;
            t16 = 0; t8 = 0;
        }

        if (!tpa_down) switch (gui_index) {
            case 0: // NORMAL
                if (TPB() && !tpb_down) {
                    tpb_down = true;
                    OLED_Clear();
                } else if (!TPB() && tpb_down) {
                    tpb_down = false;
                    Delay_Ms(500);
                    SCL_WaitStable();
                    zero = SCL_GetGram();
                }

                if (!tpb_down) {
                    mass = SCL_GetGram() - zero;
                    if (mass < -99.9f) mass = -99.9f;
                    if (mass > 999.9f) mass = 999.9f;
                    OLED_Printfc(0, 2, 1, "%3d.%1d", (int) mass, abs((int) ((mass - (int) mass) * 10)));
                } else {
                    OLED_Printfc(0, 2, 1, "ZERO");
                }
                break;
            case 1: // Flow Rate
                if (TPB() && !tpb_down) {
                    tpb_down = true;
                    OLED_Clear();
                } else if (!TPB() && tpb_down) {
                    tpb_down = false;
                    Delay_Ms(500);
                    SCL_WaitStable();
                    zero = SCL_GetGram();
                    pm = mass = 0;
                    t8 = 0;
                }

                if (!tpb_down) {
                    if (t8 < 5)
                        t8 += 1;
                    else {
                        t8 = 0;
                        dm = mass - pm;
                        if (dm < 0) dm = -dm;
                        if (dm > 9.9f) dm = 9.9f;

                        pm = mass;
                    }
                    mass = SCL_GetGram() - zero;
                    if (mass < -9.9f) mass = -9.9f;
                    if (mass > 99.9f) mass = 99.9f;
                    OLED_Printfc(0, 2, 1, "%2d.%1d %1d.%1d", (int) mass, abs((int) ((mass - (int) mass) * 10)), (int) dm, (int) ((dm - (int) dm) * 10));
                    SCL_WaitStable(); // 200ms
                } else {
                    OLED_Printfc(0, 2, 1, "READY");
                }
                break;
            case 2: // Time
                if (TPB() && !tpb_down) {
                    tpb_down = true;
                    OLED_Clear();
                } else if (!TPB() && tpb_down) {
                    tpb_down = false;
                    Delay_Ms(500);
                    SCL_WaitStable();
                    zero = SCL_GetGram();
                    t16 = 0; t8 = 0;
                    TIM_Cmd(TIM2, DISABLE);
                    TIM2->CNT = 0;
                }

                if (!tpb_down) {
                    if (mass > 1 && t8 == 0) {
                        t8 = 1;
                        TIM_Cmd(TIM2, ENABLE);
                    }
                    mass = SCL_GetGram() - zero;
                    if (mass < -99) mass = -99;
                    if (mass > 999) mass = 999;
                    t16 = TIM2->CNT;
                    OLED_Printfc(0, 2, 1, "%1d:%02d %3d", (t16 / 60) > 9 ? 9 : (t16 / 60), t16 % 60, (int) mass);
                    SCL_WaitStable(); // 200ms
                } else {
                    OLED_Printfc(0, 2, 1, "READY");
                }
                break;
            case 3:
                t16 = VBAT_Get();
                OLED_Printfc(0, 2, 1, "%1d.%03dV", t16 / 1000, t16 % 1000);
                break;
            default:
                gui_index = 0;
                break;
        }
    }
}
