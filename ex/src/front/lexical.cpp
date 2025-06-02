
#include"front/lexical.h"

#include<map>
#include<vector>
#include<cassert>
#include<string>

// #define TODO assert(0 && "todo")

// 判断是否是可结合的运算符，并返回可结合的运算符
char Combined_op(char c){
    // TODO;
    switch (c){
        case '=': return '=';
        case '<': return '=';
        case '>': return '=';
        case '!': return '=';
        case '|': return '|';
        case '&': return '&';
        default: break;//TODO;
    }
    return '\\';  // 当前运算符不可结合
}

/**
 * 判断是否是操作符或者符号
*/
bool isoperator(char c){
    // TODO;
    switch (c){
        case '+': return true;
        case '-': return true;
        case '*': return true;
        case '/': return true;
        case '%': return true;
        case '<': return true;
        case '>': return true;
        case ':': return true;
        case '=': return true;
        case ';': return true;
        case ',': return true;
        case '(': return true;
        case ')': return true;
        case '[': return true;
        case ']': return true;
        case '{': return true;
        case '}': return true;
        case '!': return true;
        case '&': return true;
        case '|': return true;
    }
    return false;
}
/**
 * 获得操作符的TokenType
*/
frontend::TokenType  get_op_type(std::string  s)  {
// TODO;
    if(s.length() > 2) return frontend::TokenType::IDENFR;  // 默认返回标识符类型
    else if(s.length() == 1) s += " ";  // 强制拼接一个空字符，使得s.length() = 2
    switch (s[0]){
        case '+': return  frontend::TokenType::PLUS;
        case '-': return  frontend::TokenType::MINU;
        case '*': return  frontend::TokenType::MULT;
        case '/': return  frontend::TokenType::DIV;
        case '%': return  frontend::TokenType::MOD;
        case '<': 
            switch (s[1]){
                case '=': return frontend::TokenType::LEQ;  // <=
                case ' ': return frontend::TokenType::LSS;  // <
                default: return frontend::TokenType::IDENFR;
            }
        case '>':
            switch (s[1]){
                case '=': return frontend::TokenType::GEQ; // >=
                case ' ': return frontend::TokenType::GTR; // >
                default: return frontend::TokenType::IDENFR;
            }
        case ':': return  frontend::TokenType::COLON;
        case '=': 
            switch (s[1]){
                case '=': return frontend::TokenType::EQL; // ==
                case ' ': return frontend::TokenType::ASSIGN; // =
                default: return frontend::TokenType::IDENFR;
            }
        case ';': return  frontend::TokenType::SEMICN;
        case ',': return  frontend::TokenType::COMMA;
        case '(': return  frontend::TokenType::LPARENT;
        case ')': return  frontend::TokenType::RPARENT;
        case '[': return frontend::TokenType::LBRACK;
        case ']': return frontend::TokenType::RBRACK;
        case '{': return frontend::TokenType::LBRACE;
        case '}': return frontend::TokenType::RBRACE;
        case '!':
            switch (s[1]){
                case '=': return frontend::TokenType::NEQ; // !=
                case ' ': return frontend::TokenType::NOT;; // !
                default: return frontend::TokenType::IDENFR;
            }
        case '&':
            switch (s[1]){
                case '&': return frontend::TokenType::AND; // &&
                default: return frontend::TokenType::IDENFR;
            }
        case '|':
            switch (s[1]){
                case '|': return frontend::TokenType::OR; // ||
                default: return frontend::TokenType::IDENFR;
            } 
        default: 
            break;
    }
    return  frontend::TokenType::INTLTR;
}


frontend::TokenType get_ID_type(std::string s) {
    std::map<std::string, int> id_index = {
        {"const", 10},
        {"void", 1},
        {"int", 2},
        {"float", 3},
        {"if", 4},
        {"else", 5},
        {"while", 6},
        {"continue", 7},
        {"break", 8},
        {"return", 9}
    };
    int value = id_index[s];
    switch (value){
        case 10 : return frontend::TokenType::CONSTTK;
        case 1 : return frontend::TokenType::VOIDTK;
        case 2 : return frontend::TokenType::INTTK;
        case 3 : return frontend::TokenType::FLOATTK;
        case 4 : return frontend::TokenType::IFTK;
        case 5 : return frontend::TokenType::ELSETK;
        case 6 : return frontend::TokenType::WHILETK;
        case 7 : return frontend::TokenType::CONTINUETK;
        case 8 : return frontend::TokenType::BREAKTK;
        case 9 : return frontend::TokenType::RETURNTK;
        default:
        break;
    }
    return frontend::TokenType::IDENFR; // 默认返回标识符
}
// #define DEBUG_DFA
// #define DEBUG_SCANNER

std::string frontend::toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::Ident: return "Ident";
    case State::IntLiteral: return "IntLiteral";
    case State::FloatLiteral: return "FloatLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

std::set<std::string> frontend::keywords= {
    "const", "int", "float", "if", "else", "while", "continue", "break", "return", "void"
};

frontend::DFA::DFA(): cur_state(frontend::State::Empty), cur_str() {}

frontend::DFA::~DFA() {}

bool frontend::DFA::next(char input, Token& buf) {  
#ifdef DEBUG_DFA
#include<iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif
    // TODO;
#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif
    // TODO;
    switch (cur_state){
        case frontend::State::Empty:{ // empty状态
            if(input >= '0' && input <= '9'){ // 数字
                cur_state = frontend::State::IntLiteral;
                cur_str += input;
                return false;
            }else if((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || input == '_'){ // 字母或者下划线
                cur_state = frontend::State::Ident;
                cur_str += input;
                return false;
            }else if(isoperator(input)) // 操作符
            {
                cur_state = frontend::State::op;
                cur_str += input;
                return false;
            }else if(input == '.'){
                cur_state = frontend::State::FloatLiteral;
                cur_str += input;
                return false;
            }else return false;
        };
        case frontend::State::Ident:{ // 标识符状态
            if((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || input == '_' || (input >= '0' && input <= '9')){
                cur_str += input;
                return false;
            }else if(isoperator(input)){
                buf.type = get_ID_type(cur_str);
                buf.value = cur_str;

                cur_state = frontend::State::op;
                cur_str =  input; 
                return true;
            }else{
                buf.type = get_ID_type(cur_str);
                buf.value = cur_str;

                cur_state = frontend::State::Empty;  // 回到empty状态
                cur_str = "";
                return true;
            }
        };
        case frontend::State::IntLiteral:{ // 整数状态
            if(input >= '0' && input <= '9'){
                cur_str += input;
                return false;
            }else if(input == '.'){
                cur_state = frontend::State::FloatLiteral;
                cur_str += input;
                return false;
            }else if(isoperator(input)){
                buf.type = frontend::TokenType::INTLTR;
                buf.value = cur_str;

                cur_state = frontend::State::op;
                cur_str =  input;
                return true;
            }else if((input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F') || input == 'x' || input == 'X'){
                cur_str += input;
                return false;
            }else{
                buf.type = frontend::TokenType::INTLTR;
                buf.value = cur_str;

                cur_state = frontend::State::Empty;  // 回到empty状态
                cur_str = "";
                return true;
            }
        };
        case frontend::State::FloatLiteral:{ // 浮点数状态
            if(input >= '0' && input <= '9'){
                cur_str += input;
                return false;
            }else if(isoperator(input)){
                buf.type = frontend::TokenType::FLOATLTR;
                buf.value = cur_str;

                cur_state = frontend::State::op;
                cur_str =  input;
                return true;
            }else {
                buf.type = frontend::TokenType::FLOATLTR;
                buf.value = cur_str;

                cur_state = frontend::State::Empty;  // 回到empty状态
                cur_str = "";
                return true;
            }
        };
        case frontend::State::op:{ // 标识符状态
            if(isoperator(input)){
                char c = Combined_op(cur_str[0]);
                if(c == input){
                    cur_str += input;
                    buf.type = get_op_type(cur_str);
                    buf.value = cur_str;

                    cur_state = frontend::State::Empty;
                    cur_str = "";
                    return true;
                }else{
                    buf.type = get_op_type(cur_str);
                    buf.value = cur_str;

                    cur_str =  input;
                    return true;
                }
            }else if(input >= '0' && input <= '9'){
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;

                cur_state = frontend::State::IntLiteral;
                cur_str =  input;
                return true;
            }else if((input >= 'a' && input <= 'z') || (input >= 'A' && input <= 'Z') || input == '_'){
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;

                cur_state = frontend::State::Ident;
                cur_str =  input;
                return true;
            }else {
                buf.type = get_op_type(cur_str);
                buf.value = cur_str;

                if(input == '.'){
                    cur_state = frontend::State::FloatLiteral;
                    cur_str = "."; 
                }else{
                    cur_state = frontend::State::Empty;
                    cur_str = "";
                }
                return true;
            }
        };
        default: return false;
            break;
    }
}

void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

/*
 * Scanner 读取文件内容，将文件转换成ifstream
*/
frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}

std::vector<frontend::Token> frontend::Scanner::run() {
    // TODO;
        std::vector<frontend::Token> tokens;
        DFA dfa;
        Token tk;
        std::string line;
        bool comment_tag = false;
        // 读取文件内容
        while (std::getline(fin, line))
        {
            line += '\n';
            // 删除字符串开头的空格
            line.erase(0, line.find_first_not_of(" "));
            for (size_t i = 0; i < line.size(); i++)
                {
                    if (i < line.size() - 1 && line[i] == '/' && line[i + 1] == '/')
                    {
                        break;
                    }
                    if (i < line.size() - 1 && line[i] == '/' && line[i + 1] == '*')
                    {
                        comment_tag = true;
                        i += 2;
                    }
                    if (i < line.size() - 1 && line[i] == '*' && line[i + 1] == '/')
                    {
                        comment_tag = false;
                        i += 2;
                    }

                    if (!comment_tag && dfa.next(line[i], tk))
                    {

                        tokens.push_back(tk);
#ifdef DEBUG_SCANNER
#include<iostream>
            std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
                    }
                }
        }
    return tokens;
}

