#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <string>
#include <sys/stat.h>
#include <direct.h>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace std;

typedef unsigned long long u64;
static string OUT = "data";
static FILE* manifest = nullptr;

struct Rng {
    mt19937_64 g;
    explicit Rng(u64 seed) : g(seed) {}
    u64 below(u64 n) { return g() % n; }
    double unit() { return (double)(g() >> 11) / 9007199254740992.0; }
    double gauss() {
        double s = 0;
        for (int i = 0; i < 12; ++i) s += unit();
        return s - 6.0;
    }
};

static void printCoord(FILE* f, long long micro) {
    fprintf(f, "%lld.%03lld", micro / 1000, micro % 1000);
}

struct P2 { long long x, y; };

static void writePointsReal(const string& name, const vector<P2>& pts, u64 seed) {
    string path = OUT + "/" + name + ".txt";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); exit(1); }
    fprintf(f, "%zu\n", pts.size());
    for (const P2& p : pts) {
        printCoord(f, p.x); fputc(' ', f); printCoord(f, p.y); fputc('\n', f);
    }
    fclose(f);
    fprintf(manifest, "%s  seed=%llu  n=%zu\n", name.c_str(), seed, pts.size());
}

static void writePointsInt(const string& name, const vector<P2>& pts, u64 seed) {
    string path = OUT + "/" + name + ".txt";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); exit(1); }
    fprintf(f, "%zu\n", pts.size());
    for (const P2& p : pts) fprintf(f, "%lld %lld\n", p.x, p.y);
    fclose(f);
    fprintf(manifest, "%s  seed=%llu  n=%zu\n", name.c_str(), seed, pts.size());
}

static void shuffle(vector<P2>& v, Rng& r) {
    for (size_t i = v.size(); i > 1; --i) swap(v[i - 1], v[r.below(i)]);
}

static const long long MAXC = 1000000000LL;

// closest pair files
static void genUniform(int idx, size_t n) {
    char name[16]; snprintf(name, sizeof name, "cp%02d", idx);
    u64 seed = 1000 + idx; Rng r(seed);
    vector<P2> p(n);
    for (auto& q : p) { q.x = (long long)r.below(MAXC + 1); q.y = (long long)r.below(MAXC + 1); }
    writePointsReal(name, p, seed);
}

static void genClustered() { // cp05
    u64 seed = 1005; Rng r(seed);
    const int K = 10; const size_t n = 1000000;
    double cx[K], cy[K];
    for (int k = 0; k < K; ++k) { cx[k] = 100000 + r.unit() * 800000; cy[k] = 100000 + r.unit() * 800000; }
    vector<P2> p; p.reserve(n);
    while (p.size() < n) {
        int k = (int)r.below(K);
        double x = cx[k] + 20000.0 * r.gauss(), y = cy[k] + 20000.0 * r.gauss();
        if (x < 0 || x > 1000000 || y < 0 || y > 1000000) continue;
        p.push_back(P2{(long long)llround(x * 1000), (long long)llround(y * 1000)});
    }
    writePointsReal("cp05", p, seed);
}

static void genManyDuplicates() { // cp06
    u64 seed = 1006; Rng r(seed);
    vector<P2> p(1000000);
    for (auto& q : p) { q.x = (long long)r.below(1000); q.y = (long long)r.below(1000); }
    writePointsInt("cp06", p, seed);
}

static void genVerticalLine() { // cp07
    u64 seed = 1007; Rng r(seed);
    unordered_set<long long> used;
    vector<P2> p;
    while (p.size() < 100000) {
        long long y = (long long)r.below(MAXC + 1);
        if (used.insert(y).second) p.push_back(P2{500000000LL, y});
    }
    writePointsReal("cp07", p, seed);
}

static void genGrid() { // cp08
    u64 seed = 1008; Rng r(seed);
    vector<P2> p;
    for (int i = 0; i < 250; ++i)
        for (int j = 0; j < 400; ++j) p.push_back(P2{(long long)i * 1000, (long long)j * 1000});
    shuffle(p, r);
    writePointsInt("cp08", p, seed);
}

static void genAcrossMiddle() { // cp09
    u64 seed = 1009; Rng r(seed);
    const size_t half = 499999;
    vector<P2> p; p.reserve(1000000);
    for (size_t i = 0; i < half; ++i)
        p.push_back(P2{(long long)r.below(499999000ULL), (long long)r.below(MAXC + 1)});
    for (size_t i = 0; i < half; ++i)
        p.push_back(P2{500001000LL + (long long)r.below(499999001ULL), (long long)r.below(MAXC + 1)});
    p.push_back(P2{499999999LL, 250500LL * 1000});
    p.push_back(P2{500000001LL, 250500LL * 1000});
    shuffle(p, r);
    writePointsReal("cp09", p, seed);
}

static void genSmallDemoPoints() { // cp10
    u64 seed = 1010; Rng r(seed);
    set<pair<long long, long long>> seen;
    vector<P2> p;
    while (p.size() < 101) {
        long long x = (long long)r.below(1001), y = (long long)r.below(1001);
        if (seen.insert({x, y}).second) p.push_back(P2{x, y});
    }
    writePointsInt("cp10", p, seed);
}

// integer multiplication
static string randomDigits(Rng& r, size_t len) {
    string s(len, '0');
    for (size_t i = 0; i < len; ++i) s[i] = (char)('0' + r.below(10));
    if (len > 0 && s[0] == '0') s[0] = (char)('1' + r.below(9));
    return s;
}

static void writeNumbers(const string& name, const string& a, const string& b, u64 seed) {
    string path = OUT + "/" + name + ".txt";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); exit(1); }
    fwrite(a.data(), 1, a.size(), f); fputc('\n', f);
    fwrite(b.data(), 1, b.size(), f); fputc('\n', f);
    fclose(f);
    fprintf(manifest, "%s  seed=%llu  digits=%zu and %zu\n", name.c_str(), seed, a.size(), b.size());
}

static void genMulRandom(int idx, size_t digits) { // im01-im04
    char name[16]; snprintf(name, sizeof name, "im%02d", idx);
    u64 seed = 2000 + idx; Rng r(seed);
    string a = randomDigits(r, digits), b = randomDigits(r, digits);
    writeNumbers(name, a, b, seed);
}

static void genMulMain() {
    { u64 s = 2005; Rng r(s); // im05
      writeNumbers("im05", randomDigits(r, 1000000), randomDigits(r, 10), s); }
    { u64 s = 2006; Rng r(s); // im06
      writeNumbers("im06", string(100000, '9'), randomDigits(r, 100000), s); }
    { u64 s = 2007; Rng r(s); // im07
      auto sparse = [&](size_t len) {
          string t(len, '0');
          for (size_t i = 0; i < len; ++i) if (r.below(10) == 0) t[i] = (char)('1' + r.below(9));
          t[0] = (char)('1' + r.below(9));
          return t; };
      string a = sparse(99999), b = sparse(99999);
      writeNumbers("im07", a, b, s); }
    { u64 s = 2008; // im08
      string t = "1" + string(999999, '0');
      writeNumbers("im08", t, t, s); }
    { u64 s = 2009; Rng r(s); // im09
      writeNumbers("im09", "0", randomDigits(r, 1000000), s); }
    { u64 s = 2010; Rng r(s); // im10
      writeNumbers("im10", randomDigits(r, 101), randomDigits(r, 101), s); }
}

static long long fileSize(const string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 ? (long long)st.st_size : -1;
}

int main(int argc, char** argv) {
    _mkdir(OUT.c_str());
    
    manifest = fopen((OUT + "/seeds.txt").c_str(), "wb");
    if (!manifest) { perror("seeds.txt"); return 1; }

    genUniform(1, 1000); genUniform(2, 10000); genUniform(3, 100000); genUniform(4, 1000000);
    genClustered(); genManyDuplicates(); genVerticalLine(); genGrid(); genAcrossMiddle(); genSmallDemoPoints();
    genMulRandom(1, 1000); genMulRandom(2, 10000); genMulRandom(3, 100000); genMulRandom(4, 1000000);
    genMulMain();
    fclose(manifest);

    printf("Generated 20 files in %s/ (seeds in seeds.txt)\n\n", OUT.c_str());
    printf("%-6s %12s   %s\n", "file", "bytes", "submit? (only files < 1 MB are submitted)");
    for (const char* pre : {"cp", "im"})
        for (int i = 1; i <= 10; ++i) {
            char name[16]; snprintf(name, sizeof name, "%s%02d", pre, i);
            long long sz = fileSize(OUT + "/" + name + ".txt");
            printf("%-6s %12lld   %s\n", name, sz, sz >= 0 && sz < 1000000 ? "yes" : "no (regenerated at marking)");
        }
    return 0;
}