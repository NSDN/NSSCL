/*
 * Copyright (c) 2013-2019 Huawei Technologies Co., Ltd. All rights reserved.
 * Copyright (c) 2020-2021 Huawei Device Co., Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of
 *    conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list
 *    of conditions and the following disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "los_tick.h"
#include "los_task.h"
#include "los_config.h"
#include "los_interrupt.h"
#include "los_debug.h"
#include "los_compiler.h"

#include <string.h>
#include <stdbool.h>

#include "oled.h"
#include "hx711.h"

/* Global define */
#define FLOW_RATE_SPS   10
#define MEAS_PERIOD     (uint32_t) (1000 / (FLOW_RATE_SPS))
#define abs(v) ((v) < 0 ? -(v) : (v))

void FUNC_Measure(uint32_t arg);
void FUNC_GUIShow(uint32_t arg);
void FUNC_KeyInput(uint32_t arg);

/* Global Variable */
__attribute__((aligned (8))) UINT8 g_memStart[LOSCFG_SYS_HEAP_SIZE];

/* Function Start */
UINT32 TASK_Create() {
    UINT32 ret;
    UINT32 taskID1, taskID2, taskID3;
    TSK_INIT_PARAM_S stTask = { 0 };

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_Measure;
    stTask.uwStackSize  = 0X400;
    stTask.pcName       = "MEAS";
    stTask.usTaskPrio   = 6;/* high priority */
    ret = LOS_TaskCreate(&taskID1, &stTask);
    if (ret != LOS_OK) return ret;

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_GUIShow;
    stTask.uwStackSize  = 0X400;
    stTask.pcName       = "GUI";
    stTask.usTaskPrio   = 7;/* low priority */
    ret = LOS_TaskCreate(&taskID2, &stTask);
    if (ret != LOS_OK) return ret;

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_KeyInput;
    stTask.uwStackSize  = 0X400;
    stTask.pcName       = "Input";
    stTask.usTaskPrio   = 7;/* low priority */
    ret = LOS_TaskCreate(&taskID3, &stTask);
    if (ret != LOS_OK) return ret;

    return LOS_OK;
}

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

void Periph_Init() {
    GPIOx_Init();
    TIMx_Init();

    HX711_Init(HX711_LS);
    OLED_Init();
}

LITE_OS_SEC_TEXT_INIT int main(void) {
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Periph_Init();

    UINT32 k_i, t_c, o_s;
    k_i = LOS_KernelInit();
    t_c = TASK_Create();
    o_s = LOS_Start();

    if ((k_i + t_c + o_s) != LOS_OK) {
        OLED_Printf(0, 0, 0, 1, "KE: %08X", k_i);
        OLED_Printf(0, 1, 0, 1, "TA: %08X", t_c);
        OLED_Printf(0, 2, 0, 1, "OS: %08X", o_s);
    }

    while (1) {
        __asm volatile("nop");
    }
}

/* Local Variable & Function */
uint16_t vbat = 0;
float mass = 0, dmass = 0, pmass = 0;

uint16_t VBAT_Get() {
    HX711_Switch(HX711_B_32);

    float vbat = 0;
    while (!HX711_Ready())
        LOS_TaskDelay(1);
    vbat += (float) HX711_GetValue() / 0x7FFFFF * 80 * 69 * 1.11f;

    return (int) vbat; // mV
}

uint8_t VBAT_Percent(uint16_t mv) {
    if (mv >= 4200) {
        return 100;
    } else if (mv >= 4000) {
        return 90 + (mv - 4000) / 20;  // 90% ~ 100%
    } else if (mv >= 3800) {
        return 70 + (mv - 3800) / 10;  // 70% ~ 90%
    } else if (mv >= 3700) {
        return 50 + (mv - 3700) / 10;  // 50% ~ 70%
    } else if (mv >= 3600) {
        return 40 + (mv - 3600) / 10;  // 40% ~ 50%
    } else if (mv >= 3500) {
        return 30 + (mv - 3500) / 10;  // 30% ~ 40%
    } else if (mv >= 3400) {
        return 20 + (mv - 3400) / 10;  // 20% ~ 30%
    } else if (mv >= 3300) {
        return 10 + (mv - 3300) / 10;  // 10% ~ 20%
    } else if (mv >= 3200) {
        return (mv - 3200) / 10;  // 0% ~ 10%
    } else {
        return 0;
    }
}

float SCL_Get() {
    HX711_Switch(HX711_A_128);

    float scl = 0;
    while (!HX711_Ready())
        LOS_TaskDelay(1);
    scl += (float) HX711_GetValue() / 0x7FFFFF * 20.0f;

    scl = scl * 2000; // 0.5uV
    return scl * 0.1013f - 34.24f;
}

enum {
    Mode_Begin,
    Mode_Normal,
    Mode_FlowRate,
    Mode_Timestop,
    Mode_Battery,
    Mode_End
} p_mode = Mode_End, mode = Mode_Begin;

void FUNC_Measure(uint32_t arg) {
    __IO uint32_t tick = 0;

    while (1) {
        tick = LOS_TickCountGet();

        if (mode == Mode_Battery) {
            vbat = VBAT_Get();
        } else {
            pmass = mass;
            mass = SCL_Get();
            dmass = (mass - pmass) * (float) (FLOW_RATE_SPS);
        }

        tick = LOS_TickCountGet() - tick;
        if (tick < MEAS_PERIOD)
            LOS_TaskDelay(MEAS_PERIOD - tick);
    }
}

float zero = 0;
bool zero_start = false;
bool time_start = false;

void SCL_Zero() {
    zero_start = true;
    LOS_TaskDelay(1000);
    zero = mass;
    zero_start = false;
}

bool mode_sw = false;

void FUNC_GUIShow(uint32_t arg) {
    float f;
    uint8_t t8;
    uint16_t t16;

    while (1) {
        if (mode_sw) {
            mode_sw = false;

            OLED_Char(111, 0, 4, 1, 0x00);  // IMG_NONE
        }

        if (p_mode ^ mode) {
            p_mode = mode;

            OLED_Clear();
            if (mode > Mode_Begin && mode < Mode_End) {
                OLED_Char(111, 0, 4, 1, mode);  // IMG_NOR, IMG_FLO, IMG_TIM, IMG_BAT
            }
        }

        switch (mode) {
        case Mode_Begin:
            OLED_Printfc(0, 1, 1, "NyaSama LABO");
            OLED_Printfc(1, 2, 1, "NSSCL");
            OLED_Printfc(3, 0, 1, "--- LOS.v1 ---");
            SCL_Zero();
            mode = Mode_Normal;
            break;
        case Mode_Normal:
            f = mass - zero;
            if (f < -99.9f) f = -99.9f;
            if (f > 999.9f) f = 999.9f;
            if (zero_start) f = 0;
            OLED_Printf(8, 1, 3, 1, "%3d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(68, 2, 1, 1, "g");
            break;
        case Mode_FlowRate:
            f = mass - zero;
            if (f < -9.9f) f = -9.9f;
            if (f > 99.9f) f = 99.9f;
            if (zero_start) f = 0;
            OLED_Printf(8, 0, 0, 1, "MASS");
            OLED_Printf(8, 1, 3, 1, "%2d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(48, 3, 1, 1, "g");

            f = abs(dmass);
            if (f > 9.9f) f = 9.9f;
            if (zero_start) f = 0;
            OLED_Printf(66, 0, 0, 1, "RATE");
            OLED_Printf(66, 1, 3, 1, "%1d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(78, 3, 1, 1, "g/s");
            break;
        case Mode_Timestop:
            f = mass - zero;
            if (f < -99.0f) f = -99.0f;
            if (f > 999.0f) f = 999.0f;
            if (zero_start) f = 0;
            OLED_Printf(8, 0, 0, 1, "MASS");
            OLED_Printf(8, 1, 3, 1, "%3d", (int) f);
            OLED_Printf(36, 3, 1, 1, "g");

            t16 = TIM2->CNT;
            if (t16 > 5999) t16 = 5999;
            if (zero_start) t16 = 0;
            OLED_Printf(52, 0, 0, 1, "TIME");
            OLED_Printf(52, 1, 2, 1, "%2d:%02d", t16 / 60, t16 % 60);
            OLED_Printf(78, 3, 1, 1, "m:s");

            if (!time_start && f > 1) {
                time_start = true;
                TIM_Cmd(TIM2, ENABLE);
            }
            break;
        case Mode_Battery:
            t16 = vbat;
            if (t16 > 9999) t16 = 9999;
            OLED_Printf(8, 0, 0, 1, "%1d.%03dV", t16 / 1000, t16 % 1000);
            t8 = VBAT_Percent(t16);
            if (t8 > 99) t8 = 99;
            OLED_Printf(8, 1, 3, 1, "%2d", t8);
            OLED_Printf(24, 3, 1, 1, "%%");
            t8 /= 9;
            LOS_TaskLock();
            for (uint8_t i = 0; i < t8; i++) {
                OLED_Char(40 + 6 * i, 1, 0, 0, ' ');
                OLED_Char(40 + 6 * i, 2, 0, 0, ' ');
            }
            for (uint8_t i = t8; i < 11; i++) {
                OLED_Char(40 + 6 * i, 1, 0, 1, ' ');
                OLED_Char(40 + 6 * i, 2, 0, 1, ' ');
            }
            LOS_TaskUnlock();
            break;
        default:
            break;
        }

        LOS_TaskDelay(20);
    }
}

bool p_tpa = false, p_tpb = false;
#define TPA() ((GPIOA->INDR & GPIO_Pin_0) != 0)
#define TPB() ((GPIOA->INDR & GPIO_Pin_1) != 0)

void FUNC_KeyInput(uint32_t arg) {
    while (1) {
        if (p_tpa ^ TPA()) {
            p_tpa = TPA();

            if (!TPA()) {
                SCL_Zero();
                mode += 1;
                if (mode == Mode_End)
                    mode = Mode_Normal;
                else if (mode == Mode_Timestop) {
                    TIM_Cmd(TIM2, DISABLE);
                    TIM2->CNT = 0;
                    time_start = false;
                }
            } else {
                mode_sw = true;
            }
        }

        if (p_tpb ^ TPB()) {
            p_tpb = TPB();

            if (!TPB()) {
                switch (mode) {
                case Mode_Normal:
                    SCL_Zero();
                    break;
                case Mode_FlowRate:
                    SCL_Zero();
                    break;
                case Mode_Timestop:
                    SCL_Zero();
                    TIM_Cmd(TIM2, DISABLE);
                    TIM2->CNT = 0;
                    time_start = false;
                    break;
                default:
                    break;
                }
            }
        }

        LOS_TaskDelay(10);
    }
}
