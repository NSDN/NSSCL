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

#include "debug.h"
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
#define FLOW_RATE_SPS   5
#define abs(v) ((v) < 0 ? -(v) : (v))

void FUNC_Measure(uint32_t arg);
void FUNC_GUIShow(uint32_t arg);
void FUNC_KeyInput(uint32_t arg);

/* Global Variable */
__attribute__((aligned (8))) UINT8 g_memStart[LOSCFG_SYS_HEAP_SIZE];

/* Function Start */
void TASK_Create() {
    UINT32 taskID1, taskID2, taskID3;
    TSK_INIT_PARAM_S stTask = { 0 };
    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_Measure;
    stTask.uwStackSize  = 0X100;
    stTask.pcName       = "Measure";
    stTask.usTaskPrio   = 6;/* high priority */
    LOS_TaskCreate(&taskID1, &stTask);

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_GUIShow;
    stTask.uwStackSize  = 0X100;
    stTask.pcName       = "GUI";
    stTask.usTaskPrio   = 7;/* low priority */
    LOS_TaskCreate(&taskID2, &stTask);

    stTask.pfnTaskEntry = (TSK_ENTRY_FUNC) FUNC_KeyInput;
    stTask.uwStackSize  = 0X100;
    stTask.pcName       = "Input";
    stTask.usTaskPrio   = 6;/* high priority */
    LOS_TaskCreate(&taskID3, &stTask);
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
    Delay_Init();
    GPIOx_Init();
    TIMx_Init();

    HX711_Init(HX711_LS);   // 12.5Hz
    OLED_Init();
    OLED_Printfc(1, 2, 1, "NSSCL");
    Delay_Ms(500);
}

int main(void) {
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);

    OLED_Init();
    OLED_Print(0, 0, 0, 1, "NSSCL");
    while (1);
}

//LITE_OS_SEC_TEXT_INIT int main(void) {
//    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
//    SystemCoreClockUpdate();
//	Periph_Init();
//
//    UINT32 ret = LOS_KernelInit();
//    TASK_Create();
//    if (ret == LOS_OK) {
//        LOS_Start();
//    }
//
//    while (1) {
//        __asm volatile("nop");
//    }
//}

/* Local Variable & Function */
uint16_t vbat = 0;
float mass = 0, dmass = 0;

uint16_t VBAT_Get() {
    HX711_Switch(HX711_B_32);

    float vbat = 0;
    while (!HX711_Ready())
        LOS_TaskDelay(1);
    vbat += (float) HX711_GetValue() / 0x7FFFFF * 80 * 69 * 1.11f;

    return (int) vbat; // mV
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

void FUNC_Measure(uint32_t arg) {
    uint32_t tick = 0;
    float pmass = 0;

    while (1) {
        tick = LOS_TickCountGet();
        vbat = VBAT_Get();
        pmass = mass;
        mass = SCL_Get();
        dmass = (mass - pmass) * (float) (FLOW_RATE_SPS);

        tick = LOS_TickCountGet() - tick;
        LOS_TaskDelay((1000 / (uint32_t) (FLOW_RATE_SPS)) - tick);
    }
}

enum {
    Mode_Begin,
    Mode_Normal,
    Mode_FlowRate,
    Mode_Timestop,
    Mode_Battery,
    Mode_End
} p_mode = Mode_End, mode = Mode_Begin;

float zero = 0;
bool time_start = false;

void SCL_Zero() {
    LOS_TaskDelay(500);
    zero = mass;
}

void FUNC_GUIShow(uint32_t arg) {
    float f;
    uint16_t t16;

    while (1) {
        if (p_mode ^ mode) {
            OLED_Clear();

            if (mode > Mode_Begin && mode < Mode_End) {
                OLED_Printf(103, 0, 0, mode != Mode_Normal, "NORM");
                OLED_Printf(103, 1, 0, mode != Mode_FlowRate, "RATE");
                OLED_Printf(103, 2, 0, mode != Mode_Timestop, "TIME");
                OLED_Printf(103, 3, 0, mode != Mode_Battery, "BATT");
            }
        }

        switch (mode) {
        case Mode_Begin:
            OLED_Printfc(0, 2, 1, "NSSCL");
            OLED_Printfc(2, 1, 1, "NyaSama Scale");
            OLED_Printfc(3, 0, 1, "v1.0-LOS");
            SCL_Zero();
            mode = Mode_Normal;
            break;
        case Mode_Normal:
            f = mass - zero;
            if (f < -99.9f) f = -99.9f;
            if (f > 999.9f) f = 999.9f;
            OLED_Printf(8, 1, 3, 1, "%3d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(68, 1, 2, 1, "g");
            break;
        case Mode_FlowRate:
            f = mass - zero;
            if (f < -9.9f) f = -9.9f;
            if (f > 99.9f) f = 99.9f;
            OLED_Printf(8, 0, 1, 1, "MASS");
            OLED_Printf(8, 1, 3, 1, "%2d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(48, 3, 1, 1, "g");

            f = abs(dmass);
            if (f > 9.9f) f = 9.9f;
            OLED_Printf(64, 0, 1, 1, "RATE");
            OLED_Printf(64, 1, 3, 1, "%1d.%1d", (int) f, abs((int) ((f - (int) f) * 10)));
            OLED_Printf(76, 3, 1, 1, "g/s");
            break;
        case Mode_Timestop:
            f = mass - zero;
            if (f < -99.0f) f = -99.0f;
            if (f > 999.0f) f = 999.0f;
            OLED_Printf(8, 0, 1, 1, "MASS");
            OLED_Printf(8, 1, 3, 1, "%3d", (int) f);
            OLED_Printf(36, 3, 1, 1, "g");

            t16 = TIM2->CNT;
            if (t16 > 5999) t16 = 5999;
            OLED_Printf(50, 0, 1, 1, "TIME");
            OLED_Printf(50, 1, 2, 1, "%2d:%02d", t16 / 60, t16 % 60);
            OLED_Printf(76, 3, 1, 1, "m:s");

            if (!time_start && f > 1) {
                time_start = true;
                TIM_Cmd(TIM2, ENABLE);
            }
            break;
        case Mode_Battery:
            OLED_Printf(8, 1, 3, 1, "%1d.%03d", vbat / 1000, vbat % 1000);
            OLED_Printf(68, 1, 2, 1, "V");
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
                case Mode_Battery:
                    break;
                default:
                    break;
                }
            }
        }

        LOS_TaskDelay(10);
    }
}
