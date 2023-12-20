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

uint16_t VBAT_Get() {
    HX711_Switch(HX711_B_32);

    while (!HX711_Ready());
    float vbat = (float) HX711_GetValue() / 0x7FFFFF * 80 * 69 * 1.11f;

    return (int) vbat; // mV
}

float SCL_Get() {
    HX711_Switch(HX711_A_128);

    while (!HX711_Ready());
    float scl = (float) HX711_GetValue() / 0x7FFFFF * 20.0f;
    scl = scl * 2000; // 0.5uV

    return scl * 0.1013f - 34.24f;
}

int main(void) {
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    Delay_Init();
    GPIOx_Init();
    TIMx_Init();

    HX711_Init(HX711_LS); // 12.5Hz
    OLED_Init();

    OLED_Printfc(0, 2, 1, "NSSCL");
    OLED_Printfc(3, 0, 1, "NyaSama Scale");

    if (VBAT_Get() < 3200) {
        OLED_Clear();
        OLED_Printfc(0, 2, 1, "LOBAT");
        Delay_Ms(5000);
        OLED_Clear();
        while (1) {
            OLED_Printfc(0, 2, 1, "%4dmV", VBAT_Get());
            Delay_Ms(1000);
        }
    }

    uint8_t gui_index = 0;
    uint8_t t8 = 0;
    uint16_t t16 = 0;

    Delay_Ms(1000);

    float zero = SCL_Get();
    float mass = 0;

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
            Delay_Ms(1000);

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
                    zero = SCL_Get();
                }

                if (!tpb_down) {
                    if (mass < -99.9f) mass = -99.9f;
                    if (mass > 999.9f) mass = 999.9f;
                    OLED_Printfc(0, 2, 1, "%3d.%1d", (int) mass, abs((int) ((mass - (int) mass) * 10)));

                    mass = SCL_Get() - zero;
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
                    zero = SCL_Get();
                    pm = mass = 0;
                    t8 = 0;
                }

                if (!tpb_down) {
                    if (mass < -9.9f) mass = -9.9f;
                    if (mass > 99.9f) mass = 99.9f;
                    OLED_Printfc(0, 2, 1, "%2d.%1d %1d.%1d", (int) mass, abs((int) ((mass - (int) mass) * 10)), (int) dm, (int) ((dm - (int) dm) * 10));
#define FLOW_RATE_CNT 10
                    if (t8 < FLOW_RATE_CNT)
                        t8 += 1;
                    else {
                        t8 = 0;
                        dm = (mass - pm) / (FLOW_RATE_CNT * 0.08f);
                        dm = abs(dm);
                        if (dm > 9.9f) dm = 9.9f;

                        pm = mass;
                    }
                    mass = SCL_Get() - zero; // ~80ms
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
                    zero = SCL_Get();
                    t16 = 0; t8 = 0;
                    TIM_Cmd(TIM2, DISABLE);
                    TIM2->CNT = 0;
                }

                if (!tpb_down) {
                    if (mass < -99) mass = -99;
                    if (mass > 999) mass = 999;
                    OLED_Printfc(0, 2, 1, "%1d:%02d %3d", (t16 / 60) > 9 ? 9 : (t16 / 60), t16 % 60, (int) mass);

                    if (mass > 1 && t8 == 0) {
                        t8 = 1;
                        TIM_Cmd(TIM2, ENABLE);
                    }
                    mass = SCL_Get() - zero;

                    t16 = TIM2->CNT;
                } else {
                    OLED_Printfc(0, 2, 1, "READY");
                }
                break;
            case 3:
                OLED_Printfc(0, 2, 1, "%1d.%03dV", t16 / 1000, t16 % 1000);
                t16 = VBAT_Get();
                break;
            default:
                gui_index = 0;
                break;
        }
    }
}
