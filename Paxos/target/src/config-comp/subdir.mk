################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/config-comp/config-comp.c

OBJS += \
./src/config-comp/config-comp.o

C_DEPS += \
./src/config-comp/config-comp.d


# Each subdirectory must supply rules for building sources it contributes
src/config-comp/%.o: ../src/config-comp/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc-4.8 -fPIC -rdynamic -std=gnu11 -DDEBUG=$(DEBUGOPT) -I"$(ROOT_DIR)/../.local/include" -O0 -g3 -Wall -pg -c -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

