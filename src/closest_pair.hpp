#pragma once
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

struct Pt {
    double x, y;
};

struct CPStep {
    enum Kind { SPLIT, BASE, STRIP } kind;
    size_t lo, hi;
    double midX;
    double deltaBefore;
    double delta;
    Pt a, b;
    std::vector<Pt> strip;
};

struct CPResult {
    Pt a, b;
    double dist;
};

class ClosestPair {
public:
    std::vector<Pt> xsorted;

    CPResult run(std::vector<Pt> pts, std::vector<CPStep>* stepLog = nullptr) {
        P = std::move(pts);
        steps = stepLog;
        const size_t n = P.size();
        if (n < 2) return {Pt{0, 0}, Pt{0, 0}, std::numeric_limits<double>::infinity()};
        T.assign(n, Pt{0, 0});
        sortByX();
        if (steps) xsorted = P;
        Best b = rec(0, n - 1);
        return {b.a, b.b, std::sqrt(b.d2)};
    }

private:
    struct Best {
        double d2;
        Pt a, b;
    };
    std::vector<Pt> P, T, strip;
    std::vector<CPStep>* steps = nullptr;
    static constexpr double INF = std::numeric_limits<double>::infinity();

    static double dist2(const Pt& p, const Pt& q) {
        double dx = p.x - q.x, dy = p.y - q.y;
        return dx * dx + dy * dy;
    }
    static bool xLess(const Pt& p, const Pt& q) { return p.x < q.x || (p.x == q.x && p.y < q.y); }
    static bool yLess(const Pt& p, const Pt& q) { return p.y < q.y || (p.y == q.y && p.x < q.x); }

    // Bottom-up merge sort by x
    void sortByX() {
        const size_t n = P.size();
        std::vector<Pt>* src = &P;
        std::vector<Pt>* dst = &T;
        for (size_t w = 1; w < n; w *= 2) {
            for (size_t lo = 0; lo < n; lo += 2 * w) {
                size_t mid = lo + w < n ? lo + w : n;
                size_t hi = lo + 2 * w < n ? lo + 2 * w : n;
                size_t i = lo, j = mid, k = lo;
                while (i < mid && j < hi) {
                    if (xLess((*src)[j], (*src)[i])) (*dst)[k++] = (*src)[j++];
                    else (*dst)[k++] = (*src)[i++];
                }
                while (i < mid) (*dst)[k++] = (*src)[i++];
                while (j < hi) (*dst)[k++] = (*src)[j++];
            }
            std::swap(src, dst);
        }
        if (src != &P) P = *src;
    }

    Best rec(size_t lo, size_t hi) {
        const size_t n = hi - lo + 1;
        if (n <= 3) {
            Best r{INF, Pt{0, 0}, Pt{0, 0}};
            for (size_t i = lo; i <= hi; ++i)
                for (size_t j = i + 1; j <= hi; ++j) {
                    double d = dist2(P[i], P[j]);
                    if (d < r.d2) r = Best{d, P[i], P[j]};
                }
            for (size_t i = lo + 1; i <= hi; ++i) {
                Pt key = P[i];
                size_t j = i;
                while (j > lo && yLess(key, P[j - 1])) {
                    P[j] = P[j - 1];
                    --j;
                }
                P[j] = key;
            }
            if (steps) {
                CPStep s;
                s.kind = CPStep::BASE; s.lo = lo; s.hi = hi; s.midX = 0;
                s.deltaBefore = -1; s.delta = std::sqrt(r.d2); s.a = r.a; s.b = r.b;
                steps->push_back(s);
            }
            return r;
        }

        const size_t mid = lo + (hi - lo) / 2;  // left 
        const double midX = P[mid].x;
        if (steps) {
            CPStep s;
            s.kind = CPStep::SPLIT; s.lo = lo; s.hi = hi; s.midX = midX;
            s.deltaBefore = -1; s.delta = -1; s.a = Pt{0, 0}; s.b = Pt{0, 0};
            steps->push_back(s);
        }

        Best L = rec(lo, mid);
        Best R = rec(mid + 1, hi);
        Best best = (L.d2 <= R.d2) ? L : R;
        const double before = std::sqrt(best.d2);

        // merge the two y-sorted halves
        {
            size_t i = lo, j = mid + 1, k = lo;
            while (i <= mid && j <= hi) {
                if (yLess(P[j], P[i])) T[k++] = P[j++];
                else T[k++] = P[i++];
            }
            while (i <= mid) T[k++] = P[i++];
            while (j <= hi) T[k++] = P[j++];
            for (size_t t = lo; t <= hi; ++t) P[t] = T[t];
        }

        strip.clear();
        for (size_t t = lo; t <= hi; ++t) {
            double dx = P[t].x - midX;
            if (dx * dx < best.d2) strip.push_back(P[t]);
        }
        for (size_t i = 0; i < strip.size(); ++i) {
            for (size_t j = i + 1; j < strip.size(); ++j) {
                double dy = strip[j].y - strip[i].y;
                if (dy * dy >= best.d2) break;
                double d = dist2(strip[i], strip[j]);
                if (d < best.d2) best = Best{d, strip[i], strip[j]};
            }
        }
        if (steps) {
            CPStep s;
            s.kind = CPStep::STRIP; s.lo = lo; s.hi = hi; s.midX = midX;
            s.deltaBefore = before; s.delta = std::sqrt(best.d2);
            s.a = best.a; s.b = best.b; s.strip = strip;
            steps->push_back(s);
        }
        return best;
    }
};