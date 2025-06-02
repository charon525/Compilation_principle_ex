/*** 
 * @Author: YourName
 * @Date: 2024-06-19 16:00:54
 * @LastEditTime: 2024-06-21 13:37:43
 * @LastEditors: YourName
 * @Description: 
 * @FilePath: \ex3\include\backend\generator.h
 * @版权声明
 */
#ifndef GENERARATOR_H
#define GENERARATOR_H

#include "ir/ir.h"
#include "backend/rv_def.h"
#include "backend/rv_inst_impl.h"

#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <set>

namespace backend
{

    // 实现变量与栈中内存地址的映射，栈中内存地址 = $sp + off
    struct stackVarMap
    {
        // 变量和其内存偏移的映射
        int total_size = 0;
        std::map<std::string, int> _table;

        /**
         * @brief 查找给定IR操作数在栈中内存的地址偏移
         * @return 偏移量
         */
        int find_operand(ir::Operand);

        /**
         * @brief 将给定IR操作数添加到当前的映射中，并为其在栈内存中分配空间。
         * @param[in] size: the space needed(in byte)
         * @return 地址偏移
         */
        int add_operand(ir::Operand, uint32_t size = 4);
    };

    struct Generator
    {
        const ir::Program &program;        // the program to gen
        std::ofstream &fout;               // output file

        stackVarMap stackVar;              // 栈数据结构
        std::set<std::string> global_vals; // 全局变量
        
        std::map<int, std::string> label_map; // 用于存储标签的映射， pc -> label
        int label_cnt; // 标签计数器

        // 临时寄存器
        std::unordered_map<std::string, bool> Registers{
                {"s4", true}, {"s5", true}, {"s6", true}, {"s7", true},
                {"t0", false}, {"t1", true}, {"t2", true}, {"t3", true}, 
                {"t4", true}, {"t5", true}, {"t6", true}, 
                {"s0", true}, {"s1", true}, {"s2", true}, {"s3", true}
        };
        // 函数返回值寄存器
        std::vector<std::string> returnRegs{"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"};
        Generator(ir::Program &, std::ofstream &);

        // 获取Rd寄存器
        rv::rvREG getRd(ir::Operand); 
        rv::rvFREG fgetRd(ir::Operand);
        rv::rvREG getRs1(ir::Operand); 
        rv::rvFREG fgetRs1(ir::Operand);
        rv::rvREG getRs2(ir::Operand); 
        rv::rvFREG fgetRs2(ir::Operand);
        rv::rvREG getAdrress(ir::Operand); // get a reg for a ir::Operand which is a address

        int calc_frame_size(const ir::Function &func); // calculate the frame size
        void load(ir::Operand, std::string, int offset = 0, bool isAddress = false);  // load a word from mem
        void load(ir::Operand, std::string, std::string, bool isAddress = false);     // load a word from mem
        void store(ir::Operand, std::string, int offset = 0, bool isAddress = false); // store a word to mem
        void store(ir::Operand, std::string, std::string, bool isAddress = false);    // store a word to mem

        std::string get_reg();
        void free_reg(std::string);
        void init_label(const ir::Function&);

        // generate wrapper function
        void gen();                              // generate the whole program
        void gen_func(const ir::Function &);     // generate a function
        void gen_instr(const ir::Instruction &, int, const std::vector<std::string>&); // generate a instruction
    };

} // namespace backend

#endif