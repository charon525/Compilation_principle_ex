#ifndef RV_DEF_H
#define RV_DEF_H

#include<string>

namespace rv {

// rv interger registers
enum class rvREG {
    /* Xn       its ABI name*/
    X0,         // zero
    X1,         // ra 返回地址 saver: Caller 
    X2,         // sp 栈指针 saver: Callee
    X3,         // gp 全局变量数据区指针
    X4,         // tp thread pointer
    // 临时寄存器 t0 - t2 saver: Caller 
    X5,         // t0
    X6,         // t1.... FIX these comment by reading the risc-v ABI, and u should figure out the role of every register in function call, including its saver(caller/callee)
    X7,         // t2
    X8,         // s0/fp saver: Callee
    X9,         // s1 saver: Callee
    // a0 - a1 函数参数或者返回值 saver: Caller 
    X10,    
    X11,
    // a2 - a7 函数返回值 saver: Caller 
    X12,
    X13,
    X14,
    X15,
    X16,
    X17,
    // s2 - s11 saver: Callee
    X18,
    X19,
    X20,
    X21,
    X22,
    X23,
    X24,
    X25,
    X26,
    X27,
    // t3 - t6 saver: Caller 
    X28, 
    X29, 
    X30, 
    X31, 
};
std::string toString(rvREG r);  // implement this in ur own way

enum class rvFREG {
    // t0 - t7 saver: Caller
    F0,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    // s0 - s1 saver: Callee
    F8,
    F9,
    // s0 - s1 saver: Caller
    F10,
    F11,
    // a2 - a7 saver: Caller 
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    // s2 - s11 saver: Callee 
    F18,  
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,
    F25,
    F26,
    F27,
    // t8 - t11 saver: Caller 
    F28,
    F29,
    F30,
    F31,
};
std::string toString(rvFREG r);  // implement this in ur own way

// rv32i instructions
// add instruction u need here!
enum class rvOPCODE {
    // RV32I Base Integer Instructions
    ADD, SUB, XOR, OR, AND, SLL, SRL, SRA, SLT, SLTU,       // arithmetic & logic
    ADDI, XORI, ORI, ANDI, SLLI, SRLI, SRAI, SLTI, SLTIU,   // immediate
    LW, SW,                                                 // load & store
    BEQ, BNE, BLT, BGE, BLTU, BGEU, BNEZ,                        // conditional branch
    JAL, JALR,                                              // jump
    // 自行添加
    NOR,
    // RV32M Multiply Extension
    MUL, DIV,
    // RV32F / D Floating-Point Extensions

    // Pseudo Instructions
    LA, LI, MOV, J,                                         // ...
};
std::string toString(rvOPCODE r);  // implement this in ur own way


} // namespace rv



#endif