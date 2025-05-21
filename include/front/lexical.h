/**
 * @file lexical.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief
 * in this part, we need to design a DFA
 * so what should this DFA do? 
 * input a char, then acording to its current status, change to another state
 * during the changes, from one state to another, there maybe output -> Token
 * 
 * then we need to design a Scanner
 * which takes the input file and output a Token stream
 * 
 * @version 0.1
 * @date 2022-12-14
 *
 * @copyright Copyright (c) 2022
 */

#ifndef LEXICAL_H
#define LEXICAL_H

#include"front/token.h"

#include<set>
#include<vector>
#include<string>
#include<fstream>
#include<iostream>

namespace frontend {

// 五种可能的状态
enum class State {
    Empty,              // space, \n, \r ...
    Ident,              // a keyword or identifier, like 'int' 'a0' 'else' ...
    IntLiteral,         // int literal, like '1' '1900', only in decimal
    FloatLiteral,       // float literal, like '0.1'
    op                  // operators and '{', '[', '(', ',' ...
};
std::string toString(State);
 
// we should distinguish the keyword and a variable(function) name, so we need a keyword table here
extern std::set<std::string> keywords;

// definition of DFA
struct DFA {
    /**
     * @brief constructor, set the init state to State::Empty
     */
    DFA();
    
    /**
     * @brief destructor
     */
    ~DFA();
    
    // the meaning of copy and assignment for a DFA is not clear, so we do not allow them
    DFA(const DFA&) = delete;   // copy constructor
    DFA& operator=(const DFA&) = delete;    // assignment

    /**
     * @brief 其根据自身当前状态和输入来决定转移后的状态
     * @param[in] input: 当前读入的字符
     * @param[out] buf: 识别token结果，可能无效
     * @return  bool: 一个完整的token是否被识别
     */
    bool next(char input, Token& buf);

    /**
     * @brief reset the DFA state to begin
     */
    void reset();

    /*
        getTokenType(cur_str)
    */
    TokenType getTokenType();

private:
    State cur_state;        // 当前DFA状态
    std::string cur_str;    // 已经接收的字符串
};

// definition of Scanner
struct Scanner {
    /**
     * @brief constructor
     * @param[in] filename: the input file  
     */
    Scanner(std::string filename); 
    
    /**
     * @brief destructor, close the file
     */
    ~Scanner();

    // rejcet copy and assignment
    Scanner(const Scanner&) = delete;
    Scanner& operator=(const Scanner&) = delete;

    /**
     * @brief 读入文件，返回token序列
     * @return std::vector<Token>: token 序列
     */
    std::vector<Token> run();

private:
    std::ifstream fin;  // the input file
};

} // namespace frontend

#endif