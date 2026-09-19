#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <memory>
#include <iomanip>
#include <format>

struct ASTNode {
    std::string type;
    std::string value;

    std::vector<ASTNode> children;

    std::unique_ptr<ASTNode> left;
    std::unique_ptr<ASTNode> right;
};

class SimpleJSONParser {
private:
    std::string s;
    size_t pos = 0;

    void skipWhitespace() {
        while (pos < s.size() && isspace(s[pos])) pos++;
    }

    bool match(const std::string& t) {
        skipWhitespace();

        if (pos + t.size() > s.size())
            return false;

        if (s.compare(pos, t.size(), t) == 0) {
            pos += t.size();
            return true;
        }
        return false;
    }

    std::string parseString() {
        skipWhitespace();

        if (pos >= s.size() || s[pos] != '"')
            return "";

        pos++; // открывающая "

        std::string result;

        while (pos < s.size()) {
            // Разбираем экранированные символы JSON
            if (s[pos] == '\\' && pos + 1 < s.size()) {
                char escaped = s[pos + 1];

                // Сохраняем как escape-последовательность для специальных символов
                switch(escaped) {
                    case 'n':
                        result += "\\n";
                        break;
                    case 't':
                        result += "\\t";
                        break;
                    case 'r':
                        result += "\\r";
                        break;
                    case '\\':
                        result += "\\\\";
                        break;
                    case '"':
                        result += "\"";
                        break;
                    default:
                        result += '\\';
                        result += escaped;
                }

                pos += 2;
                continue;
            }

            if (s[pos] == '"') {
                pos++; // закрывающая "
                break;
            }

            result += s[pos++];
        }

        return result;
    }

    ASTNode parseObject() {
        ASTNode node;

        if (!match("{"))
            return node;

        while (pos < s.size()) {

            skipWhitespace();

            if (match("}"))
                break;

            std::string key = parseString();

            if (!match(":"))
                break;

            skipWhitespace();

            if (key == "type") {
                node.type = parseString();
            }
            else if (key == "value") {
                node.value = parseString();
            }
            else if (key == "children") {
                node.children = parseArray();
            }
            else if (key == "left") {
                node.left = std::make_unique<ASTNode>(parseObject());
            }
            else if (key == "right") {
                node.right = std::make_unique<ASTNode>(parseObject());
            }
            else {
                // Неизвестные поля не участвуют в анализе
                skipUnknown();
            }

            skipWhitespace();
            match(","); // безопасно: если нет — просто игнор
        }

        return node;
    }

    std::vector<ASTNode> parseArray() {
        std::vector<ASTNode> arr;

        if (!match("["))
            return arr;

        while (pos < s.size()) {

            skipWhitespace();

            if (match("]"))
                break;

            if (pos < s.size() && s[pos] == '{') {
                arr.push_back(parseObject());
            } else {
                // защита от мусора
                pos++;
                continue;
            }

            skipWhitespace();
            match(",");
        }

        return arr;
    }

    void skipUnknown() {
        skipWhitespace();

        if (pos >= s.size()) return;

        if (s[pos] == '"') {
            parseString();
        }
        else if (s[pos] == '{') {
            parseBraces('{', '}');
        }
        else if (s[pos] == '[') {
            parseBraces('[', ']');
        }
        else {
            while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']')
                pos++;
        }
    }

    void parseBraces(char open, char close) {
        int depth = 0;

        while (pos < s.size()) {
            if (s[pos] == open) depth++;
            else if (s[pos] == close) {
                depth--;
                if (depth == 0) {
                    pos++;
                    break;
                }
            }
            pos++;
        }
    }

public:
    ASTNode parse(const std::string& input) {
        s = input;
        pos = 0;

        skipWhitespace();
        return parseObject();
    }
};

struct Symbol {
    std::string name;
    std::string type;
    std::string scope;
    bool declared = false;
    bool initialized = false;

    bool isFunction = false;
    std::string returnType;
    std::vector<std::string> paramTypes;
};

struct SymbolEntry {
    std::string name;
    std::string type;
    std::string scope;
    bool declared;
    bool initialized;
};

std::vector<Symbol> systemFuncs = {
    {
        "printf",
        "void(func)",
        "Global",
        false,
        false,
        true,
        "int",
        {},     // paramTypes
    }
};

struct Triad {
    std::string op;
    std::string arg1;
    std::string arg2;
};

struct ExprResult {
    std::string value; // имя переменной или ^triad
    std::string type;  // тип выражения
};

class SemanticAnalyzer {
private:
    std::vector<std::string> scopeStack;
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
    std::vector<Triad> triads;
    std::vector<SymbolEntry> symbolLog;
    std::vector<std::string> errors;
    int triadIndex = 0;

public:
    void enterScope(std::string val = "") {
        scopes.emplace_back();
        scopeStack.push_back(val);
    }

    void exitScope() {
        scopes.pop_back();
        if (!scopeStack.empty())
            scopeStack.pop_back();
    }

    Symbol* findCurrentFunction() {
        for (int i = scopes.size() - 1; i >= 0; --i) {
            for (auto& [_, sym] : scopes[i]) {
                if (sym.isFunction) {
                    return &sym;
                }
            }
        }
        return nullptr;
    }

    std::string currentScopeName() const {
        if (scopeStack.empty())
            return "Global";
        return scopeStack.back();
    }

    bool declareSymbol(
        const std::string& name,
        const std::string& type,
        bool isDeclared = true,
        bool isInitialized = true,
        bool allowFunctionRedefinition = false
    )
    {
        auto& scope = scopes.back();

        if (scope.count(name)) {

            Symbol& existing = scope[name];

            // если это функция — разрешаем "повторное объявление"
            if (!existing.isFunction || !allowFunctionRedefinition) {
                errors.push_back("Semantic error: redeclaration of '" + name + "'");
                return false;
            }
        }

        scope[name] = {name, type, currentScopeName(), isDeclared, isInitialized};
        symbolLog.push_back({
            name,
            type,
            currentScopeName(),
            isDeclared,
            isInitialized
        });
        // std::cout << "Declare symbol: " << name << " # " << type << " # " << currentScopeName() << " # " << isDeclared << " # " << isInitialized << "\n";
        return true;
    }

    bool checkTypes(const std::string& type1, const std::string& type2)
    {
        if (type1 == type2)
            return true;

        errors.push_back("Semantic error: type " + type1 + " is not compatible with type " + type2);
        return false;
    }

    Symbol* resolve(const std::string& name) {
        for (int i = scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end())
                return &it->second;
        }

        errors.push_back("Semantic error: use of undeclared identifier '" + name + "'");
        return nullptr;
    }

    int emit(const std::string& op, const std::string& a1 = "", const std::string& a2 = "") {
        triads.push_back({op, a1, a2});
        return triadIndex++;
    }

    ExprResult evalExpr(const ASTNode& node) {
        const std::string& type = node.type;

        if (type == "IntLiteral") {
            return { node.value, "int"};
        }

        if (type == "StringLiteral") {
            return { node.value, "string"};
        }

        if (type == "Identifier") {
            auto* sym = resolve(node.value);
            if (sym) {
                if (!sym->initialized) {
                    errors.push_back("Semantic error: variable '" + sym->name + "' used uninitialized");
                }
                return {node.value, sym->type};
            }

            return {node.value, "unknown"};
        }

        if (type == "BinaryOp") {
            ExprResult left = evalExpr(*node.left);
            ExprResult right = evalExpr(*node.right);

            if (!checkTypes(left.type, right.type))
            {
                errors.push_back("BinaryOp: left (" + left.type + ", " + left.value + "), right (" + right.type + ", " + right.value + ")\n");
            }

            int idx = emit(node.value, left.value, right.value);
            return {"^" + std::to_string(idx), left.type};
        }

        if (type == "UnaryOp") {
            auto val = evalExpr(*node.left);
            int idx = emit(node.value, val.value, "");
            return {"^" + std::to_string(idx), val.type};
        }

        if (type == "Call") {
            std::string fname = node.value;
            auto* sym = resolve(fname);

            std::vector<ExprResult> args;

            for (auto& arg : node.children)
                args.push_back(evalExpr(arg));


            if (fname == "printf") {
                if (args.empty())
                    errors.push_back("Semantic error: printf requires format string");
                else
                {
                    // 1. первый аргумент обязательно string
                    if (args[0].type != "string") {
                        errors.push_back("printf: first argument must be format string");
                    }

                    // 2. упрощённая проверка количества %
                    int expected = 0;
                    for (char c : args[0].value)
                        if (c == '%') expected++;

                    int provided = args.size() - 1;

                    if (expected != provided)
                        errors.push_back(
                            "printf: format expects " + std::to_string(expected) +
                            " args, got " + std::to_string(provided)
                        );
                }
            }
            else
            {
                // Проверяем объявление вызываемой функции
                if (!sym || !sym->isFunction) {
                    errors.push_back("Semantic error: '" + fname + "' is not a function");
                } else {
                    // 1. количество аргументов
                    if (args.size() != sym->paramTypes.size()) {
                        errors.push_back(
                            "Semantic error: function '" + fname +
                            "' expects " + std::to_string(sym->paramTypes.size()) +
                            " arguments, got " + std::to_string(args.size())
                        );
                    } else {
                        // 2. типы аргументов
                        for (size_t i = 0; i < args.size(); ++i) {
                            if (!checkTypes(sym->paramTypes[i], args[i].type)) {
                                errors.push_back(
                                    "Call: argument " + std::to_string(i) +
                                    " expected (" + sym->paramTypes[i] +
                                    "), got (" + args[i].type + ")"
                                );
                            }
                        }
                    }
                }
            }

            // Формируем передачу аргументов и вызов
            for (auto& a : args) {
                emit("push", a.value, "");
            }

            int idx = emit("call", fname, "");

            // Тип результата берём из таблицы символов
            std::string retType = sym && sym->isFunction ? sym->returnType : "unknown";

            return {"^" + std::to_string(idx), retType};
        }

        return {"unknown", "unknown"};
    }

    void handleStatement(const ASTNode& node) {
        const std::string& type = node.type;

        if (type == "VarDecl") {
            std::string name = node.value;
            std::string varType = node.children[0].value;

            if (!declareSymbol(name, varType, true, node.children.size() > 1))
                return;

            if (node.children.size() > 1) {
                auto rhs = evalExpr(node.children[1]);
                // checkTypes(varType, rhs.type);
                if (!checkTypes(varType, rhs.type))
                {
                    errors.push_back("VarDecl: left (" + varType + "), right (" + rhs.type + ", " + rhs.value + ")\n");
                }

                emit("assign", name, rhs.value);

                auto* sym = resolve(name);
                if (sym) { sym->initialized = true; }
            }
        }

        else if (type == "ExpressionStmt") {
            handleStatement(node.children[0]);
        }

        else if (type == "Assign") {
            const std::string& lhs = node.left->value;
            auto* sym = resolve(lhs);

            auto rhs = evalExpr(*node.right);
            if (sym) {
                // checkTypes(sym->type, rhs.type);
                if (!checkTypes(sym->type, rhs.type))
                {
                    errors.push_back("Assign: left (" + sym->type + "), right (" + rhs.type + ", " + rhs.value + ")\n");
                }
            }
            emit("assign", lhs, rhs.value);

            if (sym) sym->initialized = true;
        }

        else if (type == "IfStmt") {
            auto cond = evalExpr(node.children[0]);

            int jmp = emit("ifnot", cond.value, "");

            enterScope();
            handleBlock(node.children[1]);
            exitScope();

            if (node.children.size() > 2) {
                int go = emit("goto", "", "");

                triads[jmp].arg2 = std::to_string(triadIndex);

                handleStatement(node.children[2]);

                triads[go].arg1 = std::to_string(triadIndex);
            } else {
                triads[jmp].arg2 = std::to_string(triadIndex);
            }
        }

        else if (type == "Else")
        {
            enterScope();
            handleBlock(node.children[0]);
            exitScope();
        }

        else if (type == "ForStmt") {
            handleStatement(node.children[0]);

            int start = triadIndex;

            auto cond = evalExpr(node.children[1]);
            int jmp = emit("ifnot", cond.value, "");

            enterScope();
            handleBlock(node.children[3]);
            exitScope();

            evalExpr(node.children[2]);

            emit("goto", std::to_string(start), "");

            triads[jmp].arg2 = std::to_string(triadIndex);
        }

        else if (type == "ReturnStmt") {
            Symbol* func = findCurrentFunction();

            if (!func) {
                errors.push_back("Semantic error: return outside of function");
                return;
            }

            // return;
            if (node.children.empty()) {
                if (func->returnType != "void") {
                    errors.push_back("Semantic error: non-void function must return a value");
                }
                emit("ret", "", "");
                return;
            }

            auto val = evalExpr(node.children[0]);

            if (!checkTypes(func->returnType, val.type)) {
                errors.push_back("Return: expected (" + func->returnType +
                                "), got (" + val.type + ")");
            }

            emit("ret", val.value, "");
        }

        else if (type == "Call") {
            auto val = evalExpr(node);
            // auto* sym = resolve(lhs);
        }

        else if (type == "Identifier") {
            auto* sym = resolve(node.value);
            if (!sym)
                errors.push_back("Unknown identifier: value (" + node.value + ")");
        }
    }

    void handleBlock(const ASTNode& block) {
        for (auto& stmt : block.children) {
            handleStatement(stmt);
        }
    }

    void handlePrototype(const ASTNode& prot)
    {
        std::string protName = prot.value;
        std::string protType = prot.children[0].value;

        declareSymbol(protName, std::format("{}(func)", protType), true, false);

        auto* sym = resolve(protName);
        if (sym) {
            sym->isFunction = true;
            sym->returnType = protType;

            for (auto& child : prot.children) {
                if (child.type == "Param") {
                    std::string ptype = child.children[0].value;
                    sym->paramTypes.push_back(ptype);
                }
            }
        }
    }

    void handleFunction(const ASTNode& func) {
        std::string name = func.value;
        // scopeStack.push_back(name + "()");

        declareSymbol(
            name,
            std::format("{}(func)", func.children[0].value),
            true,
            true,
            true
        );
        auto* sym = resolve(name);
        if (sym) {
            sym->isFunction = true;
            sym->returnType = func.children[0].value;

            for (auto& child : func.children) {
                if (child.type == "Param") {
                    sym->paramTypes.push_back(child.children[0].value);
                }
            }
        }

        emit(".", name, ":");

        enterScope(name + "()");

        for (auto& child : func.children) {
            if (child.type == "Param") {
                std::string pname = child.children[1].value;
                std::string ptype = child.children[0].value;
                declareSymbol(pname, ptype, true, true);

                symbolLog.push_back({
                    pname,
                    ptype,
                    currentScopeName(),
                    true,
                    true
                });
            }

            if (child.type == "Block") {
                handleBlock(child);
            }
        }

        exitScope();
    }

    void analyze(const ASTNode& root) {
        enterScope("Global");

        for (auto& section : root.children) {
            if (section.type == "PreprocessorSection")
            {
                for (auto& symb : systemFuncs) {
                    declareSymbol(symb.name, symb.type, symb.declared, symb.initialized);
                    // symbolLog.push_back({
                    //     symb.name,
                    //     symb.type,
                    //     "Global",
                    //     true,
                    //     true
                    // });
                    auto* s = resolve(symb.name);
                    if (s && symb.name == "printf") {
                        s->isFunction = true;
                        s->returnType = "int";
                    }
                }
                continue;
            }
            if (section.type == "FunctionDeclSection")
            {
                for (auto& func : section.children)
                {
                    handlePrototype(func);
                }
                continue;
            }
            if (section.type == "FunctionDefSection")
            {
                for (auto& func : section.children)
                {
                    handleFunction(func);
                }
                continue;
            }
        }

        exitScope();
    }

    int getErrorsCount() const
    {
        return errors.size();
    }

    void printErrors() const {
        for (const auto& e : errors)
            std::cout << e << std::endl;
    }

    void printSymbolTable(std::ostream& out) {
        out << "=== SYMBOL TABLE ===\n";
        out << std::left
                << std::setw(15) << "Name"
                << std::setw(20) << "Type"
                << std::setw(20) << "Scope"
                << std::setw(12) << "Declared"
                << std::setw(12) << "Initialized"
                << "\n";

        out << std::string(80, '-') << "\n";

        for (auto& e : symbolLog) {

            out << std::left
                    << std::setw(15) << e.name
                    << std::setw(20) << e.type
                    << std::setw(20) << e.scope
                    << std::setw(12) << (e.declared ? "+" : "-")
                    << std::setw(12) << (e.initialized ? "+" : "-")
                    << "\n";
        }
    }

    void printTriads(std::ostream& out) {
        out << "=== TRIADS ===\n";
        for (size_t i = 0; i < triads.size(); ++i) {
            if (triads[i].op == "." && triads[i].arg2 == ":")
            {
                out << triads[i].op
                      << triads[i].arg1
                      << triads[i].arg2 << "\n";
                continue;
            }
            out << i << ": "
                      << triads[i].op << " "
                      << triads[i].arg1 << " "
                      << triads[i].arg2 << "\n";
        }
    }
};

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <filename>" << std::endl;
        return 1;
    }

    std::ifstream inputFile(argv[1]);

    if (!inputFile.is_open()) {
        std::cout << "Error opening file: " << argv[1] << std::endl;
        return 1;
    }

    std::stringstream buffer;
    buffer << inputFile.rdbuf();

    SimpleJSONParser parser;
    ASTNode root = parser.parse(buffer.str());

    SemanticAnalyzer analyzer;
    analyzer.analyze(root);

    if (analyzer.getErrorsCount() == 0)
    {
        std::cout << "Semantic analyzer finished successfully!\nNo errors detected." << std::endl;

        std::string outFileName = argv[1];
        outFileName = outFileName.substr(outFileName.find("_") + 1);
        outFileName = outFileName.substr(0, outFileName.find("."));

        std::string outFileName1 = "semAnal_symbTable_" + outFileName + ".txt";
        std::string outFileName2 = "semAnal_triads_" + outFileName + ".txt";

        std::ofstream symbTable(outFileName1);
        if (!symbTable.is_open())
        {
            std::cout << "Error: Could not create output file " << outFileName1 << std::endl;
            return 1;
        }

        std::ofstream triads(outFileName2);
        if (!triads.is_open())
        {
            std::cout << "Error: Could not create output file " << outFileName2 << std::endl;
            return 1;
        }

        analyzer.printSymbolTable(symbTable);
        symbTable.close();

        analyzer.printTriads(triads);
        triads.close();
    }
    else
    {
        std::cout << "Detected errors (" << analyzer.getErrorsCount() << "):" << std::endl;
        analyzer.printErrors();

        std::cout << "Symbol table file was NOT generated due to errors." << std::endl;
        std::cout << "Triads file was NOT generated due to errors." << std::endl;
    }

    return 0;
}
