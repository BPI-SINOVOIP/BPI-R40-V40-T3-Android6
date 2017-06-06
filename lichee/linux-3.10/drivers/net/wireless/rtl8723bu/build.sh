#!/bin/bash

#export KSRC=~/letou-3.4
#export KSRC=~/A31/lichee/linux-3.3
#export KSRC=~/A20/lichee/linux-3.3
#export KSRC=~/binlai/lichee/linux-3.3
#export KSRC=~/luyuan/linux-3.1.10
#export KSRC=~/kernel_RK3188                   #jinglongruixin
#export KSRC=/usr/src/linux-source-3.13.0
#export KSRC=~/lichee-v3.0.1/lichee/linux-3.4       #kelintong
export KSRC=/home/fengkun/ex_t3/lichee/linux-3.10
export ARCH=arm
export CROSS_COMPILE=/home/fengkun/ex_t3/lichee/brandy/gcc-linaro/bin/arm-linux-gnueabi-
#export CROSS_COMPILE=~/kkkbox/newibox/android/prebuilts/gcc/darwin-x86/arm/arm-eabi-4.7/bin/arm-eabi-gcc
#export CROSS_COMPILE=arm-linux-
export PWD=`pwd`
export KDIR=$KSRC
export CROSS_ARCH=arm


make $*
