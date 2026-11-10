#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct KStep {
    enum Kind { SPLIT, LEAF, COMBINE } kind;
    int depth;
    std::string a, b;
    std::string a1, a0, b1, b0;
    size_t m = 0;
    std::string z0, z1, z2;
    std::string result;
};

template <uint32_t B>
class Karatsuba {
    static_assert(B == 10 || B == 10000 || B == 1000000000, "B must be 10, 10^4 or 10^9");

public:
    using Num = std::vector<uint32_t>;
    static constexpr int W = (B == 10) ? 1 : (B == 10000 ? 4 : 9);

    size_t cutoff;
    std::vector<KStep>* steps;

    explicit Karatsuba(size_t cutoffLimbs = 40, std::vector<KStep>* log = nullptr)
        : cutoff(cutoffLimbs < 4 ? 4 : cutoffLimbs), steps(log) {}

    Num multiply(const Num& a, const Num& b) { return rec(a, b, 0); }

    static Num fromString(const std::string& s) {
        Num r;
        r.reserve(s.size() / W + 1);
        size_t end = s.size();
        while (end > 0) {
            size_t start = end >= (size_t)W ? end - W : 0;
            uint32_t v = 0;
            for (size_t i = start; i < end; ++i) v = v * 10 + (uint32_t)(s[i] - '0');
            r.push_back(v);
            end = start;
        }
        trim(r);
        return r;
    }

    static std::string toString(const Num& n) {
        if (n.empty()) return "0";
        std::string out = std::to_string(n.back());
        out.reserve(n.size() * W);
        for (size_t i = n.size() - 1; i-- > 0;) {
            std::string part = std::to_string(n[i]);
            out.append(W - part.size(), '0');
            out += part;
        }
        return out;
    }

    static Num schoolbook(const Num& a, const Num& b) {
        if (a.empty() || b.empty()) return Num();
        Num r(a.size() + b.size(), 0);
        for (size_t i = 0; i < a.size(); ++i) {
            uint64_t carry = 0;
            for (size_t j = 0; j < b.size(); ++j) {
                uint64_t cur = (uint64_t)r[i + j] + (uint64_t)a[i] * b[j] + carry;
                r[i + j] = (uint32_t)(cur % B);
                carry = cur / B;
            }
            r[i + b.size()] = (uint32_t)carry;
        }
        trim(r);
        return r;
    }

private:
    static void trim(Num& a) {
        while (!a.empty() && a.back() == 0) a.pop_back();
    }

    static Num add(const Num& a, const Num& b) {
        const Num& x = a.size() >= b.size() ? a : b;
        const Num& y = a.size() >= b.size() ? b : a;
        Num r;
        r.reserve(x.size() + 1);
        uint32_t carry = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            uint64_t s = (uint64_t)x[i] + (i < y.size() ? y[i] : 0) + carry;
            if (s >= B) { s -= B; carry = 1; } else carry = 0;
            r.push_back((uint32_t)s);
        }
        if (carry) r.push_back(1);
        return r;
    }

    static void subInPlace(Num& a, const Num& b) {
        int64_t borrow = 0;
        for (size_t i = 0; i < a.size(); ++i) {
            int64_t s = (int64_t)a[i] - borrow - (i < b.size() ? (int64_t)b[i] : 0);
            if (s < 0) { s += B; borrow = 1; } else borrow = 0;
            a[i] = (uint32_t)s;
        }
        trim(a);
    }

    static void addAt(Num& res, const Num& x, size_t shift) {
        uint32_t carry = 0;
        for (size_t i = 0; i < x.size() || carry; ++i) {
            uint64_t s = (uint64_t)res[shift + i] + (i < x.size() ? x[i] : 0) + carry;
            if (s >= B) { s -= B; carry = 1; } else carry = 0;
            res[shift + i] = (uint32_t)s;
        }
    }

    Num rec(const Num& a, const Num& b, int depth) {
        if (a.empty() || b.empty()) return Num();

        if (std::min(a.size(), b.size()) <= cutoff) {
            Num r = schoolbook(a, b);
            if (steps) {
                KStep s;
                s.kind = KStep::LEAF; s.depth = depth;
                s.a = toString(a); s.b = toString(b); s.result = toString(r);
                steps->push_back(s);
            }
            return r;
        }

        const size_t n = std::max(a.size(), b.size());
        const size_t m = n / 2;
        Num a0(a.begin(), a.begin() + std::min(m, a.size())), a1;
        Num b0(b.begin(), b.begin() + std::min(m, b.size())), b1;
        if (a.size() > m) a1.assign(a.begin() + m, a.end());
        if (b.size() > m) b1.assign(b.begin() + m, b.end());
        trim(a0);
        trim(b0);

        if (steps) {
            KStep s;
            s.kind = KStep::SPLIT; s.depth = depth; s.m = m;
            s.a = toString(a); s.b = toString(b);
            s.a1 = toString(a1); s.a0 = toString(a0);
            s.b1 = toString(b1); s.b0 = toString(b0);
            steps->push_back(s);
        }

        Num z0 = rec(a0, b0, depth + 1);
        Num z2 = rec(a1, b1, depth + 1);
        Num z1 = rec(add(a0, a1), add(b0, b1), depth + 1);
        subInPlace(z1, z0);
        subInPlace(z1, z2);

        Num res(a.size() + b.size() + 2, 0);
        addAt(res, z0, 0);
        addAt(res, z1, m);
        addAt(res, z2, 2 * m);
        trim(res);

        if (steps) {
            KStep s;
            s.kind = KStep::COMBINE; s.depth = depth; s.m = m;
            s.a = toString(a); s.b = toString(b);
            s.z0 = toString(z0); s.z1 = toString(z1); s.z2 = toString(z2);
            s.result = toString(res);
            steps->push_back(s);
        }
        return res;
    }
};