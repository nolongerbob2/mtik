// Лабораторная 1: препроцессор (C++). Комментарии — посимвольно (учёт строк/символов);
// нормализация пробелов и пустых строк — std::regex.

#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string>

namespace {

enum class ScanState { Normal, LineComment, BlockComment, String, Char };

// Недопустимы: управляющие ASCII (кроме таба и переводов строк) и DEL.
// Остальное (включая UTF-8 в комментариях/строках) для C++ считаем допустимым.
bool is_allowed_in_cpp_source(unsigned char c) {
    if (c < 32) {
        return c == '\t' || c == '\n' || c == '\r';
    }
    return c != 127;
}

void report_invalid_characters(const std::string& src) {
    int line = 1;
    int col = 1;
    for (unsigned char uc : src) {
        if (!is_allowed_in_cpp_source(uc)) {
            std::cerr << "Ошибка: недопустимый символ в исходном тексте (код "
                      << static_cast<int>(uc) << ") в строке " << line
                      << ", позиция " << col << ".\n";
        }
        if (uc == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
    }
}

// Удаление // и /* */ с учётом литералов "..." и '...'; при незакрытом /* задаёт флаг.
std::string strip_comments(const std::string& src, bool& unclosed_block) {
    unclosed_block = false;
    std::string out;
    out.reserve(src.size());

    ScanState st = ScanState::Normal;
    for (size_t i = 0; i < src.size(); ++i) {
        char c = src[i];

        switch (st) {
        case ScanState::Normal:
            if (c == '/' && i + 1 < src.size()) {
                if (src[i + 1] == '/') {
                    st = ScanState::LineComment;
                    ++i;
                    continue;
                }
                if (src[i + 1] == '*') {
                    st = ScanState::BlockComment;
                    ++i;
                    continue;
                }
            }
            if (c == '"') {
                st = ScanState::String;
            } else if (c == '\'') {
                st = ScanState::Char;
            }
            out += c;
            break;

        case ScanState::LineComment:
            if (c == '\n') {
                st = ScanState::Normal;
                out += c;
            }
            break;

        case ScanState::BlockComment:
            if (c == '*' && i + 1 < src.size() && src[i + 1] == '/') {
                st = ScanState::Normal;
                ++i;
            }
            break;

        case ScanState::String:
            out += c;
            if (c == '\\' && i + 1 < src.size()) {
                out += src[++i];
            } else if (c == '"') {
                st = ScanState::Normal;
            }
            break;

        case ScanState::Char:
            out += c;
            if (c == '\\' && i + 1 < src.size()) {
                out += src[++i];
            } else if (c == '\'') {
                st = ScanState::Normal;
            }
            break;
        }
    }

    if (st == ScanState::BlockComment) {
        unclosed_block = true;
    }
    return out;
}

std::string read_all(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("не удалось открыть входной файл: " + path);
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    return buf.str();
}

void write_all(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("не удалось открыть выходной файл: " + path);
    }
    out << data;
}

// Схлопывание пробелов и табов внутри строки, обрезка краёв — через regex; пустые строки убраны.
std::string normalize_whitespace_regex(const std::string& text) {
    static const std::regex crlf(R"(\r\n)");
    static const std::regex lone_cr(R"(\r)");
    static const std::regex leading_trailing(R"(^\s+|\s+$)");
    static const std::regex spaces_tabs(R"([ \t]+)");

    std::string normalized = std::regex_replace(text, crlf, "\n");
    normalized = std::regex_replace(normalized, lone_cr, "\n");

    std::istringstream iss(normalized);
    std::string line;
    std::ostringstream result;

    bool first_out = true;
    while (std::getline(iss, line)) {
        // убрать возможный '\r' в конце (Windows)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        std::string trimmed = std::regex_replace(line, leading_trailing, "");
        std::string collapsed = std::regex_replace(trimmed, spaces_tabs, " ");
        if (collapsed.empty()) {
            continue;
        }
        if (!first_out) {
            result << '\n';
        }
        result << collapsed;
        first_out = false;
    }
    return result.str();
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Использование: " << argv[0] << " <входной_файл> <выходной_файл>\n";
        std::cerr << "Пример: " << argv[0] << " test.cpp test.clean.cpp\n";
        return 1;
    }

    const std::string in_path = argv[1];
    const std::string out_path = argv[2];

    try {
        std::cout << "Чтение: " << in_path << "\n";
        std::string src = read_all(in_path);

        report_invalid_characters(src);

        bool unclosed = false;
        std::string no_comments = strip_comments(src, unclosed);
        if (unclosed) {
            std::cerr << "Ошибка: незакрытый многострочный комментарий /* ... */.\n";
        }

        std::string cleaned = normalize_whitespace_regex(no_comments);
        write_all(out_path, cleaned);

        std::cout << "Готово: результат записан в " << out_path << "\n";
        if (unclosed) {
            return 2;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Ошибка: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
