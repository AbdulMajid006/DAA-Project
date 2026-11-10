#include "raylib.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <exception>
#include <string>
#include <vector>

#include "check.hpp"
#include "closest_pair.hpp"
#include "karatsuba.hpp"

using namespace std;

static const int WIN_W = 1280, WIN_H = 720;
static const Color C_BG      = {13, 16, 26, 255};
static const Color C_PANEL   = {22, 27, 42, 255};
static const Color C_PANEL2  = {32, 38, 60, 255};
static const Color C_BORDER  = {47, 55, 84, 255};
static const Color C_TEXT    = {232, 236, 247, 255};
static const Color C_DIM     = {190, 200, 220, 255};
static const Color C_BLUE    = {96, 165, 250, 255};
static const Color C_ORANGE  = {251, 146, 60, 255};
static const Color C_GREEN   = {74, 222, 128, 255};
static const Color C_RED     = {248, 113, 113, 255};
static const Color C_YELLOW  = {250, 204, 21, 255};
static const Color C_PURPLE  = {167, 139, 250, 255};
static const Color C_POINT   = {92, 103, 138, 255};

static Font  gF, gM;
static float gSpF = 0, gSpM = 0;

static Color Alpha(Color c, float a) { c.a = (unsigned char)(max(0.f, min(1.f, a)) * 255); return c; }
static Color Mix(Color a, Color b, float t) {
    return Color{(unsigned char)(a.r + (b.r - a.r) * t), (unsigned char)(a.g + (b.g - a.g) * t),
                 (unsigned char)(a.b + (b.b - a.b) * t), 255};
}
static float ClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static string Fmt(const char* f, ...) {
    char b[1024];
    va_list ap; va_start(ap, f); vsnprintf(b, sizeof b, f, ap); va_end(ap);
    return b;
}
static string Commas(unsigned long long v) {
    string s = to_string(v);
    for (int i = (int)s.size() - 3; i > 0; i -= 3) s.insert((size_t)i, ",");
    return s;
}
static string FmtMs(double ms) {
    if (ms < 1.0) return Fmt("%.0f us", ms * 1000.0);
    if (ms < 1000.0) return Fmt("%.1f ms", ms);
    return Fmt("%.2f s", ms / 1000.0);
}

static float ReadableSize(float size) {
    return max(20.0f, size);
}

static void T(const string& s, float x, float y, float size, Color c, bool mono = false) {
    size = ReadableSize(size);
    DrawTextEx(mono ? gM : gF, s.c_str(), Vector2{x, y}, size, (mono ? gSpM : gSpF) * size, c);
}
static float TW(const string& s, float size, bool mono = false) {
    size = ReadableSize(size);
    return MeasureTextEx(mono ? gM : gF, s.c_str(), size, (mono ? gSpM : gSpF) * size).x;
}
static void TR(const string& s, float xr, float y, float size, Color c, bool mono = false) {
    T(s, xr - TW(s, size, mono), y, size, c, mono);
}
static void TC(const string& s, float cx, float y, float size, Color c, bool mono = false) {
    T(s, cx - TW(s, size, mono) / 2, y, size, c, mono);
}

static void RRect(Rectangle r, float radius, Color c) {
    float m = min(r.width, r.height);
    DrawRectangleRounded(r, min(1.0f, 2 * radius / m), 10, c);
}
static void Card(Rectangle r, float radius = 14) {
    RRect(Rectangle{r.x - 1, r.y - 1, r.width + 2, r.height + 2}, radius, C_BORDER);
    RRect(r, radius, C_PANEL);
}
static float Chip(float x, float y, const string& s, Color c, float size = 12) {
    size = ReadableSize(size);
    float w = TW(s, size) + 18;
    RRect(Rectangle{x, y, w, size + 10}, (size + 10) / 2, Alpha(c, 0.18f));
    T(s, x + 9, y + 4, size, c);
    return w;
}
static bool Hover(Rectangle r) { return CheckCollisionPointRec(GetMousePosition(), r); }

static bool Btn(Rectangle r, const string& label, Color base, bool enabled = true, float size = 15) {
    bool hv = enabled && Hover(r);
    bool dn = hv && IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    Color c = !enabled ? C_PANEL2 : (dn ? Mix(base, BLACK, 0.25f) : (hv ? Mix(base, WHITE, 0.18f) : base));
    Rectangle rr = r;
    if (dn) rr.y += 1;
    RRect(rr, 10, c);
    Color tc = !enabled ? C_DIM : (base.r + base.g + base.b > 480 ? Color{20, 24, 38, 255} : C_TEXT);
    TC(label, r.x + r.width / 2, r.y + (r.height - size) / 2 - 1 + (dn ? 1 : 0), size, tc);
    return hv && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

static float Prose(const string& s, float x, float y, float w, float size, Color c, float gap = 5) {
    size = ReadableSize(size);
    string line, word;
    float lh = size + gap;
    auto flushWord = [&]() {
        if (word.empty()) return;
        string t = line.empty() ? word : line + " " + word;
        if (!line.empty() && TW(t, size) > w) {
            T(line, x, y, size, c);
            y += lh;
            line = word;
        } else line = t;
        word.clear();
    };
    for (char ch : s) {
        if (ch == ' ') flushWord();
        else if (ch == '\n') { flushWord(); T(line, x, y, size, c); y += lh; line.clear(); }
        else word += ch;
    }
    flushWord();
    if (!line.empty()) { T(line, x, y, size, c); y += lh; }
    return y;
}

static float Digits(const string& s, float x, float y, float w, float size, const function<Color(size_t)>& colorOf) {
    size = ReadableSize(size);
    float cw = MeasureTextEx(gM, "0", size, gSpM * size).x + gSpM * size;
    int per = max(1, (int)(w / cw));
    float lh = size + 5;
    char buf[2] = {0, 0};
    for (size_t i = 0; i < s.size(); ++i) {
        buf[0] = s[i];
        DrawTextEx(gM, buf, Vector2{x + (float)(i % (size_t)per) * cw, y + (float)(i / (size_t)per) * lh}, size, 0, colorOf(i));
    }
    size_t rows = s.empty() ? 1 : (s.size() + (size_t)per - 1) / (size_t)per;
    return y + (float)rows * lh;
}
static float DigitsOne(const string& s, float x, float y, float w, float size, Color c) {
    return Digits(s, x, y, w, size, [c](size_t) { return c; });
}

enum class Screen { Welcome, Steps, Result };
enum class Kind { None, Points, Numbers };

struct FileItem { string path, name; int bytes; int group; };
struct LogView { float scroll = 0, want = 0; int lastCur = -1; };

static const int SMALL_POINTS = 300;
static const int SMALL_DIGITS = 256;

struct App {
    string dataDir = "data";
    vector<FileItem> files;
    float listScroll = 0;
    string cur, curName;
    Kind kind = Kind::None;
    Screen screen = Screen::Welcome;
    string toast; float toastT = 0;
    string pendingPath; int pendingFrames = 0;
    float t = 0;

    // closest pair
    vector<Pt> pts, xs, sample, zoom;
    vector<CPStep> cs;
    CPResult cres{};
    double cms = 0, cref = 0; bool cok = false;
    vector<int> cdepth; vector<double> cbest;

    // multiplication
    string a, b, prod;
    vector<KStep> ks;
    double kms = 0; bool kok = false, kSmallCheck = false;

    // Manual navigation
    int step = 0;
    int speedIdx = 0;
    LogView log;
} A;

static void Toast(const string& s, float seconds = 8.0f) { A.toast = s; A.toastT = seconds; }

static bool EndsWith(const string& s, const char* suf) {
    size_t n = strlen(suf);
    return s.size() >= n && s.compare(s.size() - n, n, suf) == 0;
}

static void RefreshFiles() {
    A.files.clear();
    if (!DirectoryExists(A.dataDir.c_str())) return;
    FilePathList l = LoadDirectoryFilesEx(A.dataDir.c_str(), ".txt", false);
    for (unsigned i = 0; i < l.count; ++i) {
        string path = l.paths[i], name = GetFileName(l.paths[i]);
        if (name == "seeds.txt" || EndsWith(name, ".product.txt")) continue;
        int group = name.rfind("cp", 0) == 0 ? 0 : (name.rfind("im", 0) == 0 ? 1 : 2);
        A.files.push_back(FileItem{path, name, GetFileLength(path.c_str()), group});
    }
    UnloadDirectoryFiles(l);
    sort(A.files.begin(), A.files.end(), [](const FileItem& p, const FileItem& q) {
        return p.group != q.group ? p.group < q.group : p.name < q.name;
    });
}

static bool AllDigits(const string& s) {
    if (s.empty()) return false;
    for (char c : s) if (c < '0' || c > '9') return false;
    return true;
}

static Kind Detect(const string& path) {
    ifstream in(path, ios::binary);
    if (!in) return Kind::None;
    string l1, l2;
    getline(in, l1);
    getline(in, l2);
    l1 = trimWs(l1); l2 = trimWs(l2);
    if (AllDigits(l1) && l2.find(' ') != string::npos) return Kind::Points;
    if (AllDigits(l1) && AllDigits(l2)) return Kind::Numbers;
    return Kind::None;
}

typedef chrono::steady_clock Clock;
static double MsSince(Clock::time_point t0) { return chrono::duration<double, milli>(Clock::now() - t0).count(); }

static void ClearData() {
    A.pts.clear(); A.xs.clear(); A.sample.clear(); A.zoom.clear(); A.cs.clear();
    A.cdepth.clear(); A.cbest.clear();
    A.a.clear(); A.b.clear(); A.prod.clear(); A.ks.clear();
    A.step = 0; A.log = LogView();
}

static void PrepareCP() {
    size_t N = A.cs.size();
    A.cdepth.assign(N, 0);
    A.cbest.assign(N, 0);
    int d = 0;
    double best = numeric_limits<double>::infinity();
    for (size_t i = 0; i < N; ++i) {
        const CPStep& s = A.cs[i];
        if (s.kind == CPStep::SPLIT) { A.cdepth[i] = d; ++d; }
        else if (s.kind == CPStep::BASE) { A.cdepth[i] = d; best = min(best, s.delta); }
        else { --d; A.cdepth[i] = d; best = min(best, s.delta); }
        A.cbest[i] = best;
    }
}

static void LoadPoints(const string& path) {
    vector<Pt> pts = readPoints(path);
    if (pts.size() < 2) { Toast("Need at least 2 points"); return; }
    ClearData();
    A.pts = pts;
    A.kind = Kind::Points;
    ClosestPair cp;
    Clock::time_point t0 = Clock::now();
    A.cres = cp.run(A.pts);
    A.cms = MsSince(t0);
    A.cref = checkClosestDistance(A.pts);
    A.cok = fabs(A.cref - A.cres.dist) <= 1e-9 * max(1.0, A.cref);

    if ((int)A.pts.size() <= SMALL_POINTS) {
        ClosestPair cp2;
        A.cres = cp2.run(A.pts, &A.cs);
        A.xs = cp2.xsorted;
        PrepareCP();
        A.screen = Screen::Steps;
    } else {
        size_t n = A.pts.size();
        size_t stride = max<size_t>(1, n / 6000);
        for (size_t i = 0; i < n; i += stride) A.sample.push_back(A.pts[i]);
        double hw = max(A.cres.dist * 6.0, 1e-3);
        double mx = (A.cres.a.x + A.cres.b.x) / 2, my = (A.cres.a.y + A.cres.b.y) / 2;
        for (const Pt& p : A.pts)
            if (fabs(p.x - mx) <= hw && fabs(p.y - my) <= hw && A.zoom.size() < 500) A.zoom.push_back(p);
        A.screen = Screen::Result;
    }
}

static void LoadNumbers(const string& path) {
    string a, b;
    readTwoNumbers(path, a, b);
    ClearData();
    A.a = a; A.b = b;
    A.kind = Kind::Numbers;
    typedef Karatsuba<1000000000> K9;
    K9 k(40);
    K9::Num x = K9::fromString(a), y = K9::fromString(b);
    Clock::time_point t0 = Clock::now();
    K9::Num z = k.multiply(x, y);
    A.kms = MsSince(t0);
    A.prod = K9::toString(z);
    A.kok = checkProductModular(a, b, A.prod);
    A.kSmallCheck = a.size() <= 20000 && b.size() <= 20000;
    if (A.kSmallCheck) A.kok = A.kok && (K9::toString(K9::schoolbook(x, y)) == A.prod);

    bool small = (int)a.size() <= SMALL_DIGITS && (int)b.size() <= SMALL_DIGITS;
    A.screen = Screen::Result;
    if (small) {
        typedef Karatsuba<10> K10;
        K10 k10(4, &A.ks);
        K10::Num xa = K10::fromString(a), xb = K10::fromString(b);
        k10.multiply(xa, xb);
        if (!A.ks.empty()) A.screen = Screen::Steps;
    }
}

static void LoadFile(const string& path) {
    Kind k = Detect(path);
    if (k == Kind::None) { Toast("Unrecognised file. Expected a points file or a two-number file.", 3.0f); return; }
    A.cur = path;
    A.curName = GetFileName(path.c_str());
    if (k == Kind::Points) LoadPoints(path); else LoadNumbers(path);
}

static void QueueLoad(const string& path) { A.pendingPath = path; A.pendingFrames = 2; }

static int StepCount() { return A.kind == Kind::Points ? (int)A.cs.size() : (int)A.ks.size(); }

static void DrawLog(Rectangle r, int N, int cur, const function<string(int)>& label,
                    const function<Color(int)>& colorOf, const function<int(int)>& indentOf) {
    const float rowH = 28, head = 34;
    RRect(r, 12, C_PANEL2);
    T("STEP LOG", r.x + 14, r.y + 8, 11.5f, C_DIM);
    Rectangle body{r.x + 4, r.y + head, r.width - 8, r.height - head - 6};
    float total = (float)N * rowH;
    float maxScroll = max(0.f, total - body.height);

    if (cur != A.log.lastCur) {
        bool first = (A.log.lastCur == -1);
        A.log.lastCur = cur;
        A.log.want = ClampF((float)cur * rowH - body.height / 2 + rowH / 2, 0, maxScroll);
        if (first) A.log.scroll = A.log.want;
    }
    if (Hover(body)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0) { A.log.want = ClampF(A.log.scroll - wheel * rowH * 3, 0, maxScroll); A.log.scroll = A.log.want; }
    }
    A.log.scroll += (A.log.want - A.log.scroll) * min(1.0f, GetFrameTime() * 10.0f);

    BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    int first = max(0, (int)(A.log.scroll / rowH));
    int last = min(N - 1, (int)((A.log.scroll + body.height) / rowH) + 1);
    for (int i = first; i <= last; ++i) {
        Rectangle row{body.x, body.y + (float)i * rowH - A.log.scroll, body.width, rowH - 2};
        bool hv = Hover(row) && Hover(body);
        if (i == cur) RRect(row, 6, Alpha(colorOf(i), 0.28f));
        else if (hv) RRect(row, 6, Alpha(C_TEXT, 0.07f));
        float ind = 8.0f + (float)indentOf(i) * 3.0f;
        DrawCircle((int)(row.x + ind + 3), (int)(row.y + rowH / 2 - 1), 3.5f, colorOf(i));
        T(label(i), row.x + ind + 14, row.y + 3, 12.5f, i == cur ? C_TEXT : C_DIM, true);
        if (hv && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) A.step = i;
    }
    EndScissorMode();
}

static void DrawControls(Rectangle r, int N) {
    Card(r);
    static const int JUMPS[] = {1, 2, 4, 8};
    float y = r.y + 12;
    if (Btn(Rectangle{r.x + 14, y, 90, 42}, "Reset", C_PANEL2, N > 0, 22))
        A.step = 0;
    if (Btn(Rectangle{r.x + 114, y, 90, 42}, "Prev", C_PANEL2, N > 0 && A.step > 0, 22))
        A.step = max(0, A.step - JUMPS[A.speedIdx]);
    if (Btn(Rectangle{r.x + 214, y, 90, 42}, "Next", C_BLUE, N > 0 && A.step < N - 1, 22))
        A.step = min(N - 1, A.step + JUMPS[A.speedIdx]);
    if (Btn(Rectangle{r.x + 314, y, 90, 42}, "End", C_PANEL2, N > 0, 22))
        A.step = N - 1;

    T("Speed", r.x + 426, y + 11, 20, C_DIM);
    if (Btn(Rectangle{r.x + 490, y, 40, 42}, "-", C_PANEL2, A.speedIdx > 0, 24))
        --A.speedIdx;
    TC(Fmt("%d / click", JUMPS[A.speedIdx]), r.x + 595, y + 11, 20, C_TEXT);
    if (Btn(Rectangle{r.x + 660, y, 40, 42}, "+", C_PANEL2, A.speedIdx < 3, 24))
        ++A.speedIdx;
    TR(Fmt("Step %d / %d", N > 0 ? A.step + 1 : 0, N),
       r.x + r.width - 18, y + 11, 20, C_TEXT);

    Rectangle pb{r.x + 18, r.y + 68, r.width - 36, 8};
    RRect(pb, 4, C_PANEL2);
    float frac = N > 1 ? (float)A.step / (float)(N - 1) : (N == 1 ? 1.0f : 0.0f);
    if (frac > 0)
        RRect(Rectangle{pb.x, pb.y, max(8.0f, pb.width * frac), pb.height}, 4, C_BLUE);
}

// closest pair
struct Map {
    float ox, oy, s; double minx, maxy;
    Vector2 To(const Pt& p) const { return Vector2{ox + (float)(p.x - minx) * s, oy + (float)(maxy - p.y) * s}; }
    float X(double x) const { return ox + (float)(x - minx) * s; }
};

static Map MakeMap(const vector<Pt>& pts, Rectangle area, float pad) {
    double minx = pts[0].x, maxx = pts[0].x, miny = pts[0].y, maxy = pts[0].y;
    for (const Pt& p : pts) {
        minx = min(minx, p.x); maxx = max(maxx, p.x);
        miny = min(miny, p.y); maxy = max(maxy, p.y);
    }
    double rx = max(maxx - minx, 1e-9), ry = max(maxy - miny, 1e-9);
    if (maxx - minx < 1e-12) rx = 1;
    if (maxy - miny < 1e-12) ry = 1;
    float s = (float)min((area.width - 2 * pad) / rx, (area.height - 2 * pad) / ry);
    Map m;
    m.s = s; m.minx = minx; m.maxy = maxy;
    m.ox = area.x + (area.width - (float)rx * s) / 2;
    m.oy = area.y + (area.height - (float)ry * s) / 2;
    return m;
}

static Color KindColorCP(int k) { return k == CPStep::SPLIT ? C_BLUE : (k == CPStep::BASE ? C_GREEN : C_YELLOW); }
static const char* KindNameCP(int k) { return k == CPStep::SPLIT ? "SPLIT" : (k == CPStep::BASE ? "BASE CASE" : "COMBINE / STRIP"); }

static void DrawCPCanvas(Rectangle R, int idx) {
    Card(R);
    T("Closest pair of points", R.x + 18, R.y + 14, 15, C_TEXT);
    T(Fmt("n = %zu points", A.xs.size()), R.x + 18, R.y + 34, 12, C_DIM);

    Rectangle area{R.x + 6, R.y + 56, R.width - 12, R.height - 112};
    Map m = MakeMap(A.xs, area, 30);
    const CPStep& s = A.cs[(size_t)idx];
    const size_t n = A.xs.size();
    const size_t mid = s.lo + (s.hi - s.lo) / 2;

    RRect(area, 10, Color{16, 20, 33, 255});
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);

    float sx0 = m.X(A.xs[s.lo].x) - 9, sx1 = m.X(A.xs[s.hi].x) + 9;
    DrawRectangle((int)sx0, (int)area.y, (int)(sx1 - sx0), (int)area.height, Alpha(C_TEXT, 0.045f));

    if (s.kind == CPStep::STRIP && s.deltaBefore > 0) {
        float bx0 = m.X(s.midX - s.deltaBefore), bx1 = m.X(s.midX + s.deltaBefore);
        DrawRectangle((int)bx0, (int)area.y, (int)max(2.0f, bx1 - bx0), (int)area.height, Alpha(C_YELLOW, 0.11f));
        DrawLine((int)bx0, (int)area.y, (int)bx0, (int)(area.y + area.height), Alpha(C_YELLOW, 0.55f));
        DrawLine((int)bx1, (int)area.y, (int)bx1, (int)(area.y + area.height), Alpha(C_YELLOW, 0.55f));
    }

    if (s.kind != CPStep::BASE) {
        float lx = m.X(s.midX);
        DrawLineEx(Vector2{lx, area.y}, Vector2{lx, area.y + area.height}, 2.0f, Alpha(C_PURPLE, 0.9f));
        T(Fmt("x = %.1f", s.midX), lx + 6, area.y + 6, 12, C_PURPLE);
    }

    for (size_t k = 0; k < n; ++k) {
        Vector2 p = m.To(A.xs[k]);
        Color c = C_POINT;
        float rad = 3.6f;
        if (k >= s.lo && k <= s.hi) {
            rad = 5.0f;
            if (s.kind == CPStep::BASE) c = C_GREEN;
            else if (s.kind == CPStep::SPLIT) c = (k <= mid) ? C_BLUE : C_ORANGE;
            else c = Alpha((k <= mid) ? C_BLUE : C_ORANGE, 0.65f);
        }
        DrawCircleV(p, rad, c);
    }
    if (s.kind == CPStep::STRIP) {
        for (const Pt& q : s.strip) {
            Vector2 p = m.To(q);
            DrawCircleV(p, 6.5f, C_YELLOW);
            DrawCircleLines((int)p.x, (int)p.y, 8.5f, C_TEXT);
        }
    }

    if (s.kind != CPStep::SPLIT) {
        Vector2 pa = m.To(s.a), pb = m.To(s.b);
        DrawLineEx(pa, pb, 3.0f, C_RED);
        float pulse = 10.0f + 2.5f * sin(A.t * 6.0f);
        DrawCircleLines((int)pa.x, (int)pa.y, pulse, C_RED);
        DrawCircleLines((int)pb.x, (int)pb.y, pulse, C_RED);
        float lx = (pa.x + pb.x) / 2 + 12, ly = (pa.y + pb.y) / 2 - 22;
        T(Fmt("d = %.2f", s.delta), lx, ly, 14, C_RED);
    }
    EndScissorMode();

    Vector2 mp = GetMousePosition();
    if (Hover(area)) {
        int bestK = -1; float bestD = 12;
        for (size_t k = 0; k < n; ++k) {
            Vector2 p = m.To(A.xs[k]);
            float d = sqrt((p.x - mp.x) * (p.x - mp.x) + (p.y - mp.y) * (p.y - mp.y));
            if (d < bestD) { bestD = d; bestK = (int)k; }
        }
        if (bestK >= 0) {
            string tip = Fmt("(%.1f, %.1f)   x-rank %d", A.xs[(size_t)bestK].x, A.xs[(size_t)bestK].y, bestK);
            float w = TW(tip, 13) + 20;
            float tx = min(mp.x + 14, area.x + area.width - w - 4), ty = mp.y - 34;
            RRect(Rectangle{tx, ty, w, 26}, 8, Color{8, 10, 18, 240});
            T(tip, tx + 10, ty + 6, 13, C_TEXT);
        }
    }

    float lx = R.x + 18, ly = R.y + R.height - 44;
    struct L { Color c; const char* t; };
    L legend[] = {{C_BLUE, "left half"}, {C_ORANGE, "right half"}, {C_GREEN, "brute force"}, {C_YELLOW, "strip"}, {C_RED, "best pair"}};
    for (const L& l : legend) {
        DrawCircle((int)lx + 5, (int)ly + 10, 5, l.c);
        T(l.t, lx + 15, ly + 3, 12.5f, C_DIM);
        lx += 22 + TW(l.t, 12.5f) + 12;
    }
    if (idx == (int)A.cs.size() - 1)
        TR(Fmt("Closest pair found: d = %.3f", A.cres.dist), R.x + R.width - 18, R.y + R.height - 20, 16, C_GREEN);
}

static string CPDescription(const CPStep& s, bool improved) {
    size_t cnt = s.hi - s.lo + 1;
    if (s.kind == CPStep::SPLIT)
        return Fmt("Split %zu points (ranks %zu..%zu) at x = %.1f. Solve the blue and orange halves.",
                   cnt, s.lo, s.hi, s.midX);
    if (s.kind == CPStep::BASE)
        return Fmt("Base case: only %zu points, so every pair is compared directly (brute force). Closest distance here: %.3f.", cnt, s.delta);
    return Fmt("Merge by y. Delta = %.3f, split x = %.1f. Check %zu strip points against nearby neighbours. %s",
               s.deltaBefore, s.midX, s.strip.size(), improved ? "A closer crossing pair was found!" : "No closer crossing pair.");
}

static void DrawCPInfo(Rectangle R, int idx) {
    Card(R);
    const CPStep& s = A.cs[(size_t)idx];
    int N = (int)A.cs.size();
    float x = R.x + 16, w = R.width - 32, y = R.y + 14;
    T(Fmt("Step %d / %d", idx + 1, N), x, y, 20, C_TEXT);
    TR(Fmt("depth %d", A.cdepth[(size_t)idx]), R.x + R.width - 16, y + 5, 13, C_DIM);
    y += 32;
    Chip(x, y, KindNameCP(s.kind), KindColorCP(s.kind));
    y += 34;
    bool improved = s.kind == CPStep::STRIP && s.delta + 1e-12 < s.deltaBefore;
    y = Prose(CPDescription(s, improved), x, y, w, 13.5f, C_TEXT, 4);
    y += 4;
    if (isinf(A.cbest[(size_t)idx])) T("Best: not found yet", x, y, 13, C_DIM);
    else T(Fmt("Best: %.3f", A.cbest[(size_t)idx]), x, y, 13, C_GREEN);

    float logTop = max(R.y + 190, y + 30);
    Rectangle lg{R.x + 10, logTop, R.width - 20, R.y + R.height - logTop - 10};
    DrawLog(lg, N, idx,
            [&](int i) {
                const CPStep& q = A.cs[(size_t)i];
                const char* nm = q.kind == CPStep::SPLIT ? "split " : (q.kind == CPStep::BASE ? "base  " : "strip ");
                return Fmt("%3d %s%zu..%zu", i + 1, nm, q.lo, q.hi);
            },
            [&](int i) { return KindColorCP(A.cs[(size_t)i].kind); },
            [&](int i) { return min(A.cdepth[(size_t)i], 8); });
}

// Karatsuba
static Color KindColorK(int k) { return k == KStep::SPLIT ? C_BLUE : (k == KStep::LEAF ? C_GREEN : C_PURPLE); }
static const char* KindNameK(int k) { return k == KStep::SPLIT ? "SPLIT" : (k == KStep::LEAF ? "BASE CASE" : "COMBINE"); }

static void DrawNumCanvas(Rectangle R, int idx) {
    Card(R);
    const KStep& s = A.ks[(size_t)idx];
    float x = R.x + 20, w = R.width - 40, y = R.y + 14;
    T("Karatsuba multiplication", x, y, 15, C_TEXT);
    TR(Fmt("%zu x %zu digits", A.a.size(), A.b.size()), R.x + R.width - 20, y + 2, 12.5f, C_DIM);
    y += 30;
    BeginScissorMode((int)R.x, (int)(R.y + 40), (int)R.width, (int)(R.height - 44));
    const float fs = 15;

    if (s.kind == KStep::SPLIT) {
        T("Split both numbers at m = " + to_string(s.m) + " digits", x, y, 19, C_TEXT);
        y += 34;
        size_t m = s.m;
        size_t hiA = s.a.size() > m ? s.a.size() - m : 0, hiB = s.b.size() > m ? s.b.size() - m : 0;
        T(Fmt("A   (%zu digits)", s.a.size()), x, y, 13, C_DIM); y += 20;
        y = Digits(s.a, x, y, w, fs, [&](size_t i) { return i < hiA ? C_BLUE : C_ORANGE; });
        y += 12;
        T(Fmt("B   (%zu digits)", s.b.size()), x, y, 13, C_DIM); y += 20;
        y = Digits(s.b, x, y, w, fs, [&](size_t i) { return i < hiB ? C_BLUE : C_ORANGE; });
        y += 14;
        DrawCircle((int)x + 5, (int)y + 8, 5, C_BLUE);
        T("high half  (A1, B1)", x + 16, y, 13, C_DIM);
        DrawCircle((int)x + 190, (int)y + 8, 5, C_ORANGE);
        T("low half  (A0, B0)", x + 201, y, 13, C_DIM);
        y += 34;
        T("A = A1 x 10^m + A0", x, y, 15, C_TEXT); T("B = B1 x 10^m + B0", x + 250, y, 15, C_TEXT); y += 24;
        T("Z0 = A0 x B0", x, y, 15, C_ORANGE); T("Z2 = A1 x B1", x + 250, y, 15, C_BLUE); y += 24;
        T("Z1 = (A0 + A1)(B0 + B1) - Z0 - Z2", x, y, 15, C_PURPLE); y += 24;
        T("A x B = Z2 x 10^(2m) + Z1 x 10^m + Z0", x, y, 15, C_TEXT); y += 30;
        Prose("Only three recursive multiplications are needed instead of four. The three calls follow in the log: Z0 first, then Z2, then Z1.",
              x, y, w, 13, C_DIM, 4);
    } 
    else if (s.kind == KStep::LEAF) {
        T("Base case: ordinary (schoolbook) multiplication", x, y, 19, C_TEXT);
        y += 36;
        y = Prose("The smaller number has at most 4 digits, so splitting further does not pay off.", x, y, w, 13.5f, C_DIM, 4);
        y += 22;
        T(s.a, x, y, 32, C_BLUE, true);
        float aw = TW(s.a, 32, true);
        T("x", x + aw + 16, y, 32, C_DIM, true);
        T(s.b, x + aw + 52, y, 32, C_ORANGE, true);
        y += 52;
        T("=", x, y, 32, C_DIM, true);
        T(s.result, x + 36, y, 32, C_GREEN, true);
    } 
    else {
        T("Combine the three products", x, y, 19, C_TEXT);
        y += 34;
        T(Fmt("Z0 = A0 x B0"), x, y, 13, C_ORANGE); y += 20;
        y = DigitsOne(s.z0, x, y, w, fs, C_ORANGE); y += 8;
        T(Fmt("Z1 = (A0 + A1)(B0 + B1) - Z0 - Z2"), x, y, 13, C_PURPLE); y += 20;
        y = DigitsOne(s.z1, x, y, w, fs, C_PURPLE); y += 8;
        T(Fmt("Z2 = A1 x B1"), x, y, 13, C_BLUE); y += 20;
        y = DigitsOne(s.z2, x, y, w, fs, C_BLUE); y += 12;
        y = Prose(Fmt("A x B = Z2 x 10^%zu + Z1 x 10^%zu + Z0", 2 * s.m, s.m), x, y, w, 15, C_TEXT, 6);
        y += 4;
        T(Fmt("Result  (%zu digits)", s.result.size()), x, y, 13, C_GREEN); y += 20;
        DigitsOne(s.result, x, y, w, fs, C_GREEN);
    }
    EndScissorMode();
}

static string KDescription(const KStep& s) {
    if (s.kind == KStep::SPLIT)
        return Fmt("Divide: both numbers are cut at m = %zu digits. Three smaller products Z0, Z1, Z2 will be computed recursively.", s.m);
    if (s.kind == KStep::LEAF)
        return "Base case: the numbers are short, so ordinary multiplication is used and the product is returned to the caller.";
    return Fmt("Combine: Z0, Z1 and Z2 are shifted by 0, %zu and %zu digits and added to give the product of this call.", s.m, 2 * s.m);
}

static void DrawNumInfo(Rectangle R, int idx) {
    Card(R);
    const KStep& s = A.ks[(size_t)idx];
    int N = (int)A.ks.size();
    float x = R.x + 16, w = R.width - 32, y = R.y + 14;
    T(Fmt("Step %d / %d", idx + 1, N), x, y, 20, C_TEXT);
    TR(Fmt("depth %d", s.depth), R.x + R.width - 16, y + 5, 13, C_DIM);
    y += 32;
    Chip(x, y, KindNameK(s.kind), KindColorK(s.kind));
    y += 34;
    y = Prose(KDescription(s), x, y, w, 13.5f, C_TEXT, 4);
    y += 4;
    T(Fmt("operands: %zu x %zu digits", s.a.size(), s.b.size()), x, y, 13, C_DIM);

    float logTop = max(R.y + 190, y + 30);
    Rectangle lg{R.x + 10, logTop, R.width - 20, R.y + R.height - logTop - 10};
    DrawLog(lg, N, idx,
            [&](int i) {
                const KStep& q = A.ks[(size_t)i];
                const char* nm = q.kind == KStep::SPLIT ? "split  " : (q.kind == KStep::LEAF ? "leaf   " : "combine");
                return Fmt("%4d %s %zux%zu", i + 1, nm, q.a.size(), q.b.size());
            },
            [&](int i) { return KindColorK(A.ks[(size_t)i].kind); },
            [&](int i) { return min(A.ks[(size_t)i].depth, 8); });
}

// result views
static void Stat(float x, float y, float w, const string& label, const string& value, Color vc, float vsize = 24) {
    T(label, x, y, 11.5f, C_DIM);
    T(value, x, y + 25, vsize, vc);
    (void)w;
}

static void DrawResultCP(Rectangle full) {
    Rectangle R{full.x, full.y, 580, full.height};
    Rectangle M{full.x + 592, full.y, full.width - 592, full.height};
    Card(R);
    T("Input overview", R.x + 18, R.y + 14, 15, C_TEXT);
    T(Fmt("%s of %s points shown", Commas(A.sample.size()).c_str(), Commas(A.pts.size()).c_str()), R.x + 18, R.y + 34, 12, C_DIM);
    Rectangle area{R.x + 8, R.y + 58, R.width - 16, R.height - 74};
    RRect(area, 10, Color{16, 20, 33, 255});
    Map m = MakeMap(A.pts, area, 24);
    BeginScissorMode((int)area.x, (int)area.y, (int)area.width, (int)area.height);
    for (const Pt& p : A.sample) { Vector2 v = m.To(p); DrawCircleV(v, 1.3f, Alpha(C_BLUE, 0.55f)); }
    Vector2 pa = m.To(A.cres.a), pb = m.To(A.cres.b);
    float pulse = 11.0f + 3.0f * sin(A.t * 5.0f);
    DrawLineEx(pa, pb, 2.0f, C_RED);
    DrawCircleLines((int)pa.x, (int)pa.y, pulse, C_RED);
    DrawCircleLines((int)pb.x, (int)pb.y, pulse, C_RED);
    EndScissorMode();
    T("closest pair", min(pa.x + 16, area.x + area.width - 90), max(area.y + 4, pa.y - 28), 12.5f, C_RED);

    Rectangle zr{area.x + area.width - 196, area.y + area.height - 196, 184, 184};
    RRect(Rectangle{zr.x - 1, zr.y - 1, zr.width + 2, zr.height + 2}, 10, C_BORDER);
    RRect(zr, 10, Color{12, 14, 24, 255});
    {
        double hw = max(A.cres.dist * 6.0, 1e-3);
        double cx = (A.cres.a.x + A.cres.b.x) / 2, cy = (A.cres.a.y + A.cres.b.y) / 2;
        float sc = (zr.width - 24) / (float)(2 * hw);
        auto Z = [&](const Pt& p) { return Vector2{zr.x + zr.width / 2 + (float)(p.x - cx) * sc, zr.y + zr.height / 2 - (float)(p.y - cy) * sc}; };
        BeginScissorMode((int)zr.x, (int)zr.y, (int)zr.width, (int)zr.height);
        for (const Pt& p : A.zoom) DrawCircleV(Z(p), 3.0f, Alpha(C_BLUE, 0.8f));
        Vector2 za = Z(A.cres.a), zb = Z(A.cres.b);
        DrawLineEx(za, zb, 2.0f, C_RED);
        DrawCircleV(za, 4.5f, C_RED); DrawCircleV(zb, 4.5f, C_RED);
        EndScissorMode();
        T("zoom on the pair", zr.x + 8, zr.y + 6, 11, C_DIM);
    }

    Card(M);
    float x = M.x + 22, y = M.y + 18;
    Stat(x, y, 0, "INPUT", Commas(A.pts.size()) + " points", C_TEXT); y += 58;
    Stat(x, y, 0, "RUNNING TIME", FmtMs(A.cms), C_GREEN, 34); y += 78;
    Stat(x, y, 0, "CLOSEST DISTANCE", Fmt("%.6f", A.cres.dist), C_RED, 26); y += 62;
    T("CLOSEST PAIR", x, y, 11.5f, C_DIM); y += 25;
    T(Fmt("(%.3f, %.3f)", A.cres.a.x, A.cres.a.y), x, y, 15, C_TEXT, true); y += 22;
    T(Fmt("(%.3f, %.3f)", A.cres.b.x, A.cres.b.y), x, y, 15, C_TEXT, true); y += 38;
    T("CHECK: sort + sweep", x, y, 22, C_TEXT); y += 30;
    T(A.cok ? Fmt("MATCH  (%.6f)", A.cref) : Fmt("MISMATCH  (%.6f)", A.cref), x, y, 24, A.cok ? C_GREEN : C_RED); y += 40;
    Prose("O(n log n) time\nT(n) = 2T(n/2) + O(n)\n\nOpen cp10 to explore each step.",
          x, y, M.width - 44, 22, C_TEXT, 6);
}

static void DrawResultNum(Rectangle full) {
    float tw = (full.width - 36) / 4;
    struct TileDef { const char* l; string v; Color c; };
    TileDef tiles[] = {
        {"DIGITS IN A", Commas(A.a.size()), C_BLUE},
        {"DIGITS IN B", Commas(A.b.size()), C_ORANGE},
        {"PRODUCT DIGITS", Commas(A.prod.size()), C_PURPLE},
        {"RUNNING TIME", FmtMs(A.kms), C_GREEN},
    };
    for (int i = 0; i < 4; ++i) {
        Rectangle r{full.x + (float)i * (tw + 12), full.y, tw, 92};
        Card(r);
        T(tiles[i].l, r.x + 18, r.y + 16, 11.5f, C_DIM);
        T(tiles[i].v, r.x + 18, r.y + 36, 30, tiles[i].c);
    }
    Rectangle P{full.x, full.y + 104, 640, 330};
    Card(P);
    T("Product preview", P.x + 18, P.y + 14, 15, C_TEXT);
    size_t show = 150;
    float y = P.y + 44;
    T(Fmt("first %zu digits", min(show, A.prod.size())), P.x + 18, y, 12, C_DIM); y += 25;
    y = DigitsOne(A.prod.substr(0, show), P.x + 18, y, P.width - 36, 14, C_GREEN);
    if (A.prod.size() > 2 * show) {
        y += 8;
        T(Fmt("... %s digits omitted ...", Commas(A.prod.size() - 2 * show).c_str()), P.x + 18, y, 12.5f, C_DIM); y += 24;
        T(Fmt("last %zu digits", show), P.x + 18, y, 12, C_DIM); y += 25;
        DigitsOne(A.prod.substr(A.prod.size() - show), P.x + 18, y, P.width - 36, 14, C_GREEN);
    }

    Rectangle V{full.x + 652, full.y + 104, full.width - 652, 330};
    Card(V);
    float x = V.x + 22; y = V.y + 18;
    T("CHECK", x, y, 11.5f, C_DIM); y += 25;
    T(A.kok ? "MATCH" : "MISMATCH", x, y, 26, A.kok ? C_GREEN : C_RED); y += 40;
    y = Prose(A.kSmallCheck
                  ? "Verified modulo three primes and against schoolbook multiplication."
                  : "Verified modulo three primes.",
              x, y, V.width - 44, 16, C_DIM, 4);
    y += 25;
    T("OUTPUT", x, y, 11.5f, C_DIM); y += 25;
    if (Btn(Rectangle{x, y, 190, 38}, "Save full product", C_PANEL2)) {
        string out = A.cur + ".product.txt";
        FILE* f = fopen(out.c_str(), "wb");
        if (f) { fwrite(A.prod.data(), 1, A.prod.size(), f); fputc('\n', f); fclose(f); Toast("Saved " + out); }
        else Toast("Could not write " + out);
    }

    Rectangle E{full.x, full.y + 446, full.width, full.height - 446};
    Card(E);
    float ex = E.x + 22, ey = E.y + 16;
    T("How it works", ex, ey, 15, C_TEXT); ey += 28;
    float cw = (E.width - 44 - 40) / 3;
    const char* heads[3] = {"1. Split", "2. Three products", "3. Combine"};
    const char* texts[3] = {
        "Split each number into high and low halves at m limbs. B = 10^9.",
        "z0 = x0*y0; z2 = x1*y1. Compute z1 from the product of the sums, minus z0 and z2.",
        "x*y = z2*B^(2m) + z1*B^m + z0. Time: O(n^1.585)."};
    Color cols[3] = {C_BLUE, C_PURPLE, C_GREEN};
    for (int i = 0; i < 3; ++i) {
        float cx = ex + (float)i * (cw + 20);
        T(heads[i], cx, ey, 14, cols[i]);
        Prose(texts[i], cx, ey + 22, cw, 12.5f, C_DIM, 4);
    }
}

// welcome
static void DrawWelcome(Rectangle full) {
    Card(full);
    float cx = full.x + full.width / 2;
    TC("Pick an input file", cx, full.y + 60, 34, C_TEXT);
    TC("Select a file on the left, or drop a .txt file here.", cx, full.y + 108, 15, C_DIM);
    float cw = 400, gap = 28;
    float x0 = cx - cw - gap / 2, y0 = full.y + 170;
    struct Box { const char* title; const char* sub; Color c; const char* l1; const char* l2; const char* l3; };
    Box boxes[2] = {
        {"Closest pair of points", "cp01 ... cp10", C_BLUE, "Up to 300 points: follow each split,", "base case and strip check.", "Large inputs: result and running time."},
        {"Karatsuba multiplication", "im01 ... im10", C_ORANGE, "Up to 256 digits: follow each split,", "base case and combine step.", "Large inputs: product and running time."}};
    for (int i = 0; i < 2; ++i) {
        Rectangle r{x0 + (float)i * (cw + gap), y0, cw, 240};
        RRect(r, 14, C_PANEL2);
        RRect(Rectangle{r.x, r.y, 6, r.height}, 3, boxes[i].c);
        T(boxes[i].title, r.x + 28, r.y + 24, 20, C_TEXT);
        T(boxes[i].sub, r.x + 28, r.y + 54, 13, boxes[i].c, true);
        T(boxes[i].l1, r.x + 28, r.y + 100, 14, C_DIM);
        T(boxes[i].l2, r.x + 28, r.y + 124, 14, C_DIM);
        T(boxes[i].l3, r.x + 28, r.y + 166, 14, C_TEXT);
    }
    TC("Prev / Next: move 1, 2, 4 or 8 steps per click. Reset: first step. End: last step.", cx, full.y + full.height - 54, 13.5f, C_DIM);
}

static bool exitRequested = false;

static void DrawFilePanel(Rectangle R) {
    Card(R);
    T("INPUT FILES", R.x + 18, R.y + 16, 12, C_DIM);
    T(A.dataDir + "/", R.x + 18, R.y + 34, 12, C_BLUE, true);
    Rectangle body{R.x + 6, R.y + 58, R.width - 12, R.height - 166};

    struct Row { bool header; int idx; const char* title; };
    vector<Row> rows;
    int lastGroup = -1;
    const char* gnames[3] = {"CLOSEST PAIR", "MULTIPLICATION", "OTHER"};
    for (size_t i = 0; i < A.files.size(); ++i) {
        if (A.files[i].group != lastGroup) { lastGroup = A.files[i].group; rows.push_back(Row{true, 0, gnames[lastGroup]}); }
        rows.push_back(Row{false, (int)i, ""});
    }
    float total = 0;
    for (const Row& r : rows) total += r.header ? 30.f : 34.f;
    float maxScroll = max(0.f, total - body.height);
    if (Hover(body)) A.listScroll = ClampF(A.listScroll - GetMouseWheelMove() * 40, 0, maxScroll);
    A.listScroll = ClampF(A.listScroll, 0, maxScroll);

    if (rows.empty()) {
        Prose("No .txt files found in this folder. Run ./gen first, start the GUI with another folder, or drop a file here.", body.x + 12, body.y + 12, body.width - 24, 13, C_DIM, 4);
    }
    BeginScissorMode((int)body.x, (int)body.y, (int)body.width, (int)body.height);
    float y = body.y - A.listScroll;
    for (const Row& r : rows) {
        if (r.header) {
            T(r.title, body.x + 12, y + 10, 11, C_DIM);
            y += 30;
            continue;
        }
        const FileItem& f = A.files[(size_t)r.idx];
        Rectangle rr{body.x + 2, y, body.width - 4, 32};
        bool sel = (f.path == A.cur);
        bool hv = Hover(rr) && Hover(body);
        if (sel) RRect(rr, 9, Alpha(f.group == 0 ? C_BLUE : C_ORANGE, 0.26f));
        else if (hv) RRect(rr, 9, Alpha(C_TEXT, 0.07f));
        DrawCircle((int)rr.x + 14, (int)(rr.y + 16), 4.5f, f.group == 0 ? C_BLUE : (f.group == 1 ? C_ORANGE : C_DIM));
        T(f.name, rr.x + 28, rr.y + 7, 15, sel ? C_TEXT : Mix(C_TEXT, C_DIM, 0.25f));
        string sz = f.bytes >= 1000000 ? Fmt("%.1f MB", f.bytes / 1e6) : (f.bytes >= 1000 ? Fmt("%.0f KB", f.bytes / 1e3) : Fmt("%d B", f.bytes));
        TR(sz, rr.x + rr.width - 10, rr.y + 9, 12, C_DIM);
        if (hv && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) QueueLoad(f.path);
        y += 34;
    }
    EndScissorMode();

    Rectangle hint{R.x + 12, R.y + R.height - 100, R.width - 24, 34};
    RRect(hint, 9, C_PANEL2);
    TC("drop any .txt file here", hint.x + hint.width / 2, hint.y + 9, 13, C_DIM);
    if (Btn(Rectangle{R.x + 12, R.y + R.height - 56, R.width - 24, 44},
            "Exit", C_RED, true, 22)) {
        exitRequested = true;
    }
}

static void DrawTopBar() {
    RRect(Rectangle{16, 14, 46, 46}, 12, C_BLUE);
    TC("D&C", 39, 27, 17, Color{12, 16, 30, 255});
    T("Divide & Conquer", 74, 13, 26, C_TEXT);
    T("DAA Project | Closest pair of points & Karatsuba multiplication", 75, 44, 16, C_DIM);
    if (!A.cur.empty()) {
        string mode = A.screen == Screen::Steps ? "step-by-step" : "result view";
        float w1 = TW(A.curName, 15) + 28, w2 = TW(mode, 12.5f) + 22;
        float x = WIN_W - 16 - w1 - w2 - 8;
        RRect(Rectangle{x, 20, w1, 34}, 17, C_PANEL2);
        T(A.curName, x + 14, 28, 15, C_TEXT);
        Chip(x + w1 + 8, 24, mode, A.screen == Screen::Steps ? C_GREEN : C_ORANGE, 12.5f);
    }
}

static void DrawLoading(const string& name, float elapsed) {
    Rectangle r{WIN_W / 2.0f - 280, WIN_H / 2.0f - 115, 560, 230};
    RRect(Rectangle{r.x + 6, r.y + 8, r.width, r.height}, 18, Alpha(BLACK, 0.35f));
    Card(r, 18);
    RRect(Rectangle{r.x + 24, r.y + 22, 6, 32}, 3, C_BLUE);
    T("Processing input", r.x + 46, r.y + 24, 28, C_TEXT);
    string label = name;
    while (label.size() > 4 && TW(label, 22) > r.width - 60)
        label.erase(label.size() - 4, 1);
    T(label, r.x + 28, r.y + 70, 22, C_BLUE);

    Rectangle track{r.x + 28, r.y + 116, r.width - 56, 18};
    RRect(track, 9, C_PANEL2);
    float segment = track.width * 0.30f;
    float phase = 0.5f - 0.5f * cos(elapsed * 2.6f);
    Rectangle fill{track.x + phase * (track.width - segment), track.y, segment, track.height};
    RRect(fill, 9, C_BLUE);
    RRect(Rectangle{fill.x + 5, fill.y + 3, fill.width - 10, 4}, 2, Alpha(WHITE, 0.25f));
    T("Calculating and checking the result...", r.x + 28, r.y + 152, 20, C_TEXT);
    T("Large inputs can take a few seconds", r.x + 28, r.y + 187, 20, C_DIM);
}

// main
static Font LoadBestFont(const char* const* cands, int n, bool& isDefault) {
    for (int i = 0; i < n; ++i)
        if (FileExists(cands[i])) {
            Font f = LoadFontEx(cands[i], 48, nullptr, 0);
            if (f.texture.id != 0) { SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR); isDefault = false; return f; }
        }
    isDefault = true;
    return GetFontDefault();
}

int main(int argc, char** argv) {
    string shotPath, shotFile; int shotStep = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--shot") == 0 && i + 3 < argc) {
            shotPath = argv[i + 1]; shotFile = argv[i + 2]; shotStep = atoi(argv[i + 3]); i += 3;
        } else A.dataDir = argv[i];
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(WIN_W, WIN_H, "DAA Project");
    SetTargetFPS(60);
    SetExitKey(0);

    const char* uiFonts[] = {"C:/Windows/Fonts/seguisb.ttf", "C:/Windows/Fonts/arial.ttf",
                             "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/System/Library/Fonts/Supplemental/Arial.ttf",
                             "/usr/share/fonts/TTF/DejaVuSans.ttf"};
    const char* monoFonts[] = {"C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/cour.ttf",
                               "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "/System/Library/Fonts/Menlo.ttc",
                               "/usr/share/fonts/TTF/DejaVuSansMono.ttf"};
    bool defF = false, defM = false;
    gF = LoadBestFont(uiFonts, 5, defF);
    gM = LoadBestFont(monoFonts, 5, defM);
    gSpF = defF ? 0.1f : 0.0f;
    gSpM = defM ? 0.1f : 0.0f;

    RefreshFiles();
    if (!shotFile.empty()) QueueLoad(shotFile);

    future<void> loading;
    string loadingName;
    double loadingStart = 0;
    int shotFrames = -1;
    const Rectangle filePanel{16, 80, 248, 624};
    const Rectangle mainArea{280, 80, 984, 624};

    while (!exitRequested && !WindowShouldClose()) {
        if (loading.valid()) {
            if (loading.wait_for(chrono::milliseconds(0)) != future_status::ready) {
                BeginDrawing();
                ClearBackground(C_BG);
                DrawLoading(loadingName, (float)(GetTime() - loadingStart));
                EndDrawing();
                continue;
            }
            try { loading.get(); }
            catch (const exception& e) {
                A.screen = Screen::Welcome;
                Toast(string("Could not process input: ") + e.what());
            }
            if (!shotFile.empty() && A.screen == Screen::Steps)
                A.step = max(0, min(shotStep, StepCount() - 1));
            if (!shotFile.empty()) shotFrames = 4;
        }
        float dt = min(GetFrameTime(), 0.05f);
        A.t += dt;
        if (A.toastT > 0) A.toastT -= dt;

        if (IsFileDropped()) {
            FilePathList f = LoadDroppedFiles();
            if (f.count > 0) QueueLoad(f.paths[0]);
            UnloadDroppedFiles(f);
        }
        if (A.pendingFrames > 0 && --A.pendingFrames == 0) {
            const string path = A.pendingPath;
            loadingName = GetFileName(path.c_str());
            loadingStart = GetTime();
            loading = async(launch::async, [path]() { LoadFile(path); });

            BeginDrawing();
            ClearBackground(C_BG);
            DrawLoading(loadingName, 0);
            EndDrawing();
            continue;
        }

        BeginDrawing();
        ClearBackground(C_BG);
        DrawTopBar();
        DrawFilePanel(filePanel);

        if (A.screen == Screen::Welcome) {
            DrawWelcome(mainArea);
        } else if (A.screen == Screen::Steps) {
            Rectangle canvas{280, 80, 640, 524}, info{932, 80, 332, 524}, ctl{280, 616, 984, 88};
            int N = StepCount();
            A.step = max(0, min(A.step, N - 1));
            if (A.kind == Kind::Points) { DrawCPCanvas(canvas, A.step); DrawCPInfo(info, A.step); }
            else { DrawNumCanvas(canvas, A.step); DrawNumInfo(info, A.step); }
            DrawControls(ctl, N);
        } else {
            if (A.kind == Kind::Points) DrawResultCP(mainArea); else DrawResultNum(mainArea);
        }

        if (A.toastT > 0) {
            Rectangle r{280, WIN_H - 154.0f, 984, 138};
            RRect(r, 12, Color{8, 10, 18, 255});
            T("MESSAGE", r.x + 20, r.y + 14, 22, C_YELLOW);
            Prose(A.toast, r.x + 20, r.y + 48, r.width - 40, 24, C_TEXT, 6);
        }
        if (A.pendingFrames > 0) {
            DrawRectangle(0, 0, WIN_W, WIN_H, Alpha(BLACK, 0.65f));
            DrawLoading(GetFileName(A.pendingPath.c_str()), 0);
        }
        EndDrawing();

        if (shotFrames > 0 && --shotFrames == 0) { TakeScreenshot(shotPath.c_str()); break; }
    }
    if (loading.valid()) loading.wait();
    CloseWindow();
    return 0;
}