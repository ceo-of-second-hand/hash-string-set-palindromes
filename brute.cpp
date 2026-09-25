// Еталонне (наївне) рішення для перевірки: std::unordered_set + пряма
// перевірка паліндромів. Той самий формат входу/виходу, що й у main.cpp.
//
// Використання: brute [-t] [input [output]]

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

static bool validWord(const std::string& w) {
    if (w.empty() || w.size() > 15) return false;
    for (char c : w)
        if (c < 'a' || c > 'z') return false;
    return true;
}

int main(int argc, char** argv) {
    std::ios::sync_with_stdio(false);
    bool timing = false;
    std::vector<const char*> files;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-t") == 0)
            timing = true;
        else
            files.push_back(argv[i]);
    }
    std::ifstream fin;
    std::ofstream fout;
    if (files.size() >= 1) fin.open(files[0], std::ios::binary);
    if (files.size() >= 2) fout.open(files[1], std::ios::binary);
    std::istream& in = files.size() >= 1 ? static_cast<std::istream&>(fin) : std::cin;
    std::ostream& out = files.size() >= 2 ? static_cast<std::ostream&>(fout) : std::cout;

    auto start = std::chrono::steady_clock::now();

    std::unordered_set<std::string> set;
    std::string op, word;
    while (in >> op) {
        if (op == "#") break;
        if (!(in >> word)) break;
        if (op.size() != 1 || !validWord(word)) continue;
        if (op[0] == '+')
            set.insert(word);
        else if (op[0] == '-')
            set.erase(word);
        else if (op[0] == '?')
            out << (set.count(word) ? "yes\n" : "no\n");
    }

    std::vector<std::string> pals;
    for (const std::string& s : set)
        if (std::equal(s.begin(), s.begin() + s.size() / 2, s.rbegin())) pals.push_back(s);
    std::sort(pals.begin(), pals.end());
    out << "palindromes: " << pals.size() << '\n';
    for (const std::string& s : pals) out << s << '\n';
    out.flush();

    if (timing) {
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();
        std::cerr << "time_ms=" << ms << '\n';
    }
    return 0;
}
