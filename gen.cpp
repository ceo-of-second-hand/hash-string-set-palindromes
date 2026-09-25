// Генератор тестів.
//
// Використання: gen N seed [add del ask] [pool] [pal]
//   N        — кількість операцій (<= 10^6)
//   seed     — зерно генератора (однакове зерно -> однаковий файл)
//   add del ask — відносні ваги операцій "+", "-", "?"      (типово 50 20 30)
//   pool     — кількість різних слів, з яких беруться операції (типово N/3)
//   pal      — відсоток спеціально побудованих паліндромів у пулі (типово 10)
//
// Слова беруться з обмеженого пулу, щоб були повторні додавання, вилучення
// відсутніх рядків і відповіді як "yes", так і "no".

#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: gen N seed [add del ask] [pool] [pal]\n");
        return 1;
    }
    long long n = std::atoll(argv[1]);
    unsigned long long seed = std::strtoull(argv[2], nullptr, 10);
    int wAdd = 50, wDel = 20, wAsk = 30;
    if (argc >= 6) {
        wAdd = std::atoi(argv[3]);
        wDel = std::atoi(argv[4]);
        wAsk = std::atoi(argv[5]);
    }
    long long pool = argc >= 7 ? std::atoll(argv[6]) : n / 3;
    int palPct = argc >= 8 ? std::atoi(argv[7]) : 10;
    if (pool < 1) pool = 1;
    if (wAdd + wDel + wAsk <= 0) {
        std::fprintf(stderr, "operation weights must not all be zero\n");
        return 1;
    }

    std::mt19937_64 rng(seed);
    auto rnd = [&](long long k) { return (long long)(rng() % (unsigned long long)k); };
    auto letter = [&]() { return (char)('a' + rnd(26)); };

    std::vector<std::string> words(pool);
    for (std::string& w : words) {
        int len = 1 + (int)rnd(15);
        w.resize(len);
        if (rnd(100) < palPct) {
            // половина + дзеркало (для непарної довжини — центр довільний)
            for (int i = 0; i < (len + 1) / 2; ++i) w[i] = w[len - 1 - i] = letter();
        } else {
            for (char& c : w) c = letter();
        }
    }

    std::string out;
    out.reserve((size_t)n * 12 + 4);
    int total = wAdd + wDel + wAsk;
    for (long long i = 0; i < n; ++i) {
        int r = (int)rnd(total);
        out += r < wAdd ? '+' : r < wAdd + wDel ? '-' : '?';
        out += ' ';
        out += words[rnd(pool)];
        out += '\n';
    }
    out += "#\n";
    std::fwrite(out.data(), 1, out.size(), stdout);
    return 0;
}
