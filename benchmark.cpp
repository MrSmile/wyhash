#include <ctime>
#include <vector>
#include <cstdio>
#include <cstdlib>
#define _wyp wy_secret
#include "wyhash.h"

using namespace std;


struct Test1 {
    uint32_t a, b;
    uint64_t c;
};

struct Test2 {
    uint32_t a;
    uint8_t b;
    uint64_t c;
    uint32_t d;
    uint16_t e;
};

struct Hash {
    static uint64_t hash(const void *ptr, size_t len, uint64_t seed)
    {
        return wyhash(ptr, len, seed, wy_secret);
    }

    template<typename T> static uint64_t hash(const T &val, uint64_t seed)
    {
        return wyhash(&val, sizeof(T), seed, wy_secret);
    }

    template<typename T1, typename T2> static uint64_t hash(const pair<T1, T2> &val, uint64_t seed)
    {
        seed = hash(val.first,  seed);
        seed = hash(val.second, seed);
        return seed;
    }

    static uint64_t hash(const Test1 &val, uint64_t seed)
    {
        seed = hash(val.a, seed);
        seed = hash(val.b, seed);
        seed = hash(val.c, seed);
        return seed;
    }

    static uint64_t hash(const Test2 &val, uint64_t seed)
    {
        seed = hash(val.a, seed);
        seed = hash(val.b, seed);
        seed = hash(val.c, seed);
        seed = hash(val.d, seed);
        seed = hash(val.e, seed);
        return seed;
    }
};


char buffer[1 << 20];
constexpr unsigned iter_count = 256;

void init_buffer()
{
    for (char &c : buffer)
        c = rand();
}

template<typename H, typename T> uint64_t fixed_test(const char *name, uint64_t seed)
{
    size_t size = sizeof(buffer) / sizeof(T);
    auto ptr = reinterpret_cast<const T *>(buffer);

    clock_t tm = clock();
    for (unsigned iter = 0; iter < iter_count; iter++)
        for (size_t i = 0; i < size; i++)
            seed = H::hash(ptr[i], seed);
    tm = clock() - tm;

    double factor = 1e-9 * sizeof(buffer) * CLOCKS_PER_SEC * iter_count;
    printf("Fixed   %s: %6.3f GB/s\n", name, factor / tm);
    return seed;
}

template<typename H> uint64_t dynamic_test(const vector<pair<unsigned, unsigned>> &offs, uint64_t seed)
{
    clock_t tm = clock();
    for (unsigned iter = 0; iter < iter_count; iter++)
        for (const pair<unsigned, unsigned> &pos : offs)
            seed = H::hash(buffer + pos.first, pos.second, seed);
    tm = clock() - tm;

    double factor = 1e-9 * sizeof(buffer) * CLOCKS_PER_SEC * iter_count;
    printf(" %6.3f GB/s", factor / tm);
    return seed;
}

vector<pair<unsigned, unsigned>> generate_sequence(unsigned min, unsigned max, unsigned align)
{
    align--;
    vector<pair<unsigned, unsigned>> res;
    for (size_t pos = 0; pos < sizeof(buffer); ) {
        unsigned len = min + rand() % (max - min);
        res.emplace_back(pos, len);
        pos = (pos + len + align) & ~align;
    }
    res.back().second = sizeof(buffer) - res.back().first;
    return res;
}

template<typename H> uint64_t dynamic_test(const char *name,
    unsigned min, unsigned max, unsigned align, uint64_t seed)
{
    printf("Dynamic %s:", name);
    vector<pair<unsigned, unsigned>> offs = generate_sequence(min, max, align);
    seed = dynamic_test<H>(offs, seed);

    for (size_t i = offs.size() - 1; i; i--)
        swap(offs[i], offs[rand() % (i + 1)]);
    seed = dynamic_test<H>(offs, seed);
    printf("\n");

    return seed;
}


uint64_t rand64()
{
    return (uint64_t) rand() << 48 ^ (uint64_t) rand() << 32 ^ (uint64_t) rand() << 16 ^ (uint64_t) rand();
}

inline unsigned popcount(uint64_t x)
{
    return __builtin_popcountll(x);
}

struct Quality {
    double flip, one_bit;
};

Quality test_quality(unsigned len, uint64_t (*hash)(const void *ptr, size_t len, uint64_t seed))
{
    static constexpr unsigned n_iter = 256, guard = 8;

    unsigned size = len + 2 * guard;
    vector<uint8_t> data(size);
    vector<unsigned> sum(8 * size, 0);
    for (unsigned iter = 0; iter < n_iter; iter++) {
        uint64_t seed = rand64();
        for (uint8_t &val : data)
            val = rand();

        uint64_t base = hash(data.data() + guard, len, seed);
        for (unsigned i = 0; i < 8 * size; i++) {
            uint8_t bit = 1 << (i & 7);
            data[i >> 3] ^= bit;
            sum[i] += popcount(hash(data.data() + guard, len, seed) ^ base);
            data[i >> 3] ^= bit;
        }
    }

    unsigned stop = 8 * len + 8 * guard;
    for (unsigned i = 0; i < 8 * guard; i++)
        if (sum[i] || sum[i + stop])
            return {-1, -1};

    unsigned worst = -1;
    for (unsigned i = 8 * guard; i < stop; i++)
        worst = min(worst, sum[i]);

    unsigned one_bit_sum = 0;
    vector<uint64_t> one_bit_hash(8 * len);
    for (uint8_t pattern : {0, -1}) {
        for (uint8_t &val : data)
            val = pattern;

        for (unsigned iter = 0; iter < n_iter; iter++) {
            unsigned one_bit = 64;
            uint64_t seed = rand64();
            for (unsigned i = 0; i < 8 * len; i++) {
                data[guard + (i >> 3)] = pattern ^ (1 << (i & 7));
                one_bit_hash[i] = hash(data.data() + guard, len, seed);
                data[guard + (i >> 3)] = pattern;

                for (unsigned j = 0; j < i; j++)
                    one_bit = min(one_bit, popcount(one_bit_hash[i] ^ one_bit_hash[j]));
            }
            one_bit_sum += one_bit;
        }
    }
    static constexpr double mul = 1 / double(n_iter);
    return {worst * mul, one_bit_sum * (mul / 2)};
}

double test_quality(uint64_t (*hash)(const void *ptr, size_t len, uint64_t seed))
{
    double worst = 64;
    for (unsigned len = 1; len <= 64; len++) {
        Quality res = test_quality(len, hash);
        printf("Length %2u:  ", len);
        if (res.flip >= 0) {
            printf("%5.2f  %5.2f\n", res.flip, res.one_bit);
            worst = min(worst, res.flip);
            continue;
        }
        printf("FAIL\n");
        return -1;
    }
    return worst;
}


int main()
{
    if (test_quality(&Hash::hash) < 24)
        return 0;

    printf("==================================================\n");

    init_buffer();
    uint64_t seed = rand64();

    seed = fixed_test<Hash,  uint8_t>("    1", seed);
    seed = fixed_test<Hash, uint16_t>("    2", seed);
    seed = fixed_test<Hash, uint32_t>("    4", seed);
    seed = fixed_test<Hash, uint64_t>("    8", seed);
    seed = fixed_test<Hash, pair<uint32_t, uint32_t>>("  4x2", seed);
    seed = fixed_test<Hash, pair<uint64_t, uint64_t>>("  8x2", seed);
    seed = fixed_test<Hash, Test1>("  st1", seed);
    seed = fixed_test<Hash, Test2>("  st2", seed);

    seed = dynamic_test<Hash>("  8a1", 1,     8, 1, seed);
    seed = dynamic_test<Hash>("  8a4", 1,     8, 4, seed);
    seed = dynamic_test<Hash>("  8a8", 1,     8, 8, seed);
    seed = dynamic_test<Hash>(" 16a1", 1,    16, 1, seed);
    seed = dynamic_test<Hash>(" 16a4", 1,    16, 4, seed);
    seed = dynamic_test<Hash>(" 16a8", 1,    16, 8, seed);
    seed = dynamic_test<Hash>(" 24a1", 1,    24, 1, seed);
    seed = dynamic_test<Hash>(" 24a4", 1,    24, 4, seed);
    seed = dynamic_test<Hash>(" 24a8", 1,    24, 8, seed);
    seed = dynamic_test<Hash>("256a1", 1,   256, 1, seed);
    seed = dynamic_test<Hash>("256a4", 1,   256, 4, seed);
    seed = dynamic_test<Hash>("256a8", 1,   256, 8, seed);
    seed = dynamic_test<Hash>(" 4Ka1", 1,  4096, 1, seed);
    seed = dynamic_test<Hash>(" 4Ka4", 1,  4096, 4, seed);
    seed = dynamic_test<Hash>(" 4Ka8", 1,  4096, 8, seed);
    seed = dynamic_test<Hash>("64Ka1", 1, 65536, 1, seed);
    seed = dynamic_test<Hash>("64Ka4", 1, 65536, 4, seed);
    seed = dynamic_test<Hash>("64Ka8", 1, 65536, 8, seed);

    printf("==================================================\n");
    return seed;
}
