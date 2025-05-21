#include"../../include/front/syntax.h"
#include"../../include/front/token.h"
#include<iostream>
#include<cassert>

using frontend::Parser;


// #define DEBUG_PARSER
#define TODO assert(0 && "todo")

#define CUR_TOKEN_IS(tk_type) \
            (index < token_stream.size() && token_stream[index].type == TokenType::tk_type)

#define PARSE_TOKEN(tk_type) parseTerm(root, TokenType::tk_type)

#define PARSE(name, type) \
            [&]() -> bool { \
                auto name = new type(root); \
                if (parse##type(name)) { \
                    root->children.push_back(name); \
                    return true; \
                } else { \
                    delete name; \
                    return false; \
                } \
            }()


// 构造函数
Parser::Parser(const std::vector<Token>& tokens): index(0), token_stream(tokens) {}

Parser::~Parser() {}

// 对外接口 
frontend::CompUnit* Parser::get_abstract_syntax_tree(){

    CompUnit* root = new CompUnit();
    bool is_compunit = parseCompUnit(root);
    if (!is_compunit) 
        assert (0 && "parseCompUnit failed");

    return root;

}


bool Parser::parseCompUnit(AstNode* root){
    // CompUnit -> (Decl | FuncDef) [CompUnit]
    log(root);
    auto cur_idx = index;

    // 创建 Decl 类型的子结点，以root为父节点
    if (PARSE(decl, Decl)){
        if (index < token_stream.size()){
            return PARSE(compunit, CompUnit);  // 不调用 PARSE, 否则将创建新的 CompUnit（嵌套结构）
        }
        return true;
    }

    index = cur_idx;

    if(PARSE(funcdef, FuncDef)){
        if(index < token_stream.size()){
            return PARSE(compunit, CompUnit);
        }
        return true;
    }
    
    return false;
}

bool Parser::parseDecl(AstNode* root){
    // Decl -> ConstDecl | VarDecl
    log(root);

    auto cur_idx = index;
    if(PARSE(constdecl, ConstDecl))
        return true;
    
    index = cur_idx;

    if(PARSE(vardecl, VarDecl))
        return true;

    return false;
}

bool Parser::parseFuncDef(AstNode* root){
    // FuncDef -> FuncType Ident '(' [FuncFParams] ')' Block
    log(root);
    
    if (!PARSE(functype, FuncType)) return false;

    if (!PARSE_TOKEN(IDENFR)) return false;

    if (!PARSE_TOKEN(LPARENT)) return false;

    if (CUR_TOKEN_IS(RPARENT)){
        if(!PARSE_TOKEN(RPARENT)) return false;
    } else {
        if (!PARSE(funcparams, FuncFParams)) return false;
        if (!PARSE_TOKEN(RPARENT)) return false;
    }

    if(!PARSE(block, Block)) return false;
    
    return true;
}

bool Parser::parseConstDecl(AstNode* root){
    // ConstDecl -> 'const' BType ConstDef { ',' ConstDef } ';'
    log(root);
    
    if(!PARSE_TOKEN(CONSTTK)) return false;

    if(!PARSE(btype, BType)) return false;

    if(!PARSE(constdef, ConstDef)) return false;
    
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA); // 一定保证当前是 ','
        if (!PARSE(constdef, ConstDef)) return false;
    }

    if(CUR_TOKEN_IS(SEMICN)){
        return PARSE_TOKEN(SEMICN);
    }
    return false;

}

bool Parser::parseBType(AstNode* root){
    // BType -> 'int' | 'float'
    log(root);
    
    if(CUR_TOKEN_IS(INTTK))
        return PARSE_TOKEN(INTTK);

    if(CUR_TOKEN_IS(FLOATTK))
        return PARSE_TOKEN(FLOATTK);
    
    return false;
}

bool Parser::parseConstDef(AstNode* root){
    // ConstDef -> Ident { '[' ConstExp ']' } '=' ConstInitVal
    log(root);
    
    if(!PARSE_TOKEN(IDENFR))
        return false;
    while(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        if(!PARSE(consexp, ConstExp))
            return false;
        if(!PARSE_TOKEN(RBRACK))
            return false;
    }
    if(!PARSE_TOKEN(ASSIGN))
        return false;
    if(!PARSE(constinitval, ConstInitVal))
        return false;
    return true;
}

bool Parser::parseConstInitVal(AstNode* root){
    // ConstInitVal -> ConstExp | '{' [ConstInitVal { ',' ConstInitVal }] '}'
    log(root);
    
    if(CUR_TOKEN_IS(LBRACE)){
        PARSE_TOKEN(LBRACE);
        if(CUR_TOKEN_IS(RBRACE))
            return PARSE_TOKEN(RBRACE);
        if(!PARSE(constinitval, ConstInitVal))
            return false;
        while(CUR_TOKEN_IS(COMMA)){
            PARSE_TOKEN(COMMA);
            if(!PARSE(constinitval, ConstInitVal))
                return false;
        }
        return PARSE_TOKEN(RBRACE);
    }

    return PARSE(constExp, ConstExp);
}

bool Parser::parseVarDecl(AstNode* root){
    // VarDecl -> BType VarDef { ',' VarDef } ';'
    log(root);
    
    if(!PARSE(btype,BType))
        return false;
    if(!PARSE(vardef,VarDef))
        return false;

    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        if(!PARSE(vardef,VarDef))
            return false;
    }

    return PARSE_TOKEN(SEMICN);
}

bool Parser::parseVarDef(AstNode* root){
    // VarDef -> Ident { '[' ConstExp ']' } [ '=' InitVal ]
    log(root);
    
    if(!PARSE_TOKEN(IDENFR)) return false;

    while(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        if(!PARSE(consexp, ConstExp)) return false;
        if(!PARSE_TOKEN(RBRACK)) return false;
    }
    if(CUR_TOKEN_IS(ASSIGN)){
        PARSE_TOKEN(ASSIGN);
        if(!PARSE(initval, InitVal)) return false;
    }
    return true;
}

bool Parser::parseInitVal(AstNode* root){
    // InitVal -> Exp | '{' [InitVal { ',' InitVal }] '}'
    log(root);
    
    if(CUR_TOKEN_IS(LBRACE)){
        PARSE_TOKEN(LBRACE);
        if(CUR_TOKEN_IS(RBRACE)) return PARSE_TOKEN(RBRACE);
        if(!PARSE(initval, InitVal)) return false;
        while(CUR_TOKEN_IS(COMMA)){
            PARSE_TOKEN(COMMA);
            if(!PARSE(initval, InitVal)) return false;
        }
        return PARSE_TOKEN(RBRACE);
    }
    return PARSE(exp, Exp);
}

bool Parser::parseFuncType(AstNode* root){
    // FuncType -> 'void' | 'int' | 'float'
    log(root);
    
    if(CUR_TOKEN_IS(VOIDTK)){
        return PARSE_TOKEN(VOIDTK);;
    }
    if(CUR_TOKEN_IS(INTTK)){
        return PARSE_TOKEN(INTTK);
    }
    return PARSE_TOKEN(FLOATTK);
}

bool Parser::parseFuncFParam(AstNode* root){
    // FuncFParam -> BType Ident ['[' ']']
    log(root);
    
    if(!PARSE(btype, BType)) return false;
    if(!PARSE_TOKEN(IDENFR)) return false;
    if(CUR_TOKEN_IS(LBRACK)){
        PARSE_TOKEN(LBRACK);
        if(!PARSE_TOKEN(RBRACK)) return false;
    }
    return true;
}

bool Parser::parseFuncFParams(AstNode* root){
    // FuncFParams -> FuncFParam { ',' FuncFParam }
    log(root);
    
    if(!PARSE(funcfparam, FuncFParam)) return false;
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        if(!PARSE(funcfparam, FuncFParam)) return false;
    }
    return true;
}

bool Parser::parseBlock(AstNode* root){
    // Block -> '{' { BlockItem } '}'
    log(root);
    
    if(!PARSE_TOKEN(LBRACE)) return false;

    if(CUR_TOKEN_IS(RBRACE))
        return PARSE_TOKEN(RBRACE);

    while(!CUR_TOKEN_IS(RBRACE)){
        if(!PARSE(blockitem, BlockItem)) return false;
    }
    return PARSE_TOKEN(RBRACE);
}

bool Parser::parseBlockItem(AstNode* root){
    // BlockItem -> Decl | Stmt
    log(root);
    
    if(PARSE(decl, Decl)) return true;

    if(PARSE(stmt, Stmt)) return true;

    return false;
}

bool Parser::parseStmt(AstNode* root){
    // Stmt -> LVal '=' Exp ';' |                           --> Ident {'[' Exp ']'} 
    //         Block |                                      --> {
    //         'if' '(' Cond ')' Stmt [ 'else' Stmt ] |     --> if
    //         'while' '(' Cond ')' Stmt |                  --> while
    //         'break' ';' |                                --> break
    //         'continue' ';' |                             --> continue
    //         'return' [Exp] ';' |                         --> return
    //         [Exp] ';'                                    --> UnaryOp.. | Ident.. | Number | '(' 
    
    log(root);
    
    // -----------------------------------------
    if(CUR_TOKEN_IS(LBRACE))
        return PARSE(block, Block);
    
    // TODO: 是否需要保存旧值？？？
    if(CUR_TOKEN_IS(IFTK)){
        PARSE_TOKEN(IFTK);
        if(!PARSE_TOKEN(LPARENT)) return false;
        if(!PARSE(cond, Cond)) return false;
        if(!PARSE_TOKEN(RPARENT)) return false;
        if(!PARSE(stmt, Stmt)) return false;
        if(CUR_TOKEN_IS(ELSETK)){
            PARSE_TOKEN(ELSETK);
            return PARSE(stmt, Stmt);
        }
        return true;
    }

    if(CUR_TOKEN_IS(WHILETK)){
        PARSE_TOKEN(WHILETK);
        if(!PARSE_TOKEN(LPARENT)) return false;
        if(!PARSE(cond, Cond)) return false;
        if(!PARSE_TOKEN(RPARENT)) return false;
        return PARSE(stmt, Stmt);
    }

    if(CUR_TOKEN_IS(BREAKTK)){
        PARSE_TOKEN(BREAKTK);
        return PARSE_TOKEN(SEMICN);
    }

    if(CUR_TOKEN_IS(CONTINUETK)){
        PARSE_TOKEN(CONTINUETK);
        return PARSE_TOKEN(SEMICN);
    }

    if(CUR_TOKEN_IS(RETURNTK)){
        PARSE_TOKEN(RETURNTK);
        if(PARSE(exp, Exp)) return PARSE_TOKEN(SEMICN);
        return PARSE_TOKEN(SEMICN);
    }   

    // -----------------------------------------

    if(CUR_TOKEN_IS(SEMICN)) return PARSE_TOKEN(SEMICN);

    /* LVal '=' Exp ';' | Exp ';' */

    if ( CUR_TOKEN_IS(LPARENT) || CUR_TOKEN_IS(INTTK) || CUR_TOKEN_IS(FLOATTK) || 
         CUR_TOKEN_IS(PLUS) || CUR_TOKEN_IS(MINU) || CUR_TOKEN_IS(NOT)) {
        
            if(!PARSE(exp, Exp)) return false;
            return PARSE_TOKEN(SEMICN);

    }
    
    if(CUR_TOKEN_IS(IDENFR) && token_stream[index+1].type == TokenType::LPARENT){
        if(PARSE(exp, Exp)){
            return PARSE_TOKEN(SEMICN);
        } 
        return false;
    }
    
    auto cur_idx = index;
    if(PARSE(lval, LVal)){
        if(CUR_TOKEN_IS(ASSIGN)){
            // LVal '=' Exp ';'
            PARSE_TOKEN(ASSIGN);
            if(!PARSE(exp,Exp)) return false;
            return PARSE_TOKEN(SEMICN);
        }
        else{
            // Exp ';'
            index = cur_idx;
            root->children.pop_back();
            if(!PARSE(exp, Exp)) return false;
            return PARSE_TOKEN(SEMICN);
        }
    }    
    return false;
}

bool Parser::parseExp(AstNode* root){
    // Exp -> AddExp
    log(root);
    
    return PARSE(addExp, AddExp);
}

bool Parser::parseCond(AstNode* root){
    // Cond -> LOrExp
    log(root);
    
    return PARSE(lorExp, LOrExp);
}

bool Parser::parseLVal(AstNode* root){
    // LVal -> Ident { '[' Exp ']' }
    log(root);
    
    if(!PARSE_TOKEN(IDENFR)) return false;
    while(PARSE_TOKEN(LBRACK)){
        if(!PARSE(exp, Exp)) return false;
        if(!PARSE_TOKEN(RBRACK)) return false;
    }

    return true;
}

bool Parser::parseNumber(AstNode* root){
    // Number -> IntConst | FloatConst
    log(root);
    
    if(PARSE_TOKEN(INTLTR)) return true;

    if(PARSE_TOKEN(FLOATLTR)) return true;

    return false;
}

bool Parser::parsePrimaryExp(AstNode* root){
    // PrimaryExp -> '(' Exp ')' | LVal | Number
    log(root);
    
    if(PARSE_TOKEN(LPARENT)){
        if(!PARSE(exp, Exp)) return false;
        return PARSE_TOKEN(RPARENT);
    }

    if(PARSE(lval, LVal)) return true;

    if(PARSE(number, Number)) return true;

    return false;
}

bool Parser::parseUnaryExp(AstNode* root){
    // UnaryExp -> PrimaryExp | Ident '(' [FuncRParams] ')' | UnaryOp UnaryExp
    //                 -> '(' Exp ')' | Ident {'[' Exp ']'} | Number
    log(root);
    

    auto cur_idx = index;

    if(PARSE(unaryop, UnaryOp)){
        return PARSE(unaryexp, UnaryExp);
    }

    index = cur_idx;

    if (CUR_TOKEN_IS(IDENFR) && token_stream[index+1].type == TokenType::LPARENT){
        PARSE_TOKEN(IDENFR);
        PARSE_TOKEN(LPARENT);
        if(CUR_TOKEN_IS(RPARENT)) return PARSE_TOKEN(RPARENT);
        if(!PARSE(funcrparams, FuncRParams)) return false;
        return PARSE_TOKEN(RPARENT);
    }

    return PARSE(primaryexp, PrimaryExp);
}

bool Parser::parseUnaryOp(UnaryOp* root){
    // UnaryOp -> '+' | '-' | '!'
    log(root);

    if(CUR_TOKEN_IS(PLUS)){
        root->op = TokenType::PLUS;
        PARSE_TOKEN(PLUS);
    }
    else if(CUR_TOKEN_IS(MINU)){
        root->op = TokenType::MINU;
        PARSE_TOKEN(MINU);
    }
    else if(CUR_TOKEN_IS(NOT)){
        root->op = TokenType::NOT;
        PARSE_TOKEN(NOT);
    }
    else
        return false;
}

bool Parser::parseFuncRParams(AstNode* root){
    // FuncRParams -> Exp { ',' Exp }
    log(root);
    
    if(!PARSE(exp, Exp)) return false;
    while(CUR_TOKEN_IS(COMMA)){
        PARSE_TOKEN(COMMA);
        if(!PARSE(exp, Exp)) return false;
    }

    return true;
}

bool Parser::parseMulExp(AstNode* root){
    // MulExp -> UnaryExp { ('*' | '/' | '%') UnaryExp }
    log(root);
    
    if(!PARSE(unaryexp, UnaryExp)) return false;
    while(PARSE_TOKEN(MULT) || PARSE_TOKEN(DIV) || PARSE_TOKEN(MOD)){
        if(!PARSE(unaryexp, UnaryExp)) return false;
    }
    return true;
}

bool Parser::parseAddExp(AstNode* root){
    // AddExp -> MulExp { ('+' | '-') MulExp }
    log(root);
    
    if(!PARSE(mulexp, MulExp)) return false;
    while(PARSE_TOKEN(PLUS) || PARSE_TOKEN(MINU)){
        if(!PARSE(mulexp, MulExp)) return false;
    }
    return true;
}

bool Parser::parseRelExp(AstNode* root){
    // RelExp -> AddExp { ( '>' | '<' | '>=' | '<=' ) AddExp }
    log(root);
    
    if(!PARSE(addexp, AddExp)) return false;
    while(PARSE_TOKEN(LSS) || PARSE_TOKEN(GTR) || PARSE_TOKEN(LEQ) || PARSE_TOKEN(GEQ)){
        if(!PARSE(addexp, AddExp)) return false;
    }
    return true;
}
bool Parser::parseEqExp(AstNode* root){
    // EqExp -> RelExp [ ( '==' | '!=' ) RelExp ]
    log(root);
    
    if(!PARSE(relexp, RelExp)) return false;
    if(PARSE_TOKEN(EQL) || PARSE_TOKEN(NEQ)) return PARSE(relexp, RelExp);
    return true;
}

bool Parser::parseLAndExp(AstNode* root){
    // LAndExp -> EqExp [ '&&' LAndExp ]
    log(root);
    
    if(!PARSE(eqexp, EqExp)) return false;
    if(PARSE_TOKEN(AND)){
        return PARSE(landexp, LAndExp);
    }
    return true;
}

bool Parser::parseLOrExp(AstNode* root){
    // LOrExp -> LAndExp [ '||' LOrExp ]
    log(root);
    
    if(!PARSE(landexp, LAndExp)) return false;
    if(PARSE_TOKEN(OR)){
        return PARSE(lorexp, LOrExp);
    }
    return true;
}

bool Parser::parseConstExp(AstNode* root){
    // ConstExp -> AddExp
    log(root);
    
    return PARSE(addexp, AddExp);
}

bool Parser::parseTerm(AstNode* parent, TokenType expected){
    if(token_stream[index].type == expected){
        // std::cout << "Token " << token_stream[index].value << " OK~\n";
        auto node =  new Term({expected, token_stream[index++].value}, parent);  
        parent->children.push_back(node);   // 完成子结点挂载
        node->parent = parent;
        return true;
    }
        return false;
}

void Parser::log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
}