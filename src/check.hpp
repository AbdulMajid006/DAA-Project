#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "closest_pair.hpp"

inline std::vector<Pt> readPoints(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "cannot open " << path << "\n"; std::exit(1); }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string buf = ss.str();
    const char* p = buf.c_str();
    char* e = nullptr;
    long n = std::strtol(p, &e, 10);
    p = e;
    std::vector<Pt> pts;
    pts.reserve(n > 0 ? (size_t)n : 0);
    for (long i = 0; i < n; ++i) {
        double x = std::strtod(p, &e); p = e;
        double y = std::strtod(p, &e); p = e;
        pts.push_back(Pt{x, y});
    }
    return pts;
}

inline std::string trimWs(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

inline void readTwoNumbers(const std::string& path, std::string& a, std::string& b) {
    std::ifstream in(path, std::ios::binary);
    if (!in) { std::cerr << "cannot open " << path << "\n"; std::exit(1); }
    std::getline(in, a);
    std::getline(in, b);
    a = trimWs(a);
    b = trimWs(b);
    if (a.empty()) a = "0";
    if (b.empty()) b = "0";
}

// closest pair checker
inline double checkClosestDistance(std::vector<Pt> p) {
    const size_t n = p.size();
    if (n < 2) return std::numeric_limits<double>::infinity();
    double minx = p[0].x, maxx = p[0].x, miny = p[0].y, maxy = p[0].y;
    for (const Pt& q : p) {
        minx = std::min(minx, q.x); maxx = std::max(maxx, q.x);
        miny = std::min(miny, q.y); maxy = std::max(maxy, q.y);
    }
    if (maxx - minx < maxy - miny)
        for (Pt& q : p) std::swap(q.x, q.y);
    std::sort(p.begin(), p.end(), [](const Pt& a, const Pt& b) { return a.x < b.x; });
    double best = std::numeric_limits<double>::infinity();
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            double dx = p[j].x - p[i].x;
            if (dx * dx >= best) break;
            double dy = p[j].y - p[i].y;
            double d = dx * dx + dy * dy;
            if (d < best) best = d;
        }
        if (best == 0) break;
    }
    return std::sqrt(best);
}

// multiplication checker
inline uint64_t decimalMod(const std::string& s, uint64_t mod) {
    uint64_t r = 0;
    for (char c : s) r = (r * 10 + (uint64_t)(c - '0')) % mod;
    return r;
}

inline bool checkProductModular(const std::string& a, const std::string& b, const std::string& prod) {
    const uint64_t primes[3] = {1000000007ULL, 998244353ULL, 1000000009ULL};
    for (uint64_t p : primes) {
        uint64_t lhs = (decimalMod(a, p) * decimalMod(b, p)) % p;
        if (lhs != decimalMod(prod, p)) return false;
    }
    return true;
}