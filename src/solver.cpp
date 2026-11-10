#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

#include "check.hpp"
#include "closest_pair.hpp"
#include "karatsuba.hpp"

using namespace std;

typedef chrono::steady_clock Clock;
static double msSince(Clock::time_point t0) {
    return chrono::duration<double, milli>(Clock::now() - t0).count();
}

struct Row {
    string name, size, answer;
    double ms = 0;
    bool ok = false;
};

// closest pair
static Row runClosest(const string& path, const string& name, bool showSteps, bool verbose) {
    vector<Pt> pts = readPoints(path);
    vector<CPStep> log;
    ClosestPair cp;
    Clock::time_point t0 = Clock::now();
    CPResult r = cp.run(pts, showSteps ? &log : nullptr);
    double ms = msSince(t0);

    double ref = checkClosestDistance(pts);
    bool ok = fabs(ref - r.dist) <= 1e-9 * max(1.0, ref);

    char buf[256];
    snprintf(buf, sizeof buf, "d=%.6f  (%.3f,%.3f)-(%.3f,%.3f)", r.dist, r.a.x, r.a.y, r.b.x, r.b.y);
    Row row;
    row.name = name; row.size = to_string(pts.size()) + " points"; row.answer = buf; row.ms = ms; row.ok = ok;

    if (verbose) {
        printf("input            : %s (%zu points)\n", path.c_str(), pts.size());
        printf("closest pair     : (%.6f, %.6f) and (%.6f, %.6f)\n", r.a.x, r.a.y, r.b.x, r.b.y);
        printf("distance         : %.9f\n", r.dist);
        printf("running time     : %.2f ms\n", ms);
        printf("check (sort+sweep): %.9f  -> %s\n", ref, ok ? "MATCH" : "MISMATCH");
    }
    if (showSteps) {
        for (size_t i = 0; i < log.size(); ++i) {
            const CPStep& s = log[i];
            if (s.kind == CPStep::SPLIT)
                printf("[%3zu] SPLIT  x-ranks %zu..%zu at x = %.3f\n", i, s.lo, s.hi, s.midX);
            else if (s.kind == CPStep::BASE)
                printf("[%3zu] BASE   x-ranks %zu..%zu brute force -> d = %.3f\n", i, s.lo, s.hi, s.delta);
            else
                printf("[%3zu] STRIP  x-ranks %zu..%zu line x = %.3f, %zu points in strip, d %.3f -> %.3f\n",
                            i, s.lo, s.hi, s.midX, s.strip.size(), s.deltaBefore, s.delta);
        }
    }
    return row;
}

// multiplication
static string headTail(const string& s) {
    if (s.size() <= 40) return s;
    return s.substr(0, 18) + "..." + s.substr(s.size() - 18);
}

static Row runKaratsuba(const string& path, const string& name, bool showSteps, bool verbose) {
    string a, b;
    readTwoNumbers(path, a, b);
    string prod;
    double ms;
    vector<KStep> log;

    if (showSteps) {
        typedef Karatsuba<10> K;
        K k(4, &log);  // schoolbook when the shorter number has <= 4 digits
        K::Num x = K::fromString(a), y = K::fromString(b);
        Clock::time_point t0 = Clock::now();
        K::Num z = k.multiply(x, y);
        ms = msSince(t0);
        prod = K::toString(z);
    } else {
        typedef Karatsuba<1000000000> K;
        K k(40);       // schoolbook when the shorter number has <= 40 limbs (360 digits)
        K::Num x = K::fromString(a), y = K::fromString(b);
        Clock::time_point t0 = Clock::now();
        K::Num z = k.multiply(x, y);
        ms = msSince(t0);
        prod = K::toString(z);
    }

    bool ok = checkProductModular(a, b, prod);
    // small inputs: also compare with a plain schoolbook product
    if (a.size() <= 20000 && b.size() <= 20000) {
        typedef Karatsuba<1000000000> K;
        ok = ok && (K::toString(K::schoolbook(K::fromString(a), K::fromString(b))) == prod);
    }

    Row row;
    row.name = name;
    row.size = to_string(a.size()) + " x " + to_string(b.size()) + " digits";
    row.answer = to_string(prod.size()) + " digits: " + headTail(prod);
    row.ms = ms; row.ok = ok;

    if (verbose) {
        printf("input          : %s (%zu x %zu digits)\n", path.c_str(), a.size(), b.size());
        printf("product        : %zu digits\n", prod.size());
        if (prod.size() <= 400) printf("                 %s\n", prod.c_str());
        else printf("                 %s\n", headTail(prod).c_str());
        printf("running time   : %.2f ms\n", ms);
        printf("check (mod 3 primes%s) -> %s\n", a.size() <= 20000 && b.size() <= 20000 ? " + schoolbook" : "",
                    ok ? "MATCH" : "MISMATCH");
        string full = path + ".product.txt";
        FILE* f = fopen(full.c_str(), "wb");
        if (f) { fwrite(prod.data(), 1, prod.size(), f); fputc('\n', f); fclose(f);
                 printf("full product written to %s\n", full.c_str()); }
    }
    if (showSteps) {
        for (size_t i = 0; i < log.size(); ++i) {
            const KStep& s = log[i];
            string pad(2 * s.depth, ' ');
            if (s.kind == KStep::SPLIT)
                printf("[%3zu]%s SPLIT   %s x %s  at m=%zu  (a1=%s a0=%s | b1=%s b0=%s)\n", i, pad.c_str(),
                            s.a.c_str(), s.b.c_str(), s.m, s.a1.c_str(), s.a0.c_str(), s.b1.c_str(), s.b0.c_str());
            else if (s.kind == KStep::LEAF)
                printf("[%3zu]%s LEAF    %s x %s = %s\n", i, pad.c_str(), s.a.c_str(), s.b.c_str(), s.result.c_str());
            else
                printf("[%3zu]%s COMBINE z0=%s z1=%s z2=%s  ->  %s\n", i, pad.c_str(), s.z0.c_str(), s.z1.c_str(),
                            s.z2.c_str(), s.result.c_str());
        }
    }
    return row;
}

// main
static void usage() {
    puts("usage:\n  solver cp  <file> [--steps]\n  solver im  <file> [--steps]\n  solver all <data_dir>");
}

int main(int argc, char** argv) {
    if (argc < 3) { usage(); return 1; }
    string mode = argv[1], target = argv[2];
    bool steps = argc > 3 && strcmp(argv[3], "--steps") == 0;

    if (mode == "cp") { Row r = runClosest(target, "cp", steps, true); return r.ok ? 0 : 2; }
    if (mode == "im") { Row r = runKaratsuba(target, "im", steps, true); return r.ok ? 0 : 2; }

    if (mode == "all") {
        vector<Row> rows;
        for (const char* pre : {"cp", "im"})
            for (int i = 1; i <= 10; ++i) {
                char name[16]; snprintf(name, sizeof name, "%s%02d", pre, i);
                string path = target + "/" + name + ".txt";
                ifstream probe(path);
                if (!probe) { printf("%s: missing, skipped\n", path.c_str()); continue; }
                probe.close();
                Row r = (pre[0] == 'c') ? runClosest(path, name, false, false) : runKaratsuba(path, name, false, false);
                printf("%-5s %-26s %12.2f ms  %-5s  %s\n", r.name.c_str(), r.size.c_str(), r.ms, r.ok ? "OK" : "FAIL",
                            r.answer.c_str());
                fflush(stdout);
                rows.push_back(r);
            }
        FILE* f = fopen("results.csv", "wb");
        if (f) {
            fprintf(f, "file,input size,time_ms,verified,answer\n");
            for (const Row& r : rows)
                fprintf(f, "%s,\"%s\",%.2f,%s,\"%s\"\n", r.name.c_str(), r.size.c_str(), r.ms, r.ok ? "yes" : "NO",
                             r.answer.c_str());
            fclose(f);
            puts("\nresults written to results.csv");
        }
        return 0;
    }
    usage();
    return 1;
}