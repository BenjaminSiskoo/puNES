################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/filters/audio/blip.c \
../src/filters/audio/blip2.c \
../src/filters/audio/original.c 

OBJS += \
./src/filters/audio/blip.o \
./src/filters/audio/blip2.o \
./src/filters/audio/original.o 

C_DEPS += \
./src/filters/audio/blip.d \
./src/filters/audio/blip2.d \
./src/filters/audio/original.d 


# Each subdirectory must supply rules for building sources it contributes
src/filters/audio/%.o: ../src/filters/audio/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: Cross GCC Compiler'
	i686-pc-linux-gnu-gcc -DGTK -DGLEW_STATIC -I../src -I/usr/i686-pc-linux-gnu/usr/include/SDL -O3 -Wall -ffast-math -msse2 -mfpmath=sse -c -fmessage-length=0 -finline-functions -Winline `i686-pc-linux-gnu-pkg-config --libs --cflags gtk+-2.0` -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


