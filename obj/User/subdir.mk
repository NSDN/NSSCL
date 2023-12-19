################################################################################
# MRS Version: 1.9.1
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../User/ch32v20x_it.c \
../User/font.c \
../User/hx711.c \
../User/main.c \
../User/oled.c \
../User/system_ch32v20x.c 

OBJS += \
./User/ch32v20x_it.o \
./User/font.o \
./User/hx711.o \
./User/main.o \
./User/oled.o \
./User/system_ch32v20x.o 

C_DEPS += \
./User/ch32v20x_it.d \
./User/font.d \
./User/hx711.d \
./User/main.d \
./User/oled.d \
./User/system_ch32v20x.d 


# Each subdirectory must supply rules for building sources it contributes
User/%.o: ../User/%.c
	@	@	riscv-none-embed-gcc -march=rv32imacxw -mabi=ilp32 -msmall-data-limit=8 -msave-restore -Os -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-common -Wunused -Wuninitialized  -g -I"E:\Works\NSSCL\Core" -I"E:\Works\NSSCL\User" -I"E:\Works\NSSCL\Peripheral\inc" -std=gnu99 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"
	@	@

