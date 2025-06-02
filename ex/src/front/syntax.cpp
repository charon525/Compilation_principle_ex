/*** 
 * @Author: YourName
 * @Date: 2024-04-07 08:42:36
 * @LastEditTime: 2024-05-08 17:43:07
 * @LastEditors: YourName
 * @Description: 
 * @FilePath: \Compilation_principle\ex2\src\front\syntax.cpp
 * @版权声明
 */
#include"front/syntax.h"

#include<iostream>
#include<cassert>

using frontend::Parser;

// #define DEBUG_PARSER
#define TODO assert(0 && "todo")
#define CUR_TOKEN_IS(tk_type) (token_stream[index].type == TokenType::tk_type)
#define PARSE_TOKEN(tk_type) root->children.push_back(parseTerm(root, TokenType::tk_type))
#define PARSE(name, type) auto name = new type(root); assert(parse##type(name)); root->children.push_back(name); 


Parser::Parser(const std::vector<frontend::Token>& tokens): index(0), token_stream(tokens) {}

Parser::~Parser() {}


frontend::CompUnit* Parser::get_abstract_syntax_tree(){
    CompUnit* root = new CompUnit();
    if(parseCompUnit(root)){
        return root;
    }else return nullptr;
}

frontend::Term* Parser::parseTerm(AstNode* parent, TokenType t){
    if(token_stream[index].type == t){
        auto node = new Term(token_stream[index], parent);
        index ++;
        parent->children.push_back(node);
        return node;
    }
    else return nullptr;
}

bool  Parser::parseCompUnit(AstNode* root){
    /**
     * CompUnit -> (Decl | FuncDef) [CompUnit]  
     * FIRST(Decl) = FIRST(ConstDecl) U FIRST(VarDecl) = { 'const', 'int', 'float'}
     * FIRST(FuncDef) = FIRST(FuncType) = { 'void', 'int', 'float' }
     * FIRST(CompUnit) = FIRST(Decl) U FIRST(FuncDef) = {'const', 'int', 'float', 'void'}
    */
    if(token_stream[index].type == TokenType::CONSTTK){  // const关键字
        PARSE(const_node, Decl);
    }else if(token_stream[index].type == TokenType::VOIDTK){ // void关键字
        PARSE(void_node, FuncDef);
    }else if(token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK){
        // int|float id ()···
        if(index+2 < token_stream.size() && token_stream[index+2].type == TokenType::LPARENT){
            PARSE(int_float_node, FuncDef);
        }else {// int|float id;
            PARSE(int_float_node, Decl);
        }
    }else return false;
    // [CompUnit]
    if(index < token_stream.size() && (token_stream[index].type == TokenType::CONSTTK || token_stream[index].type == TokenType::VOIDTK || token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK)){
        PARSE(comp_node, CompUnit);
    }
    return true;
}

bool  Parser::parseDecl(AstNode* root){ 
    /*
     * Decl -> ConstDecl | VarDecl  
     * FIRST(ConstDecl) = { 'const' }
     * FIRST(VarDecl) = FIRST(BType) = { 'int', 'float' }
    */ 
    if(token_stream[index].type == TokenType::CONSTTK){  // const关键字
        PARSE(constdecl_node, ConstDecl);
        return true;
    }else if(token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK){
        PARSE(vardecl_node, VarDecl);
        return true;
    }else return false;
}


bool  Parser::parseConstDecl(AstNode* root){
    /**
     * ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
     * FIRST(BType) = { 'int', 'float' }
     * FIRST(ConstDef) = {Ident}
    */
    if(token_stream[index].type == TokenType::CONSTTK){ // const
        parseTerm(root, TokenType::CONSTTK);
        PARSE(btype_node, BType);
        PARSE(constdef_node, ConstDef);
        while(token_stream[index].type == TokenType::COMMA){
            parseTerm(root, TokenType::COMMA);
            PARSE(constdef_node, ConstDef);
        }
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
        return true;
    }
    return false;
}

bool  Parser::parseBType(AstNode* root){
    /**
     * BType -> 'int' | 'float'
    */
    if(token_stream[index].type == TokenType::INTTK){
        parseTerm(root, TokenType::INTTK);
        return true;
    }else if(token_stream[index].type == TokenType::FLOATTK){
        parseTerm(root, TokenType::FLOATTK);
        return true;
    }else return false;
}

bool  Parser::parseConstDef(AstNode* root){
    /**
     * ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
     * FIRST(ConstExp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(ConstInitVal) = FIRST(ConstExp) U { '{' } = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
    if(token_stream[index].type == TokenType::IDENFR){
        parseTerm(root, TokenType::IDENFR);
        while(token_stream[index].type == TokenType::LBRACK){ // [
            parseTerm(root, TokenType::LBRACK);
            PARSE(constexp_node, ConstExp);
            if(token_stream[index].type != TokenType::RBRACK) return false;
            parseTerm(root, TokenType::RBRACK);
        }
    
        if(token_stream[index].type == TokenType::ASSIGN){ // =
            parseTerm(root, TokenType::ASSIGN);
            PARSE(constinitval_node, ConstInitVal);
            return true;
        }
    }
    return false;
}

bool  Parser::parseConstInitVal(AstNode* root){
    /**
     * ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
     * FIRST(ConstExp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(ConstInitVal) = FIRST(ConstExp) U { '{' } = { '{', '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
    if(token_stream[index].type == TokenType::LBRACE){  // {
        parseTerm(root, TokenType::LBRACE);
        // [ ConstInitVal { ',' ConstInitVal } ]
        if(token_stream[index].type == TokenType::LPARENT || token_stream[index].type == TokenType::IDENFR 
            || token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::FLOATLTR 
            || token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU 
            || token_stream[index].type == TokenType::NOT || token_stream[index].type == TokenType::LBRACE){ 
                PARSE(constinitval_node, ConstInitVal);
            // { ',' ConstInitVal }
            while(token_stream[index].type == TokenType::COMMA){
                parseTerm(root, TokenType::COMMA);
                PARSE(constinitval_node, ConstInitVal);
            }
        }
        // }
        if(token_stream[index].type == TokenType::RBRACE){
            parseTerm(root, TokenType::RBRACE);
            return true;
        }
    } else {
        PARSE(constexp_node, ConstExp);
        return true;
    }
    return false;
}



bool  Parser::parseVarDecl(AstNode* root){
    /**
     * VarDecl -> BType VarDef { ',' VarDef } ';'
     * FIRST(BType) = { 'int', 'float' }
     * FIRST(VarDef) = {Ident}
    */
   PARSE(btype_node, BType);
   PARSE(vardef_node, VarDef);
   while(token_stream[index].type == TokenType::COMMA){
        parseTerm(root, TokenType::COMMA);
        PARSE(vardef_node, VarDef);
   }
   if(token_stream[index].type != TokenType::SEMICN) return false;
   parseTerm(root, TokenType::SEMICN);
   return true;
}

bool  Parser::parseVarDef(AstNode* root){
    /**
     * VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
     * FIRST(ConstExp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(InitVal) = FIRST(Exp) U { '{' } = { '(', Ident, IntConst, floatConst, '+', '-', '!', '{' }
    */
    if(token_stream[index].type == TokenType::IDENFR){
        parseTerm(root, TokenType::IDENFR);
        // { '[' ConstExp ']' }
        while(token_stream[index].type == TokenType::LBRACK){
            parseTerm(root, TokenType::LBRACK);
            PARSE(constexp_node, ConstExp);
            if(token_stream[index].type != TokenType::RBRACK) return false;
            parseTerm(root, TokenType::RBRACK);
        }
        // [ '=' InitVal ]
        if(index < token_stream.size() && token_stream[index].type == TokenType::ASSIGN){
            parseTerm(root, TokenType::ASSIGN);
            PARSE(initexp_node, InitVal);
        }
        return true;
    }
    return false;
}

bool  Parser::parseInitVal(AstNode* root){
    /**
     * InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'
     * FIRST(Exp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(InitVal) = FIRST(Exp) U { '{' } = { '(', Ident, IntConst, floatConst, '+', '-', '!', '{' }
    */
    if(token_stream[index].type == TokenType::LBRACE){  // 
        parseTerm(root, TokenType::LBRACE); // {
        // [ InitVal { ',' InitVal } ]
        if(token_stream[index].type == TokenType::LPARENT || token_stream[index].type == TokenType::IDENFR
            || token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::FLOATLTR
            || token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU
            || token_stream[index].type == TokenType::NOT || token_stream[index].type == TokenType::LBRACE){ 
                PARSE(initval_node, InitVal);
                // { ',' InitVal }
                while(token_stream[index].type == TokenType::COMMA){
                    parseTerm(root, TokenType::COMMA);
                    PARSE(initval_node, InitVal);
                }
        }
        // }
        if(token_stream[index].type == TokenType::RBRACE){
            parseTerm(root, TokenType::RBRACE);
            return true;
        }
    }else{
        PARSE(exp_node, Exp);
        return true;
    }
    return false;
}

bool  Parser::parseFuncDef(AstNode* root){
    /**
     * FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
     * FIRST(FuncType) = { 'void', 'int', 'float' }
     * FIRST(FuncFParams) = FIRST(FuncFParam) = { 'int', 'float' }
     * FIRST(Block) = { '{' };
    */
   PARSE(functype_node, FuncType);
   if(token_stream[index].type == TokenType::IDENFR){
       parseTerm(root, TokenType::IDENFR);
       if(token_stream[index].type == TokenType::LPARENT){
           parseTerm(root, TokenType::LPARENT);
           if(token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK){
                PARSE(funcfparams_node, FuncFParams);
           }
           if(token_stream[index].type != TokenType::RPARENT) return false;
           parseTerm(root, TokenType::RPARENT);
           PARSE(block_node, Block);
           return true;
       }
   }
   return false;
}

bool  Parser::parseFuncType(AstNode* root){
    /**
     * FuncType -> 'void' | 'int' | 'float'
    */
   if(token_stream[index].type == TokenType::VOIDTK || token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK){
       parseTerm(root, token_stream[index].type);
       return true;
   }
   return false;
}

bool  Parser::parseFuncFParam(AstNode* root){
    /**
     * FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
    */
   PARSE(btype_node, BType);
   if(token_stream[index].type == TokenType::IDENFR){
       parseTerm(root, TokenType::IDENFR);
       // ['[' ']' { '[' Exp ']' }]
       if(index < token_stream.size() && token_stream[index].type == TokenType::LBRACK){
           parseTerm(root, TokenType::LBRACK);
           if(token_stream[index].type == TokenType::RBRACK){
               parseTerm(root, TokenType::RBRACK);
               while(index < token_stream.size() && token_stream[index].type == TokenType::LBRACK){
                    parseTerm(root, TokenType::LBRACK);
                    PARSE(exp_node, Exp);
                    if(token_stream[index].type != TokenType::RBRACK) return false;
                    parseTerm(root, TokenType::RBRACK);
               }
               return true;
           }else return false;
       }
       return true;
   }
   return false;
}

bool  Parser::parseFuncFParams(AstNode* root){
    /**
     * FuncFParams -> FuncFParam { ',' FuncFParam }
    */
   PARSE(func_fparam_node, FuncFParam);
   while(index < token_stream.size() && token_stream[index].type == TokenType::COMMA){
       parseTerm(root, TokenType::COMMA);
       PARSE(func_fparam_node, FuncFParam);
   }
   return true;
}

bool  Parser::parseBlock(AstNode* root){
    /**
     * Block -> '{' { BlockItem } '}'
     * FIRST(BlockItem) = FIRST(Decl) U FIRST(Stmt) = { '(','{', ';', Ident, IntConst, floatConst, '+', '-', '!', 'if', 'while', 'break', 'continue', 'return', 'const', 'int', 'float' }
    */
   if(token_stream[index].type == TokenType::LBRACE){
       parseTerm(root, TokenType::LBRACE);

       while(token_stream[index].type == TokenType::CONSTTK || token_stream[index].type == TokenType::INTTK 
            || token_stream[index].type == TokenType::FLOATTK || token_stream[index].type == TokenType::LPARENT
            || token_stream[index].type == TokenType::LBRACE || token_stream[index].type == TokenType::SEMICN
            || token_stream[index].type == TokenType::IDENFR || token_stream[index].type == TokenType::INTLTR
            || token_stream[index].type == TokenType::FLOATLTR || token_stream[index].type == TokenType::PLUS
            || token_stream[index].type == TokenType::MINU || token_stream[index].type == TokenType::NOT
            || token_stream[index].type == TokenType::IFTK || token_stream[index].type == TokenType::WHILETK
            ||token_stream[index].type == TokenType::BREAKTK || token_stream[index].type == TokenType::CONTINUETK
            || token_stream[index].type == TokenType::RETURNTK ){
                PARSE(blockitem_node, BlockItem);
            }
        if(token_stream[index].type != TokenType::RBRACE) return false;
        parseTerm(root, TokenType::RBRACE);
        return true;
   }
   return false;
}

bool  Parser::parseBlockItem(AstNode* root){
    /**
     * BlockItem -> Decl | Stmt
     * FIRST(Decl) = FIRST(ConstDecl) U FIRST(VarDecl) = { 'const', 'int', 'float'}
     * FIRST(Stmt) = { '(', Ident, IntConst, floatConst, '+', '-', '!', 'if', 'while', 'break', 'continue', 'return' }
    */
   if(token_stream[index].type == TokenType::CONSTTK || token_stream[index].type == TokenType::INTTK || token_stream[index].type == TokenType::FLOATTK){
       PARSE(decl_node, Decl);
       return true;
   }else {
       PARSE(stmt_node, Stmt);
       return true;
   }
}

bool  Parser::parseStmt(AstNode* root){
    /**
     * Stmt -> LVal '=' Exp ';' | Block | 'if' '(' Cond ')' Stmt [ 'else' Stmt ] | 'while' '(' Cond ')' Stmt | 'break' ';' |
     'continue'';' | 'return' [Exp] ';' | [Exp] ';'
     * FIRST(LVal) = {Ident}
     * FIRST(Exp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(Block) = { '{' };
     * FIRST(Stmt) = { '(', '{', ';',Ident, IntConst, floatConst, '+', '-', '!', 'if', 'while', 'break', 'continue', 'return' }
     * FIRST(Cond) = FIRST(LOrExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
    if(token_stream[index].type == TokenType::IDENFR &&(index+1<token_stream.size() && (token_stream[index+1].type == TokenType::ASSIGN || token_stream[index+1].type == TokenType::LBRACK))){ // LVal '=' Exp ';'
        PARSE(lval_node, LVal);
        if(token_stream[index].type != TokenType::ASSIGN) return false;
        parseTerm(root, TokenType::ASSIGN);
        PARSE(exp_node, Exp);
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
    }else if(token_stream[index].type == TokenType::LBRACE){ // Block
        PARSE(block_node, Block);
        return true;
    }else if(token_stream[index].type == TokenType::IFTK){ // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
        parseTerm(root, TokenType::IFTK);

        if(token_stream[index].type != TokenType::LPARENT) return false;
        parseTerm(root, TokenType::LPARENT);

        PARSE(cond_node, Cond);

        if(token_stream[index].type != TokenType::RPARENT) return false;
        parseTerm(root, TokenType::RPARENT);

        PARSE(stmt_node, Stmt);
        if(index < token_stream.size() && token_stream[index].type == TokenType::ELSETK){
            parseTerm(root, TokenType::ELSETK);
            PARSE(stmt_node, Stmt);
        }
    }else if(token_stream[index].type == TokenType::WHILETK){ // 'while' '(' Cond ')' Stmt 
        parseTerm(root, TokenType::WHILETK);

        if(token_stream[index].type != TokenType::LPARENT) return false;
        parseTerm(root, TokenType::LPARENT);

        PARSE(cond_node, Cond);

        if(token_stream[index].type != TokenType::RPARENT) return false;
        parseTerm(root, TokenType::RPARENT);

        PARSE(stmt1_node, Stmt);
    }else if(token_stream[index].type == TokenType::BREAKTK){ // 'break' ';' 
        parseTerm(root, TokenType::BREAKTK);
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
    }else if(token_stream[index].type == TokenType::CONTINUETK){ // 'continue'';'
        parseTerm(root, TokenType::CONTINUETK);
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
    }else if(token_stream[index].type == TokenType::RETURNTK){ //'return' [Exp] ';' 
        parseTerm(root, TokenType::RETURNTK);
        if(token_stream[index].type == TokenType::LPARENT || token_stream[index].type == TokenType::IDENFR
        || token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::FLOATLTR
        || token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU
        || token_stream[index].type == TokenType::NOT){
            PARSE(exp_node, Exp);
        }
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
    }else { //[Exp] ';'
        if(token_stream[index].type == TokenType::LPARENT || token_stream[index].type == TokenType::IDENFR
            || token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::INTLTR
            || token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU
            || token_stream[index].type == TokenType::NOT){ 
                PARSE(exp_node, Exp);
        }
        if(token_stream[index].type != TokenType::SEMICN) return false;
        parseTerm(root, TokenType::SEMICN);
    }
    return true;
}


bool  Parser::parseExp(AstNode* root){
    /**
     * Exp -> AddExp
     * FIRST(AddExp) = FIRST(MulExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(addexp_node, AddExp);
   return true;
}

bool  Parser::parseCond(AstNode* root){
    /**
     * Cond -> LOrExp
     * FIRST(LOrExp) = FIRST(LAndExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(lorexp_node, LOrExp);
   return true;
}

bool  Parser::parseLVal(AstNode* root){
    /**
     * LVal -> Ident {'[' Exp ']'}
     * FIRST(Exp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   if(token_stream[index].type == TokenType::IDENFR){
       parseTerm(root, TokenType::IDENFR);
       while(index < token_stream.size() && token_stream[index].type == TokenType::LBRACK){
           parseTerm(root, TokenType::LBRACK);
           PARSE(exp_node, Exp);
           if(token_stream[index].type != TokenType::RBRACK) return false;
           parseTerm(root, TokenType::RBRACK);
       }
       return true;
   }
   return false;
}

bool  Parser::parseNumber(AstNode* root){
    /**
     * Number -> IntConst | floatConst
    */
   if(token_stream[index].type == TokenType::INTLTR){
        parseTerm(root, TokenType::INTLTR);
   }else if(token_stream[index].type == TokenType::FLOATLTR){
        parseTerm(root, TokenType::FLOATLTR);
   }else return false;
   return true;
}

///---> primary后可能跟着（
bool  Parser::parsePrimaryExp(AstNode* root){
    /**
     * PrimaryExp -> '(' Exp ')' | LVal | Number
     * FIRST(Exp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(Number) = { IntConst, floatConst }
     * FIRST(LVal) = {Ident}
    */
   if(token_stream[index].type == TokenType::LPARENT){
       parseTerm(root, TokenType::LPARENT);
       PARSE(exp_node, Exp);
       if(token_stream[index].type != TokenType::RPARENT) return false;
       parseTerm(root, TokenType::RPARENT);
   }else if(token_stream[index].type == TokenType::IDENFR){
        PARSE(lval_node, LVal);
   }else if(token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::FLOATLTR){
       PARSE(number_node, Number);
   }else return false;
   return true;
}

bool  Parser::parseUnaryExp(AstNode* root){
    /**
     * UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
     * FIRST(PrimaryExp) = { '(' } U FIRST(LVal) U FIRST(Number) = { '(', Ident, IntConst, floatConst }
     * FIRST(FuncRParams) = FIRST(Exp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
     * FIRST(UnaryOp) = { '+', '-', '!' }
    */
   if(token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU || token_stream[index].type == TokenType::NOT){
        PARSE(unaryop_node, UnaryOp);
        PARSE(unaryexp_node, UnaryExp);
        return true;
   }else if(token_stream[index].type == TokenType::IDENFR &&(index +1 < token_stream.size() && token_stream[index + 1].type == TokenType::LPARENT)){
       parseTerm(root, TokenType::IDENFR);
       if(token_stream[index].type != TokenType::LPARENT) return false;
       parseTerm(root, TokenType::LPARENT);
       // [FuncRParams]
       if(token_stream[index].type == TokenType::LPARENT || token_stream[index].type == TokenType::IDENFR
            || token_stream[index].type == TokenType::INTLTR || token_stream[index].type == TokenType::FLOATLTR
            || token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU
            || token_stream[index].type == TokenType::NOT){
            PARSE(funcrparams_node, FuncRParams);
       }
       if(token_stream[index].type != TokenType::RPARENT) return false;
       parseTerm(root, TokenType::RPARENT);
       return true;
   }else {
        PARSE(primaryexp_node, PrimaryExp);
        return true;
   }
   return false;
}


bool  Parser::parseUnaryOp(AstNode* root){
    /**
     * UnaryOp -> '+' | '-' | '!'
    */
   if(token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU || token_stream[index].type == TokenType::NOT){
        parseTerm(root, token_stream[index].type);
        return true;
   }
   return false;
}

bool  Parser::parseFuncRParams(AstNode* root){
    /**
     * FuncRParams -> Exp { ',' Exp }
     * FIRST(Exp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(exp_node, Exp);
   while (index < token_stream.size() && token_stream[index].type == TokenType::COMMA)
   {
        parseTerm(root, TokenType::COMMA);
        PARSE(exp_node, Exp);
   }
   return true;
}

bool  Parser::parseMulExp(AstNode* root){
    /**
     * MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
     * FIRST(UnaryExp) = FIRST(PrimaryExp) U {Ident} U FIRST(UnaryOp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(unaryexp_node, UnaryExp);
   while(index < token_stream.size() && (token_stream[index].type == TokenType::MULT || token_stream[index].type == TokenType::DIV || token_stream[index].type == TokenType::MOD)){
       parseTerm(root, token_stream[index].type);
       PARSE(unaryexp_node, UnaryExp);
   }
   return true;
}

bool  Parser::parseAddExp(AstNode* root){
    /**
     * AddExp -> MulExp { ('+' | '-') MulExp }
     * FIRST(MulExp) = FIRST(UnaryExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(mulexp_node, MulExp);
   while(index < token_stream.size() && (token_stream[index].type == TokenType::PLUS || token_stream[index].type == TokenType::MINU)){
        parseTerm(root, token_stream[index].type);
        PARSE(mulexp_node, MulExp);
   }
   return true;
}

bool  Parser::parseRelExp(AstNode* root){
    /**
     * RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
     * FIRST(AddExp) = FIRST(MulExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(addexp_node, AddExp);
   while(index < token_stream.size() && (token_stream[index].type == TokenType::LSS || token_stream[index].type == TokenType::GTR || token_stream[index].type == TokenType::LEQ || token_stream[index].type == TokenType::GEQ)){
        parseTerm(root, token_stream[index].type);
        PARSE(addexp_node, AddExp);
   }
   return true;
}

bool  Parser::parseEqExp(AstNode* root){
    /**
     * EqExp -> RelExp { ('==' | '!=') RelExp }
     * FIRST(RelExp) = FIRST(AddExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(relexp_node, RelExp);
   while(index < token_stream.size() && (token_stream[index].type == TokenType::EQL || token_stream[index].type == TokenType::NEQ)){
        parseTerm(root, token_stream[index].type);
        PARSE(relexp_node, RelExp);
   }
   return true;
}

bool  Parser::parseLAndExp(AstNode* root){
    /**
     * LAndExp -> EqExp [ '&&' LAndExp ]
     * FIRST(EqExp) = FIRST(RelExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(eqexp_node, EqExp);
   if(index < token_stream.size() && token_stream[index].type == TokenType::AND){
        parseTerm(root, TokenType::AND);
        PARSE(landexp_node, LAndExp);
   }
   return true;
}

bool  Parser::parseLOrExp(AstNode* root){
    /**
     * LOrExp -> LAndExp [ '||' LOrExp ]
     * FIRST(LAndExp) = FIRST(EqExp) = { '(', Ident, IntConst, floatConst, '+', '-', '!' }
    */
   PARSE(landexp_node, LAndExp);
   if(index < token_stream.size() && token_stream[index].type == TokenType::OR){
        parseTerm(root, TokenType::OR);
        PARSE(lorexp, LOrExp);
   }
   return true;
}


bool  Parser::parseConstExp(AstNode* root){
    /**
     * ConstExp -> AddExp
    */
   PARSE(addexp_node, AddExp);
   return true;
}



void Parser::log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}
