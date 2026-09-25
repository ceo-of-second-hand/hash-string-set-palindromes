// Множина рядків на хеш-таблиці з ланцюгами (Лекція 1) та подвійним
// поліноміальним хешем (Лекція 3). Рядки: 1..15 рядкових латинських літер.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

class StringSet {
public:
    static const int MAX_LEN = 15;

    // seed = 0 -> основа p генерується випадково при кожному запуску
    // (захист від підбору колізій, Лекція 3 «Вибір основи p»).
    explicit StringSet(uint64_t seed = 0) {
        std::mt19937_64 rng(seed ? seed : std::random_device{}() ^
                                          ((uint64_t)std::random_device{}() << 32));
        // непарне p, max(a_i) = 26 < p < M1
        uint64_t p = 27 + rng() % (M1 - 28);
        if (p % 2 == 0) ++p;
        pw1_[0] = 1;
        pw2_[0] = 1;
        for (int i = 1; i < MAX_LEN; ++i) {
            pw1_[i] = (uint32_t)((uint64_t)pw1_[i - 1] * p % M1);
            pw2_[i] = pw2_[i - 1] * p;  // mod 2^64 автоматично
        }
        head_.assign(INIT_CAP, -1);
    }

    // Додавання. Повертає true, якщо рядок був доданий (його ще не було).
    bool insert(const char* s, int len) {
        Hash h = hash(s, len);
        if (find(s, len, h, nullptr) != -1) return false;
        if (size_ + 1 > head_.size()) grow();  // тримаємо α = n/cap <= 1

        int id;
        if (free_ != -1) {
            id = free_;
            free_ = pool_[id].next;
        } else {
            id = (int)pool_.size();
            pool_.emplace_back();
        }
        Node& nd = pool_[id];
        std::memcpy(nd.s, s, len);
        nd.s[len] = '\0';
        nd.len = (uint8_t)len;
        nd.h1 = h.h1;
        nd.h2 = h.h2;
        // прямий хеш == хеш оберненого рядка -> паліндром (з ймовірністю ~1),
        // підтверджуємо прямим порівнянням за O(L)
        nd.pal = h.h1 == h.r1 && h.h2 == h.r2 && isPalindrome(s, len);

        size_t b = bucket(h.h1, h.h2);
        nd.next = head_[b];
        head_[b] = id;
        ++size_;
        return true;
    }

    // Вилучення. Повертає true, якщо рядок був у множині.
    bool erase(const char* s, int len) {
        Hash h = hash(s, len);
        int prev;
        int id = find(s, len, h, &prev);
        if (id == -1) return false;
        if (prev == -1)
            head_[bucket(h.h1, h.h2)] = pool_[id].next;
        else
            pool_[prev].next = pool_[id].next;
        pool_[id].next = free_;
        free_ = id;
        --size_;
        return true;
    }

    bool contains(const char* s, int len) const {
        return find(s, len, hash(s, len), nullptr) != -1;
    }

    // Усі паліндроми множини: один прохід по таблиці, O(cap + n).
    std::vector<std::string> palindromes() const {
        std::vector<std::string> res;
        for (int first : head_)
            for (int id = first; id != -1; id = pool_[id].next)
                if (pool_[id].pal) res.emplace_back(pool_[id].s, pool_[id].len);
        return res;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return head_.size(); }

    static bool isPalindrome(const char* s, int len) {
        for (int i = 0, j = len - 1; i < j; ++i, --j)
            if (s[i] != s[j]) return false;
        return true;
    }

private:
    static const uint32_t M1 = 1000000123;  // m2 = 2^64 (переповнення uint64_t)
    static const size_t INIT_CAP = 16;      // степінь двійки

    struct Hash {
        uint32_t h1, r1;  // прямий / обернений хеш за модулем M1
        uint64_t h2, r2;  // прямий / обернений хеш за модулем 2^64
    };

    struct Node {
        char s[MAX_LEN + 1];
        uint8_t len;
        bool pal;     // чи є рядок паліндромом (обчислюється при вставці)
        uint32_t h1;  // хеш mod M1
        uint64_t h2;  // хеш mod 2^64
        int next;     // наступний вузол ланцюга (або вільного списку), -1 = кінець
    };

    uint32_t pw1_[MAX_LEN];
    uint64_t pw2_[MAX_LEN];
    std::vector<int> head_;   // голови ланцюгів
    std::vector<Node> pool_;  // усі вузли; вилучені йдуть у вільний список
    int free_ = -1;
    size_t size_ = 0;

    // Прямий H = sum a_i p^i та обернений R = sum a_{L-1-i} p^i за один прохід, O(L).
    // a_i = c - 'a' + 1, щоб код символу не був нулем ("a" != "aa").
    Hash hash(const char* s, int len) const {
        uint64_t h1 = 0, r1 = 0, h2 = 0, r2 = 0;
        for (int i = 0; i < len; ++i) {
            uint64_t a = (uint64_t)(s[i] - 'a' + 1);
            uint64_t b = (uint64_t)(s[len - 1 - i] - 'a' + 1);
            h1 += a * pw1_[i];
            r1 += b * pw1_[i];
            h2 += a * pw2_[i];
            r2 += b * pw2_[i];
        }
        // 15 доданків < 27 * 1e9 кожен -> сума вміщується в uint64_t, mod один раз
        return {(uint32_t)(h1 % M1), (uint32_t)(r1 % M1), h2, r2};
    }

    size_t bucket(uint32_t h1, uint64_t h2) const {
        uint64_t x = h2 ^ ((uint64_t)h1 * 0x9E3779B97F4A7C15ULL);
        x ^= x >> 32;
        return (size_t)(x & (head_.size() - 1));
    }

    // Пошук у ланцюгу: спершу O(1) порівняння хешів і довжини,
    // при збігу — memcmp за O(L) (захист від хибного спрацювання).
    int find(const char* s, int len, const Hash& h, int* prev) const {
        int pr = -1;
        for (int id = head_[bucket(h.h1, h.h2)]; id != -1; pr = id, id = pool_[id].next) {
            const Node& nd = pool_[id];
            if (nd.h2 == h.h2 && nd.h1 == h.h1 && nd.len == len &&
                std::memcmp(nd.s, s, len) == 0) {
                if (prev) *prev = pr;
                return id;
            }
        }
        return -1;
    }

    // Подвоєння таблиці; хеші збережені у вузлах, тому не перераховуються.
    void grow() {
        std::vector<int> old;
        old.swap(head_);
        head_.assign(old.size() * 2, -1);
        for (int first : old) {
            for (int id = first; id != -1;) {
                int nx = pool_[id].next;
                size_t b = bucket(pool_[id].h1, pool_[id].h2);
                pool_[id].next = head_[b];
                head_[b] = id;
                id = nx;
            }
        }
    }
};
