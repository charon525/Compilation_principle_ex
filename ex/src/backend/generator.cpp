#include "backend/generator.h"

#include <assert.h>
#include <set>
#include <vector>
#include <algorithm>
#include <iostream>

#define TODO assert(0 && "todo")
// 生成R型指令(add, addi ······)    
#define GEN_R_INSTR(op, rs, rt, rd, fout) \
    fout<<"\t"<<op<<" "<<rd<<", "<<rs<<", "<<rt<<std::endl;

// load word指令
#define GEN_LW_INSTR(rs, offset, rd, fout) \
    fout << "\tlw " << rd << ", " << offset << "(" << rs << ")" << std::endl;

// load address指令
# define GEN_LA_INSTR(rs, rd, fout) \
    fout << "\tla " << rd << ", " << rs << std::endl;

// store word指令 
#define GEN_SW_INSTR(address, offset, data, fout) \
    fout<<"\tsw "<<data<<", "<<offset<<"("<<address<<")"<<std::endl;

// load imm指令
#define GEN_LI_INSTR(rd, imm, fout) \
    fout << "\tli " << rd << ", " << imm << std::endl;

// mv 指令
#define GEN_MV_INSTR(rs, rd, fout) \
    fout << "\tmv " << rd << ", " << rs << std::endl;


int backend::stackVarMap::add_operand(ir::Operand operand, uint32_t size)
{
    _table[operand.name] = total_size;
    total_size += size;
    return _table[operand.name];
}

int backend::stackVarMap::find_operand(ir::Operand operand)
{
    return _table[operand.name];
}

// Gnerator 构造函数
backend::Generator::Generator(ir::Program &program, std::ofstream &file) 
    : program(program), fout(file), label_cnt(0) {}

void backend::Generator::load(ir::Operand operand, std::string reg, int offset, bool isAddress)
{
    // 根据全局变量寻址
    if (global_vals.find(operand.name) != global_vals.end())
    {
        GEN_LA_INSTR(operand.name, reg, fout);
        GEN_LW_INSTR(reg, offset, reg, fout);
        return;
    }else{
        // 根据局部变量寻址 lw reg, offset(sp)
        std::string tmp_reg = get_reg();
        GEN_R_INSTR("addi", "sp", stackVar.find_operand(operand), tmp_reg, fout);
        if(isAddress){
            GEN_LW_INSTR(tmp_reg, 0, tmp_reg, fout);
        }
        GEN_LW_INSTR(tmp_reg, offset, reg, fout); 
        free_reg(tmp_reg);
    }
}

void backend::Generator::load(ir::Operand operand, std::string reg, std::string offset, bool isAddress){
    std::string tmp_reg = get_reg();
    // 根据全局变量寻址
    if (global_vals.find(operand.name) != global_vals.end())
    {
        GEN_LA_INSTR(operand.name, tmp_reg, fout);
        GEN_R_INSTR("add", tmp_reg, offset, tmp_reg, fout);
        GEN_LW_INSTR(tmp_reg, 0, reg, fout);
    }else{
        // 根据局部变量寻址 
        GEN_R_INSTR("addi", "sp", std::to_string(stackVar.find_operand(operand)), tmp_reg, fout );
        if(isAddress){
            GEN_LW_INSTR(tmp_reg, 0, tmp_reg, fout);
        }
        GEN_R_INSTR("add", tmp_reg, offset, tmp_reg, fout);
        GEN_LW_INSTR(tmp_reg, 0, reg, fout);
    }
    free_reg(tmp_reg);
}
void backend::Generator::store(ir::Operand operand, std::string reg, int offset, bool isAddress)
{
    std::string tmp_reg = get_reg();
    if (global_vals.find(operand.name) != global_vals.end())
    {
        // 根据全局变量寻址
        GEN_LA_INSTR(operand.name, tmp_reg, fout);
        GEN_SW_INSTR(tmp_reg, offset, reg, fout);
    }else{
        // 根据局部变量寻址
        if(isAddress){
            GEN_LW_INSTR("sp", stackVar.find_operand(operand), tmp_reg, fout);
        }else{
            GEN_R_INSTR("addi", "sp", stackVar.find_operand(operand), tmp_reg, fout); 
        }
        GEN_SW_INSTR(tmp_reg, offset, reg, fout);
    }
    free_reg(tmp_reg);
}

void backend::Generator::store(ir::Operand operand, std::string reg, std::string offset, bool isAddress)
{
    std::string tmp_reg = get_reg();
    if (global_vals.find(operand.name) != global_vals.end())
    {
        // 根据全局变量寻址
        GEN_LA_INSTR(operand.name, tmp_reg, fout);
        GEN_R_INSTR("add", tmp_reg, offset, tmp_reg, fout);
        GEN_SW_INSTR(tmp_reg, 0, reg, fout);
    }else{
        // 根据局部变量寻址
        GEN_R_INSTR("addi", "sp", std::to_string(stackVar.find_operand(operand)), tmp_reg, fout);
        if(isAddress){
            GEN_LW_INSTR(tmp_reg, 0, tmp_reg, fout);
        }
        GEN_R_INSTR("add", tmp_reg, offset, tmp_reg, fout);
        GEN_SW_INSTR(tmp_reg, 0, reg, fout);
    }
    free_reg(tmp_reg);
}

std::string backend::Generator::get_reg(){
    
    for(auto &it : Registers){
        if(it.second){
            it.second = false;
            return it.first;
        }
    }
    assert(0 && "No Temp Register!");
}

void backend::Generator::free_reg(std::string reg){
    try
    {
        Registers[reg] = true;
    }
    catch(const std::exception& e)
    {
        std::cout<<"Error: No Such Register: "<<reg<<std::endl;
        assert(0);
    }
}

void backend::Generator::gen()
{
    // generate global variables
    fout << "\t.data" << std::endl;
    for (auto &global_val : program.globalVal)
    {
        global_vals.insert(global_val.val.name);
        fout << "\t.global " << global_val.val.name << std::endl;
        fout << "\t.type " << global_val.val.name << ", @object" << std::endl;
        fout << "\t.align\t4" << std::endl;
        fout << global_val.val.name << ":" << std::endl;

        int size = (global_val.maxlen > 0) ? global_val.maxlen * 4 : 4;
        // fout << "\t.space " << size << std::endl;
        fout<<"\t.word ";
        for(int i=0; i<size/4; i++){
            fout<<"0";
            if(i < size/4-1) fout<<", ";
        }
        fout<<std::endl;
        
        

        fout << "\t.size " << global_val.val.name << ", .-" << global_val.val.name << std::endl;
        fout << std::endl;
        
    }
    // generate functions
    for (auto &func : program.functions)
    {
        gen_func(func);
    }
    fout.close();
}

int backend::Generator::calc_frame_size(const ir::Function &func)
{
    stackVar._table.clear();
    stackVar.total_size = 0;
    auto &params = func.ParameterList;
    auto &inst_vec = func.InstVec;
    int frame_size = 0;

    std::set<std::string> var_set;
    std::set<std::string> param_set;

    for(auto parm : params) param_set.insert(parm.name);
    // 变量
    for(auto inst : inst_vec){
        if(var_set.find(inst -> des.name) != var_set.end() || param_set.find(inst -> des.name) != param_set.end()){
            continue;
        }
       
        if(inst->op == ir::Operator::alloc){
            var_set.insert(inst -> des.name);
            stackVar.add_operand(inst -> des, std::stoi(inst -> op1.name) * 4);
            frame_size += std::stoi(inst -> op1.name) * 4;
        }else{
            var_set.insert(inst -> des.name);
            stackVar.add_operand(inst -> des, 4);
            frame_size += 4;
        }
    }
    // 参数(栈先进先出)
    for (int i = params.size() - 1; params.size() && i >= 0; i--)
    {
        auto &param = params[i];
        if (var_set.find(param.name) != var_set.end())
            continue; // avoid duplicate (struct)
        var_set.insert(param.name);
        stackVar.add_operand(param, 4);
        frame_size += 4;
    }

    // ra
    frame_size += 4;
    return frame_size;
}

void backend::Generator::init_label(const ir::Function &func){
    label_map.clear();
    int index = 0; // 用于统计当前函数的指令数目，便于计算pc
    for(auto &instr : func.InstVec){
        if(instr -> op == ir::Operator::_goto){
            int pc = index + std::stoi(instr -> des.name);
            label_map[pc] = "L" + std::to_string(label_cnt++);
        }
        index ++;
    }
}


void backend::Generator::gen_func(const ir::Function &function)
{
    // initial
    fout<< std::endl;
    fout << "\t.text" << std::endl; // 汇编代码的文本段
    fout << "\t.global " << function.name << std::endl; // 声明为全局可见
    fout << "\t.type " << function.name << ", @function" << std::endl; // 声明为函数
    fout << function.name << ":" << std::endl; // 函数名

    
    uint32_t frame_size = calc_frame_size(function);
    init_label(function);
    std::string reg_tmp = "t0";
    GEN_LI_INSTR(reg_tmp, -1 * frame_size, fout);
    GEN_R_INSTR("add", "sp", reg_tmp, "sp", fout);

    GEN_LI_INSTR(reg_tmp, frame_size - 4, fout);
    GEN_R_INSTR("add", "sp", reg_tmp, reg_tmp, fout);
    GEN_SW_INSTR(reg_tmp, 0, "ra", fout);


    int index = 0;
    if(function.ParameterList.size() <= returnRegs.size()){
        for(auto parm : function.ParameterList){
            std::string tmp_reg = get_reg();
            GEN_R_INSTR("addi", returnRegs[index++], "0", tmp_reg, fout);
            GEN_SW_INSTR("sp", stackVar.find_operand(parm), tmp_reg, fout);
            free_reg(tmp_reg);
        }
    }

    index = 0;
    for(auto &instr : function.InstVec){
        if(label_map.find(index) != label_map.end()){
            fout << label_map[index] << ":" << std::endl;
        }
        std::vector<std::string> param_names;
        for(auto parm : function.ParameterList){
            param_names.push_back(parm.name);
        }
        if(!(function.name == "_global" && instr->op == ir::Operator::store && instr->des.name == "0")){
            gen_instr(*instr, index, param_names);
        }
        
        index ++;
        if(instr -> op == ir::Operator::_return){
            
            std::string reg_tmp = get_reg();
            GEN_LI_INSTR(reg_tmp, frame_size - 4, fout);
            GEN_R_INSTR("add", "sp", reg_tmp, reg_tmp, fout);
            GEN_LW_INSTR(reg_tmp, 0, "ra", fout);

            GEN_LI_INSTR(reg_tmp, frame_size, fout);
            GEN_R_INSTR("add", "sp", reg_tmp, "sp", fout);
            free_reg(reg_tmp);
            
            fout << "\tjr ra" << std::endl;
        }
    }
    while(label_map.find(index) != label_map.end()){
        fout << label_map[index] << ":" << std::endl;
        index ++;
    }

    fout << "\t.size " << function.name << ", .-" << function.name << std::endl;

}

void backend::Generator::gen_instr(const ir::Instruction& instr, int index, const std::vector<std::string>& params){
    auto &op = instr.op;
    auto &des = instr.des;
    auto &op1 = instr.op1;
    auto &op2 = instr.op2;
    
    // call des, op1(arg1, arg2, ...) : des = op1(arg1, arg2, ...)
    if(op == ir::Operator::call) {
        fout << "# call"  << ", IR: " + instr.draw()<<std::endl;
        auto call_instr = dynamic_cast<const ir::CallInst *>(&instr);
        // 处理参数  
        auto argNums = call_instr->argumentList.size();
        
        if(argNums <= 8){
            for(int i = 0; i < argNums; i++){
                auto arg = call_instr->argumentList[i];
                bool isAddress = !params.empty() && (find(params.begin(), params.end(), arg.name)!=params.end()) && (arg.type == ir::Type::IntPtr);
                auto returnReg = returnRegs[i];
                std::string tmp_reg = get_reg();// "t0";
                switch (arg.type){
                    case ir::Type::Int:
                        load(arg, tmp_reg, 0);
                        GEN_R_INSTR("addi", tmp_reg, 0, returnReg, fout);
                        break;
                    case ir::Type::IntPtr:
                        if(isAddress){
                            load(arg, tmp_reg, 0);
                            GEN_R_INSTR("addi", tmp_reg, 0, returnReg, fout);
                        }else{
                            if(stackVar._table.count(arg.name)){
                                GEN_R_INSTR("addi", "sp", stackVar.find_operand(arg), returnReg, fout);
                            }else{
                                GEN_LA_INSTR(arg.name, returnReg, fout);
                            } 
                        }
                        break;
                    case ir::Type::IntLiteral:
                        GEN_LI_INSTR(returnReg, arg.name, fout);
                        break;
                    default:
                        assert(0 && "Float Type Not Completed");
                        break;
                }
                free_reg(tmp_reg);   
            }
        }else{
            for(int i=0; i<argNums; i++){
                auto &arg = call_instr->argumentList[i];
                std::string tmp_reg = get_reg();
                switch (arg.type){
                    case ir::Type::Int:
                    case ir::Type::IntPtr:
                        load(arg, tmp_reg, 0);
                        GEN_SW_INSTR("sp", -(i+2)*4, tmp_reg, fout);
                        break;
                    case ir::Type::IntLiteral:
                        GEN_LI_INSTR(tmp_reg, arg.name, fout);
                        GEN_SW_INSTR("sp", -(i+2)*4, tmp_reg, fout);
                        break;
                    default:
                        assert(0 && "Float Type Not Completed");
                        break;
                }
                free_reg(tmp_reg);
            }
        }

        // call
        fout<<"\tcall " << call_instr->op1.name <<std::endl;

        // 保存返回值
        std::string tmp_reg = get_reg();
        switch (des.type){
            case ir::Type::Int:
            case ir::Type::IntPtr:
                GEN_R_INSTR("addi", "a0", 0, tmp_reg, fout);
                store(des, tmp_reg, 0);
                break;
            case ir::Type::null:
                break;
            default:
                assert(0 && "Float Type Not Completed");
                break;
        }
        free_reg(tmp_reg);
        
    }else if(op == ir::Operator::_return){ // return op1 
        fout << "# return" << ", IR: " + instr.draw()<<std::endl;
        std::string tmp_reg = get_reg();
        switch (op1.type){
            case ir::Type::Int:
            case ir::Type::IntPtr:
                load(op1, tmp_reg, 0);
                GEN_R_INSTR("addi", tmp_reg, 0, "a0", fout);
                break;
            case ir::Type::IntLiteral:
                GEN_LI_INSTR("a0", op1.name, fout);
                break;
            case ir::Type::null:
                break;
            default:
                assert(0 && "Float Type Not Completed");
                break;
        }
        free_reg(tmp_reg);
    }else if(op == ir::Operator::def || op == ir::Operator::mov){
        fout<<(op == ir::Operator::def ? "# def" : "# mov") << ", IR: " + instr.draw() <<std::endl;
        std::string tmp_reg1 = get_reg();
        std::string tmp_reg2 = get_reg();
        switch (op1.type){
            case ir::Type::Int:
                load(op1, tmp_reg1, 0);
                GEN_R_INSTR("addi", tmp_reg1, 0, tmp_reg2, fout);
                break;
            case ir::Type::IntLiteral:
                GEN_LI_INSTR(tmp_reg2, op1.name, fout);
                break;
            case ir::Type::null:
                break;
            default:
                assert(0 && "Float Type Not Completed");
                break;
        }
        store(des, tmp_reg2, 0);
        free_reg(tmp_reg1);
        free_reg(tmp_reg2);
    }else if(op == ir::Operator::sub || op == ir::Operator::mul || op == ir::Operator::div 
        || op == ir::Operator::mod || op == ir::Operator::add){ 
        // sub des, op1, op2 | mul des, op1, op2 | div des, op1, op2
        std::string op_type_string = ir::toString(op);
        fout<<"# " + op_type_string << ", IR: " + instr.draw()<<std::endl;
        if(op == ir::Operator::mod) op_type_string = "rem";
        std::string rd = get_reg(), rs = get_reg(), rt = get_reg();
        if(op1.type == ir::Type::Int && op2.type == op1.type){
            load(op1, rs, 0);
            load(op2, rt, 0);
        }else if(op1.type == op2.type && op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            load(op2, rt, 0);
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, rs, 0);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else assert(0 && "Wrong Op Type");
        GEN_R_INSTR(op_type_string, rs, rt, rd, fout);
        store(des, rd, 0);
        free_reg(rd); free_reg(rs), free_reg(rt);
    }else if(op == ir::Operator::eq || op == ir::Operator::neq){ // eq(neq) des, op1, op2
        std::string op_type_string = ir::toString(op);
        fout<<"# " + op_type_string << ", IR: " + instr.draw()<<std::endl;
        std::string rd = get_reg(), rs = get_reg(), rt = get_reg();
        if(op1.type == ir::Type::Int && op2.type == op1.type){
            load(op1, rs, 0);
            load(op2, rt, 0);
        }else if(op1.type == op2.type && op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            load(op2, rt, 0);
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, rs, 0);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else assert(0 && "Wrong Op Type");
        GEN_R_INSTR("xor", rs, rt, rd, fout);
        if(op == ir::Operator::eq){
            fout<<"\tseqz "<<rd <<", "<<rd<<std::endl; 
        }
        store(des, rd, 0);
        free_reg(rd); free_reg(rs), free_reg(rt);
    }else if(op == ir::Operator::_and || op == ir::Operator::_or){ // and(or) des, op1, op2
        std::string op_type_string = op == ir::Operator::_and ? "and" : "or"; 
        fout<<"# " + op_type_string << ", IR: " + instr.draw()<<std::endl;
        std::string rd = get_reg(), rs = get_reg(), rt = get_reg();
        if(op1.type == ir::Type::Int && op2.type == op1.type){
            load(op1, rs, 0);
            load(op2, rt, 0);
        }else if(op1.type == op2.type && op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            load(op2, rt, 0);
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, rs, 0);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else assert(0 && "Wrong Op Type");
        fout<<"\tsnez "<<rs<<", "<<rs<<std::endl;
        fout<<"\tsnez "<<rt<<", "<<rt<<std::endl;
        GEN_R_INSTR(op_type_string, rs, rt, rd, fout);
        store(des, rd, 0);
        free_reg(rs), free_reg(rt), free_reg(rd);
    }else if(op == ir::Operator::_not){
        fout<<"# not" << ", IR: " + instr.draw()<<std::endl;
        // std::string rd = "t0", rs = "t1", rt = "t2";
        std::string rd = get_reg(), rs = get_reg(), get_reg();
        if(op1.type == ir::Type::Int){
            load(op1, rs, 0);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
        }else{
            assert(0 && ("Wrong Op Type"));
        }
        fout<<"\tseqz "<<rd<<", "<<rs<<std::endl;
        store(des, rd, 0);
        free_reg(rd), free_reg(rs);
    }else if(op == ir::Operator::lss || op == ir::Operator::gtr){ // ----> 有问题
        fout<<"# " + ir::toString(op) << ", IR: " + instr.draw()<<std::endl;
        std::string rd = get_reg(), rt = get_reg(), rs = get_reg();
        if(op1.type == ir::Type::Int && op2.type == op1.type){
            load(op1, rs, 0);
            load(op2, rt, 0);
        }else if(op1.type == op2.type && op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            load(op2, rt, 0);
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, rs, 0);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else assert(0 && "Wrong Op Type");
        
        if(op == ir::Operator::lss){
            GEN_R_INSTR("slt", rs, rt, rd, fout);
        }else{
            GEN_R_INSTR("slt", rt, rs, rd, fout);
        }
        store(des, rd, 0);
        free_reg(rd), free_reg(rt), free_reg(rs);
    }else if(op == ir::Operator::leq || op == ir::Operator::geq){ // // ----> 有问题
        fout<<"# " + ir::toString(op) << ", IR: " + instr.draw()<<std::endl;
        std::string rd = get_reg(), rt = get_reg(), rs = get_reg();
        if(op1.type == ir::Type::Int && op2.type == op1.type){
            load(op1, rs, 0);
            load(op2, rt, 0);
        }else if(op1.type == op2.type && op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            load(op2, rt, 0);
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, rs, 0);
            GEN_LI_INSTR(rt, op2.name, fout);
        }else assert(0 && "Wrong Op Type");
        if(op == ir::Operator::geq){
            GEN_R_INSTR("slt", rs, rt, rd, fout);
            fout<<"\tseqz "<<rd<<", "<<rd<<std::endl;
        }else{
            GEN_R_INSTR("slt", rt, rs, rd, fout);
            fout<<"\tseqz "<<rd<<", "<<rd<<std::endl;
        }
        store(des, rd, 0);
        free_reg(rd), free_reg(rt), free_reg(rs);
    }else if(op == ir::Operator::_goto){ // goto op1, des: if(op1) goto des
        fout<<"# goto" << ", IR: " + instr.draw()<<std::endl;
        std::string rs = get_reg();
        if(op1.type == ir::Type::null){
            fout<<"\tj " << label_map[index + std::stoi(des.name)] <<std::endl;
        }else if(op1.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(rs, op1.name, fout);
            fout<<"\tbnez "<<rs<<", "<<label_map[index + std::stoi(des.name)]<<std::endl;
        }else if(op1.type == ir::Type::Int){
            load(op1, rs, 0);
            fout<<"\tbnez "<<rs<<", "<<label_map[index + std::stoi(des.name)]<<std::endl;
        }else assert(0 && "Wrong Op Type");
        free_reg(rs);
    }else if(op == ir::Operator::store){ // store des, op1, op2 : op1[op2] = des
        bool isAddress = !params.empty() && (find(params.begin(), params.end(), op1.name)!=params.end()) 
            && (op1.type == ir::Type::IntPtr);
        fout<<"# store" << ", IR: " + instr.draw()<<std::endl;
        std::string data_reg = get_reg();
        if(des.type == ir::Type::IntLiteral){
            GEN_LI_INSTR(data_reg, des.name, fout);
        }else if(des.type == ir::Type::Int){
            load(des, data_reg, 0);
        }else assert(0 && "Wrong Op Type");

        if(op2.type == ir::Type::Int){
            std::string offset_reg = get_reg();
            load(op2, offset_reg, 0);
            GEN_R_INSTR("slli", offset_reg, "2", offset_reg, fout);
            store(op1, data_reg, offset_reg, isAddress);
            free_reg(offset_reg); 
        }else if(op2.type == ir::Type::IntLiteral){
            std::string offset_reg = get_reg();
            GEN_LI_INSTR(offset_reg, std::stoi(op2.name)*4, fout);
            store(op1, data_reg, offset_reg, isAddress);
            free_reg(offset_reg);
        }else assert(0 && "Wrong Op Type");
        free_reg(data_reg);
    }else if(op == ir::Operator::load){ // load des, op1, op2 : des = op1[op2]
        bool isAddress = !params.empty() && (find(params.begin(), params.end(), op1.name) != params.end()) 
            && (op1.type == ir::Type::IntPtr);
        fout<<"# load" << ", IR: " + instr.draw()<<std::endl;
        std::string data_reg = get_reg();
        if(op2.type == ir::Type::Int){
            std::string offset_reg = get_reg();
            load(op2, offset_reg, 0);
            GEN_R_INSTR("slli", offset_reg, "2", offset_reg, fout);
            load(op1, data_reg, offset_reg, isAddress);
            free_reg(offset_reg); 
        }else if(op2.type == ir::Type::IntLiteral){
            load(op1, data_reg, std::stoi(op2.name)*4, isAddress);
        }else assert(0 && "Wrong Op Type");
        store(des, data_reg, 0);
        free_reg(data_reg);
    }else if(op != ir::Operator::alloc){
        assert(0 && "Cur op Not Completed or wrong op!");
    }
}
