// =============================================================================
// main.cpp — ПРОГРАМА ЛАБОРАТОРНОЇ (варіант 2). Збирається в lab.exe.
//
// Відповідає за «спілкування» з користувачем: читає операції, викликає методи
// StringSet (string_set.h), друкує відповіді. Саме зберігання рядків — у StringSet.
//
// Використання: lab [-t] [-s seed] [input [output]]
//   без файлів — читає з клавіатури (stdin), пише на екран (stdout);
//   input       — файл з операціями; output — файл для відповідей;
//   -t          — вивести час обробки й розміри таблиці (у stderr);
//   -s seed     — фіксована основа хешу p (для відтворюваності); інакше випадкова.
//
// Вхід:  рядки "+ word", "- word", "? word"; кінець — рядок "#" (умова задачі).
// Вихід: yes/no на кожну "?", потім "palindromes: k" і самі паліндроми за абеткою.
// =============================================================================

#include <algorithm>  // std::sort — сортування паліндромів для виводу
#include <chrono>     // std::chrono::steady_clock — заміри часу для -t
#include <cstdio>     // FILE*, fopen, getc, fputs, fprintf — швидкий ввід/вивід
#include <cstdlib>    // std::strtoull — рядок -> число (для -s)
#include <cstring>    // std::strcmp — порівняння аргументів командного рядка
#include <string>
#include <vector>

#include "string_set.h"  // наш клас множини (лапки — шукати поруч з main.cpp)

// -----------------------------------------------------------------------------
// ЧИТАННЯ ОДНОГО «СЛОВА» (токена) з файлу.
// Файл читається НЕ рядками, а словами — групами символів між пробілами /
// переносами рядка. "+ abba\n? abba\n#" -> "+", "abba", "?", "abba", "#".
//
// buf — масив, куди записати слово; cap — його розмір (скільки влізе).
// Повертає ПОВНУ довжину слова (навіть якщо записано менше — так validWord
// побачить, що слово задовге) або -1, якщо файл закінчився.
// static перед функцією — вона видима лише в цьому файлі.
// -----------------------------------------------------------------------------
static int readToken(FILE* in, char* buf, int cap) {
    // Пропускаємо пробільні символи перед словом (\r — частина переносу у Windows).
    int c = std::getc(in);
    while (c == ' ' || c == '\n' || c == '\r' || c == '\t') c = std::getc(in);
    if (c == EOF) return -1;  // слів більше немає

    // Читаємо символи до наступного пробілу / кінця рядка / кінця файлу.
    int n = 0;
    while (c != EOF && c != ' ' && c != '\n' && c != '\r' && c != '\t') {
        if (n < cap) buf[n] = (char)c;  // захист від виходу за межі масиву
        ++n;                            // але довжину рахуємо повну
        c = std::getc(in);
    }
    return n;
}

// Перевірка слова за умовою: 1..15 символів, лише 'a'..'z'.
// Неправильні рядки пропускаються (StringSet очікує вже коректні дані).
static bool validWord(const char* w, int len) {
    if (len < 1 || len > StringSet::MAX_LEN) return false;
    for (int i = 0; i < len; ++i)
        if (w[i] < 'a' || w[i] > 'z') return false;
    return true;
}

// argc — кількість аргументів командного рядка, argv — самі аргументи
// (argv[0] — ім'я програми). Напр. "lab.exe -t in.txt" -> argc = 3.
int main(int argc, char** argv) {
    // --- 1. Розбір аргументів командного рядка -------------------------------
    bool timing = false;             // чи друкувати час (-t)
    uint64_t seed = 0;               // 0 = випадкова основа p
    std::vector<const char*> files;  // усе, що не ключ, — імена файлів
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-t") == 0)
            timing = true;
        else if (std::strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            seed = std::strtoull(argv[++i], nullptr, 10);  // наступний аргумент — число
        else
            files.push_back(argv[i]);
    }

    // --- 2. Відкриття файлів --------------------------------------------------
    // За замовчуванням — клавіатура/екран. Перший файл — вхід, другий — вихід.
    // "rb"/"wb" — двійковий режим: байти читаються/пишуться як є.
    FILE* in = stdin;
    FILE* out = stdout;
    if (files.size() >= 1 && !(in = std::fopen(files[0], "rb"))) {
        std::fprintf(stderr, "cannot open %s\n", files[0]);
        return 1;  // ненульовий код виходу = помилка
    }
    if (files.size() >= 2 && !(out = std::fopen(files[1], "wb"))) {
        std::fprintf(stderr, "cannot open %s\n", files[1]);
        return 1;
    }
    // Великі буфери (64 КБ) для вводу/виводу: менше звернень до диска,
    // суттєво швидше на 10^6 операцій. static — масиви живуть весь час роботи.
    static char inBuf[1 << 16], outBuf[1 << 16];
    std::setvbuf(in, inBuf, _IOFBF, sizeof inBuf);
    std::setvbuf(out, outBuf, _IOFBF, sizeof outBuf);

    auto start = std::chrono::steady_clock::now();  // початок заміру часу

    // --- 3. Головний цикл: одна ітерація = одна операція ----------------------
    StringSet set(seed);
    // op — символ операції (+ - ? #), word — рядок. Розміри з запасом:
    // потрібно 1 і 15 символів; довжину перевіряємо окремо (opLen, len).
    char op[8], word[64];
    long long lineNo = 0;  // номер операції — для повідомлень про помилки
    for (;;) {             // нескінченний цикл, вихід — через break
        // Читаємо операцію. opLen — її довжина або -1 (файл закінчився).
        int opLen = readToken(in, op, sizeof op);
        if (opLen == -1) break;  // файл без "#" — завершуємо як на "#"
        ++lineNo;
        // "#" — ознака кінця вхідних даних (умова). Усе після неї ігнорується.
        if (opLen == 1 && op[0] == '#') break;

        // Читаємо рядок, над яким виконується операція.
        int len = readToken(in, word, sizeof word);
        // Операція має бути рівно 1 символ, рядок — коректний; інакше пропуск.
        if (opLen != 1 || len == -1 || !validWord(word, len)) {
            std::fprintf(stderr, "line %lld: invalid operation, skipped\n", lineNo);
            continue;
        }
        // Виконання операції — виклик відповідного методу StringSet.
        switch (op[0]) {
            case '+': set.insert(word, len); break;  // додати (дубль ігнорується)
            case '-': set.erase(word, len); break;   // вилучити (відсутній — ігнорується)
            case '?': std::fputs(set.contains(word, len) ? "yes\n" : "no\n", out); break;
            default: std::fprintf(stderr, "line %lld: unknown operation, skipped\n", lineNo);
        }
    }

    // --- 4. Варіант 2: вивід усіх паліндромів ---------------------------------
    // Збір — O(n) (ознака pal уже обчислена при вставці).
    // Сортування — O(k log k · L); умова порядку не вимагає, але без нього порядок
    // залежав би від внутрішньої будови таблиці (і від p) — вивід був би
    // непередбачуваним і його важко було б перевіряти.
    std::vector<std::string> pals = set.palindromes();
    std::sort(pals.begin(), pals.end());
    std::fprintf(out, "palindromes: %zu\n", pals.size());  // %zu — формат для size_t
    for (const std::string& s : pals) {
        std::fputs(s.c_str(), out);
        std::fputc('\n', out);
    }
    std::fflush(out);  // виштовхнути все з буфера у файл/на екран

    // --- 5. Час роботи (лише з -t) --------------------------------------------
    // Друкується в stderr, щоб не змішувати з відповідями. Включає читання
    // входу, усі операції й вивід.
    if (timing) {
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - start).count();
        std::fprintf(stderr, "time_ms=%.2f size=%zu capacity=%zu\n", ms, set.size(),
                     set.capacity());
    }
    if (in != stdin) std::fclose(in);
    if (out != stdout) std::fclose(out);
    return 0;
}
