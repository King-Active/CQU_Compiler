#include <map>
#include <cassert>
#include <string>
#include <iostream>
#include <vector>
#include <set>
#include <queue>
#include <cctype>
#include <sstream>

#define TODO assert(0 && "TODO")
// #define DEBUG_DFA
// #define DEBUG_PARSER

// enumerate for Status
enum class State {
    Empty,              // space, \n, \r ...
    IntLiteral,         // int literal, like '1' '01900', '0xAB', '0b11001'
    op                  // operators and '(', ')'
};
std::string toString(State s) {
    switch (s) {
    case State::Empty: return "Empty";
    case State::IntLiteral: return "IntLiteral";
    case State::op: return "op";
    default:
        assert(0 && "invalid State");
    }
    return "";
}

// enumerate for Token type
enum class TokenType{
    INTLTR,        // int literal
    PLUS,        // +
    MINU,        // -
    MULT,        // *
    DIV,        // /
    LPARENT,        // (
    RPARENT,        // )
};
std::string toString(TokenType type) {
    switch (type) {
    case TokenType::INTLTR: return "INTLTR";
    case TokenType::PLUS: return "PLUS";
    case TokenType::MINU: return "MINU";
    case TokenType::MULT: return "MULT";
    case TokenType::DIV: return "DIV";
    case TokenType::LPARENT: return "LPARENT";
    case TokenType::RPARENT: return "RPARENT";
    default:
        assert(0 && "invalid token type");
        break;
    }
    return "";
}

// definition of Token
struct Token {
    TokenType type;
    std::string value;
};

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
     * @brief take a char as input, change state to next state, and output a Token if necessary
     * @param[in] input: the input character
     * @param[out] buf: the output Token buffer
     * @return  return true if a Token is produced, the buf is valid then
     */
    bool next(char input, Token& buf);

    /**
     * @brief reset the DFA state to begin
     */
    void reset();

private:
    State cur_state;    // record current state of the DFA
    std::string cur_str;    // record input characters
};


DFA::DFA(): cur_state(State::Empty), cur_str() {}

DFA::~DFA() {}

// helper function, you are not require to implement these, but they may be helpful
bool isoperator(char c) {
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')';
}

TokenType get_op_type(std::string s) {
    if (s == "+") return TokenType::PLUS;
    if (s == "-") return TokenType::MINU;
    if (s == "*") return TokenType::MULT;
    if (s == "/") return TokenType::DIV;
    if (s == "(") return TokenType::LPARENT;
    if (s == ")") return TokenType::RPARENT;
    assert(0 && "invalid operator");
    return TokenType::PLUS;
}

bool DFA::next(char input, Token& buf) {
    if (isspace(input)) {
        if (cur_state == State::IntLiteral) {
            buf.type = TokenType::INTLTR;
            buf.value = cur_str;
            reset();
            cur_state = State::Empty;
            return true;
        } else if (cur_state == State::op) {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            reset();
            cur_state = State::Empty;
            return true;
        }
        reset();
        return false;
    }

    if (isoperator(input)) {
        if (cur_state == State::IntLiteral) {
            buf.type = TokenType::INTLTR;
            buf.value = cur_str;
            reset();
            cur_state = State::op;
            cur_str += input;
            return true;
        } else if (cur_state == State::op) {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            reset();
            cur_state = State::op;
            cur_str += input;
            return true;
        } else {
            // current state is Empty
            cur_state = State::op;
            cur_str += input;
            return false;
        }
    } else {
        // digit or part of number (like 0x, 0b)
        if (cur_state == State::op) {
            buf.type = get_op_type(cur_str);
            buf.value = cur_str;
            reset();
            cur_state = State::IntLiteral;
            cur_str += input;
            return true;
        } else {
            cur_state = State::IntLiteral;
            cur_str += input;
            return false;
        }
    }
}

void DFA::reset() {
    cur_state = State::Empty;
    cur_str = "";
}

// hw2
enum class NodeType {
    TERMINAL,       // terminal lexical unit
    EXP,
    NUMBER,
    PRIMARYEXP,
    UNARYEXP,
    UNARYOP,
    MULEXP,
    ADDEXP,
    NONE
};
std::string toString(NodeType nt) {
    switch (nt) {
    case NodeType::TERMINAL: return "Terminal";
    case NodeType::EXP: return "Exp";
    case NodeType::NUMBER: return "Number";
    case NodeType::PRIMARYEXP: return "PrimaryExp";
    case NodeType::UNARYEXP: return "UnaryExp";
    case NodeType::UNARYOP: return "UnaryOp";
    case NodeType::MULEXP: return "MulExp";
    case NodeType::ADDEXP: return "AddExp";
    case NodeType::NONE: return "NONE";
    default:
        assert(0 && "invalid node type");
        break;
    }
    return "";
}

// tree node basic class
struct AstNode{
    int value;
    NodeType type;  // the node type
    AstNode* parent;    // the parent node
    std::vector<AstNode*> children;     // children of node

    /**
     * @brief constructor
     */
    AstNode(NodeType t = NodeType::NONE, AstNode* p = nullptr): type(t), parent(p), value(0) {}

    /**
     * @brief destructor
     */
    virtual ~AstNode() {
        for(auto child: children) {
            delete child;
        }
    }

    // rejcet copy and assignment
    AstNode(const AstNode&) = delete;
    AstNode& operator=(const AstNode&) = delete;
};

// definition of Parser
// a parser should take a token stream as input, then parsing it, output a AST
struct Parser {
    uint32_t index; // current token index
    const std::vector<Token>& token_stream;

    /**
     * @brief constructor
     * @param tokens: the input token_stream
     */
    Parser(const std::vector<Token>& tokens): index(0), token_stream(tokens) {}

    /**
     * @brief destructor
     */
    ~Parser() {}

    /**
     * @brief creat the abstract syntax tree
     * @return the root of abstract syntax tree
     */
    AstNode* get_abstract_syntax_tree() {
        AstNode* root = parse_Exp();
        return root;
    }

    // u can define member funcition of Parser here
    void log(AstNode* node){
#ifdef DEBUG_PARSER
        std::cout << "in parse" << toString(node->type) << ", cur_token_type::" << toString(token_stream[index].type) << ", token_val::" << token_stream[index].value << '\n';
#endif
    }

    AstNode* parse_Exp() {
        AstNode* node = new AstNode(NodeType::EXP);
        AstNode* child = parse_AddExp();
        node->children.push_back(child);
        node->value = child->value;
        return node;
    }

    AstNode* parse_AddExp() {
        AstNode* node = new AstNode(NodeType::ADDEXP);
        AstNode* left = parse_MulExp();
        node->children.push_back(left);
        node->value = left->value;

        while (index < token_stream.size()) {
            TokenType op = token_stream[index].type;
            if (op == TokenType::PLUS || op == TokenType::MINU) {
                AstNode* op_node = new AstNode(NodeType::TERMINAL);
                op_node->type = NodeType::TERMINAL;
                op_node->value = 0; // placeholder
                node->children.push_back(op_node);
                index++;

                AstNode* right = parse_MulExp();
                node->children.push_back(right);

                if (op == TokenType::PLUS) {
                    node->value += right->value;
                } else {
                    node->value -= right->value;
                }
            } else {
                break;
            }
        }
        return node;
    }

    AstNode* parse_MulExp() {
        AstNode* node = new AstNode(NodeType::MULEXP);
        AstNode* left = parse_UnaryExp();
        node->children.push_back(left);
        node->value = left->value;

        while (index < token_stream.size()) {
            TokenType op = token_stream[index].type;
            if (op == TokenType::MULT || op == TokenType::DIV) {
                AstNode* op_node = new AstNode(NodeType::TERMINAL);
                op_node->type = NodeType::TERMINAL;
                op_node->value = 0; // placeholder
                node->children.push_back(op_node);
                index++;

                AstNode* right = parse_UnaryExp();
                node->children.push_back(right);

                if (op == TokenType::MULT) {
                    node->value *= right->value;
                } else {
                    node->value /= right->value;
                }
            } else {
                break;
            }
        }
        return node;
    }

    AstNode* parse_UnaryExp() {
        AstNode* node = new AstNode(NodeType::UNARYEXP);
        if (index < token_stream.size()) {
            TokenType op = token_stream[index].type;
            if (op == TokenType::PLUS || op == TokenType::MINU) {
                AstNode* op_node = new AstNode(NodeType::UNARYOP);
                op_node->type = NodeType::UNARYOP;
                op_node->value = (op == TokenType::MINU) ? -1 : 1;
                node->children.push_back(op_node);
                index++;

                AstNode* child = parse_UnaryExp();
                node->children.push_back(child);
                node->value = op_node->value * child->value;
                return node;
            }
        }
        AstNode* child = parse_PrimaryExp();
        node->children.push_back(child);
        node->value = child->value;
        return node;
    }

    AstNode* parse_PrimaryExp() {
        AstNode* node = new AstNode(NodeType::PRIMARYEXP);
        if (index < token_stream.size()) {
            TokenType type = token_stream[index].type;
            if (type == TokenType::LPARENT) {
                index++; // skip '('
                AstNode* child = parse_Exp();
                node->children.push_back(child);
                node->value = child->value;
                if (index >= token_stream.size() || token_stream[index].type != TokenType::RPARENT) {
                    assert(0 && "missing right parenthesis");
                }
                index++; // skip ')'
                return node;
            }
        }
        AstNode* child = parse_Number();
        node->children.push_back(child);
        node->value = child->value;
        return node;
    }

    AstNode* parse_Number() {
        AstNode* node = new AstNode(NodeType::NUMBER);
        if (index >= token_stream.size() || token_stream[index].type != TokenType::INTLTR) {
            assert(0 && "expected number");
        }
        std::string num_str = token_stream[index].value;
        int num = 0;

        // Handle different bases
        if (num_str.size() > 1 && num_str[0] == '0') {
            if (num_str[1] == 'x' || num_str[1] == 'X') {
                // Hexadecimal
                num = std::stoi(num_str.substr(2), nullptr, 16);
            } else if (num_str[1] == 'b' || num_str[1] == 'B') {
                // Binary
                num = std::stoi(num_str.substr(2), nullptr, 2);
            } else {
                // Octal
                num = std::stoi(num_str.substr(1), nullptr, 8);
            }
        } else {
            // Decimal
            num = std::stoi(num_str);
        }

        node->value = num;
        index++;
        return node;
    }
};

int main(){
    std::string stdin_str;
    std::getline(std::cin, stdin_str);
    stdin_str += "\n";
    DFA dfa;
    Token tk;
    std::vector<Token> tokens;
    for (size_t i = 0; i < stdin_str.size(); i++) {
        if(dfa.next(stdin_str[i], tk)){
            tokens.push_back(tk);
        }
    }

    // hw2
    Parser parser(tokens);
    auto root = parser.get_abstract_syntax_tree();
    std::cout << root->value;

    delete root;
    return 0;
}
