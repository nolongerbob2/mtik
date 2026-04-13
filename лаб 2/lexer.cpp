// Лабораторная 2: лексический анализатор для подмножества C++ (лексемы из test.cpp, ЛР1).
// Вход: очищенный исходный текст (результат препроцессора ЛР1).

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

const char* const T_KEYWORD = "KEYWORD";
const char* const T_IDENTIFIER = "IDENTIFIER";
const char* const T_INT = "CONSTANT_INT";
const char* const T_FLOAT = "CONSTANT_FLOAT";
const char* const T_STRING = "CONSTANT_STRING";
const char* const T_BOOL = "CONSTANT_BOOL";
const char* const T_OPERATOR = "OPERATOR";
const char* const T_DELIMITER = "DELIMITER";

struct Token {
    std::string type;
    std::string lexeme;
    int line = 1;
    int col = 1;
};

struct Lexer {
    const std::string& s;
    size_t pos = 0;
    int line = 1;
    int col = 1;
    std::vector<std::string> errors;

    explicit Lexer(const std::string& src) : s(src) {}

    bool eof() const { return pos >= s.size(); }
    char peek(size_t ahead = 0) const {
        size_t j = pos + ahead;
        return j < s.size() ? s[j] : '\0';
    }

    void advance() {
        if (eof()) {
            return;
        }
        if (s[pos] == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
        ++pos;
    }

    void skip_spaces() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    void error(const std::string& kind, const std::string& msg) {
        std::ostringstream os;
        os << "Строка " << line << ", позиция " << col << ": [" << kind << "] " << msg;
        errors.push_back(os.str());
    }

    bool is_ident_start(char c) {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    bool is_ident_char(char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }
};

const std::unordered_set<std::string> kKeywords = {
    "include", "static", "const", "int", "bool", "void", "double",
    "if", "else", "for", "while", "return", "break",
};

const std::unordered_set<std::string> kBool = {"true", "false"};

// Длинные операторы первыми (жадное совпадение по длине).
const char* kOperators[] = {
    "<<", ">>", "++", "--", "==", "!=", "<=", ">=",
    "&&", "||", "+=", "-=", "*=", "/=", "%=", "::",
    "=", "+", "-", "*", "/", "%", "<", ">", "!", "?", "&", "|", "^", "~",
};

const char* kDelimiters = "(){}[];,.:#";

bool try_operator(Lexer& L, Token& out) {
    for (const char* op : kOperators) {
        size_t len = std::strlen(op);
        if (L.pos + len <= L.s.size() && L.s.compare(L.pos, len, op) == 0) {
            out.lexeme.assign(op, len);
            out.type = T_OPERATOR;
            out.line = L.line;
            out.col = L.col;
            for (size_t k = 0; k < len; ++k) {
                L.advance();
            }
            return true;
        }
    }
    return false;
}

bool try_delimiter(Lexer& L, Token& out) {
    char c = L.peek();
    if (std::strchr(kDelimiters, c)) {
        out.lexeme = c;
        out.type = T_DELIMITER;
        out.line = L.line;
        out.col = L.col;
        L.advance();
        return true;
    }
    return false;
}

void lex_string(Lexer& L, std::vector<Token>& tokens) {
    int start_line = L.line;
    int start_col = L.col;
    L.advance();  // "
    std::string buf;
    while (!L.eof()) {
        char c = L.peek();
        if (c == '"') {
            L.advance();
            tokens.push_back({T_STRING, buf, start_line, start_col});
            return;
        }
        if (c == '\\') {
            L.advance();
            if (L.eof()) {
                L.error("незакрытая строка", "escape в конце файла");
                tokens.push_back({T_STRING, buf, start_line, start_col});
                return;
            }
            char e = L.peek();
            L.advance();
            switch (e) {
            case 'n': buf += '\n'; break;
            case 't': buf += '\t'; break;
            case 'r': buf += '\r'; break;
            case '\\': buf += '\\'; break;
            case '"': buf += '"'; break;
            case '\'': buf += '\''; break;
            case '0': buf += '\0'; break;
            default:
                buf += e;
                break;
            }
            continue;
        }
        buf += c;
        L.advance();
    }
    L.error("незакрытая строка", "ожидалась закрывающая кавычка \"");
    tokens.push_back({T_STRING, buf, start_line, start_col});
}

void lex_char_literal(Lexer& L, std::vector<Token>& tokens) {
    int start_line = L.line;
    int start_col = L.col;
    L.advance();  // '
    if (L.eof()) {
        L.error("незакрытый символьный литерал", "ожидался символ после '");
        return;
    }
    char c = L.peek();
    if (c == '\\') {
        L.advance();
        if (L.eof()) {
            L.error("незакрытый символьный литерал", "незавершённый escape");
            return;
        }
        L.advance();
    } else {
        L.advance();
    }
    if (L.eof() || L.peek() != '\'') {
        L.error("незакрытый символьный литерал", "ожидался '");
        while (!L.eof() && L.peek() != '\n' && L.peek() != '\'') {
            L.advance();
        }
        if (!L.eof() && L.peek() == '\'') {
            L.advance();
        }
        return;
    }
    L.advance();
    tokens.push_back({T_STRING, "'char'", start_line, start_col});  // условное имя типа для отчёта
}

void lex_number(Lexer& L, std::vector<Token>& tokens) {
    int start_line = L.line;
    int start_col = L.col;
    std::string raw;

    while (!L.eof() && std::isdigit(static_cast<unsigned char>(L.peek()))) {
        raw += L.peek();
        L.advance();
    }

    bool is_float = false;
    if (!L.eof() && L.peek() == '.') {
        is_float = true;
        raw += '.';
        L.advance();
        if (!L.eof() && L.peek() == '.') {
            L.error("некорректное число", "две точки подряд");
            if (!raw.empty() && raw.back() == '.') {
                raw.pop_back();
            }
            tokens.push_back({T_INT, raw, start_line, start_col});
            return;
        }
        while (!L.eof() && std::isdigit(static_cast<unsigned char>(L.peek()))) {
            raw += L.peek();
            L.advance();
        }
    }

    if (!L.eof() && (std::isalpha(static_cast<unsigned char>(L.peek())) || L.peek() == '_')) {
        L.error("некорректное число", "буква или «_» сразу после числового литерала");
        while (!L.eof() && L.is_ident_char(L.peek())) {
            L.advance();
        }
        return;
    }

    if (is_float) {
        tokens.push_back({T_FLOAT, raw, start_line, start_col});
    } else {
        tokens.push_back({T_INT, raw, start_line, start_col});
    }
}

void lex_identifier_or_keyword(Lexer& L, std::vector<Token>& tokens) {
    int start_line = L.line;
    int start_col = L.col;
    std::string name;
    name += L.peek();
    L.advance();
    while (!L.eof() && L.is_ident_char(L.peek())) {
        name += L.peek();
        L.advance();
    }

    if (kBool.count(name)) {
        tokens.push_back({T_BOOL, name, start_line, start_col});
    } else if (kKeywords.count(name)) {
        tokens.push_back({T_KEYWORD, name, start_line, start_col});
    } else {
        tokens.push_back({T_IDENTIFIER, name, start_line, start_col});
    }
}

bool invalid_source_char(unsigned char c) {
    if (c < 32) {
        return c != '\t' && c != '\n' && c != '\r';
    }
    return c == 127;
}

std::vector<Token> tokenize(Lexer& L) {
    std::vector<Token> tokens;
    while (true) {
        L.skip_spaces();
        if (L.eof()) {
            break;
        }

        unsigned char uc = static_cast<unsigned char>(L.peek());
        if (invalid_source_char(uc)) {
            L.error("недопустимый символ", "код " + std::to_string(static_cast<int>(uc)));
            L.advance();
            continue;
        }

        if (L.peek() == '"') {
            lex_string(L, tokens);
            continue;
        }
        if (L.peek() == '\'') {
            lex_char_literal(L, tokens);
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(L.peek()))) {
            lex_number(L, tokens);
            continue;
        }

        if (L.is_ident_start(L.peek())) {
            lex_identifier_or_keyword(L, tokens);
            continue;
        }

        Token op{};
        if (try_operator(L, op)) {
            tokens.push_back(std::move(op));
            continue;
        }

        Token del{};
        if (try_delimiter(L, del)) {
            tokens.push_back(std::move(del));
            continue;
        }

        char bad = L.peek();
        L.error("неизвестный оператор или символ", std::string("символ '") + bad + "'");
        L.advance();
    }
    return tokens;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("не удалось открыть файл: " + path);
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

void print_table(const std::vector<Token>& tokens) {
    std::cout << "Лексема     | Тип\n";
    std::cout << "------------+----------------------\n";
    for (const auto& t : tokens) {
        std::string lex = t.lexeme;
        if (lex.size() > 11) {
            lex = lex.substr(0, 8) + "...";
        }
        std::cout.width(12);
        std::cout << std::left << lex << "| " << t.type << "\n";
    }
}

void print_list(const std::vector<Token>& tokens) {
    auto esc = [](const std::string& x) {
        std::string o = "\"";
        for (char c : x) {
            if (c == '\\' || c == '"') {
                o += '\\';
            }
            o += c;
        }
        o += '"';
        return o;
    };
    std::cout << "[";
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i) {
            std::cout << ", ";
        }
        std::cout << "(" << tokens[i].type << ", " << esc(tokens[i].lexeme) << ")";
    }
    std::cout << "]\n";
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: " << argv[0] << " <очищенный_исходный_файл>\n";
        std::cerr << "Пример: " << argv[0] << " \"../лаб 1/test.clean.cpp\"\n";
        return 1;
    }

    try {
        std::string src = read_file(argv[1]);
        Lexer L(src);
        std::vector<Token> tokens = tokenize(L);

        print_table(tokens);
        std::cout << "\n";
        print_list(tokens);
        std::cout << "\n";

        for (const auto& e : L.errors) {
            std::cerr << e << "\n";
        }

        if (!L.errors.empty()) {
            std::cout << "Лексический анализ завершён с ошибками. Обнаружено " << tokens.size()
                      << " токенов. Ошибок: " << L.errors.size() << ".\n";
            return 2;
        }

        std::cout << "Лексический анализ завершён успешно. Обнаружено " << tokens.size()
                  << " токенов. Ошибок не найдено.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "Ошибка: " << ex.what() << "\n";
        return 1;
    }
}
