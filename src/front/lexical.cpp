#include"front/lexical.h"

#include <map>
#include <cassert>
#include <string>
#include <unordered_map>
#include <algorithm>

#define TODO assert(0 && "todo")

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

/********* Lab1 代码 *********/

frontend::TokenType frontend::DFA::getTokenType() {
    
    static const std::unordered_map<std::string, TokenType> token_map = {
        {"+", TokenType::PLUS}, {"-", TokenType::MINU}, 
        {"*", TokenType::MULT}, {"/", TokenType::DIV}, {"%", TokenType::MOD},
        {"<", TokenType::LSS}, {">", TokenType::GTR}, {"=", TokenType::ASSIGN},
        {"<=", TokenType::LEQ}, {">=", TokenType::GEQ}, {"==", TokenType::EQL},
        {"!=", TokenType::NEQ}, {"&&", TokenType::AND}, {"||", TokenType::OR},
        {"(", TokenType::LPARENT}, {")", TokenType::RPARENT},
        {"[", TokenType::LBRACK}, {"]", TokenType::RBRACK},
        {"{", TokenType::LBRACE}, {"}", TokenType::RBRACE},
        {"const", TokenType::CONSTTK}, {"void", TokenType::VOIDTK},
        {"int", TokenType::INTTK}, {"float", TokenType::FLOATTK},
        {"if", TokenType::IFTK}, {"else", TokenType::ELSETK},
        {"while", TokenType::WHILETK}, {"continue", TokenType::CONTINUETK},
        {"break", TokenType::BREAKTK}, {"return", TokenType::RETURNTK},
        {",",TokenType::COMMA}, {";",TokenType::SEMICN}, {":",TokenType::COLON}, {"!",TokenType::NOT}
    };

    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    
    // 新增浮点数识别逻辑
    auto isFloatLiteral = [](const std::string& str) {
        size_t dot_pos = str.find('.');
        size_t e_pos = str.find_first_of("eE");
        
        // 有效浮点格式：1.23 / 1e5 / 1.2e-3 等
        return (dot_pos != std::string::npos || e_pos != std::string::npos) &&
               std::all_of(str.begin(), str.end(), [](char c) {
                   return isdigit(c) || c == '.' || c == 'e' || c == 'E' || c == '+' || c == '-';
               });
    };

    // 优先检查保留字和运算符
    if (auto it = token_map.find(cur_str); it != token_map.end()) {
        return it->second;
    }
    
    // 数字常量识别 0x...   0...   0b...   0...   .0 x
    if (isDigit(cur_str[0]) || cur_str[0]=='.') {
        return isFloatLiteral(cur_str) ? TokenType::FLOATLTR : TokenType::INTLTR;
    }
    
    // 默认作为标识符
    return TokenType::IDENFR;
}

/********* Lab1 代码 *********/

bool frontend::DFA::next(char input, Token& buf) {
#ifdef DEBUG_DFA
#include<iostream>
    std::cout << "in state [" << toString(cur_state) << "], input = \'" << input << "\', str = " << cur_str << "\t";
#endif

/********* Lab1 代码 *********/

    bool isDigit   = (input >= '0' && input <= '9');
    bool isLetter  = (input >= 'A' && input <= 'Z') || (input >= 'a' && input <= 'z') || (input == '_');
    bool isSigns   =  (input == ':') ||
                      (input == ';') || (input == ',') || (input == '(') ||
                      (input == ')') || (input == '{') || (input == '}') || 
                      (input == '[') || (input == ']') || (input == '+') || (input == '-') || (input == '*') || (input == '/') || 
                      (input == '%') || (input == '<') || (input == '>') || (input == '=') ||
                      (input == '!') || (input == '&') || (input == '|');

    bool isDot     = (input == '.');
    bool isHex     = isDigit || (input >= 'a' && input <= 'f') || (input >= 'A' && input <= 'F') || (input == 'x');

    bool ret = false;
    auto pre = cur_str.back();

    bool pre_isOp      = (pre == '+') || (pre == '-') || (pre == '*') || (pre == '/') || 
                         (pre == '%') || (pre == '<') || (pre == '>') || (pre == '=') ||
                         (pre == '!') || (pre == '&') || (pre == '|');

    switch(cur_state){
        case(State::Empty):
            // a new begin
            
            cur_str += input;

            if      (isDigit)       cur_state = State::IntLiteral;
            else if (isLetter)      cur_state = State::Ident;
            else if (isSigns) cur_state = State::op;
            else if (isDot)         cur_state = State::FloatLiteral;
            else                {
                                    // 空格等空白字符无效
                                    cur_str.pop_back();
                                }
            ret = false;   break;

        case(State::Ident):
            // 标识符

            if      (isDigit || isLetter)   {
                                                cur_str += input;
                                                ret = false;   break;
                                            }
            else if (isSigns)         {   
                                                // Successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::op;
                                                cur_str = input;
                                                ret = true;   break;
                                            }
            else if (!isDot)                {
                                                // Successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::Empty;
                                                cur_str = "";
                                                ret = true;   break;
                                            }
            else                                ret = false;   
            break;       // invalid

        case(State::IntLiteral):
            // 整数

            if      (isDigit || isHex)      {
                                                cur_str += input;
                                                ret = false;   break;
                                            }
            else if (isDot)                 {
                                                cur_str += input;
                                                cur_state = State::FloatLiteral;
                                                ret = false;   break;
                                            }
            else if (isSigns)         {
                                                // successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::op;
                                                cur_str = input;
                                                ret = true;   break;
                                            }
            else                            {
                                                // successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::Empty;
                                                cur_str = "";
                                                ret = true;   break;
                                            }

        case(State::FloatLiteral):
            // 浮点数
            
            if      (isDigit)               {
                                                cur_str += input;
                                                ret = false;   break;
                                            }
            else if (isSigns)         {
                                                // successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::op;
                                                cur_str = input;
                                                ret = true;   break;
                                            }
            else                            {
                                                // successful
                                                buf = Token{ getTokenType(), cur_str};
                                                cur_state = State::Empty;
                                                cur_str = "";
                                                ret = true;   break;
                                            }

        case(State::op):
            // 浮点数
            
            if      ((pre == '=' || pre == '|' || pre == '&') && pre == input)             
                                            {
                                                cur_str += input;
                                                ret = false;   break;
                                            }
            else if (pre_isOp && input=='='){
                                                cur_str += input;
                                                ret = false;   break;
                                            }
            else if (isSigns)         {
                                                buf = Token{ getTokenType() , cur_str};
                                                cur_str = input;
                                                ret = true;   break;
                                            }
            else if (isDigit)               {
                                                buf = Token{ getTokenType() , cur_str};
                                                cur_str = input;
                                                cur_state = State::IntLiteral;
                                                ret = true;   break;
                                            }
            else if (isLetter)              {
                                                buf = Token{ getTokenType() , cur_str};
                                                cur_str = input;
                                                cur_state = State::Ident;
                                                ret = true;   break;
                                            }
            else if (isDot)                 {
                                                buf = Token{ getTokenType() , cur_str};
                                                cur_str = input;
                                                cur_state = State::FloatLiteral;
                                                ret = true;   break;
                                            }
            else                            {
                                                buf = Token{ getTokenType() , cur_str};
                                                cur_str = "";
                                                cur_state = State::Empty;
                                                ret = true;   break;                                             
                                            }

        default:
            ret = false;   break;

    }


/********* Lab1 代码 *********/

#ifdef DEBUG_DFA
    std::cout << "next state is [" << toString(cur_state) << "], next str = " << cur_str << "\t, ret = " << ret << std::endl;
#endif

    return ret;
}

void frontend::DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

frontend::Scanner::Scanner(std::string filename): fin(filename) {
    if(!fin.is_open()) {
        assert(0 && "in Scanner constructor, input file cannot open");
    }
}

frontend::Scanner::~Scanner() {
    fin.close();
}

std::string preprocess(std::ifstream& fin) {
    std::string processed;
    char c, prev = '\0';
    bool in_line_comment = false;   // 单行注释
    bool in_block_comment = false;  // 多行注释

    while (fin.get(c)) {
        // 单行注释 //
        if (!in_block_comment && c == '/' && prev == '/') {
            in_line_comment = true;
            processed.pop_back();  // 移除前一个'/'
            continue;
        }
        
        // 处理多行注释 /* */
        if (!in_line_comment) {
            if (!in_block_comment && c == '*' && prev == '/') {
                in_block_comment = true;
                processed.pop_back();  // 移除前一个'/'
                continue;
            }
            
            if (in_block_comment && c == '/' && prev == '*') {
                in_block_comment = false;
                prev = '\0';  // 重置prev避免误判
                continue;
            }
        }
        
        if (!in_line_comment && !in_block_comment) {
            // 保留非注释内容
            processed += c;
        }

        // 单行注释遇到换行符终止
        if (in_line_comment && c == '\n') {
            in_line_comment = false;
        }
        
        prev = c;  // 更新前一个字符
    }
    
    return processed;
}

std::vector<frontend::Token> frontend::Scanner::run() {
    std::vector<Token> ret;
    Token tk;
    frontend::DFA dfa;
    std::string s = preprocess(fin); 
    for(auto c: s) {
        if(dfa.next(c, tk)){
            ret.push_back(tk);
        }
    }

    if (dfa.next('\0', tk)) {  // 使用结束符触发
        ret.push_back(tk);
    }
        
#ifdef DEBUG_SCANNER
        std::cout << "token: " << toString(tk.type) << "\t" << tk.value << std::endl;
#endif
    
    return ret;
}