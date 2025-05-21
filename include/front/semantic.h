/**
 * @file semantic.h
 * @author Yuntao Dai (d1581209858@live.com)
 * @brief 
 * @version 0.1
 * @date 2023-01-06
 * 
 * a Analyzer should 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef SEMANTIC_H
#define SEMANTIC_H

#include"ir/ir.h"
#include"front/abstract_syntax_tree.h"

#include<map>
#include<string>
#include<vector>
using std::map;
using std::string;
using std::vector;

namespace frontend
{

// definition of symbol table entry
struct STE {
    ir::Operand operand;        // 常变量/常数组：type::IntLiteral/FloatLiteral       变量/数组：type::Int/Float
    vector<int> dimension;      // 常变量: size=0  常数组：size>0       变量：size=0  数组：size>0
    vector<string> const_val;   // 常量字面量，若为数组，则有多值
};

using map_str_ste = map<string, STE>;
// definition of scope infomation
struct ScopeInfo {
    int cnt;
    string name;
    map_str_ste table;      // 存储当前上下文所有变量定义，最顶层上下文为全局，结束时需拷贝到 Program::globalVal
};

// surpport lib functions
map<std::string,ir::Function*>* get_lib_funcs();

// definition of symbol table
struct SymbolTable{
    vector<ScopeInfo> scope_stack;              // 当前编译时的上下文
    map<std::string,ir::Function*> functions;   // 存储所有函数的符号信息（包括用户定义函数和库函数）
                                                // 分析结束，将 用户定义函数 拷贝至 Program::functions
    ir::Function* cur_func;

    /**
     * @brief enter a new scope, record the infomation in scope stacks
     * @param node: a Block node, entering a new Block means a new name scope
     */
    void add_scope(Block*);

    /**
     * @brief exit a scope, pop out infomations
     */
    void exit_scope();

    /**
     * @brief Get the scoped name, to deal the same name in different scopes, we change origin id to a new one with scope infomation,
     * for example, we have these code:
     * "     
     * int a;
     * {
     *      int a; ....
     * }
     * "
     * in this case, we have two variable both name 'a', after change they will be 'a' and 'a_block'
     * @param id: origin id 
     * @return string: new name with scope infomations
     */
    string get_scoped_name(string id) const;

    /**
     * @brief get the right operand with the input name
     * @param id identifier name
     * @return Operand 
     */
    ir::Operand get_operand(string id) const;

    /**
     * @brief get the right ste with the input name
     * @param id identifier name
     * @return STE 
     */
    STE get_ste(string id) const;
};


// singleton class
struct Analyzer {
    int tmp_cnt;
    vector<ir::Instruction*> g_init_inst;  // 全局初始化IR：CompUnit下的所有 Decl 或 FuncDef
    SymbolTable symbol_table;

    /**
     * @brief constructor
     */
    Analyzer();

    // analysis functions
    ir::Program get_ir_program(CompUnit*);

    // reject copy & assignment
    Analyzer(const Analyzer&) = delete;
    Analyzer& operator=(const Analyzer&) = delete;

    void analyzeCompUnit(CompUnit* root);
    
    void analyzeDecl(Decl* root, vector<ir::Instruction *> &buf);
    
    void analyzeConstDecl(ConstDecl* root, vector<ir::Instruction *> &buf);
    
    void analyzeConstDef(ConstDef* root, vector<ir::Instruction *> &buf, Type t);
    
    void analyzeBType(BType* root);

    void analyzeConstInitVal(ConstInitVal* root, vector<ir::Instruction *> &buf, int ofst, int dim);

    void analyzeConstExp(ConstExp* root);

    void analyzeVarDecl(VarDecl* root, vector<ir::Instruction *> &buf);
    
    void analyzeVarDef(VarDef* root, vector<ir::Instruction *> &buf, Type t);
    
    void analyzeInitVal(InitVal* root, vector<ir::Instruction *> &buf, int ofst);
    
    void analyzeExp(Exp* root, vector<ir::Instruction *> &buf);

    void analyzeFuncDef(FuncDef* root);
    
    void analyzeFuncFParams(FuncFParams* root, vector<ir::Operand>& pl);
    
    void analyzeFuncFParam(FuncFParam* root, vector<ir::Operand>& pl);
    
    void analyzeBlock(Block* root, vector<ir::Instruction *> &buf);
    
    void analyzeBlockItem(BlockItem* root, vector<ir::Instruction *> &buf);
    
    void analyzeStmt(Stmt* root, vector<ir::Instruction *> &buf);
    
    void analyzeAddExp(AddExp* root, vector<ir::Instruction *> &buf);

    void analyzeLVal(LVal* root, vector<ir::Instruction *> &buf, ir::Operand& ofst);
    
    void analyzeCond(Cond* root, vector<ir::Instruction *> &buf);

    void analyzeLOrExp(LOrExp* root, vector<ir::Instruction *> &buf);

    void analyzeLAndExp(LAndExp* root, vector<ir::Instruction *> &buf);
    
    void analyzeEqExp(EqExp* root, vector<ir::Instruction *> &buf);

    void analyzeRelExp(RelExp* root, vector<ir::Instruction*>& buf);

    void analyzeNumber(Number *root, vector<ir::Instruction *> &buf);

    void analyzeMulExp(MulExp* root, vector<ir::Instruction*>& buf);

    void analyzeUnaryExp(UnaryExp* root, vector<ir::Instruction*>& buf);

    void analyzeFuncRParams( FuncRParams* root, 
                             vector<ir::Instruction*>&buf, 
                             std::vector<ir::Operand>& par_list, 
                             std::vector<ir::Operand>& params );

    void analyzePrimaryExp(PrimaryExp* root, vector<ir::Instruction*>& buf);

    void log(AstNode* node);
};

} // namespace frontend

#endif