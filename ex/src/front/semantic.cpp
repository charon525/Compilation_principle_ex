#include"front/semantic.h"

#include<cassert>
#include <numeric>
#include <iostream>
#include <cmath>

using namespace frontend;
using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

using std::vector;
using std::map;

#define TODO assert(0 && "TODO");

// 获取一个树节点的指定类型子节点的指针，若获取失败，使用 assert 断言来停止程序的执行
#define GET_CHILD_PTR(node, node_type, index)                     \
    auto node = dynamic_cast<node_type *>(root->children[index]); \
    assert(node);


// 将一个表达式节点的信息复制到另一个表达式节点中
#define COPY_EXP_NODE(from, to)              \
    to->is_computable = from->is_computable; \
    to->v = from->v;                         \
    to->t = from->t;

// 判断节点类型
#define NODE_IS(node_type, index) root->children[index]->type == NodeType::node_type

map<std::string,ir::Function*>* frontend::get_lib_funcs() {
    static map<std::string,ir::Function*> lib_funcs = {
        {"getint", new Function("getint", Type::Int)},
        {"getch", new Function("getch", Type::Int)},
        {"getfloat", new Function("getfloat", Type::Float)},
        {"getarray", new Function("getarray", {Operand("arr", Type::IntPtr)}, Type::Int)},
        {"getfarray", new Function("getfarray", {Operand("arr", Type::FloatPtr)}, Type::Int)},
        {"putint", new Function("putint", {Operand("i", Type::Int)}, Type::null)},
        {"putch", new Function("putch", {Operand("i", Type::Int)}, Type::null)},
        {"putfloat", new Function("putfloat", {Operand("f", Type::Float)}, Type::null)},
        {"putarray", new Function("putarray", {Operand("n", Type::Int), Operand("arr", Type::IntPtr)}, Type::null)},
        {"putfarray", new Function("putfarray", {Operand("n", Type::Int), Operand("arr", Type::FloatPtr)}, Type::null)},
    };
    return &lib_funcs;
}

void frontend::SymbolTable::add_scope() {
    scope_stack.push_back(ScopeInfo());
}
void frontend::SymbolTable::exit_scope() {
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const {
    return scope_stack.back().name + "_" + id ;
}

Operand frontend::SymbolTable::get_operand(string id) const {
    return get_ste(id).operand;
}

frontend::STE frontend::SymbolTable::get_ste(string id) const {
    for (int i = scope_stack.size() - 1; i >= 0; i--) {
        if (scope_stack[i].table.find(id) != scope_stack[i].table.end()) {
            return scope_stack[i].table.at(id);
        }
    }
    return STE();
}

frontend::Analyzer::Analyzer(): tmp_cnt(0), symbolTable() {
    symbolTable.add_scope();
    symbolTable.scope_stack.back().name = "global";
}



void frontend::SymbolTable::add_ste(std::string name, STE ste)
{
    scope_stack.back().table[name] = ste;
}


ir::Program frontend::Analyzer::get_ir_program(CompUnit* root) {
    ir::Program program;
    
    analysisCompUnit(root);

    // 添加全局变量
    for (auto &p : symbolTable.scope_stack[0].table) // 遍历最外层作用域的符号表
    {
        if (p.second.dimension.size()) // 如果是数组
        {
            // 计算数组大小
            int size = std::accumulate(p.second.dimension.begin(), p.second.dimension.end(), 1, std::multiplies<int>());
            // 添加数组
            program.globalVal.push_back({p.second.operand, size});
            continue;
        }
        if (p.second.operand.type == ir::Type::FloatLiteral || p.second.operand.type == ir::Type::IntLiteral)
            p.second.operand.name = p.first;
        // 添加变量
        program.globalVal.push_back({p.second.operand});
    }

    // 把全局变量的初始化指令添加到 _global 函数中
    Function g("_global", ir::Type::null);
    for (auto &i : g_init_inst) // 遍历全局变量的初始化指令
    {
        g.addInst(i);
    }
    g.addInst(new Instruction({}, {}, {}, Operator::_return));
    program.addFunction(g);

    auto call_global = new ir::CallInst(Operand("_global", ir::Type::null), Operand());

    // 把函数添加到 ir::Program 中
    for (auto &f : symbolTable.functions)
    {
        if (f.first == "main") // 如果是main函数
        {
            // 在main函数前面添加一个调用 _global 函数的指令
            f.second->InstVec.insert(f.second->InstVec.begin(), call_global);
        }
        program.functions.push_back(*f.second);
    }
    return program;
}

/**
 * CompUnit：表示整个程序单元，包含了程序中的声明和函数定义。
 * CompUnit -> (Decl | FuncDef) [CompUnit] 
*/
void Analyzer::analysisCompUnit(frontend::CompUnit* root) {
    if(NODE_IS(DECL, 0)){ // 如果是声明，即是全局变量
        
        GET_CHILD_PTR(decl, Decl, 0);
        // 生成全局变量的初始化指令
        analysisDecl(decl, g_init_inst);
    }else if(NODE_IS(FUNCDEF, 0)){ // 如果是函数定义
        GET_CHILD_PTR(funcdef, FuncDef, 0);
        // 函数定义进入新的作用域
        symbolTable.add_scope();
        
        // 解析节点
        analysisFuncDef(funcdef);
        // 函数解析完成退出作用域
        symbolTable.exit_scope();
    }
    if(root->children.size() == 2){
        GET_CHILD_PTR(comp, CompUnit, 1);
        analysisCompUnit(comp);
    }
}

/**
 * Decl：表示声明，可以是常量声明（ConstDecl）或变量声明（VarDecl）。
 * Decl -> ConstDecl | VarDecl  
*/
void Analyzer::analysisDecl(Decl *root, vector<Instruction *> &instructions) {
    if(NODE_IS(VARDECL, 0)){ // 变量声明
        GET_CHILD_PTR(vardecl, VarDecl, 0);
        // 将变量声明指令添加到instructions中
        analysisVarDecl(vardecl, instructions);
    }else if(NODE_IS(CONSTDECL, 0)){ // 常量声明
        
        GET_CHILD_PTR(constdecl, ConstDecl, 0);
        // 将常量声明指令添加到instructions中
        analysisConstDecl(constdecl, instructions);
    }
}

/**
 * ConstDecl：常量声明，用于定义常量变量。
 * ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
 *      ConstDecl.t
*/
void Analyzer::analysisConstDecl(ConstDecl *root, vector<Instruction *> &instructions) {
    GET_CHILD_PTR(btype, BType, 1);
    analysisBType(btype);
    root -> t = btype -> t;
    
    for(size_t index = 2; index < root -> children.size(); index += 2){
        GET_CHILD_PTR(constdef, ConstDef, index);
        // 将常量定义指令添加到instructions中, 同时将类型传递
        analysisConstDef(constdef, instructions, root->t);
    }
}

/**
 * BType：基本类型，可以是整型（int）或浮点型（float）。
 * BType -> 'int' | 'float'
 *   BType.t
*/
void frontend::Analyzer::analysisBType(BType *root)
{
    GET_CHILD_PTR(term, Term, 0);
    if(term->token.type == TokenType::INTTK){
        root -> t = Type::Int;
    }else if(term->token.type == TokenType::FLOATTK){
        root -> t = Type::Float;
    }
}


/**
 * ConstDef：常量定义，用于定义具体的常量变量。
 * ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
 *      ConstDef.t
*/
void Analyzer::analysisConstDef(ConstDef *root, vector<Instruction *> &instructions, Type t) {
    GET_CHILD_PTR(ident, Term, 0);
    root -> arr_name = symbolTable.get_scoped_name(ident->token.value);
    
    vector<int> dimension;
    for(size_t index = 2; index < root -> children.size(); index += 3){
        if(!(NODE_IS(CONSTEXP, index))) break; // 不是数组终止循环
        GET_CHILD_PTR(constexp, ConstExp, index);
        constexp -> t = t;
        analysisConstExp(constexp, instructions);
        dimension.push_back(std::stoi(constexp -> v));
    }
    // 计算数组大小 
    int size = accumulate(dimension.begin(), dimension.end(), 1, std::multiplies<int>());
    Type res_type = t;

    if(dimension.empty()){ // 不是数组情况
        
        if(res_type == Type::Int){
            // def 定义整形变量， op1立即数或变量，op2不适用，des被复制变量
            instructions.push_back(new Instruction(
                {"0", Type::IntLiteral},
                {},
                {root -> arr_name, Type::Int},
                Operator::def));
            res_type = Type::IntLiteral;
        }else if(res_type == Type::Float){
            instructions.push_back(new Instruction(
                {"0.0", Type::FloatLiteral},
                {},
                {root -> arr_name, Type::Float},
                Operator::fdef));
            res_type = Type::FloatLiteral;
        }
    }else{// 数组的情况
        
        if(res_type == Type::Int){
            // alloc内存分配， op1数组长度，op2不适用，des数组名，数组名被视为一个指针
            instructions.push_back(new Instruction(
                {std::to_string(size), Type::IntLiteral},
                {},
                {root -> arr_name, Type::IntPtr},
                Operator::alloc));
            res_type = Type::IntPtr;
        }else if(res_type == Type::Float){
            instructions.push_back(new Instruction(
                {std::to_string(size), Type::IntLiteral},
                {},
                {root -> arr_name, Type::FloatPtr},
                Operator::alloc));
            res_type = Type::FloatPtr;
        }else assert(0);
    }

    GET_CHILD_PTR(constinitval, ConstInitVal, root -> children.size() - 1);
    // 分析初始化值
    constinitval -> v = root -> arr_name;
    constinitval -> t = t;
    analysisConstInitVal(constinitval, instructions);

    // 如果是常量，constinitval->v是一个立即数，Type应该为Literal; 
    // 如果是数组，constinitval->v是数组名，Type应该为Ptr
    // 将记录添加至记录表
    symbolTable.add_ste(ident->token.value,
                             {Operand(constinitval->v, res_type),
                              dimension});
}

/**
 * ConstInitVal：常量初始值，可以是一个常量表达式（ConstExp）或者一组初始值。
 * ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
 *   ConstInitVal.v
 *   ConstInitVal.t
*/
void Analyzer::analysisConstInitVal(ConstInitVal *root, vector<Instruction *> &instructions) {
    if(NODE_IS(CONSTEXP, 0)){ // 常量情况
        GET_CHILD_PTR(constexp, ConstExp, 0);
        constexp -> v = "t" + std::to_string(tmp_cnt++);
        constexp -> t = root -> t; // int or float
        analysisConstExp(constexp, instructions);
        if(!(constexp->v == "0" || constexp->v == "0.0")){
            if(root -> t == Type::Int){
                // 如果存在 int a = 1.1等情况
                 if (constexp->t == ir::Type::FloatLiteral)
                {
                    constexp->t = ir::Type::IntLiteral;
                    constexp->v = std::to_string((int)std::stof(constexp->v));
                }
                // mov赋值，op1立即数或变量，op2不使用，des被赋值变量
                instructions.push_back(new Instruction(
                    {constexp->v, Type::IntLiteral},
                    {},
                    {root -> v, Type::Int},
                    Operator::mov));
            }else if(root -> t == Type::Float){
                // 如果存在 float a = 1等情况
                if (constexp->t == ir::Type::IntLiteral)
                {
                    constexp->t = ir::Type::FloatLiteral;
                    constexp->v = std::to_string((float)std::stoi(constexp->v));
                }
                instructions.push_back(new Instruction(
                    {constexp->v, Type::FloatLiteral},
                    {},
                    {root -> v, Type::Float},
                    Operator::fmov));
            }
        }
        // 用于更新记录表
        root -> v = constexp -> v; 
        root -> t = constexp -> t;
        
    }else{ // 数组情况 '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
        int insert_index = 0;
        for(size_t index=1; index < root -> children.size() - 1; index += 2){
            if(NODE_IS(CONSTINITVAL, index)){
                GET_CHILD_PTR(constinitval, ConstInitVal, index);
                constinitval -> v = "t" + std::to_string(tmp_cnt++);
                constinitval -> t = root -> t;
                analysisConstInitVal(constinitval, instructions);
                if(root -> t == Type::Int){
                    // store:存储，op1:数组名，op2:下标，des:存入的数
                    instructions.push_back(new Instruction(
                        {root->v, Type::IntPtr},
                        {std::to_string(insert_index), Type::IntLiteral},
                        {constinitval -> v, Type::IntLiteral},
                        Operator::store));
                }else if(root -> t == Type::Float){
                    instructions.push_back(new Instruction(
                        {root->v, Type::FloatPtr},
                        {std::to_string(insert_index), Type::IntLiteral},
                        {constinitval -> v, Type::FloatLiteral},
                        Operator::store));
                }
            }
            insert_index ++;
            
        }
    }
}

/**
 * VarDecl：变量声明，用于定义变量。
 * VarDecl -> BType VarDef { ',' VarDef } ';'
 *   VarDecl.t
*/
void Analyzer::analysisVarDecl(VarDecl *root, vector<Instruction *> &instructions) {
    GET_CHILD_PTR(btype, BType, 0);
    analysisBType(btype);
    root -> t = btype -> t;
    GET_CHILD_PTR(vardef, VarDef, 1);
    analysisVarDef(vardef, instructions, root -> t);
    for (int i = 3; i < root -> children.size(); i += 2) {
        GET_CHILD_PTR(vardef, VarDef, i);
        analysisVarDef(vardef, instructions, btype -> t);
    }
}

/**
 * yes
 * VarDef：变量定义，用于定义具体的变量。
 * VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
 *   VarDef.arr_name
*/
void frontend::Analyzer::analysisVarDef(VarDef *root, vector<Instruction *> &instructions, ir::Type type)
{
    GET_CHILD_PTR(ident, Term, 0);
    root->arr_name = symbolTable.get_scoped_name(ident->token.value);

    std::vector<int> dimension;

    for (size_t index = 2; index < root->children.size(); index += 3)
    {
        if (!(NODE_IS(CONSTEXP, index)))
            break;
        GET_CHILD_PTR(constexp, ConstExp, index);
        constexp->v = root->arr_name;
        constexp->t = type;
        analysisConstExp(constexp, instructions);
        dimension.push_back(std::stoi(constexp->v));
    }

    int size = std::accumulate(dimension.begin(), dimension.end(), 1, std::multiplies<int>());

    if (dimension.empty())
    {
        switch (type)
        {
        case ir::Type::Int:
            // def:定义整形变量，op1:立即数或变量，op2:不使用，des:被定义变量
            instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Int},
                                                   Operator::def));
            break;
        case ir::Type::Float:
            //  fdef:定义浮点数变量，op1:立即数或变量，op2:不使用，des:被定义变量
            instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::Float},
                                                   Operator::fdef));
            break;
        default:
            break;
        }
    }
    else
    {
        switch (type)
        {
        case ir::Type::Int:
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针。
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::IntPtr},
                                                   Operator::alloc));
            type = ir::Type::IntPtr;
            // 初始化数组
            for (int insert_index = 0; insert_index < size; insert_index++)
            {
                // store:存储，op1:数组名，op2:下标，des:存入的数
                instructions.push_back(new Instruction({root->arr_name, ir::Type::IntPtr},
                                                       {std::to_string(insert_index), ir::Type::IntLiteral},
                                                       {"0", ir::Type::IntLiteral},
                                                       Operator::store));
            }
            break;
        case ir::Type::Float:
            // alloc:内存分配，op1:数组长度，op2:不使用，des:数组名，数组名被视为一个指针。
            instructions.push_back(new Instruction({std::to_string(size), ir::Type::IntLiteral},
                                                   {},
                                                   {root->arr_name, ir::Type::FloatPtr},
                                                   Operator::alloc));
            type = ir::Type::FloatPtr;
            // 初始化数组
            for (int insert_index = 0; insert_index < size; insert_index++)
            {
                // store:存储，op1:数组名，op2:下标，des:存入的数
                instructions.push_back(new Instruction({root->arr_name, ir::Type::FloatPtr},
                                                       {std::to_string(insert_index), ir::Type::IntLiteral},
                                                       {"0.0", ir::Type::FloatLiteral},
                                                       Operator::store));
            }
            break;
        default:
            break;
        }
    }

    if (NODE_IS(INITVAL, root->children.size() - 1)) // 如果有初始化值
    {
        GET_CHILD_PTR(initval, InitVal, root->children.size() - 1);
        initval->v = root->arr_name;
        initval->t = type;
        analysisInitVal(initval, instructions);
    }

    symbolTable.add_ste(ident->token.value,
                             {Operand(root->arr_name, type),
                              dimension});
}

/**
 * yes
 * InitVal：变量初始值，可以是一个表达式（Exp）或者一组初始值。
 * InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
 *  InitVal.is_computable
 *  InitVal.v
 *  InitVal.t
*/
void Analyzer::analysisInitVal(InitVal *root, vector<Instruction *> &instructions) {
    if(NODE_IS(EXP, 0)){
        GET_CHILD_PTR(exp, Exp, 0);
        exp->v = "t" + std::to_string(tmp_cnt++);
        exp->t = root->t; // int or float
        analysisExp(exp, instructions);
        
        if(!(exp->v == "0" || exp->v == "0.0")){ // 说明表达式的值不是0，需要赋值给根节点
            if(root->t == Type::Int){
                if(exp->t == Type::FloatLiteral){ // 如果是浮点立即数直接转换即可
                    exp->t = Type::IntLiteral;
                    exp->v = std::to_string((int)std::stof(exp->v));
                }else if(exp->t == Type::Float){ //如果是浮点数表达式,则需要添加类型转换指令
                    // cvt_f2i:浮点数转整数, op1浮点数，op2不使用，des整数
                    instructions.push_back(new Instruction(
                        {exp->v, Type::Float},
                        {},
                        {exp->v, Type::Int},
                        {Operator::cvt_f2i}
                    ));
                    exp->t = Type::Int;
                }

                // 添加赋值指令
                // mov赋值，op1立即数或变量，op2不使用，des被赋值变量
                instructions.push_back(new Instruction(
                    {exp->v, exp->t},
                    {},
                    {root->v, Type::Int},
                    {Operator::mov}
                ));
            }
        }else if(root->t == Type::Float){
            if(exp->t == Type::IntLiteral){ //表达式的值是整数立即数，则直接转换即可
                exp->t = Type::FloatLiteral;
                exp->v = std::to_string((float)std::stoi(exp->v));
            }else if(exp->t == Type::Int){ //如果是整数表达式，则需要添加类型转换指令
                // cvt_i2f:整数转浮点数, op1整数，op2不使用，des浮点数
                instructions.push_back(new Instruction(
                    {exp->v, Type::Int},
                    {},
                    {exp->v, Type::Float},
                    {Operator::cvt_i2f}
                ));
                exp->t = Type::Float;
            }

            // 添加赋值指令
            // fmov:浮点数赋值，op1立即数或变量，op2不使用，des被赋值变量
            instructions.push_back(new Instruction(
                {exp->v, exp->t},
                {},
                {root->v, Type::Float},
                {Operator::fmov}
            ));
        }
        root->v = exp->v;
        root->t = exp->t;
        
    }else{
        int insert_index = 0;
        for(size_t index = 1; index < root -> children.size(); index += 2){
            if(!(NODE_IS(INITVAL, index))) break; // 不是数组终止循环或者是空数组
            GET_CHILD_PTR(initval, InitVal, index);
            initval->v = "t" + std::to_string(tmp_cnt++);
            initval->t = root->t; // IntLiteral or FloatLiteral
            analysisInitVal(initval, instructions);
            switch (root->t)
            {   
                case Type::IntPtr:
                    if(initval -> t == Type::Float){
                        instructions.push_back(new Instruction(
                            {initval->v, initval->t},
                            {},
                            {initval->v, Type::Int},
                            {Operator::cvt_f2i}
                        ));
                        initval->t = Type::Int;
                    }else if(initval -> t == Type::FloatLiteral){
                        initval -> t = Type::IntLiteral;
                        initval -> v = std::to_string((int)std::stof(initval->v));
                    }
                    break;
                case Type::FloatPtr:
                    if(initval -> t == Type::Int){
                        instructions.push_back(new Instruction(
                            {initval->v, initval->t},
                            {},
                            {initval->v, Type::Float},
                            {Operator::cvt_i2f}
                        ));
                        initval->t = Type::Float;
                    }else if(initval -> t == Type::IntLiteral){
                        initval -> t = Type::FloatLiteral;
                        initval -> v = std::to_string((float)std::stoi(initval->v));
                    }
                    break;
                default:
                    assert(0);
            }
            // store存储，op1数组名，op2下标，des存入的数
            instructions.push_back(new Instruction(
                {root->v, root->t},
                {std::to_string(insert_index), Type::IntLiteral},
                {initval->v, initval->t},
                {Operator::store}
            ));
            insert_index ++;
            
        }
    }
}

/**
 * yes
 * FuncDef：函数定义，用于定义函数。
 * FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
 *  FuncDef.t
 *  FuncDef.n
*/
void Analyzer::analysisFuncDef(FuncDef *root) {
    // 得到函数的返回值类型
    GET_CHILD_PTR(functype, FuncType, 0);
    analysisFuncType(functype, root->t);
    
    // 得到函数名称
    GET_CHILD_PTR(ident, Term, 1);
    root->n = ident->token.value;
    symbolTable.scope_stack.back().name = root->n;

    auto params = new vector<Operand>();
    if(NODE_IS(FUNCFPARAMS, 3)){ // 有函数参数
        // 得到参数列表
        GET_CHILD_PTR(funcfparams, FuncFParams, 3);
        analysisFuncFParams(funcfparams, *params);
    }
    
    // 添加至符号表
    symbolTable.functions[root->n] = new Function(root->n, *params, root->t);

    // 得到函数指令
    GET_CHILD_PTR(block, Block, root->children.size()-1);
    analysisBlock(block, symbolTable.functions[root->n]->InstVec);

    // 若函数没有return语句
    if(symbolTable.functions[root->n]->InstVec.back()->op != Operator::_return){
        symbolTable.functions[root->n]->InstVec.push_back(new Instruction(
            {},
            {},
            {},
            {Operator::_return}
        ));
    }
}

/**
 * FuncType：函数返回类型，可以是void、int或float。
 * FuncType -> 'void' | 'int' | 'float'
*/
void Analyzer::analysisFuncType(FuncType *root, Type &t) {
    GET_CHILD_PTR(term, Term, 0);
    if(term->token.type == TokenType::VOIDTK){
        t = Type::null;
    }else if(term->token.type == TokenType::INTTK){
        t = Type::Int;
    }else if(term->token.type == TokenType::FLOATTK){
        t = Type::Float;
    }
}

/**
 * yes
 * FuncFParam：函数形参，用于定义函数参数。
 * FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
*/
void frontend::Analyzer::analysisFuncFParam(FuncFParam *root, vector<Operand> &params)
{
    GET_CHILD_PTR(btype, BType, 0);
    analysisBType(btype);
    auto type = btype->t;

    GET_CHILD_PTR(ident, Term, 1);

    vector<int> dimension;
    if (root->children.size() > 2)
    {
        if (root->children.size() == 4) // 维度为1
        {
            switch (type)
            {
            case ir::Type::Int:
            case ir::Type::IntLiteral:
                type = ir::Type::IntPtr;
                break;
            case ir::Type::Float:
            case ir::Type::FloatLiteral:
                type = ir::Type::FloatPtr;
                break;
            default:
                break;
            }
            dimension.push_back(-1);
        }
        else
        {
            TODO;
        }
    }

    params.push_back(Operand(symbolTable.get_scoped_name(ident->token.value), type));
    symbolTable.add_ste(ident->token.value,
                             {Operand(symbolTable.get_scoped_name(ident->token.value), type), dimension});
}

/**
 * yes
 * FuncFParams：函数形参列表，包含多个函数形参。
 * FuncFParams -> FuncFParam { ',' FuncFParam }
*/
void frontend::Analyzer::analysisFuncFParams(FuncFParams *root, vector<Operand> &params)
{
    // ---->  
    for (size_t i = 0; i < root->children.size(); i += 2)
    {
        if (NODE_IS(FUNCFPARAM, i))
        {
            GET_CHILD_PTR(funcfparam, FuncFParam, i);
            analysisFuncFParam(funcfparam, params);
        }
    }
}

/**
 * Block：代码块，表示一组语句的集合。
 * Block -> '{' { BlockItem } '}'
*/
void Analyzer::analysisBlock(Block *root, vector<Instruction*> &instructions) {
    // 添加指令
    for(size_t i=1; i<root->children.size()-1; i++){
        GET_CHILD_PTR(blockitem, BlockItem, i);
        analysisBlockItem(blockitem, instructions);
    }
}

/**
 * BlockItem：代码块中的元素，可以是声明（Decl）或语句（Stmt）。
 * BlockItem -> Decl | Stmt
*/
void Analyzer::analysisBlockItem(BlockItem *root, vector<Instruction*> &instructions) {
    if(NODE_IS(DECL, 0)){ // 如果是声明
        GET_CHILD_PTR(decl, Decl, 0);
        analysisDecl(decl, instructions);
    }else{ // 如果是语句
        GET_CHILD_PTR(stmt, Stmt, 0);
        analysisStmt(stmt, instructions);
    }
}

/**
 * yes
 * Stmt：语句，可以是赋值语句、条件语句、循环语句等。
 * Stmt -> LVal '=' Exp ';' 
 *  | Block 
 *  | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] 
 *  | 'while' '(' Cond ')' Stmt 
 *  | 'break' ';' 
 *  | 'continue' ';' 
 *  | 'return' [Exp] ';' 
 *  | [Exp] ';'
 *  Stmt.jump_eow; end of while
 *  Stmt.jump_bow; begin of while
*/
void Analyzer::analysisStmt(Stmt *root, vector<Instruction*> &instructions){
    if(NODE_IS(LVAL, 0)){ // 赋值语句
        // 获取右值
        GET_CHILD_PTR(exp, Exp, 2);
        exp->v = "t" + std::to_string(tmp_cnt++);
        exp->t = Type::Int; // 初始化为Int
        analysisExp(exp, instructions);

        GET_CHILD_PTR(lval, LVal, 0);
        // 获取左值
        COPY_EXP_NODE(exp, lval);

        // LVal被赋值，is_left = true
        analysisLVal(lval, instructions, true);

        
    }else if(NODE_IS(BLOCK, 0)){ // 代码块
        // 增加作用域
        symbolTable.add_scope();
        symbolTable.scope_stack.back().name = "_block";
        GET_CHILD_PTR(block, Block, 0);
        analysisBlock(block, instructions);
        // 解析完后删除作用域
        symbolTable.exit_scope();
    }else if(NODE_IS(EXP, 0)){ // 表达式语句
        GET_CHILD_PTR(exp, Exp, 0);
        exp->v = "t" + std::to_string(tmp_cnt++);
        exp->t = Type::Int; // 初始化为Int
        analysisExp(exp, instructions);
        
    }else if(NODE_IS(TERMINAL, 0)){
        GET_CHILD_PTR(term, Term, 0);
        switch (term->token.type)
        {
        case TokenType::IFTK: // 'if' '(' Cond ')' Stmt [ 'else' Stmt ] 
        {
            symbolTable.add_scope();
            symbolTable.scope_stack.back().name = "_IF";
            GET_CHILD_PTR(cond, Cond, 2);
            cond->v = "t" + std::to_string(tmp_cnt++);
            cond->t = ir::Type::null; // 初始化为null
            analysisCond(cond, instructions);
            // op1:跳转条件，整形变量(条件跳转)或null(无条件跳转)
            // op2:不使用
            // des:常量，值为跳转相对目前pc的偏移量

            // 当条件为真时跳转到if_true_stmt（下下条指令）
            // 否则继续执行下一条指令，下一条指令也应为跳转指令，指向if_true_stmt结束之后的指令
            instructions.push_back(new Instruction({cond->v, cond->t},
                                                   {},
                                                   {"2", ir::Type::IntLiteral},
                                                   Operator::_goto));

            vector<Instruction *> if_true_instructions;
            GET_CHILD_PTR(if_true_stmt, Stmt, 4);
            analysisStmt(if_true_stmt, if_true_instructions);
            symbolTable.exit_scope();

            vector<Instruction *> if_false_instructions;
            if (root->children.size() == 7)
            {
                symbolTable.add_scope();
                symbolTable.scope_stack.back().name = "_ELSE";
                GET_CHILD_PTR(if_false_stmt, Stmt, 6);
                analysisStmt(if_false_stmt, if_false_instructions);
                symbolTable.exit_scope();
                // 执行完if_true_stmt后跳转到if_false_stmt后面
                if_true_instructions.push_back(new Instruction({},
                                                               {},
                                                               {std::to_string(if_false_instructions.size() + 1), ir::Type::IntLiteral},
                                                               Operator::_goto));
            }

            // 执行完if_true_stmt后跳转到if_false_stmt/if-else代码段（此时无if_false_stmt）后面
            instructions.push_back(new Instruction({},
                                                   {},
                                                   {std::to_string(if_true_instructions.size() + 1), ir::Type::IntLiteral},
                                                   Operator::_goto));
            instructions.insert(instructions.end(), if_true_instructions.begin(), if_true_instructions.end());
            instructions.insert(instructions.end(), if_false_instructions.begin(), if_false_instructions.end());

            
            break;
        }
        case TokenType::WHILETK: // 'while' '(' Cond ')' Stmt 
        {   
            symbolTable.add_scope();
            symbolTable.scope_stack.back().name = "_WHILE";
            GET_CHILD_PTR(cond, Cond, 2);
            cond->v = "t" + std::to_string(tmp_cnt++);
            cond->t = Type::null; // 初始化为null

            vector<Instruction*> cond_stmt;
            analysisCond(cond, cond_stmt);
            // op1跳转条件，整型变量（条件跳转）或null（无条件跳转）
            // op2不使用
            // 先执行cond， 如果为真则执行while_stmt，跳到下下条（while_stmt部分）
            // 否则执行下一条指令，跳出while_stmt
            // 条件判断的指令
            instructions.insert(instructions.end(), cond_stmt.begin(), cond_stmt.end());
            // 若cond为真进入while_stmt
            instructions.push_back(new Instruction(
                {cond->v, cond->t},
                {},
                {"2", Type::IntLiteral},
                {Operator::_goto}
            ));

            // 获取循环体指令
            GET_CHILD_PTR(stmt, Stmt, 4);
            vector<Instruction*> while_stmt;
            analysisStmt(stmt, while_stmt);
            symbolTable.exit_scope();

            // 循环体执行结束，返回cond判断
            while_stmt.push_back(new Instruction(
                {},
                {},
                {std::to_string(-int(while_stmt.size() + 2 + cond_stmt.size())), Type::IntLiteral},
                {Operator::_goto}
            ));

            // 添加判断条件为假的指令，跳到while_stmt后面
            instructions.push_back(new Instruction(
                {},
                {},
                {std::to_string(while_stmt.size()+1), Type::IntLiteral},
                {Operator::_goto}
            ));
            

            for (size_t i = 0; i < while_stmt.size(); i++)
            {
                if (while_stmt[i]->op == Operator::__unuse__ && while_stmt[i]->op1.name == "break")
                {//break
                    while_stmt[i] = new Instruction(
                        {},
                        {},
                        {std::to_string(int(while_stmt.size()) - i), ir::Type::IntLiteral},
                        Operator::_goto);
                }
                else if (while_stmt[i]->op == Operator::__unuse__ && while_stmt[i]->op1.name == "continue")
                { // continue
                    while_stmt[i] = new Instruction(
                        {},
                        {},
                        {std::to_string(-int(i + 2 + cond_stmt.size())), ir::Type::IntLiteral},
                        Operator::_goto);
                }
            }

            instructions.insert(instructions.end(), while_stmt.begin(), while_stmt.end());
            
            break;
        }
        case TokenType::BREAKTK: // 'break' ';'
        {
            instructions.push_back(new Instruction(
                {"break", Type::null},
                {},
                {},
                {Operator::__unuse__}
            ));
            break;
        }
        case TokenType::CONTINUETK: // 'continue' ';'
        {
            instructions.push_back(new Instruction(
                {"continue", Type::null},
                {},
                {},
                {Operator::__unuse__}
            ));
            break;
        }
        case TokenType::RETURNTK: // 'return' [exp] ';'
        {
            if(NODE_IS(EXP, 1)){ // 说明有返回值
                GET_CHILD_PTR(exp, Exp, 1);
                exp->v = "t" + std::to_string(tmp_cnt++);
                exp->t = Type::null; // 初始化为null
                analysisExp(exp, instructions);
                // return: op1:返回值，op2:不使用，des:不使用
                instructions.push_back(new Instruction(
                   {exp->v, exp->t},
                   {},
                   {},
                   {Operator::_return}
                ));
                
            }
            else{ // 没有返回值
                instructions.push_back(new Instruction(
                    {"0", Type::null},
                    {},
                    {},
                    {Operator::_return}
                ));
            }
            break;
        }
        default:
            break;
        }
    }
}

/**
 * Exp：表达式，可以是各种运算表达式的组合。
 * Exp -> AddExp
 *  Exp.is_computable
 *  Exp.v
 *  Exp.t
*/
void Analyzer::analysisExp(Exp *root, vector<Instruction *> &instructions) {
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    COPY_EXP_NODE(addexp, root);
    
}

/**
 * Cond：条件表达式，用于条件判断。
 * Cond -> LOrExp
 *  Cond.is_computable
 *  Cond.v
 *  Cond.t
*/
void Analyzer::analysisCond(Cond *root, vector<Instruction *> &instructions) {
    GET_CHILD_PTR(lor, LOrExp, 0);
    COPY_EXP_NODE(root, lor);
    analysisLOrExp(lor, instructions);
    COPY_EXP_NODE(lor, root);
}

/**
 * (Yes)
 * LVal：左值，用于表示赋值操作的目标。
 * LVal -> Ident {'[' Exp ']'}
 *  LVal.is_computable
 *  LVal.v
 *  LVal.t
 *  LVal.i: array index, legal if t is IntPtr or FloatPtr(当t是数组指针是有效)
*/
void Analyzer::analysisLVal(LVal *root, vector<Instruction *> &instructions, bool is_left=false) {
    GET_CHILD_PTR(ident, Term, 0);
    auto var = symbolTable.get_ste(ident->token.value);

    if(root -> children.size() == 1){ //没有下标
        if(is_left){
            switch (root->t)
            {
            case Type::Int:
            case Type::IntLiteral:
                switch (var.operand.type)
                {
                case Type::Float:
                case Type::FloatLiteral:
                    // 添加类型转换指令
                    instructions.push_back(new Instruction(
                        {root->v, Type::Float},
                        {},
                        {root->v, root->t},
                        {Operator::cvt_i2f}
                    ));
                    break;
                default:
                    break;
                }
                break;
            case Type::Float:
            case Type::FloatLiteral:
                switch (var.operand.type)
                {
                case Type::Int:
                case Type::IntLiteral:
                    // 添加类型转换指令
                    instructions.push_back(new Instruction(
                        {root->v, Type::Int},
                        {},
                        {root->v, root->t},
                        {Operator::cvt_f2i}
                    ));
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }

            // mov赋值，op1立即数或变量，op2不使用，des被赋值变量
            instructions.push_back(new Instruction(
                {root->v, root->t},
                {},
                {var.operand.name, var.operand.type},
                {var.operand.type == Type::Float ? Operator::fmov : Operator::mov}
            ));
            return ;
        }
        switch (var.operand.type)
        {
        case Type::IntPtr:
        case Type::FloatPtr:
            root->is_computable = false;
            root->v = var.operand.name;
            root->t = var.operand.type;
            break;
        case ir::Type::IntLiteral:
        case ir::Type::FloatLiteral:
            root->is_computable = true;
            root->t = var.operand.type;
            root->v = var.operand.name;
            break;
        default:
            root->t = var.operand.type;
            root->v = "t" + std::to_string(tmp_cnt++);
            instructions.push_back(new Instruction(
                {var.operand.name, var.operand.type},
                {},
                {root->v, var.operand.type},
                {var.operand.type == Type::Float ? Operator::fmov : Operator::mov}
            )); 
            break;
        }
    }else{ // 有下标
        vector<Operand> load_indexs;
        for(size_t index = 2; index < root->children.size(); index += 3){
            if(NODE_IS(EXP, index)){
                GET_CHILD_PTR(exp, Exp, index);
                // exp->v = "t" + std::to_string(tmp_cnt++);
                // exp->t = Type::Int;
                analysisExp(exp, instructions);
                load_indexs.push_back({exp->v, exp->t});
            }else break;
        }

        auto res_index = Operand({"t" + std::to_string(tmp_cnt++), Type::Int});
        instructions.push_back(new Instruction(
           {"0", Type::IntLiteral},
           {},
           res_index,
           {Operator::def}
        ));

        // 计算数组偏移量
        for(size_t i=0; i<load_indexs.size(); i++){
            int mul_dim = 1;
            if(i+1 < var.dimension.size())
                mul_dim = std::accumulate(var.dimension.begin()+i+1, var.dimension.end(), 1, std::multiplies<int>());
            auto tmp_name = "t" + std::to_string(tmp_cnt++);
            instructions.push_back(new Instruction(
                load_indexs[i],
                {std::to_string(mul_dim), Type::IntLiteral},
                {tmp_name, Type::Int},
                {Operator::mul}
            ));
            instructions.push_back(new Instruction(
                res_index,
                {tmp_name, Type::Int},
                res_index,
                {Operator::add}
            ));
        }

        // 如果是左值，存入数组中
        // // store:存数指令，op1:数组名，op2:下标，des:存放变量
        if(is_left){
            switch (var.operand.type)
            {   
                case Type::IntPtr:
                    if(root -> t == Type::Float){
                        instructions.push_back(new Instruction(
                            {root->v, root->t},
                            {},
                            {root->v, Type::Int},
                            {Operator::cvt_f2i}
                        ));
                        root->t = Type::Int;
                    }else if(root -> t == Type::FloatLiteral){
                        root -> t = Type::IntLiteral;
                        root -> v = std::to_string((int)std::stof(root->v));
                    }
                    break;
                case Type::FloatPtr:
                    if(root -> t == Type::Int){
                        instructions.push_back(new Instruction(
                            {root->v, root->t},
                            {},
                            {root->v, Type::Float},
                            {Operator::cvt_i2f}
                        ));
                        root->t = Type::Float;
                    }else if(root -> t == Type::IntLiteral){
                        root -> t = Type::FloatLiteral;
                        root -> v = std::to_string((float)std::stoi(root->v));
                    }
                    break;
                default:
                    assert(0);
            }
            instructions.push_back(new Instruction(
                var.operand,
                res_index,
                {root->v, root->t},
                {Operator::store}
            ));
        }else{
            root->t = (var.operand.type == Type::IntPtr) ? Type::Int : Type::Float;
            // load:取数指令，op1:数组名，op2:下标，des:存放变量
            instructions.push_back(new Instruction(
                var.operand,
                res_index,
                {root->v, root->t},
                {Operator::load}
            ));
        }

    }
}

/**
 * (Yes)
 * Number：数字，可以是整数常量（IntConst）或浮点数常量（floatConst）。
 * Number -> IntConst | floatConst
 *  Number.is_computable
 *  Number.v
 *  Number.t
*/
void Analyzer::analysisNumber(Number *root, vector<Instruction *> &instructions) {
    root->is_computable = true;
    GET_CHILD_PTR(term, Term, 0);
    switch (term->token.type)
    {
    case TokenType::INTLTR: // 整数常量
        root->t = Type::IntLiteral;
        if (term->token.value.substr(0, 2) == "0x")
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 16));
        }
        else if (term->token.value.substr(0, 2) == "0b")
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 2));
        }
        else if (term->token.value.substr(0, 1) == "0" && !(term->token.value.substr(0, 2) == "0x") && !(term->token.value.substr(0, 2) == "0b"))
        {
            root->v = std::to_string(std::stoi(term->token.value, nullptr, 8));
        }
        else
        {
            root->v = term->token.value;
        }
        break;
    case TokenType::FLOATLTR: // 浮点数常量
        root->t = Type::FloatLiteral;
        root->v = term->token.value;
        break;
    default:
        break;
    }
}

/**
 * (Y)
 * PrimaryExp：基本表达式，可以是括号包裹的表达式、变量或数字。
 * PrimaryExp -> '(' Exp ')' | LVal | Number
 *  PrimaryExp.is_computable
 *  PrimaryExp.v
 *  PrimaryExp.t
*/
void Analyzer::analysisPrimaryExp(PrimaryExp *root, vector<Instruction *> &instructions) {
    if(NODE_IS(LVAL, 0)){
        GET_CHILD_PTR(lval, LVal, 0);
        COPY_EXP_NODE(root, lval);
        analysisLVal(lval, instructions, false);
        COPY_EXP_NODE(lval, root);

    }else if(NODE_IS(NUMBER, 0)){
        GET_CHILD_PTR(number, Number, 0);
        COPY_EXP_NODE(root, number);
        analysisNumber(number, instructions);
        COPY_EXP_NODE(number, root);

    }else{
        GET_CHILD_PTR(exp, Exp, 1)
        COPY_EXP_NODE(root, exp);
        analysisExp(exp, instructions);
        COPY_EXP_NODE(exp, root);
    }
}

/**
 * UnaryExp：一元表达式，可以是基本表达式、函数调用或一元运算。
 * UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
 *  UnaryExp.is_computable
 *  UnaryExp.v
 *  UnaryExp.t
*/
void Analyzer::analysisUnaryExp(UnaryExp *root, vector<Instruction *> &instructions) {
    if(NODE_IS(PRIMARYEXP, 0)){
        GET_CHILD_PTR(primaryexp, PrimaryExp, 0);
        COPY_EXP_NODE(root, primaryexp);
        analysisPrimaryExp(primaryexp, instructions);
        COPY_EXP_NODE(primaryexp, root);
    }else if(NODE_IS(UNARYOP, 0)){
        GET_CHILD_PTR(unaryop, UnaryOp, 0);
        analysisUnaryOp(unaryop, instructions);
        
        GET_CHILD_PTR(unaryexp, UnaryExp, 1);
        COPY_EXP_NODE(root, unaryexp);
        analysisUnaryExp(unaryexp, instructions);
        COPY_EXP_NODE(unaryexp, root);

        // 考虑负数情况
        if(unaryop->op == TokenType::MINU){
            auto temp_name = "t" + std::to_string(tmp_cnt++);
            switch (root->t)
            {
            case Type::Int:
                instructions.push_back(new Instruction(
                    {"0", Type::IntLiteral},
                    {root->v, root->t},
                    {temp_name, Type::Int},
                    {Operator::sub}
                ));
                root->t = Type::Int;
                root->v = temp_name;
                break;
            case Type::IntLiteral:
                root->t = Type::IntLiteral;
                root->v = std::to_string(-std::stoi(root->v));
                break;
            case Type::Float:
                instructions.push_back(new Instruction(
                    {"0.0", Type::FloatLiteral},
                    {root->v, root->t},
                    {temp_name, Type::Float},
                    {Operator::fsub}
                ));
                root->t = Type::Float;
                root->v = temp_name;
                break;
            case Type::FloatLiteral:
                root->t = Type::FloatLiteral;
                root->v = std::to_string(-std::stof(root->v));
                break;
            default:
                assert(0);
            }
        }else if(unaryop->op == TokenType::NOT){ // 考虑非运算
            auto temp_name = "t" + std::to_string(tmp_cnt++);
            switch (root->t)
            {
            case ir::Type::Int:
            case ir::Type::IntLiteral:
                instructions.push_back(new Instruction({"0", ir::Type::IntLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::eq));
                root->t = ir::Type::Int;
                break;
            case ir::Type::Float:
            case ir::Type::FloatLiteral:
                instructions.push_back(new Instruction({"0.0", ir::Type::FloatLiteral},
                                                       {root->v, root->t},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::feq));
                root->t = ir::Type::Float;
                break;
            default:
                assert(0);
            }
            root->v = temp_name;
        }
    }else{ // Ident '(' [FuncRParams] ')'函数调用
        GET_CHILD_PTR(ident, Term, 0);
        Function *func;
        if(symbolTable.functions.count(ident->token.value)){ // 用户自定义函数
            func = symbolTable.functions[ident->token.value];
        }else if(get_lib_funcs()->count(ident->token.value)){ // 内置函数
            func = (*get_lib_funcs())[ident->token.value];
        }else assert(0);

        root->v = "t" + std::to_string(tmp_cnt++);
        root->t = func->returnType;
        if(NODE_IS(FUNCRPARAMS, 2)){ // 如果存在函数参数
            GET_CHILD_PTR(funcrparams, FuncRParams, 2);
            vector<Operand> params;

            for(size_t i=0; i<func->ParameterList.size(); i++){
                params.push_back({"t" + std::to_string(tmp_cnt++), Type::Int});
                params[i].type = (func->ParameterList)[i].type;
            }

            analysisFuncRParams(funcrparams, params, instructions);
            instructions.push_back(new ir::CallInst(
                {ident->token.value, Type::null},
                params,
                {root->v, root->t}
            ));
        }else{
            instructions.push_back(new ir::CallInst(
                {ident->token.value, Type::null},
                {root->v, root->t}
            ));
        }
    }
}

/**
 * UnaryOp：一元运算符，可以是加号、减号或逻辑非。
 * UnaryOp -> '+' | '-' | '!'
*/
void Analyzer::analysisUnaryOp(UnaryOp *root, vector<Instruction*> &instructions) {
    GET_CHILD_PTR(term, Term, 0);
    root->op = term->token.type;
}

/**
 * FuncRParams：函数实参，用于函数调用时传递参数。
 * FuncRParams -> Exp { ',' Exp }
*/
void Analyzer::analysisFuncRParams(FuncRParams *root, vector<Operand> &params, vector<Instruction*> &instructions) {
    size_t index = 0; 
    for(size_t i = 0; i<root->children.size(); i += 2){
        if(!(NODE_IS(EXP, i))) break;
        GET_CHILD_PTR(exp, Exp, i);
        
        exp->v = params[index].name;
        exp->t = params[index].type;
        analysisExp(exp, instructions);
        
        // 添加类型转换
        auto temp_name = "t" + std::to_string(tmp_cnt++);
        switch (params[index].type)
        {
            case Type::Int:
                if(exp->t == Type::Float){
                    instructions.push_back(new Instruction(
                    {"0", Type::Int},
                    {},
                    {temp_name, Type::Int},
                    {Operator::def}
                    ));
                    instructions.push_back(new Instruction(
                        {exp->v, exp->t},
                        {},
                        {temp_name, Type::Int},
                        {Operator::cvt_f2i}
                    ));
                    params[index] = {temp_name, Type::Int};
                    index ++;
                }else if(exp->t == Type::FloatLiteral){
                    exp -> t = Type::IntLiteral;
                    exp -> v = std::to_string((int)std::stof(exp->v)); 
                    params[index] = {exp->v, exp->t};
                    index ++;
                }else{
                    params[index] = {exp->v, exp->t};
                    index ++;
                }
                break;
            case Type::Float:
                if(exp->t == Type::Int){
                    instructions.push_back(new Instruction(
                    {"0.0", Type::Float},
                    {},
                    {temp_name, Type::Float},
                    {Operator::fdef}
                    ));
                    instructions.push_back(new Instruction(
                        {exp->v, exp->t},
                        {},
                        {temp_name, Type::Float},
                        {Operator::cvt_i2f}
                    ));
                    params[index] = {temp_name, Type::Float};
                    index ++;
                }else if(exp->t == Type::IntLiteral){
                    exp -> t = Type::FloatLiteral;
                    exp -> v = std::to_string((float)std::stoi(exp->v)); 
                    params[index] = {exp->v, exp->t};
                    index ++;
                }else{
                    params[index] = {exp->v, exp->t};
                    index ++;
                }
                break;
            default:
                params[index] = {exp->v, exp->t};
                index ++;
                break;
        }
        
    }
}

/**
 * MulExp：乘法表达式，表示乘法、除法或取余运算的组合。
 * MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
 *  MulExp.is_computable
 *  MulExp.v
 *  MulExp.t
*/
void frontend::Analyzer::analysisMulExp(MulExp *root, std::vector<Instruction *> &instructions)
{
    GET_CHILD_PTR(unaryexp, UnaryExp, 0);
    COPY_EXP_NODE(root, unaryexp);
    analysisUnaryExp(unaryexp, instructions);
    COPY_EXP_NODE(unaryexp, root);

    // 如果有多个UnaryExp
    for (size_t i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(unaryexp, UnaryExp, i);
        unaryexp->v = "t" + std::to_string(tmp_cnt++);
        unaryexp->t = root->t;
        analysisUnaryExp(unaryexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);

        auto temp_name = "t" + std::to_string(tmp_cnt++);
        switch (root->t)
        {
        case ir::Type::Int:
            if (unaryexp->t == ir::Type::Float || unaryexp->t == ir::Type::FloatLiteral)
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::cvt_f2i));
            }
            else
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mov));
            }
            switch (term->token.type)
            {
            case TokenType::MULT:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mul));
                break;
            case TokenType::DIV:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::div));
                break;
            case TokenType::MOD:
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mod));
                break;
            default:
                break;
            }
            root->v = temp_name;
            root->t = ir::Type::Int;
            break;

        case ir::Type::IntLiteral:
            if (unaryexp->t == ir::Type::IntLiteral)
            {
                switch (term->token.type)
                {
                case TokenType::MULT:
                    root->v = std::to_string(std::stoi(root->v) * std::stoi(unaryexp->v));
                    break;
                case TokenType::DIV:
                    root->v = std::to_string(std::stoi(root->v) / std::stoi(unaryexp->v));
                    break;
                case TokenType::MOD:
                    root->v = std::to_string(std::stoi(root->v) % std::stoi(unaryexp->v));
                    break;
                default:
                    break;
                }
                root->t = ir::Type::IntLiteral;
            }
            else
            {
                if (unaryexp->t == ir::Type::Float || unaryexp->t == ir::Type::FloatLiteral)
                {
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::cvt_f2i));
                }
                else
                {
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mov));
                }
                switch (term->token.type)
                {
                case TokenType::MULT:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mul));
                    break;
                case TokenType::DIV:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::div));
                    break;
                case TokenType::MOD:
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mod));
                    break;
                default:
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Int;
            }
            break;

        case ir::Type::Float:
            if (unaryexp->t == ir::Type::Int || unaryexp->t == ir::Type::IntLiteral)
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::cvt_i2f));
            }
            else
            {
                instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fmov));
            }
            switch (term->token.type)
            {
            case TokenType::MULT:
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fmul));
                break;
            case TokenType::DIV:
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fdiv));
                break;
            case TokenType::MOD:
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::mod));
                break;
            default:
                break;
            }
            break;
        case ir::Type::FloatLiteral:
            if (unaryexp->t == ir::Type::FloatLiteral)
            {
                switch (term->token.type)
                {
                case TokenType::MULT:
                    root->v = std::to_string(std::stof(root->v) * std::stof(unaryexp->v));
                    break;
                case TokenType::DIV:
                    root->v = std::to_string(std::stof(root->v) / std::stof(unaryexp->v));
                    break;
                case TokenType::MOD:
                    root->v = std::to_string(std::fmod(std::stof(root->v), std::stof(unaryexp->v)));
                    break;
                }
                root->t = ir::Type::FloatLiteral;
            }else{
                if(unaryexp->t == ir::Type::Int || unaryexp->t == ir::Type::IntLiteral){
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::cvt_i2f));
                }else{
                    instructions.push_back(new Instruction({unaryexp->v, unaryexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fmov));
                }
                switch (term->token.type)
                {
                case TokenType::MULT:
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fmul));
                    break;
                case TokenType::DIV:
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fdiv));
                    break;
                case TokenType::MOD:
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::mod));
                    break;
                default:
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Float;
            }
            break;
        default:
            break;
        }
    }
}

/**
 * AddExp：加法表达式，表示加法或减法运算的组合。
 * AddExp -> MulExp { ('+' | '-') MulExp }
 *  AddExp.is_computable
 *  AddExp.v
 *  AddExp.t
*/
void frontend::Analyzer::analysisAddExp(AddExp *root, std::vector<Instruction *> &instructions)
{
    
    GET_CHILD_PTR(mulexp, MulExp, 0);
    COPY_EXP_NODE(root, mulexp);
    analysisMulExp(mulexp, instructions);
    COPY_EXP_NODE(mulexp, root);


    // 如果有多个MulExp
    for (size_t i = 2; i < root->children.size(); i += 2)
    {
        GET_CHILD_PTR(mulexp, MulExp, i);
        mulexp->v = "t" + std::to_string(tmp_cnt++);
        mulexp->t = Type::Float;
        analysisMulExp(mulexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);

        auto temp_name = "t" + std::to_string(tmp_cnt++);
        switch (root->t)
        {
        case ir::Type::Int:
            if (mulexp->t == ir::Type::Float || mulexp->t == ir::Type::FloatLiteral)
            {
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::cvt_f2i));
            }
            else
            {
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::mov));
            }
            if (term->token.type == TokenType::PLUS)
            {
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::add));
            }
            else
            {
                instructions.push_back(new Instruction({root->v, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::sub));
            }
            root->v = temp_name;
            root->t = ir::Type::Int;

            break;

        case ir::Type::IntLiteral:
            if (mulexp->t == ir::Type::IntLiteral)
            {
                if (term->token.type == TokenType::PLUS)
                    root->v = std::to_string(std::stoi(root->v) + std::stoi(mulexp->v));
                else
                    root->v = std::to_string(std::stoi(root->v) - std::stoi(mulexp->v));
            }
            else
            {
                if (mulexp->t == ir::Type::Float || mulexp->t == ir::Type::FloatLiteral)
                {
                    instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::cvt_f2i));
                }
                else
                {
                    instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::mov));
                }
                if (term->token.type == TokenType::PLUS)
                {
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::add));
                }
                else
                {
                    instructions.push_back(new Instruction({root->v, ir::Type::IntLiteral},
                                                           {temp_name, ir::Type::Int},
                                                           {temp_name, ir::Type::Int},
                                                           Operator::sub));
                }
                root->v = temp_name;
                root->t = ir::Type::Int;
            }
            break;

        case ir::Type::Float:
            if (mulexp->t == ir::Type::Int || mulexp->t == ir::Type::IntLiteral)
            {
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::cvt_i2f));
            }
            else
            {
                instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                       {},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fmov));
            }
            if (term->token.type == TokenType::PLUS)
            {
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fadd));
            }
            else
            {
                instructions.push_back(new Instruction({root->v, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       {temp_name, ir::Type::Float},
                                                       Operator::fsub));
            }
            root->v = temp_name;
            root->t = ir::Type::Float;
            break;
        case ir::Type::FloatLiteral:
            if (mulexp->t == ir::Type::FloatLiteral)
            {
                if (term->token.type == TokenType::PLUS)
                    root->v = std::to_string(std::stof(root->v) + std::stof(mulexp->v));
                else
                    root->v = std::to_string(std::stof(root->v) - std::stof(mulexp->v));
            }
            else
            {
                if (mulexp->t == ir::Type::Int || mulexp->t == ir::Type::IntLiteral)
                {
                    instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::cvt_i2f));
                }
                else
                {
                    instructions.push_back(new Instruction({mulexp->v, mulexp->t},
                                                           {},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fmov));
                }
                if (term->token.type == TokenType::PLUS)
                {
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fadd));
                }
                else
                {
                    instructions.push_back(new Instruction({root->v, ir::Type::FloatLiteral},
                                                           {temp_name, ir::Type::Float},
                                                           {temp_name, ir::Type::Float},
                                                           Operator::fsub));
                }
                root->v = temp_name;
                root->t = ir::Type::Float;
            }
            break;
        default:
            break;
        }
    }
}

/**
 * RelExp：关系表达式，表示关系运算的组合。
 * RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
 *  RelExp.is_computable
 *  RelExp.v
 *  RelExp.t
*/
void frontend::Analyzer::analysisRelExp(RelExp *root, vector<Instruction *> &instructions)
{
    // std::cout << "RelExp: " + toString(root->t) + " " + root->v << std::endl;
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    COPY_EXP_NODE(addexp, root);

    // std::cout << "RelExp: " + toString(root->t) + " " + root->v << std::endl;

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个AddExp
    {
        GET_CHILD_PTR(addexp, AddExp, i);
        analysisAddExp(addexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);
        // std::cout << toString(root->t) + " " + root->v + " " + toString(term->token.type) + " " + toString(addexp->t) + " " + addexp->v << std::endl;
        if (root->t == ir::Type::IntLiteral && addexp->t == ir::Type::IntLiteral)
        {

            switch (term->token.type)
            {
            case TokenType::LSS:
                root->v = std::to_string(std::stoi(root->v) < std::stoi(addexp->v));
                break;
            case TokenType::LEQ:
                root->v = std::to_string(std::stoi(root->v) <= std::stoi(addexp->v));
                break;
            case TokenType::GTR:
                root->v = std::to_string(std::stoi(root->v) > std::stoi(addexp->v));
                break;
            case TokenType::GEQ:
                root->v = std::to_string(std::stoi(root->v) >= std::stoi(addexp->v));
                break;
            default:
                break;
            }
        }
        else
        {
            auto temp_name = "t" + std::to_string(tmp_cnt++);
            if(root->t == Type::Int || root -> t == Type::IntLiteral){
                switch (term->token.type) // 整型
                {
                case TokenType::LSS: // 整型变量<运算，逻辑运算结果用变量表示
                {   
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Int},
                                                        Operator::lss));
                    break;
                }
                case TokenType::LEQ: // 整型变量<=运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Int},
                                                        Operator::leq));
                    break;
                }
                case TokenType::GTR: // 整型变量>运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Int},
                                                        Operator::gtr));
                    break;
                }
                case TokenType::GEQ: // 整型变量>=运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Int},
                                                        Operator::geq));
                    break;
                }
                default:
                    assert(false);
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Int;
            }else if(root->t == Type::Float || root -> t == Type::FloatLiteral){
                if(addexp->t == Type::Int){
                    instructions.push_back(new Instruction(
                        {addexp->v, addexp->t},
                        {},
                        {addexp->v, Type::Float},
                        {Operator::cvt_i2f}
                    ));
                    addexp -> t = Type::Float;
                }else if(addexp->t == Type::IntLiteral){
                    addexp -> v = std::to_string((float)(std::stoi(addexp->v)));
                    addexp -> t = Type::FloatLiteral;
                }
                switch (term->token.type) // 整型
                {
                case TokenType::LSS: // 整型变量<运算，逻辑运算结果用变量表示
                {   
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Float},
                                                        Operator::flss));
                    break;
                }
                case TokenType::LEQ: // 整型变量<=运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Float},
                                                        Operator::fleq));
                    break;
                }
                case TokenType::GTR: // 整型变量>运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Float},
                                                        Operator::fgtr));
                    break;
                }
                case TokenType::GEQ: // 整型变量>=运算，逻辑运算结果用变量表示
                {
                    instructions.push_back(new Instruction({root->v, root->t},
                                                        {addexp->v, addexp->t},
                                                        {temp_name, ir::Type::Float},
                                                        Operator::fgeq));
                    break;
                }
                default:
                    assert(false);
                    break;
                }
                root->v = temp_name;
                root->t = ir::Type::Float;
            }
            
        }
    }
}

/**
 * EqExp：相等表达式，表示相等或不等运算的组合。
 * EqExp -> RelExp { ('==' | '!=') RelExp }
 *  EqExp.is_computable
 *  EqExp.v
 *  EqExp.t
*/
void frontend::Analyzer::analysisEqExp(EqExp *root, vector<Instruction *> &instructions)
{
    GET_CHILD_PTR(relexp, RelExp, 0);
    COPY_EXP_NODE(root, relexp);
    analysisRelExp(relexp, instructions);
    COPY_EXP_NODE(relexp, root);

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个RelExp
    {
        GET_CHILD_PTR(relexp, RelExp, i);
        analysisRelExp(relexp, instructions);

        GET_CHILD_PTR(term, Term, i - 1);
        if (root->t == ir::Type::IntLiteral && relexp->t == ir::Type::IntLiteral)
        {
            switch (term->token.type)
            {
            case TokenType::EQL:
                root->v = std::to_string(std::stoi(root->v) == std::stoi(relexp->v));
                break;
            case TokenType::NEQ:
                root->v = std::to_string(std::stoi(root->v) != std::stoi(relexp->v));
                break;
            default:
                assert(false);
                break;
            }
        }
        else
        {
            auto temp_name = "t" + std::to_string(tmp_cnt++);

            switch (term->token.type) // TODO: 只考虑整型
            {
            case TokenType::EQL: // 整型变量==运算，逻辑运算结果用变量表示。
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {relexp->v, relexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::eq));
                break;
            }
            case TokenType::NEQ:
            {
                instructions.push_back(new Instruction({root->v, root->t},
                                                       {relexp->v, relexp->t},
                                                       {temp_name, ir::Type::Int},
                                                       Operator::neq));
                break;
            }
            default:
                assert(false);
                break;
            }
            root->v = temp_name;
            root->t = ir::Type::Int;
        }
    }
}

/**
 * LAndExp：逻辑与表达式，表示逻辑与运算的组合。
 * LAndExp -> EqExp [ '&&' LAndExp ]
 *  LAndExp.is_computable
 *  LAndExp.v
 *  LAndExp.t
*/
void frontend::Analyzer::analysisLAndExp(LAndExp *root, vector<Instruction *> &instructions)
{
    GET_CHILD_PTR(eqexp, EqExp, 0);
    COPY_EXP_NODE(root, eqexp);
    analysisEqExp(eqexp, instructions);
    COPY_EXP_NODE(eqexp, root);

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个LAndExp
    {
        GET_CHILD_PTR(landexp, LAndExp, i);
        vector<Instruction *> second_and_instructions;
        landexp->v = "t" + std::to_string(tmp_cnt++);
        landexp->t = ir::Type::Int; // 初始化为Int
        analysisLAndExp(landexp, second_and_instructions);

        if (root->t == ir::Type::IntLiteral && landexp->t == ir::Type::IntLiteral)
        {
            root->v = std::to_string(std::stoi(root->v) && std::stoi(landexp->v));
            return;
        }

        second_and_instructions.push_back(new Instruction({root->v, root->t},
                                                          {landexp->v, landexp->t},
                                                          {root->v, ir::Type::Int},
                                                          Operator::_and));

        auto opposite = "t" + std::to_string(tmp_cnt++);
        // 对第一个操作数取反
        if(root->t == ir::Type::Float){
            instructions.push_back(new Instruction(
                {root->v, root->t},
                {},
                {root->v, Type::Int},
                {Operator::cvt_f2i}
            ));
            root -> t = Type::Int;
        }else if(root->t == ir::Type::FloatLiteral){
            root -> v = std::to_string((int)std::stof(root->v));
            root -> t = Type::IntLiteral;
        }
        instructions.push_back(new Instruction({root->v, root->t},
                                               {},
                                               {opposite, ir::Type::Int},
                                               Operator::_not));

        // 如果第一个操作数为0（取反为opposite = 1），跳过后面的操作数 
        // LAndExp -> EqExp [ '&&' LAndExp ]
        // second_and_instructions表示第二个表达式LAndExp的指令
        // 跳转到第二个表达式的指令的下一条指令
        instructions.push_back(new Instruction({opposite, ir::Type::Int},
                                {},
                                {std::to_string(second_and_instructions.size() + 1), ir::Type::IntLiteral},
                                Operator::_goto));

        instructions.insert(instructions.end(), second_and_instructions.begin(), second_and_instructions.end());

        
        
    }
}

/**
 * LOrExp：逻辑或表达式，表示逻辑或运算的组合。
 * LOrExp -> LAndExp [ '||' LOrExp ]
 *  LOrExp.is_computable
 *  LOrExp.v
 *  LOrExp.t
*/
void frontend::Analyzer::analysisLOrExp(LOrExp *root, std::vector<Instruction *> &instructions)
{
    GET_CHILD_PTR(landexp, LAndExp, 0);
    COPY_EXP_NODE(root, landexp);
    analysisLAndExp(landexp, instructions);
    COPY_EXP_NODE(landexp, root);

    for (size_t i = 2; i < root->children.size(); i += 2) // 如果有多个LOrExp
    {
        GET_CHILD_PTR(lorexp, LOrExp, i);
        vector<Instruction *> second_lor_instructions;
        lorexp->v = "t" + std::to_string(tmp_cnt++);
        lorexp->t = ir::Type::Int; // 初始化为Int
        analysisLOrExp(lorexp, second_lor_instructions);

        if (root->t == ir::Type::IntLiteral && lorexp->t == ir::Type::IntLiteral)
        {
            root->v = std::to_string(std::stoi(root->v) || std::stoi(lorexp->v));
            return;
        }
        if(root->t == ir::Type::Float){
            instructions.push_back(new Instruction(
                {root->v, root->t},
                {},
                {root->v, Type::Int},
                {Operator::cvt_f2i}
            ));
            root -> t = Type::Int;
        }else if(root->t == ir::Type::FloatLiteral){
            root -> v = std::to_string((int)(std::stof(root->v)));
            root -> t = Type::IntLiteral;
        }
        if(lorexp->t == ir::Type::Float){
            instructions.push_back(new Instruction(
                {lorexp->v, lorexp->t},
                {},
                {lorexp->v, Type::Int},
                {Operator::cvt_f2i}
            ));
            lorexp -> t = Type::Int;
        }else if(lorexp->t == ir::Type::FloatLiteral){
            lorexp -> v = std::to_string((int)(std::stof(lorexp->v)*10.0));
            lorexp -> t = Type::IntLiteral;
        }
        second_lor_instructions.push_back(new Instruction({root->v, root->t},
                                                          {lorexp->v, lorexp->t},
                                                          {root->v, ir::Type::Int},
                                                          Operator::_or));

        // 如果第一个操作数为1，跳过后面的操作数
        instructions.push_back(new Instruction({root->v, root->t},
                                               {},
                                               {std::to_string(second_lor_instructions.size() + 1), ir::Type::IntLiteral},
                                               Operator::_goto));

        instructions.insert(instructions.end(), second_lor_instructions.begin(), second_lor_instructions.end());
        
    }
}


/**
 * yes
 * ConstExp：常量表达式，表示一个常量值。
 * ConstExp -> AddExp
 *  ConstExp.is_computable: true
 *  ConstExp.v
 *  ConstExp.t
*/
void frontend::Analyzer::analysisConstExp(ConstExp *root, vector<Instruction *> &instructions)
{
    GET_CHILD_PTR(addexp, AddExp, 0);
    COPY_EXP_NODE(root, addexp);
    analysisAddExp(addexp, instructions);
    COPY_EXP_NODE(addexp, root);
}