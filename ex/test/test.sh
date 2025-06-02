#!/bin/bash
###
 # @Author: YourName
 # @Date: 2024-06-11 22:06:56
 # @LastEditTime: 2024-06-22 13:09:13
 # @LastEditors: YourName
 # @Description: 
 # @FilePath: \ex3\test\test.sh
 # 版权声明
### 
../bin/compiler ./test.sy -e -o test_ir.out
../bin/compiler ./test.sy -S -o test.s
riscv32-unknown-linux-gnu-gcc ./test.s  sylib-riscv-linux.a -o ./test.exe
qemu-riscv32.sh ./test.exe < ./test.in > ./test.out
