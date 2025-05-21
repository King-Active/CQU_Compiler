#include"front/semantic.h"
#include <iostream>
#include<fstream>
#include<cassert>

using ir::Instruction;
using ir::Function;
using ir::Operand;
using ir::Operator;

#define TODO assert(0 && "TODO");
#define DEBUG_ANALYZE
void frontend::Analyzer::log(AstNode* node){
#ifdef DEBUG_ANALYZE
        std::cout << "- " << toString(node->type) << '\n';
#endif
}

#define GET_CHILD_PTR(node, type, index) auto node = dynamic_cast<type*>(root->children[index]); assert(node); 
#define COPY_EXP_NODE(from, to) to->is_computable = from->is_computable; to->v = from->v; to->t = from->t;
#define IS_T(index, t) root->children[index]->type == NodeType::t
int tmp_cnt = 0;
#define GET_TMP() "tmp_" + std::to_string(tmp_cnt++)

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

void frontend::SymbolTable::add_scope(Block* node) {
    // 进入新作用域时, 向符号表中添加 ScopeInfo, 相当于压栈
    int idx = int(scope_stack.size());
    frontend::ScopeInfo si { idx, "scope_" + std::to_string(idx)};
    scope_stack.emplace_back(si);
}

void frontend::SymbolTable::exit_scope() {
    // 退出时弹栈
    scope_stack.pop_back();
}

string frontend::SymbolTable::get_scoped_name(string id) const {
    // 输入一个变量名, 返回其在当前作用域下重命名后的名字 (相当于加后缀)
    assert(scope_stack.size()>0 && id != "");
    return id + "_" + scope_stack.back().name;
}

Operand frontend::SymbolTable::get_operand(string id) const {
    //输入一个原始变量名, 在符号表中寻找最近的同名变量, 返回对应的 Operand(注意，此 Operand 的 name 是重命名后的)
    for(auto s = scope_stack.rbegin(); s != scope_stack.rend(); s++){
        const auto& f = s->table.find(id);
        if(f != s->table.end()){
            return f->second.operand;
        }
    }
    assert(0 && "Operand no found!");
}

frontend::STE frontend::SymbolTable::get_ste(string id) const {
    // 输入一个变量名, 在符号表中寻找最近的同名变量, 返回 STE
    for(auto s = scope_stack.rbegin(); s != scope_stack.rend(); s++){
        const auto& f = s->table.find(id);
        if(f != s->table.end()){
            return f->second;
        }
    }
    assert(0 && "STE no found!");
}

frontend::Analyzer::Analyzer(): tmp_cnt(0), symbol_table() {}

// 入口
// TODO: g++ 17
ir::Program frontend::Analyzer::get_ir_program(CompUnit* root) {
    ir::Program prog;

    // 全局作用域
    symbol_table.add_scope(nullptr);

    // 全局库函数
    const auto& lib_funcs = *get_lib_funcs();
    symbol_table.functions.insert(lib_funcs.begin(), lib_funcs.end());
    
    // 解析
    analyzeCompUnit(root);

    /*
        global_init_func();
        main();
        {other functions ...}
    */

    // 全局初始化函数
    Function g_f("global_init", Type::null);
    g_f.InstVec.insert(g_f.InstVec.end(), g_init_inst.begin(), g_init_inst.end());
    g_f.addInst(new Instruction({},{},{}, Operator::_return));
    prog.addFunction(g_f);

    // 用户函数
    for (auto& [name, func]: lib_funcs)
        symbol_table.functions.erase(name);
    
    for (auto& [name, func]: symbol_table.functions){
        if(name == "main"){
            func->InstVec.insert(
                func->InstVec.begin(),  // 最前头
                new ir::CallInst(Operand("global_init", Type::null), {})  // CallInst(const Operand& op1, const Operand& des);  
            );
        }
        prog.addFunction(*func);
    }

    // 全局变量
    // map<string, STE>
    for (auto& [name, ste]: symbol_table.scope_stack[0].table) {
        if (ste.dimension.size() != 0) {
            // 数组
            int max_len = 1;
            for (auto d : ste.dimension) 
                max_len *= d;
            prog.globalVal.emplace_back(ste.operand, max_len); 
        } else {
            prog.globalVal.emplace_back(ste.operand);  
        }
    }

    symbol_table.exit_scope();
    return prog;
}

/*  --------------------------------------------------------------------------  */ 

void frontend::Analyzer::analyzeCompUnit(CompUnit* root) {
    // CompUnit -> (Decl | FuncDef) [CompUnit]
    log(root);
    if (IS_T(0, DECL)){
        GET_CHILD_PTR(decl, Decl, 0)
        // Decl 需区分为全局声明 或 局部声明
        analyzeDecl(decl, g_init_inst);
    } 
    else{
        assert(IS_T(0, FUNCDEF) && "Invalid CompUnit Child Node");
        GET_CHILD_PTR(funcdef, FuncDef, 0)
        // FuncDef 一定为全局声明
        analyzeFuncDef(funcdef);
    }
    if (root->children.size() > 1){
        GET_CHILD_PTR(cpu, CompUnit, 1)
        analyzeCompUnit(cpu);
    }
}

void frontend::Analyzer::analyzeDecl(Decl* root, vector<Instruction *> &buf){
    log(root);
    // Decl -> ConstDecl | VarDecl
    if(IS_T(0, CONSTDECL)){
        GET_CHILD_PTR(constdecl, ConstDecl, 0)
        analyzeConstDecl(constdecl, buf);
    } else {
        assert(IS_T(0, VARDECL) && "Invalid Decl Children");
        GET_CHILD_PTR(vardecl, VarDecl, 0)
        analyzeVarDecl(vardecl, buf);
    }
}

void frontend::Analyzer::analyzeConstDecl(ConstDecl* root, vector<Instruction *> &buf){
    log(root);
    // ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
    GET_CHILD_PTR(btype, BType, 1)
    analyzeBType(btype);
    auto const_t = btype->t==Type::Int? Type::IntLiteral : Type::FloatLiteral;
    root->t = const_t;
    for (int i = 2; i < int(root->children.size())-1; i += 2){
        GET_CHILD_PTR(constdef, ConstDef, i);
        analyzeConstDef(constdef, buf, root->t);
    }   
}

void frontend::Analyzer::analyzeConstDef(ConstDef* root, vector<Instruction *> &buf, Type t){
    log(root);
    // ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
    // t: Type::IntLiteral OR Type::FloatLiteral

    GET_CHILD_PTR(term, Term, 0);
    root->arr_name = symbol_table.get_scoped_name(term->token.value);

    GET_CHILD_PTR(constinitval, ConstInitVal, root->children.size()-1)
    constinitval->v = term->token.value;    // 原变量名
    constinitval->t = t;

    if(root->children.size() == 3){
        // Ident '=' ConstInitVal
    
        /* 上下文内部符号表索引存放 原始变量名，表项存储 重命名 结果 */
        symbol_table.scope_stack.back().table[term->token.value] = STE( {Operand(root->arr_name, t)} );
        analyzeConstInitVal(constinitval, buf, -1, -1);
    } else {
        // Ident { '[' ConstExp ']' } '=' ConstInitVal

        vector<int> dims;
        int size = 1;
        for (int i = 2; i < int(root->children.size())-3; i+=3) {
            GET_CHILD_PTR(constexp, ConstExp, i)
            analyzeConstExp(constexp);
            assert(constexp->t == Type::IntLiteral && "Invalid Array Index");
            int d = std::stoi(constexp->v);
            dims.push_back(d);
            size *= d;
        }
        
        symbol_table.scope_stack.back().table[term->token.value] = STE( {Operand(root->arr_name, t==Type::IntLiteral ? Type::IntPtr:Type::FloatPtr), dims} );
        // 分配空间，随后赋值
        buf.emplace_back(new Instruction(
            {std::to_string(size), Type::IntLiteral},   // 数组大小 本身是 整数常量
            {}, 
            {root->arr_name, t==Type::IntLiteral ? Type::IntPtr:Type::FloatPtr},           
            Operator::alloc
        ));
        /**
         * 1 - 根据 constinitval->v 获取 dims
         * 2 - 赋值 类型：constinitval->t = t
         */
        analyzeConstInitVal(constinitval, buf, 0, 0);
    }
}

void frontend::Analyzer::analyzeBType(BType* root){
    log(root);
    // BType -> 'int' | 'float'
    GET_CHILD_PTR(term, Term, 0)
    if (term->token.type == TokenType::INTTK)
        root->t = Type::Int;
    else {
        assert(term->token.type == TokenType::FLOATTK && "Invalid ConstDecl Btype");
        root->t = Type::Float;
    }
}
 
void frontend::Analyzer::analyzeConstInitVal(ConstInitVal* root, vector<Instruction *> &buf, int ofst, int dim){
    log(root);
    // ConstInitVal -> ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
    auto& ste = symbol_table.scope_stack.back().table[root->v];
    auto& t = symbol_table.scope_stack.back().table[root->v].operand.type;

    if(root->children.size() == 1){
        GET_CHILD_PTR(constexp, ConstExp, 0)
        analyzeConstExp(constexp);

        assert(constexp->t == Type::IntLiteral || constexp->t == Type::FloatLiteral);
        
        if ((t == Type::IntLiteral || t == Type::IntPtr) && constexp->t != Type::IntLiteral){
            // 类型转换
            if(constexp->t == Type::FloatLiteral)
                constexp->v = std::to_string(int(std::stof(constexp->v)));  // 截断
            // 整数 -> 浮点：无需处理
        }

        // 普通常量 -> 存储至编译时常量表
        if(ofst == -1)
            ste.const_val.push_back(constexp->v);
        else{
            // TODO  赋值类型转换
            buf.push_back(new Instruction(
                ste.operand,   // base
                {std::to_string(ofst), Type::IntLiteral},   // ofst
                {constexp->v, t==Type::IntPtr? Type::IntLiteral : Type::FloatLiteral},   // val
                Operator::store
            ));
        }

    } else {
        // 获取维度信息
        const auto& dims = ste.dimension;
        assert(dim < int(dims.size()) && "Array initialization dimension mismatch");
        assert(t == Type::IntPtr || t == Type::FloatPtr);

        // 本维度每个元素大小
        int ele_size = 1;
        for (int i = dim + 1; i < int(dims.size()); i++) {
            ele_size *= dims[i];
        }

        // 本维度第几个元素
        int ele_idx = 0;
        for (size_t i = 1; i < root->children.size(); i += 2) {
            GET_CHILD_PTR(constinitval, ConstInitVal, i);
            constinitval->t = root->t;
            constinitval->v = root->v;

            if (dim == int(dims.size()) - 1) 
                assert(constinitval->children.size() == 1 && "Too Many Dimensions");    
            
            analyzeConstInitVal(constinitval, buf, 
                ofst + ele_idx * ele_size, 
                dim + 1
            );

            ele_idx++;
        }

        if (dims.size() > 0) {
            int total_size = 1;
            for (int dim : dims) 
                total_size *= dim;
            
            // 对于数组初始化，如果有未指定的元素，用0填充
            while (ele_idx < total_size) {
                buf.push_back(new Instruction(
                    ste.operand,   // base
                    {std::to_string(ele_idx), Type::IntLiteral},   // ofst
                    {"0", t==Type::IntPtr? Type::IntLiteral : Type::FloatLiteral},   // 默认值0
                    Operator::store
                ));
                ele_idx++;
            }
        }
    }
}

void frontend::Analyzer::analyzeConstExp(ConstExp* root){
    log(root);
    // ConstExp -> AddExp
    GET_CHILD_PTR(addexp, AddExp, 0);
    auto tmp = vector<Instruction*>();
    analyzeAddExp(addexp, tmp);
    COPY_EXP_NODE(addexp, root);
}

void frontend::Analyzer::analyzeVarDecl(VarDecl* root, vector<Instruction *> &buf){
    log(root);
    // VarDecl -> BType VarDef { ',' VarDef } ';'
    GET_CHILD_PTR(btype, BType, 0)
    analyzeBType(btype);
    root->t = btype->t;
    for(int i = 1; i < int(root->children.size())-1; i+=2){
        GET_CHILD_PTR(vardef, VarDef, i)
        analyzeVarDef(vardef, buf, root->t);    // root->t 为元素类型
    }
}

void frontend::Analyzer::analyzeVarDef(VarDef* root, vector<Instruction *>& buf, Type t){
    log(root);
    // t为元素类型
    // VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]

    GET_CHILD_PTR(term, Term, 0);
    root->arr_name = symbol_table.get_scoped_name(term->token.value);   // 重命名

    if (root->children.size() > 3){
       // 数组
        vector<int> dims;
        int size = 1;
        for (int i = 2; i < int(root->children.size())-1; i+=3){     // 保证不越界，不误触 InitVal
            GET_CHILD_PTR(constexp, ConstExp, i)
            analyzeConstExp(constexp);
            assert(constexp->t == Type::IntLiteral && "Array Size Fault!");
            dims.push_back(std::stoi(constexp->v));
            size *= std::stoi(constexp->v);
        }

        auto ste = STE( {Operand(root->arr_name, t==Type::Int ? Type::IntPtr:Type::FloatPtr), dims} );
        symbol_table.scope_stack.back().table[term->token.value] = ste;

        // 分配空间，随后赋值
        buf.emplace_back(new Instruction(
            {std::to_string(size), Type::IntLiteral},   // 数组大小 本身是 整数常量
            {}, 
            {root->arr_name, t==Type::Int ? Type::IntPtr:Type::FloatPtr},           
            Operator::alloc
        ));

        if(IS_T(root->children.size()-1, INITVAL)){
            GET_CHILD_PTR(initval, InitVal, root->children.size()-1)
            initval->t = t;
            initval->v = term->token.value; // 原变量名
            analyzeInitVal(initval, buf, -1);
        }else{
            // Acking666 全部初始化为0 测试点62
            int g_ofst = 0;
            // 对于数组初始化，如果有未指定的元素，用0填充
            while (g_ofst < size) {
                buf.push_back(new Instruction(
                    ste.operand,   // base
                    {std::to_string(g_ofst), Type::IntLiteral},   // ofst
                    {"0", t==Type::Int? Type::IntLiteral : Type::FloatLiteral},   // 默认值0
                    Operator::store
                ));
                g_ofst++;
            }
        }
    } else {
        // 单变量
        symbol_table.scope_stack.back().table[term->token.value] = STE( {Operand(root->arr_name, t)} );
        if(root->children.size()>1){
            GET_CHILD_PTR(initval, InitVal, root->children.size()-1)
            initval->t = t;
            initval->v = term->token.value; // 原变量名
            analyzeInitVal(initval, buf, -1);
        }
    }
}

void frontend::Analyzer::analyzeInitVal(InitVal* root, vector<Instruction *> &buf, int ofst){
    log(root);
    // ofst >= 0 表示数组元素赋值 
    // InitVal -> Exp | '{' [ InitVal { ',' InitVal } ] '}'

    auto& ste = symbol_table.scope_stack.back().table[root->v];
    auto& t = ste.operand.type;    // Int Float
    const auto& dims = ste.dimension;    // 获取维度信息
    
    if(dims.size() > 0)
        assert(t == Type::IntPtr || t == Type::FloatPtr);

    if(root->children.size() == 1){
        // 普通变量
        GET_CHILD_PTR(exp, Exp, 0)
        analyzeExp(exp, buf);

        if ((t == Type::Int || t == Type::IntPtr) && (exp->t == Type::FloatLiteral || exp->t == Type::Float)){
            if(exp->t == Type::Float)
                assert(0 && "TODO") ;
            // 类型转换
            // exp必须确定数值
            exp->v = std::to_string(int(std::stof(exp->v)));  // 截断
        }

        if(dims.size() == 0){
            if(exp->t == Type::FloatLiteral || exp->t == Type::Float){
                buf.push_back(new Instruction(
                    // Acking!!! 必须使用 exp->t，声明为常量，否则会被解析为变量
                    {exp->v, exp->t},   // 变量字面量 与 类型
                    {}, 
                    ste.operand,   // 重命名后变量名 与 类型
                    Operator::fdef
                ));
            } else {
                buf.push_back(new Instruction(
                    // Acking!!! 必须使用 exp->t，声明为常量，否则会被解析为变量
                    {exp->v, exp->t},   // 变量字面量 与 类型
                    {}, 
                    ste.operand,   // 重命名后变量名 与 类型
                    Operator::def
                ));
            }

        }
        else{
            // 数组元素
            assert(t == Type::IntPtr || t == Type::FloatPtr);

            Operand res = {GET_TMP(), t==Type::IntPtr? Type::Int : Type::Float};

            if(t == Type::IntPtr && (exp->t == Type::Float || exp->t == Type::FloatLiteral)){
                buf.push_back(new Instruction(
                    {exp->v, exp->t},
                    {},
                    res,
                    Operator::cvt_f2i
                ));
            }
            else if(t == Type::FloatPtr && (exp->t == Type::Int || exp->t == Type::IntLiteral)){
                buf.push_back(new Instruction(
                    {exp->v, exp->t},
                    {},
                    res,
                    Operator::cvt_i2f
                ));
            }
            else 
                res = {exp->v, exp->t};

            buf.push_back(new Instruction(
                ste.operand,   // base
                {std::to_string(ofst), Type::IntLiteral},   // ofst
                res,   // val
                Operator::store
            ));
        }
    
    } 
    else {
        // 第几个元素
        int g_ofst = 0;
        // 非空
        if(IS_T(1, INITVAL)){
            for (size_t i = 1; i < root->children.size(); i += 2) {
                GET_CHILD_PTR(initval, InitVal, i)
                initval->t = root->t;
                initval->v = root->v;
                analyzeInitVal(initval, buf, g_ofst);
                g_ofst++;
            }
        }

        // test79 - 处理未初始化的元素（填充默认值）
        if (dims.size() > 0) {
            int total_size = 1;
            for (int dim : dims) 
                total_size *= dim;
            
            // 对于数组初始化，如果有未指定的元素，用0填充
            while (g_ofst < total_size) {
                buf.push_back(new Instruction(
                    ste.operand,   // base
                    {std::to_string(g_ofst), Type::IntLiteral},   // ofst
                    {"0", t==Type::IntPtr? Type::IntLiteral : Type::FloatLiteral},   // 默认值0
                    Operator::store
                ));
                g_ofst++;
            }
        }
    }
}

void frontend::Analyzer::analyzeExp(Exp* root, vector<Instruction *> &buf){
    log(root);
    // Exp -> AddExp
    GET_CHILD_PTR(addexp, AddExp, 0);
    analyzeAddExp(addexp, buf);
    COPY_EXP_NODE(addexp, root);
}

void frontend::Analyzer::analyzeFuncDef(FuncDef* root){
    log(root);
    // FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
    symbol_table.add_scope(nullptr);

    // 返回类型  FuncType -> 'void' | 'int' | 'float'
    GET_CHILD_PTR(f_t, FuncType, 0)
    auto f_t_c = static_cast<Term*>(f_t->children[0]); 
    assert(f_t_c); 
    if(f_t_c->token.type == TokenType::VOIDTK)
        root->t = Type::null;
    else if(f_t_c->token.type == TokenType::INTTK)
        root->t = Type::Int;
    else {
        assert (f_t_c->token.type == TokenType::FLOATTK && "Invalid FuncType");
        root->t = Type::Float;
    }

    // 函数名 
    GET_CHILD_PTR(f_n, Term, 1) 
    root->n = f_n->token.value;

    // 形参
    vector<Operand> pl;
    if(IS_T(3, FUNCFPARAMS)){
        GET_CHILD_PTR(fprams, FuncFParams, 3);
        analyzeFuncFParams(fprams, pl);
    }
    auto f = new Function(
        root->n,
        pl,
        root->t
    );
    symbol_table.functions[root->n] = f;

    // 记录当前函数信息：测试点95返回值类型检查
    symbol_table.cur_func = f;

    // 函数体(指令)
    GET_CHILD_PTR(blk, Block, root->children.size()-1)
    analyzeBlock(blk, f->InstVec);

    if(f->InstVec.empty() || f->InstVec.back()->op != Operator::_return){
        if(root->n == "main")
            f->addInst(new Instruction(
                {"0", Type::IntLiteral},{},{},Operator::_return
            ));
        else 
            f->addInst(new Instruction(
                {},{},{},Operator::_return
            ));
    }

    symbol_table.exit_scope();
}

void frontend::Analyzer::analyzeFuncFParams(FuncFParams* root, vector<Operand>& pl){
    log(root);
    // FuncFParams -> FuncFParam { ',' FuncFParam }
    for(int i = 0; i < int(root->children.size()); i+=2){
        GET_CHILD_PTR(fp, FuncFParam, i)
        analyzeFuncFParam(fp, pl);
    }
}

void frontend::Analyzer::analyzeFuncFParam (FuncFParam* root, vector<Operand>& pl){
    log(root);
    // FuncFParam -> BType Ident ['[' ']' { '[' Exp ']' }]
    GET_CHILD_PTR(btype, BType, 0)
    analyzeBType(btype);
    auto t = btype->t;

    GET_CHILD_PTR(name, Term, 1)
    auto n = name->token.value;

    auto r_n = symbol_table.get_scoped_name(n); // 参数在本函数内的重命名

    if(root->children.size() > 2){
        //数组
        vector<int> dims;
        dims.push_back(-1);  // 数组第一维度

        for(int i = 5; i < int(root->children.size()); i+=3){
            GET_CHILD_PTR(constexp, ConstExp, i)
            analyzeConstExp(constexp);
            dims.push_back(std::stoi(constexp->v));
        }

        Operand op = {r_n, t==Type::Int? Type::IntPtr : Type::FloatPtr};
        symbol_table.scope_stack.back().table[n] = STE( {op, dims} );
        pl.push_back(op);

    } else {
        // 单变量
        Operand op = {r_n, t};
        symbol_table.scope_stack.back().table[n] = STE( {op} );
        pl.push_back(op);
    }
}

void frontend::Analyzer::analyzeBlock(Block* root, vector<Instruction *> &buf){
    log(root);
    // Block -> '{' { BlockItem } '}'
    // 已添加 scope
    
    if(root->children.size() > 2){
        // 非空块
        for(int i = 1; i < int(root->children.size()-1); i++){
            GET_CHILD_PTR(blk, BlockItem, i)
            analyzeBlockItem(blk, buf);
        }
    }
}

void frontend::Analyzer::analyzeBlockItem(BlockItem* root, vector<Instruction *> &buf){
    log(root);
    // BlockItem -> Decl | Stmt
    if(IS_T(0, DECL)){
        GET_CHILD_PTR(decl, Decl, 0);
        analyzeDecl(decl, buf);
    } else {
        assert(IS_T(0, STMT) && "Invalid BlockItem Type");
        GET_CHILD_PTR(stmt, Stmt, 0);
        analyzeStmt(stmt, buf);
    }
}

void frontend::Analyzer::analyzeStmt(Stmt* root, vector<Instruction *> &buf){
    log(root);
    auto first_c = root->children[0];

    switch (first_c->type) {
        case NodeType::LVAL:{
            // LVal '=' Exp ';'
            GET_CHILD_PTR(lval, LVal, 0)    // v, t, i
            Operand ofst({GET_TMP(), Type::Int});
            analyzeLVal(lval, buf, ofst);

            GET_CHILD_PTR(exp, Exp, 2)
            analyzeExp(exp, buf);

            Operand lv (lval->v, lval->t);

            if(exp->t == Type::Float || exp->t == Type::Int){
                // 右值是变量 exp->v 是变量名
                Operand rv (exp->v, exp->t);    
                // 值

                Operand res ({GET_TMP(), Type::Int});
                if((lval->t == Type::Int || lval->t == Type::IntPtr) && exp->t == Type::Float){
                    // lval 只能是 Type::Int || Type::Float || Type::IntPtr || Type::FloatPtr 
                    buf.push_back(new Instruction(
                        rv,
                        {},
                        res,
                        Operator::cvt_f2i
                    ));
                }else{
                    res = rv;
                }

                if (lval->t == Type::Int || lval->t == Type::Float) // 变量
                    buf.push_back(new Instruction(res, {}, lv, Operator::mov));
                else{ // 数组
                    assert((lval->t == Type::IntPtr || lval->t == Type::FloatPtr) && "Invalid Right Value");
                    buf.push_back(new Instruction(    
                                                    lv,                           // Base
                                                    ofst,  // Ofst
                                                    res, 
                                                    Operator::store
                                                ));
                }

            } else {
                // 右值是常量 exp->v 是值
                Operand rv (exp->v, Type::null);
                // 值
                if((lval->t == Type::Int || lval->t == Type::IntPtr) && exp->t == Type::FloatLiteral){
                    // lval 只能是 Type::Int || Type::Float || Type::IntPtr || Type::FloatPtr 
                    rv.name = std::to_string( int( std::stof(exp->v) ) );
                }

                // 类型
                if(lval->t == Type::Int || lval->t == Type::IntPtr)
                    rv.type = Type::IntLiteral;
                else
                    rv.type = Type::FloatLiteral;

                if (lval->t == Type::Int || lval->t == Type::Float) // 变量
                    buf.push_back(new Instruction(rv, {}, lv, Operator::mov));
                else{ // 数组
                    assert((lval->t == Type::IntPtr || lval->t == Type::FloatPtr) && "Invalid Right Value");

                    buf.push_back(new Instruction(    
                                                    lv,                           // Base
                                                    ofst,  // Ofst
                                                    rv, 
                                                    Operator::store
                                                ));
                }
            }

            break;
        }

        case NodeType::BLOCK: {
            // Block
            symbol_table.add_scope(nullptr);
            GET_CHILD_PTR(blk, Block, 0)
            analyzeBlock(blk, buf);
            symbol_table.exit_scope();
            break;
        }

        case NodeType::EXP: {
            // Exp ';'
            GET_CHILD_PTR(exp, Exp, 0)
            analyzeExp(exp, buf);
            break;
        }

        default:{
            assert(first_c->type == NodeType::TERMINAL && "Invalid Stmt NodeType");

            switch (static_cast<Term*>(first_c)->token.type){
                case TokenType::IFTK :{
                    // 'if' '(' Cond ')' Stmt [ 'else' Stmt ]
                    GET_CHILD_PTR(cond, Cond, 2)
                    analyzeCond(cond, buf);

                    vector<Instruction*> if_blk;
                    GET_CHILD_PTR(stmt, Stmt, 4)
                    analyzeStmt(stmt, if_blk);

                    if( cond->t == Type::IntLiteral || cond->t == Type::FloatLiteral ){
                        if(  (cond->t == Type::IntLiteral && std::stoi(cond->v) != 0) || 
                             (cond->t == Type::FloatLiteral && std::stof(cond->v) != 0) )
                            buf.insert(buf.end(), if_blk.begin(), if_blk.end());
                        else if(root->children.size() > 5){
                            GET_CHILD_PTR(stmt_e, Stmt, 6)
                            analyzeStmt(stmt_e, buf);
                        }
                    }
                    else{
                        /* 
                        [ 有 else ]                      [ 无 else ]
                          (1) - cal cond                  (1) - cal cond
                           2 - satisfy _goto if_blk        2 - satisfy _goto if_blk 
                           3 - nosatisfy _goto else        3 - nosatisfy _goto end
                           4 - if_blk ...                  4 - if_blk ...
                           5 - goto_ end                   5 - end
                           6 - else_blk ...
                           7 - end
                        */
                       if(cond->t == Type::Float){
                            Operand tmp = Operand(GET_TMP(), Type::Float);
                            buf.push_back(new Instruction({cond->v, cond->t}, {"0.0", Type::FloatLiteral}, tmp, Operator::fneq));  // 1
                            buf.push_back(new Instruction(tmp, {}, {"2", Type::IntLiteral}, Operator::_goto));    // 2  非零（不等）跳转
                       } else {
                            buf.push_back(new Instruction({cond->v, cond->t}, {}, {"2", Type::IntLiteral}, Operator::_goto));    // 2
                       }

                       if(root->children.size() > 5){
                            buf.push_back(new Instruction( 
                                {/*无条件跳转*/},
                                { },
                                { std::to_string(if_blk.size()+2), Type::IntLiteral} ,
                                Operator::_goto 
                            ));     // 3
                            buf.insert(buf.end(), if_blk.begin(), if_blk.end());    // 4
                            
                            vector<Instruction*> else_blk;
                            GET_CHILD_PTR(stmt_e, Stmt, 6)
                            analyzeStmt(stmt_e, else_blk);

                            buf.push_back(new Instruction( 
                                {/*无条件跳转*/},
                                { },
                                { std::to_string(else_blk.size()+1), Type::IntLiteral} ,
                                Operator::_goto 
                              ));     // 5

                            buf.insert(buf.end(), else_blk.begin(), else_blk.end());    // 6

                       } else {
                            // 无 else
                            buf.push_back(new Instruction( 
                                {/*无条件跳转*/},
                                { },
                                { std::to_string(if_blk.size()+1), Type::IntLiteral} ,
                                Operator::_goto 
                            ));     // 3

                            buf.insert(buf.end(), if_blk.begin(), if_blk.end());    // 4
                       }

                       buf.push_back(new Instruction({}, {}, {}, Operator::__unuse__));  // end
                    }

                    break;
                }

                case TokenType::WHILETK: {
                    // 'while' '(' Cond ')' Stmt
                    // Attention to break & continue
                    /*
                        1   -  Cond
                        (2)  -  cond_jump_jdg 
                        2   -  wl_blk
                        3   -  goto 1
                        4   -  end
                    */

                    int init_pos = buf.size();  // Cond第一条指令的位置

                    GET_CHILD_PTR(cond, Cond, 2)
                    analyzeCond(cond, buf);     // 1 - Cond已被插入指令序列

                    vector<Instruction*> wl_blk;
                    GET_CHILD_PTR(stmt, Stmt, 4)
                    analyzeStmt(stmt, wl_blk);
                    
                    // break -> goto end
                    // continue -> goto wl_blk
                    for(int i = 0; i < int(wl_blk.size()); i++){
                        if(wl_blk[i]->op1.name == "break"){
                            wl_blk[i] = new Instruction(
                                {/*无条件跳转*/}, 
                                {}, 
                                {std::to_string(wl_blk.size() - i + 1), Type::IntLiteral},  // end
                                Operator::_goto
                            );
                        } else if (wl_blk[i]->op1.name == "continue"){
                            int ofst_tmp = -(buf.size() - init_pos + i);
                            /* 编译时可确定逻辑，不占用 cond 到 wl_blk 之间的指令数 */
                            /* 编译时不可确定的逻辑，Float占用2，Int占用1 */
                            if(cond->t == Type::Float)
                                ofst_tmp -= 2;
                            else if(cond->t == Type::Int)
                                ofst_tmp -= 1;

                            wl_blk[i] = new Instruction(
                                {/*无条件跳转*/}, 
                                {}, 
                                {std::to_string(ofst_tmp), Type::IntLiteral}, 
                                Operator::_goto
                            );
                        }
                    }

                    if(cond->t == Type::IntLiteral || cond->t == Type::FloatLiteral){
                        if( (cond->t == Type::IntLiteral && std::stoi(cond->v) != 0) || 
                            (cond->t == Type::FloatLiteral && std::stof(cond->v) != 0) ) {
                            
                                buf.insert(buf.end(), wl_blk.begin(), wl_blk.end());    
                                buf.push_back(new Instruction({}, {}, {std::to_string(-int(buf.size()-init_pos)), Type::IntLiteral}, Operator::_goto));    // 3
                                buf.push_back(new Instruction({}, {}, {}, Operator::__unuse__));  // end
                        }   
                        // 假，则不插入 while_blk
                    } else {
                        if(cond->t == Type::Float){
                            assert(0 && "Condition should not be float");
                            // Operand tmp = Operand(GET_TMP(), Type::Int);
                            // buf.push_back(new Instruction({cond->v, cond->t}, {"0.0", Type::FloatLiteral}, tmp, Operator::feq));  
                            // buf.push_back(new Instruction(tmp, {}, {std::to_string(wl_blk.size()+2), Type::IntLiteral}, Operator::_goto));  // 结束
                            // buf.insert(buf.end(), wl_blk.begin(), wl_blk.end());
                            // buf.push_back(new Instruction({}, {}, {std::to_string(-int(buf.size()-init_pos)), Type::IntLiteral}, Operator::_goto));  // 重新
                        } else {
                            Operand tmp = Operand(GET_TMP(), Type::Int);
                            buf.push_back(new Instruction({cond->v, cond->t}, {"0", Type::IntLiteral}, tmp, Operator::eq));  
                            buf.push_back(new Instruction(tmp, {}, {std::to_string(wl_blk.size()+2), Type::IntLiteral}, Operator::_goto));  // 结束
                            buf.insert(buf.end(), wl_blk.begin(), wl_blk.end());
                            buf.push_back(new Instruction({}, {}, {std::to_string(-int(buf.size()-init_pos)), Type::IntLiteral}, Operator::_goto)); // 重新
                        }
                        buf.push_back(new Instruction({}, {}, {}, Operator::__unuse__));  // end
                    }
                
                    break;
                }

                case TokenType::BREAKTK: {
                    // 'break' ';'
                    buf.push_back(new Instruction({"break"}, {}, {}, {Operator::__unuse__}));   // 交由上游处理 break
                    break;
                }

                case TokenType::CONTINUETK: {
                    // 'continue' ';'
                    buf.push_back(new Instruction({"continue"}, {}, {}, {Operator::__unuse__}));   // 交由上游处理
                    break;
                }

                case TokenType::RETURNTK: {
                    // 'return' [Exp] ';'
                    if(root->children.size() == 2)
                        buf.push_back(new Instruction({"return"}, {}, {}, Operator::_return));   // 交由上游处理
                    else {
                        assert(IS_T(1, EXP) && "Return Must Follow Exp");
                        GET_CHILD_PTR(exp, Exp, 1)
                        // try {
                            analyzeExp(exp, buf);
                        // } catch (...) {
                        //     throw std::runtime_error(
                        //         "RETURN FALSE!!!"
                        //     );
                        // }

                        auto& func = symbol_table.cur_func;
                        Operand rt = {GET_TMP(), Type::null};
                        if(func->returnType != Type::null){
                            // 返回值类型检查
                            if(func->returnType == Type::Int){
                                rt.type = Type::Int;
                                if(exp->t == Type::Float || exp->t == Type::FloatLiteral){
                                    // 1 - 要求 Int，返回 Float
                                    buf.push_back(new Instruction(
                                        {exp->v, exp->t},
                                        {},
                                        rt,
                                        Operator::cvt_f2i
                                    ));
                                } 
                                else {
                                    rt = {exp->v, exp->t};
                                }
                            }else{
                                rt = {exp->v, exp->t};
                            }
                        }
                        
                        buf.push_back(new Instruction(rt, {}, {}, Operator::_return));
                    }
                    break;
                }
                
                case TokenType::SEMICN:
                    // ;
                break;

                default:
                    assert(0 && "Invalid Terminal Token in Stmt");
            }
        }
    }
}

void frontend::Analyzer::analyzeLVal(LVal* root, vector<Instruction *> &buf, Operand& ofst){
    log(root);
    // LVal -> Ident {'[' Exp ']'}
    // LVal 被赋值对象，保证已声明定义
    // 1 - 常量符号
    // 2 - 变量符号
    // 3 - 常量数组元素
    // 4 - 常量数组
    // 5 - 变量数组
    // 6 - 变量数组元素

    auto id = static_cast<Term*>(root->children[0])->token.value;   // 原变量名
    auto ste = symbol_table.get_ste(id);

    /**
        string v; // 变量名
        Type t;   // 类型
        int i;    // 数组偏移
     */
    auto type = ste.operand.type;
    auto name = ste.operand.name;
    auto& dims = ste.dimension;     // 数组总维度

    // 1 - 常量符号
    if(root->children.size()==1 && dims.size()==0 && (type==Type::IntLiteral || type==Type::FloatLiteral)){
        root->t = type;
        root->v = ste.const_val[0];
        root->i = -1;
    }
    // 2 - 变量符号
    else if(root->children.size()==1 && (type==Type::Int || type==Type::Float)){
        root->t = type;
        root->v = name;
        root->i = -1;
    }
    
    else if( (dims.size()>0 && (type==Type::IntLiteral || type==Type::FloatLiteral)) || ((type==Type::IntPtr || type==Type::FloatPtr)) ){
        int accessed_dims = (root->children.size() - 1) / 3;    // 实际访问维度数
        Operand tmp = {GET_TMP(), Type::Int};
        
        buf.push_back(new Instruction({"0", Type::IntLiteral}, {}, ofst , Operator::def));  // 数组下标 ofst 动态计算
        buf.push_back(new Instruction({"0", Type::IntLiteral}, {}, tmp , Operator::def));

        int d = 0;
        int ele_size = 1;
        for (int i = 0; i < int(dims.size()); i++) {
            ele_size *= dims[i];
        }

        for(int i = 2; i < int(root->children.size())-1; i+=3){
            // 获取当前维度下标
            GET_CHILD_PTR(exp, Exp, i)
            analyzeExp(exp, buf);
            
            ele_size /= dims[d++];
            
            // exp 有可能为常量，但无妨
            
            buf.push_back(new Instruction(
                {exp->v, exp->t},     // 数据下标编译时确定
                {std::to_string(ele_size), Type::IntLiteral}, 
                tmp, 
                Operator::mul
            ));
            buf.push_back(new Instruction(
                tmp, 
                ofst, 
                ofst, 
                Operator::add
            ));
        }
        
        root->t = (type==Type::IntLiteral || type==Type::IntPtr)? Type::IntPtr : Type::FloatPtr;
        root->v = name;

        if(accessed_dims < int(dims.size())){
            // 4/5 - 数组地址
            // 如：传递数组参数情况
            root->i = -3;   // 表示地址          
        } 
        else{
            // 3/6 - 数组元素访问
            // 偏移需要动态计算
            assert(accessed_dims == int(dims.size()) && "Invalid array access dims");
            root->i = -2;
        }
    }else{
        assert(0 && "Undefined LVal");
    }
}

void frontend::Analyzer::analyzeCond(Cond* root, vector<Instruction *> &buf){
    log(root);
    // Cond -> LOrExp
    GET_CHILD_PTR(lorexp, LOrExp, 0)
    analyzeLOrExp(lorexp, buf);
    COPY_EXP_NODE(lorexp, root)
}

void frontend::Analyzer::analyzeLOrExp(LOrExp* root, vector<Instruction *> &buf){
    log(root);
    // LOrExp -> LAndExp [ '||' LOrExp ]
    GET_CHILD_PTR(landexp, LAndExp, 0)
    vector<Instruction *> landexp_blk;
    analyzeLAndExp(landexp, landexp_blk); 

    if(root->children.size() == 1){
        buf.insert(buf.end(), landexp_blk.begin(), landexp_blk.end());
        COPY_EXP_NODE(landexp, root);
    } else {

        GET_CHILD_PTR(lorexp, LOrExp, 2)
        vector<Instruction *> lorexp_blk;
        analyzeLOrExp(lorexp, lorexp_blk); 
        
        bool c_i_1 = (landexp->t == Type::IntLiteral),
             c_f_1 = (landexp->t == Type::FloatLiteral), 
             c_i_2 = (lorexp->t == Type::IntLiteral), 
             c_f_2 = (lorexp->t == Type::FloatLiteral) ;

        if ((c_i_1 || c_f_1) && (c_i_2 || c_f_2)){
            root->t = Type::IntLiteral;
            if (c_i_1 && c_i_2){
                root->v = std::to_string(std::stoi(landexp->v) || std::stoi(lorexp->v));
            }
            else if (c_i_1 && c_f_2){
                root->v = std::to_string(std::stoi(landexp->v) || std::stof(lorexp->v));
            }
            else if (c_f_1 && c_i_2){
                root->v = std::to_string(std::stof(landexp->v) || std::stoi(lorexp->v));
            }
            else{
                root->v = std::to_string(std::stof(landexp->v) || std::stof(lorexp->v));
            }
        }
        else if(c_i_1 || c_f_1){
            // 第一个为常量 (第二个变量)
            if ((c_i_1 && std::stoi(landexp->v) != 0) || (c_f_1 && std::stof(landexp->v) != 0)){
                // 短路
                root->v = "1";
                root->t = Type::IntLiteral;
            } else {
                // 完全看第二个
                buf.insert(buf.end(), lorexp_blk.begin(), lorexp_blk.end());
                COPY_EXP_NODE(lorexp, root)
            }
        }
        else {
            buf.insert(buf.end(), landexp_blk.begin(), landexp_blk.end());
            Operand op1 = Operand(landexp->v, landexp->t);      // landexp->v 为LAndExp计算结果存储的变量名
            Operand op2;
            Operand tmp = Operand(GET_TMP(), Type::Int);

            if(c_i_2)
                op2 = Operand(lorexp->v, Type::Int);
            else if(c_f_2)
                op2 = Operand(lorexp->v, Type::Float);
            else 
                op2 = Operand(lorexp->v, lorexp->t);
            
            buf.push_back(new Instruction(
                op1, 
                {}, 
                tmp, 
                Operator::mov
            ));

            buf.push_back(new Instruction(   // 不为零 <=> op1为True，跳转
                op1,
                {}, 
                {std::to_string(lorexp_blk.size() + 2), Type::IntLiteral}, 
                Operator::_goto // 此时，tmp的值已经更新为 op1
            ));

            buf.insert(buf.end(), lorexp_blk.begin(), lorexp_blk.end());

            buf.push_back(new Instruction(
                op1, 
                op2, 
                tmp,
                Operator::_or
            ));
            
            root->v = tmp.name;
            root->t = tmp.type;
        }
    }
}

void frontend::Analyzer::analyzeLAndExp(LAndExp* root, vector<Instruction *> &buf){
    log(root);
    // LAndExp -> EqExp [ '&&' LAndExp ]
    GET_CHILD_PTR(eqexp, EqExp, 0)
    vector<Instruction *> eqexp_blk;
    analyzeEqExp(eqexp, eqexp_blk);
    
    if(root->children.size() == 1){
        buf.insert(buf.end(), eqexp_blk.begin(), eqexp_blk.end());
        COPY_EXP_NODE(eqexp, root)
    }
    else{
        GET_CHILD_PTR(landexp, LAndExp, 2)
        vector<Instruction *> landexp_blk;
        analyzeLAndExp(landexp, landexp_blk);

        bool c_i_1 = (eqexp->t == Type::IntLiteral),
             c_f_1 = (eqexp->t == Type::FloatLiteral), 
             c_i_2 = (landexp->t == Type::IntLiteral), 
             c_f_2 = (landexp->t == Type::FloatLiteral) ;

        if ((c_i_1 || c_f_1) && (c_i_2 || c_f_2)){
            root->t = Type::IntLiteral;
            if (c_i_1 && c_i_2){
                root->v = std::to_string(std::stoi(eqexp->v) && std::stoi(landexp->v));
            }
            else if (c_i_1 && c_f_2){
                root->v = std::to_string(std::stoi(eqexp->v) && std::stof(landexp->v));
            }
            else if (c_f_1 && c_i_2){
                root->v = std::to_string(std::stof(eqexp->v) && std::stoi(landexp->v));
            }
            else{
                root->v = std::to_string(std::stof(eqexp->v) && std::stof(landexp->v));
            }
        }
        else if(c_i_1 || c_f_1){
            // 第一部分为常量
            if ((c_i_1 && std::stoi(eqexp->v) == 0) || (c_f_1 && std::stof(eqexp->v) == 0)){
                // 短路
                root->v = "0";
                root->t = Type::IntLiteral;
            } else {
                // 完全看第二个
                buf.insert(buf.end(), landexp_blk.begin(), landexp_blk.end());
                COPY_EXP_NODE(landexp, root)
            }
        }
        else{
            buf.insert(buf.end(), eqexp_blk.begin(), eqexp_blk.end());
            Operand op1 = Operand(eqexp->v, eqexp->t); 
            Operand op2;
            Operand tmp = Operand(GET_TMP(), Type::Int);
            Operand tmp2 = Operand("tmp2", Type::Int);

            if(c_i_2)
                op2 = Operand(landexp->v, Type::Int);
            else if(c_f_2)
                op2 = Operand(landexp->v, Type::Float);
            else 
                op2 = Operand(landexp->v, landexp->t);
            
            buf.push_back(new Instruction(op1, {}, tmp, Operator::mov)); // op1只会是 0.0(1.0) 或 0(1) 统一到 Int
            buf.push_back(new Instruction(tmp, {"0", Type::IntLiteral}, tmp2, Operator::eq));   // 条件一是否False
            buf.push_back(new Instruction(
                tmp2, 
                {}, 
                {std::to_string(landexp_blk.size() + 2), Type::IntLiteral}, 
                Operator::_goto     // 结果已存储至tmp
            ));

            buf.insert(buf.end(), landexp_blk.begin(), landexp_blk.end());

            buf.push_back(new Instruction(
                op1, 
                op2, 
                tmp,
                Operator::_and
            ));
            
            root->v = tmp.name;
            root->t = tmp.type;
        }
    }
}

void frontend::Analyzer::analyzeEqExp(EqExp* root, vector<Instruction *> &buf){
    log(root);
    // EqExp -> RelExp { ( '==' | '!=' ) RelExp }
    GET_CHILD_PTR(relexp, RelExp, 0)
    vector<Instruction *> relexp_blk;
    analyzeRelExp(relexp, relexp_blk);

    // 全常量情况
    bool cpt_ok = ((relexp->t == Type::IntLiteral) || (relexp->t == Type::FloatLiteral));   // 先确定首常量

    if(cpt_ok){
        vector<Instruction *> relexp2_blk;
        for(int i = 2; i < int(root->children.size()); i+=2){
            relexp2_blk.clear();

            TokenType op = static_cast<Term*> (root->children[i-1])->token.type;
            GET_CHILD_PTR(relexp2, RelExp, i)
            analyzeRelExp(relexp2, relexp2_blk);
            
            if(!(relexp2->t == Type::IntLiteral || relexp2->t == Type::FloatLiteral)){
                cpt_ok = false;
                break;
            }

            bool result;
            if (relexp->t == Type::IntLiteral && relexp2->t == Type::IntLiteral) {
                int val1 = std::stoi(relexp->v);
                int val2 = std::stoi(relexp2->v);
                result = (op == TokenType::EQL) ? (val1 == val2) : (val1 != val2);
            } 
            else {
                float val1 = std::stof(relexp->v);
                float val2 = std::stof(relexp2->v);
                result = (op == TokenType::EQL) ? (val1 == val2) : (val1 != val2);
            }
            relexp->v = std::to_string(result);
            relexp->t = Type::IntLiteral;
        }
    }

    if (!cpt_ok) {
        buf.insert(buf.end(),relexp_blk.begin(),relexp_blk.end());

        for (int i = 2; i < int(root->children.size()); i += 2) {

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(relexp2, RelExp, i)
            analyzeRelExp(relexp2, buf);

            Operand tmp = Operand(GET_TMP(), Type::Int);
            Operator ir_op = (op == TokenType::EQL) ? Operator::eq : Operator::neq;
            buf.push_back(new Instruction(
                {relexp->v, relexp->t},
                {relexp2->v, relexp2->t},
                tmp,
                ir_op
            ));

            relexp->v = tmp.name;
            relexp->t = tmp.type;
        }
    }

    COPY_EXP_NODE(relexp, root);
}

void frontend::Analyzer::analyzeRelExp(RelExp* root, vector<Instruction*>& buf) {
    log(root);
    // RelExp -> AddExp { ('<' | '>' | '<=' | '>=') AddExp }
    GET_CHILD_PTR(addexp, AddExp, 0)
    vector<Instruction*> addexp_blk;
    analyzeAddExp(addexp, addexp_blk);

    // 首常量
    bool cpt_ok = (addexp->t == Type::IntLiteral || addexp->t == Type::FloatLiteral);
    vector<Instruction*> addexp2_blk;

    if (cpt_ok) {
        for (int i = 2; i < int(root->children.size()); i += 2) {
            addexp2_blk.clear();

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(addexp2, AddExp, i)
            analyzeAddExp(addexp2, addexp2_blk);

            if (!(addexp2->t == Type::IntLiteral || addexp2->t == Type::FloatLiteral)) {
                cpt_ok = false;
                break;
            }

            bool result;
            if (addexp->t == Type::IntLiteral && addexp2->t == Type::IntLiteral) {
                int val1 = std::stoi(addexp->v);
                int val2 = std::stoi(addexp2->v);
                switch (op) {
                    case TokenType::LSS: result = (val1 < val2); break;
                    case TokenType::GTR: result = (val1 > val2); break;
                    case TokenType::LEQ: result = (val1 <= val2); break;
                    case TokenType::GEQ: result = (val1 >= val2); break;
                    default: assert(0 && "Invalid relexp op");
                }
            } 
            else {
                float val1 = std::stof(addexp->v);
                float val2 = std::stof(addexp2->v);
                switch (op) {
                    case TokenType::LSS: result = (val1 < val2); break;
                    case TokenType::GTR: result = (val1 > val2); break;
                    case TokenType::LEQ: result = (val1 <= val2); break;
                    case TokenType::GEQ: result = (val1 >= val2); break;
                    default: assert(0 && "Invalid relexp op");
                }
            }

            addexp->v = std::to_string(result);
            addexp->t = Type::IntLiteral; 
        }
    }

    // 非常量情况
    if (!cpt_ok) {
        buf.insert(buf.end(),addexp_blk.begin(),addexp_blk.end());
        
        for (int i = 2; i < int(root->children.size()); i += 2) {

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(addexp2, AddExp, i)
            analyzeAddExp(addexp2, buf);

            Operator ir_op;
            bool needs_float = (addexp->t == Type::Float || addexp->t == Type::FloatLiteral ||
                                addexp2->t == Type::Float || addexp2->t == Type::FloatLiteral);
            Operand tmp = Operand(GET_TMP(), needs_float? Type::Float : Type::Int);
            switch (op) {
                case TokenType::LSS: ir_op = needs_float ? Operator::flss : Operator::lss; break;
                case TokenType::GTR: ir_op = needs_float ? Operator::fgtr : Operator::gtr; break;
                case TokenType::LEQ: ir_op = needs_float ? Operator::fleq : Operator::leq; break;
                case TokenType::GEQ: ir_op = needs_float ? Operator::fgeq : Operator::geq; break;
                default: assert(0 && "Invalid relexp op");
            }
            if(needs_float && (addexp->t==Type::Int || addexp->t==Type::IntLiteral)){
                addexp->t = (addexp->t==Type::Int)? Type::Float : Type::FloatLiteral;
            }
            if(needs_float && (addexp2->t==Type::Int || addexp2->t==Type::IntLiteral)){
                addexp2->t = (addexp2->t==Type::Int)? Type::Float : Type::FloatLiteral;
            }
            buf.push_back(new Instruction(
                {addexp->v, addexp->t},
                {addexp2->v, addexp2->t},
                tmp,
                ir_op
            ));

            addexp->v = tmp.name;
            addexp->t = tmp.type;
        }
    }

    // 最终结果存储在 addexp 中
    COPY_EXP_NODE(addexp, root);
}

void frontend::Analyzer::analyzeNumber(Number *root, vector<Instruction *> &buf){
    log(root);
    // Number -> IntConst | floatConst
    auto term = static_cast<Term*>(root->children[0]);
    TokenType type = term->token.type;
    const string val = term->token.value;

    if (type == TokenType::INTLTR){ // IntConst
        root->t = Type::IntLiteral; 

        if (val.length() >= 3 && val[0] == '0' && (val[1] == 'x' || val[1] == 'X')) // 0x...
            root->v = std::to_string(std::stoi(val, nullptr, 16));
        else if (val.length() >= 3 && val[0] == '0' && (val[1] == 'b' || val[1] == 'B')) // 0b...
            root->v = std::to_string(std::stoi(val.substr(2), nullptr, 2));
        else if (val.length() >= 2 && val[0] == '0')    // 0...
            root->v = std::to_string(std::stoi(val, nullptr, 8));
        else 
            root->v = val;
    }
    else{ // FloatConst
        assert(type == TokenType::FLOATLTR && "invalid analyze number");
        root->t = Type::FloatLiteral;
        root->v = val;
    }
}

void frontend::Analyzer::analyzeAddExp(AddExp* root, vector<Instruction*>& buf) {
    log(root);
    // AddExp -> MulExp { ('+' | '-') MulExp }
    GET_CHILD_PTR(mulexp, MulExp, 0)
    vector<Instruction*> mulexp_blk;
    analyzeMulExp(mulexp, mulexp_blk);

    bool cpt_ok = (mulexp->t == Type::IntLiteral || mulexp->t == Type::FloatLiteral);
    vector<Instruction*> mulexp2_blk;

    if (cpt_ok) {
        for (int i = 2; i < int(root->children.size()); i += 2) {
            mulexp2_blk.clear();

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(mulexp2, MulExp, i)
            analyzeMulExp(mulexp2, mulexp2_blk);

            if (!(mulexp2->t == Type::IntLiteral || mulexp2->t == Type::FloatLiteral)) {
                cpt_ok = false;
                break;
            }

            string result;
            if (mulexp->t == Type::IntLiteral && mulexp2->t == Type::IntLiteral) {
                int val1 = std::stoi(mulexp->v);
                int val2 = std::stoi(mulexp2->v);
                result = std::to_string(op == TokenType::PLUS ? val1 + val2 : val1 - val2);
            } 
            else {
                // 浮点或混合
                float val1 = std::stof(mulexp->v);
                float val2 = std::stof(mulexp2->v);
                result = std::to_string(op == TokenType::PLUS ? val1 + val2 : val1 - val2);
                
                if (mulexp->t == Type::FloatLiteral || mulexp2->t == Type::FloatLiteral) 
                    mulexp->t = Type::FloatLiteral;
            }

            mulexp->v = result;
        }
    }

    // 非常量
    if (!cpt_ok) {
        buf.insert(buf.end(), mulexp_blk.begin(), mulexp_blk.end());

        for (int i = 2; i < int(root->children.size()); i += 2) {
            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(mulexp2, MulExp, i)
            analyzeMulExp(mulexp2, buf);

            bool needs_float = (mulexp->t == Type::Float || mulexp->t == Type::FloatLiteral ||
                                mulexp2->t == Type::Float || mulexp2->t == Type::FloatLiteral);

            Operand tmp = Operand(GET_TMP(), needs_float ? Type::Float : Type::Int);

            Operator ir_op = (op == TokenType::PLUS) ? 
                                needs_float? Operator::fadd : Operator::add  
                                 : 
                                needs_float? Operator::fsub : Operator::sub;

            buf.push_back(new Instruction(
                {mulexp->v, mulexp->t},
                {mulexp2->v, mulexp2->t},
                tmp,
                ir_op
            ));

            mulexp->v = tmp.name;
            mulexp->t = tmp.type;
        }
    }

    COPY_EXP_NODE(mulexp, root);
}

void frontend::Analyzer::analyzeMulExp(MulExp* root, vector<Instruction*>& buf) {
    log(root);
    // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }

    GET_CHILD_PTR(unaryexp, UnaryExp, 0)
    vector<Instruction*> unaryexp_blk;
    analyzeUnaryExp(unaryexp, unaryexp_blk);

    bool cpt_ok = (unaryexp->t == Type::IntLiteral || unaryexp->t == Type::FloatLiteral);
    vector<Instruction*> unaryexp2_blk;

    if (cpt_ok) {
        for (int i = 2; i < int(root->children.size()); i += 2) {
            unaryexp2_blk.clear();

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(unaryexp2, UnaryExp, i)
            analyzeUnaryExp(unaryexp2, unaryexp2_blk);

            if (!(unaryexp2->t == Type::IntLiteral || unaryexp2->t == Type::FloatLiteral)) {
                cpt_ok = false;
                break;
            }

            string result;
            if (unaryexp->t == Type::IntLiteral && unaryexp2->t == Type::IntLiteral) {
                int val1 = std::stoi(unaryexp->v);
                int val2 = std::stoi(unaryexp2->v);
                switch (op) {
                    case TokenType::MULT: result = std::to_string(val1 * val2); break;
                    case TokenType::DIV:  
                        if (val2 == 0) throw std::runtime_error("Division by zero");
                        result = std::to_string(val1 / val2); 
                        break;
                    case TokenType::MOD:  
                        if (val2 == 0) throw std::runtime_error("Modulo by zero");
                        result = std::to_string(val1 % val2); 
                        break;
                    default: assert(false);
                }
            } 
            else {
                // 处理浮点数或混合类型（% 不适用于浮点）
                if (op == TokenType::MOD) 
                    throw std::runtime_error("Modulo operation on float");
                
                float val1 = std::stof(unaryexp->v);
                float val2 = std::stof(unaryexp2->v);
                if (op == TokenType::DIV && val2 == 0.0) 
                    throw std::runtime_error("Division by zero");
                
                result = std::to_string(op == TokenType::MULT ? val1 * val2 : val1 / val2);
                
                if (unaryexp->t == Type::FloatLiteral || unaryexp2->t == Type::FloatLiteral) {
                    unaryexp->t = Type::FloatLiteral;
                }
            }
            unaryexp->v = result;
        }
    }

    if (!cpt_ok) {
        buf.insert(buf.end(), unaryexp_blk.begin(),unaryexp_blk.end());

        for (int i = 2; i < int(root->children.size()); i += 2) {
            unaryexp2_blk.clear();

            TokenType op = static_cast<Term*>(root->children[i-1])->token.type;
            GET_CHILD_PTR(unaryexp2, UnaryExp, i)
            analyzeUnaryExp(unaryexp2, buf);

            bool needs_float = (unaryexp->t == Type::Float || unaryexp->t == Type::FloatLiteral ||
                                unaryexp2->t == Type::Float || unaryexp2->t == Type::FloatLiteral);
            
            Operand tmp = Operand(GET_TMP(), needs_float? Type::Float : Type::Int);    

            Operator ir_op;
            switch (op) {
                case TokenType::MULT: 
                    ir_op = needs_float ? Operator::fmul : Operator::mul; 
                    break;
                case TokenType::DIV:  
                    ir_op = needs_float ? Operator::fdiv : Operator::div; 
                    break;
                case TokenType::MOD:  
                    if (needs_float) 
                        throw std::runtime_error("Modulo operation on float");
                    ir_op = Operator::mod; 
                    break;
                default: assert(false);
            }

            Operand res1 = {GET_TMP(), tmp.type};
            Operand res2 = {GET_TMP(), tmp.type};

            if(needs_float){
                if(unaryexp->t == Type::Int){
                    buf.push_back(new Instruction(
                        {unaryexp->v, unaryexp->t},
                        {},
                        res1,
                        Operator::cvt_i2f
                    ));
                }
                else if(unaryexp->t == Type::IntLiteral){
                    res1.name = std::to_string( float(std::stoi(unaryexp->v)) );
                    res1.type = Type::FloatLiteral;
                }else{
                    res1 = {unaryexp->v ,unaryexp->t};   
                }
                
                if(unaryexp2->t == Type::Int){
                    buf.push_back(new Instruction(
                        {unaryexp2->v, unaryexp2->t},
                        {},
                        res2,
                        Operator::cvt_i2f
                    ));
                }
                else if(unaryexp2->t == Type::IntLiteral){
                    res2.name = std::to_string( float(std::stoi(unaryexp2->v)) );
                    res2.type = Type::FloatLiteral;
                }else{
                    res2 = {unaryexp2->v, unaryexp2->t};
                }
            }else{
                res1 = {unaryexp->v ,unaryexp->t};  
                res2 = {unaryexp2->v, unaryexp2->t};
            }

            buf.push_back(new Instruction(
                res1,
                res2,
                tmp,
                ir_op
            ));

            unaryexp->v = tmp.name;
            unaryexp->t = tmp.type;
        }
    }

    COPY_EXP_NODE(unaryexp, root);
}

void frontend::Analyzer::analyzeUnaryExp(UnaryExp* root, vector<Instruction*>& buf){
    log(root);
    // UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
    if(root->children.size() == 1){
        // PrimaryExp
        GET_CHILD_PTR(priexp, PrimaryExp, 0)
        analyzePrimaryExp(priexp, buf);
        COPY_EXP_NODE(priexp, root)
    } 
    else if (root->children[0]->type==NodeType::UNARYOP){

        // UnaryOp UnaryExp
        UnaryOp* uop = dynamic_cast<UnaryOp*>(root->children[0]);
        // std::ofstream output_file = std::ofstream("./xxx.out");
        // Json::Value json_output;
        // Json::StyledWriter writer;
        // uop->get_json_output(json_output);
        // output_file << writer.write(json_output);
        // output_file.close(); 

        GET_CHILD_PTR(unaexp, UnaryExp, 1)
        analyzeUnaryExp(unaexp, buf); 
        
        switch (uop->op){
            case TokenType::PLUS:{
                COPY_EXP_NODE(unaexp, root)
                break;
            }
            
            case TokenType::MINU:{
                // 取相反数
                // 立即数
                
                if(unaexp->t == Type::IntLiteral || unaexp->t == Type::FloatLiteral){
                    if(unaexp->t == Type::IntLiteral) {
                        int val = -std::stoi(unaexp->v);
                        unaexp->v = std::to_string(val); // 确保整型格式
                    } else {
                        float val = -std::stof(unaexp->v);
                        unaexp->v = std::to_string(val); // 浮点格式
                    }

                    COPY_EXP_NODE(unaexp, root)
                } else {
                    Operand op1({unaexp->v, unaexp->t});
                    Operand res({GET_TMP(), unaexp->t});
                    buf.push_back(new Instruction(
                        {"0", unaexp->t==Type::Int? Type::IntLiteral:Type::FloatLiteral}, 
                        op1, 
                        res, 
                        unaexp->t==Type::Int? Operator::sub : Operator::fsub 
                    ));
                    root->t = res.type;
                    root->v = res.name;
                }
                break;
            }
            
            case TokenType::NOT:{
                // 逻辑取反
                // 立即数
                if(unaexp->t == Type::IntLiteral || unaexp->t == Type::FloatLiteral){
                    unaexp->v = std::to_string(
                        (unaexp->t == Type::IntLiteral) ? !std::stoi(unaexp->v) : !std::stof(unaexp->v)
                    );
                    
                    COPY_EXP_NODE(unaexp, root)
                } else {
                    Operand op1({unaexp->v, unaexp->t});
                    Operand res({GET_TMP(), unaexp->t});
                    buf.push_back(new Instruction(
                        op1,
                        {}, 
                        res, 
                        Operator::_not
                    ));
                    root->t = res.type;
                    root->v = res.name;
                }
                break;
            }

            default:{
                // std::cout << frontend::toString(uop->op) << std::endl;
                // std::cout << frontend::toString(root->children[0]->type) << std::endl;
                // std::cout << unaexp->v << std::endl;
                assert(0 && "Invalid UnaryOp");
            }
        }
    }
    else {
        // Ident '(' [FuncRParams] ')'
        string f_n = static_cast<Term*> (root->children[0])->token.value;
        
        auto ret_type = symbol_table.functions[f_n]->returnType;
        auto par_list = symbol_table.functions[f_n]->ParameterList;

        vector<Operand> params;
        if(root->children.size() > 3){
            GET_CHILD_PTR(funcrparams, FuncRParams, 2)
            analyzeFuncRParams(funcrparams, buf, par_list, params); // par_list用于参数类型检查
        }

        if (ret_type == Type::null){
            buf.push_back(new ir::CallInst(f_n, params, {}));
            root->t = Type::null;
        }

        else{
            Operand ret = Operand({GET_TMP(), ret_type});
            buf.push_back(new ir::CallInst(f_n, params, ret));
            root->v = ret.name;
            root->t = ret.type;
        }
    }
}

void frontend::Analyzer::analyzeFuncRParams(FuncRParams* root, 
                                            vector<Instruction*>&buf, 
                                            std::vector<Operand>& par_list, 
                                            std::vector<Operand>& params){
    log(root);
    // par_list 用于校验
    // FuncRParams -> Exp { ',' Exp }
    int para_n = 0;
    for (int i = 0; i < int(root->children.size()); i += 2){
        GET_CHILD_PTR(exp, Exp, i)
        analyzeExp(exp, buf);

        Operand true_p = par_list[para_n++];        // 要求参数
        Operand cur_p = Operand(exp->v, exp->t);    // 提供参数

        if (true_p.type == Type::Int){
            if (cur_p.type == Type::FloatLiteral){
                cur_p.name = std::to_string(int(std::stof(cur_p.name)));
                cur_p.type = Type::IntLiteral;
            }
            else if(cur_p.type == Type::Float){
                Operand tmp = Operand(GET_TMP(), Type::Int);
                buf.push_back(new Instruction(cur_p, {}, tmp, Operator::cvt_f2i));  // float->int
                cur_p = tmp;
            }
            else 
                assert((cur_p.type == Type::Int || cur_p.type == Type::IntLiteral) && "Invalid Param Type");

            params.push_back(cur_p);
        }
        else if (true_p.type == Type::Float){
            if (cur_p.type == Type::IntLiteral){
                cur_p.name = std::to_string(float(std::stoi(cur_p.name)));
                cur_p.type = Type::FloatLiteral;
            }
            else if(cur_p.type == Type::Int){
                Operand tmp = Operand(GET_TMP(), Type::Float);
                buf.push_back(new Instruction(cur_p, {}, tmp, Operator::cvt_i2f));  // int->float
                cur_p = tmp;
            }
            else
                assert((cur_p.type == Type::Float || cur_p.type == Type::FloatLiteral) && "Invalid Param Type");
            params.push_back(cur_p);
        }
        else if (true_p.type == Type::IntPtr || true_p.type == Type::FloatPtr){
            assert(cur_p.type == true_p.type && "Pointer type parameter mismatch");
            params.push_back(cur_p);
        }
        else{
            // std::cout << "!!! " <<  static_cast<int>(true_p.type) << std::endl;
            // std::cout << "!!! " <<  static_cast<int>(par_list[0].type) << std::endl;
            // std::cout << "!!! " <<  static_cast<int>(par_list[1].type) << std::endl;
            assert(0 && "Invalid params type");
        }
    }
}

void frontend::Analyzer::analyzePrimaryExp(PrimaryExp* root, vector<Instruction*>& buf){
    log(root);
    // PrimaryExp -> '(' Exp ')' | LVal | Number
    if(IS_T(0, NUMBER)){
        GET_CHILD_PTR(num, Number, 0)
        analyzeNumber(num, buf);
        COPY_EXP_NODE(num, root)
    }
    else if(IS_T(0, LVAL)){
        GET_CHILD_PTR(lval, LVal, 0)
        Operand ofst({GET_TMP(), Type::Int});

        analyzeLVal(lval, buf, ofst);

        // 常量
        if(lval->t == Type::IntLiteral || lval->t == Type::FloatLiteral){
            COPY_EXP_NODE(lval, root)
        }
        
        // 变量
        else if(!(lval->t == Type::IntPtr || lval->t == Type::FloatPtr)){
            COPY_EXP_NODE(lval, root)
        }

        // 常量/变量 数组
        else {
            Operand base = Operand(lval->v, lval->t);
            // -1: 数组地址  -2: 数组元素
            if(lval->i == -1){
                // 数组指针类型
                assert(0 && "TODO Imcompleted Array Access");
                // auto res = Operand(GET_TMP(), lval->t);
                // buf.push_back(new Instruction(
                //     base, 
                //     {std::to_string(lval->i), Type::Int}, 
                //     res, 
                //     Operator::getptr
                // ));
                // root->v = res.name;
                // root->t = res.type;
            }

            else if(lval->i == -2){
                // 数组元素
                auto res = Operand(GET_TMP(), lval->t==Type::IntPtr? Type::Int : Type::Float);
                buf.push_back(new Instruction(
                    base, 
                    ofst, 
                    res, 
                    Operator::load
                ));

                root->v = res.name;
                root->t = res.type;
            }
            else{
                assert(lval->i == -3 && "Invalid Array Element Access");
                // 数组地址
                auto res = Operand(GET_TMP(), lval->t);
                buf.push_back(new Instruction(
                    base, 
                    ofst, 
                    res, 
                    Operator::getptr
                ));

                root->v = res.name;
                root->t = res.type;
            }
        }
    }
    else{
        assert(IS_T(0, TERMINAL) && "Invalid primary exp");
        GET_CHILD_PTR(exp, Exp, 1)
        analyzeExp(exp, buf);
        COPY_EXP_NODE(exp, root)
    }
}

