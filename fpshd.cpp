#include "splashkit.h"
#include <stdexcept>
#include <new>
#include <fstream>
#include <vector>
#include <filesystem>
#include <sys/stat.h>
#include <cstring>      // strchr
#include <unordered_set>
#include <set>
#include <cctype>       // isdigit
#include <sstream>
#include <algorithm>
#include <cmath>
using namespace std;
namespace fs = std::filesystem;

// =======================================================
// Theme / Colors / Fonts
// =======================================================
static bool g_font_ok = false;
static font g_ui_font = nullptr;

// Color helpers compatible with SplashKit
static color make_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    return rgba_color(r, g, b, 255);
}
static color make_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    return rgba_color(r, g, b, a);
}

// Palette
static const color COL_BG    = make_rgb(12, 14, 22);
static const color COL_HUD   = make_rgba(0, 0, 0, 140);
static const color COL_CARD  = make_rgba(0, 0, 0, 120);
static const color COL_TEXT  = make_rgb(235, 235, 235);
static const color COL_SUB   = make_rgba(220, 220, 220, 200);
static const color COL_EMPH  = make_rgb(255, 222, 89);
static const color COL_SEP   = make_rgba(255, 255, 255, 60);
static const color COL_OK    = make_rgb(60, 200, 120);
static const color COL_WARN  = make_rgb(240, 180, 50);
static const color COL_BAD   = make_rgb(230, 90, 70);
static const color COL_WIN   = make_rgb(0, 170, 170);
static const color COL_LOSS  = make_rgb(200, 60, 160);

// Text helpers
void setup_font()
{
    if (!has_font("ui_font"))
    {
        g_font_ok = load_font("ui_font", "OpenSans-Regular.ttf");
    }
    else g_font_ok = true;

    if (!g_font_ok)
    {
        if (!has_font("ui_font"))
            g_font_ok = load_font("ui_font", "arial.ttf");
    }

    g_ui_font = g_font_ok ? font_named("ui_font") : nullptr;
}

void draw_ui_text(const string &txt, const color &c, double x, double y, int size)
{
    if (g_ui_font != nullptr) draw_text(txt, c, g_ui_font, size, x, y);
    else draw_text(txt, c, x, y);
}

int ui_text_w(const string &txt, int size)
{
    if (g_ui_font != nullptr) return text_width(txt, g_ui_font, size);
    return text_width(txt, "default", size);
}

int ui_text_h(const string &txt, int size)
{
    if (g_ui_font != nullptr) return text_height(txt, g_ui_font, size);
    return text_height(txt, "default", size);
}

// =======================================================
// Background / Theme (unchanged behavior, “cover” + dimmer)
// =======================================================
static bitmap g_bg_img = nullptr;
static bool   g_has_bg = false;

static double clampd(double v, double a, double b) { return v < a ? a : (v > b ? b : v); }

static bool try_load_bg_base(const string &base)
{
    static const char* exts[] = { ".jpg",".jpeg",".png",".bmp",".JPG",".JPEG",".PNG",".BMP" };
    for (auto ext : exts)
    {
        string p = base + ext;
        if (fs::exists(p))
        {
            g_bg_img = load_bitmap("ui_bg", p);
            return g_bg_img != nullptr;
        }
    }
    return false;
}

void setup_theme()
{
    if (g_has_bg) return;
    if (!has_bitmap("ui_bg"))
    {
        g_has_bg = try_load_bg_base("ui_bg");
        if (!g_has_bg && fs::exists("assets")) g_has_bg = try_load_bg_base("assets/ui_bg");
    }
    if (!g_has_bg && has_bitmap("ui_bg"))
    {
        g_bg_img = bitmap_named("ui_bg");
        g_has_bg = (g_bg_img != nullptr);
    }
}

void draw_background_cover()
{
    if (!g_has_bg || g_bg_img == nullptr)
    {
        clear_screen(COL_BG);
        return;
    }

    int ww = screen_width();
    int wh = screen_height();
    int bw = bitmap_width(g_bg_img);
    int bh = bitmap_height(g_bg_img);
    if (bw <= 0 || bh <= 0 || ww <= 0 || wh <= 0) return;

    double sx = (double)ww / (double)bw;
    double sy = (double)wh / (double)bh;
    double s  = (sx > sy) ? sx : sy; // cover

    double sw = bw * s;
    double sh = bh * s;
    double dx = (ww - sw) * 0.5;
    double dy = (wh - sh) * 0.5;

    draw_bitmap(g_bg_img, dx, dy, option_scale_bmp(s, s));
}

void draw_bg_with_dimmer(double dim_alpha = 0.35)
{
    draw_background_cover();
    if (dim_alpha > 0.0)
    {
        color overlay = rgba_color(0,0,0, (int)clampd(dim_alpha*255.0, 0, 255));
        fill_rectangle(overlay, 0, 0, screen_width(), screen_height());
    }
}

// =======================================================
// HUD / Cards / Lines
// =======================================================
void draw_hud(const string &title, const string &hints = "")
{
    fill_rectangle(COL_HUD, 0, 0, screen_width(), 48);
    draw_ui_text(title, COL_TEXT, 16, 12, 26);
    if (!hints.empty())
        draw_ui_text(hints, COL_SUB, 16 + ui_text_w(title, 26) + 18, 16, 18);
}

void draw_card_row(double x, double y, double w, double h)
{
    fill_rectangle(COL_CARD, x, y, w, h);
}

void draw_sep(double x1, double y1, double x2, double y2)
{
    draw_line(COL_SEP, x1, y1, x2, y2);
}

// =======================================================
// Toasts / Prompts
// =======================================================
void draw_toast(const string &msg)
{
    color bg = rgba_color(0, 0, 0, 180);
    int h = 50;
    fill_rectangle(bg, 0, screen_height() - h, screen_width(), h);
    draw_ui_text(msg, COL_EMPH, 16, screen_height() - h + 14, 18);
    refresh_screen();
    uint32_t start = current_ticks();
    while (current_ticks() - start < 1000)
    {
        process_events();
        if (key_typed(RETURN_KEY) || key_typed(ESCAPE_KEY) || mouse_clicked(LEFT_BUTTON)) break;
        delay(10);
    }
}

string prompt_line(const string &prompt)
{
    string buf;
    while (true)
    {
        process_events();

        // Letters (lowercase)
        for (int k = A_KEY; k <= Z_KEY; ++k)
        {
            key_code kc = static_cast<key_code>(k);
            if (key_typed(kc)) buf.push_back(char('a' + (k - A_KEY)));
        }
        // Digits
        for (int k = NUM_0_KEY; k <= NUM_9_KEY; ++k)
        {
            key_code kc = static_cast<key_code>(k);
            if (key_typed(kc)) buf.push_back(char('0' + (k - NUM_0_KEY)));
        }
        // Space
        if (key_typed(SPACE_KEY)) buf.push_back(' ');

        // Limited punctuation
        for (key_code kc : {
                MINUS_KEY, EQUALS_KEY, LEFT_BRACKET_KEY, RIGHT_BRACKET_KEY, BACKSLASH_KEY,
                SEMI_COLON_KEY, QUOTE_KEY, COMMA_KEY, PERIOD_KEY, SLASH_KEY })
        {
            if (key_typed(kc))
            {
                char ch = 0;
                switch (kc)
                {
                    case MINUS_KEY:         ch = '-'; break;
                    case EQUALS_KEY:        ch = '='; break;
                    case LEFT_BRACKET_KEY:  ch = '['; break;
                    case RIGHT_BRACKET_KEY: ch = ']'; break;
                    case BACKSLASH_KEY:     ch = '\\'; break;
                    case SEMI_COLON_KEY:    ch = ';'; break;
                    case QUOTE_KEY:         ch = '\''; break;
                    case COMMA_KEY:         ch = ','; break;
                    case PERIOD_KEY:        ch = '.'; break;
                    case SLASH_KEY:         ch = '/'; break;
                    default: break;
                }
                if (ch) buf.push_back(ch);
            }
        }

        // Backspace/Delete
        if (key_typed(BACKSPACE_KEY) || key_typed(DELETE_KEY))
        {
            if (!buf.empty()) buf.pop_back();
        }

        // Render
        draw_bg_with_dimmer(0.35);
        draw_ui_text(prompt, COL_TEXT, 24, 64, 22);
        draw_ui_text("Type and press Enter. ESC to cancel.", COL_SUB, 24, 94, 16);
        draw_ui_text("> " + buf + "_", COL_EMPH, 24, 130, 20);
        refresh_screen(60);

        if (key_typed(ESCAPE_KEY)) return "";
        if (key_typed(RETURN_KEY)) return buf;

        delay(16);
    }
}

bool confirm_yes(const string &question)
{
    string a = to_lowercase(prompt_line(question + " (y/n):"));
    return (a == "y" || a == "yes");
}

void show_long_text(const string &title, const string &body)
{
    // Simple wrapping
    vector<string> lines;
    {
        string cur;
        int col = 0;
        for (char c : body)
        {
            if (c == '\n')
            {
                lines.push_back(cur); cur.clear(); col = 0;
            }
            else
            {
                cur.push_back(c);
                if (++col >= 90) { lines.push_back(cur); cur.clear(); col = 0; }
            }
        }
        if (!cur.empty()) lines.push_back(cur);
    }

    int start = 0;
    while (true)
    {
        process_events();
        draw_bg_with_dimmer(0.45);

        draw_ui_text(title, COL_TEXT, 20, 20, 26);

        int x = 24;
        int y = 60;
        int max_y = screen_height() - 60;
        int shown = 0;

        for (int i = start; i < (int)lines.size(); ++i)
        {
            draw_ui_text(lines[i], COL_EMPH, x, y, 18);
            y += 22;
            shown++;
            if (y > max_y) break;
        }

        draw_ui_text("UP/DOWN to scroll, ENTER to close", COL_SUB, 24, screen_height() - 30, 16);
        refresh_screen(60);

        if (key_typed(RETURN_KEY) || mouse_clicked(LEFT_BUTTON)) break;
        if (key_typed(UP_KEY)) { if (start > 0) start--; }
        if (key_typed(DOWN_KEY)) { if (start + shown < (int)lines.size()) start++; }

        delay(10);
    }
}

// =======================================================
// Domain
// =======================================================
enum class GameType { Valorant, CSGO, Unknown };

struct Session
{
    string game_name;
    GameType game_type;
    int kills;
    int deaths;
    int assists;
    string rank;
    string outcome;       // "Win" or "Loss"
    string session_date;
};

struct Player
{
    string player_name;
    string preferred_game;
    vector<Session> sessions;          // MIGRATED: std::vector
    vector<string> achievements;       // ids of unlocked achievements
};

const int MAX_ALLOWED = 20;

// =======================================================
// Utilities
// =======================================================
GameType parse_game_type(const string &s)
{
    string t = to_lowercase(s);
    if (t == "valorant") return GameType::Valorant;
    if (t == "cs:go" || t == "csgo" || t == "cs 2" || t == "cs2") return GameType::CSGO;
    return GameType::Unknown;
}

string game_type_to_string(GameType g)
{
    switch (g)
    {
        case GameType::Valorant: return "Valorant";
        case GameType::CSGO:     return "CS:GO";
        default:                 return "Unknown";
    }
}

bool is_win(const string &outcome) { return to_lowercase(outcome) == "win"; }
bool is_loss(const string &outcome) { return to_lowercase(outcome) == "loss"; }

double kda_for(const Session &s)
{
    int denom = (s.deaths <= 0) ? 1 : s.deaths;
    return (s.kills + s.assists) / static_cast<double>(denom);
}

// =======================================================
// Input with exceptions
// =======================================================
int parse_int_in_range(const string &prompt, int lo, int hi)
{
    string s = prompt_line(prompt);
    if (s == "") throw runtime_error("No input provided.");
    if (!is_integer(s)) throw runtime_error("Please enter a valid integer.");
    int v = convert_to_integer(s);
    if (v < lo || v > hi) throw runtime_error("Value must be between " + to_string(lo) + " and " + to_string(hi) + ".");
    return v;
}

string parse_nonempty(const string &prompt)
{
    string s = prompt_line(prompt);
    if (s == "") throw runtime_error("Input cannot be empty.");
    return s;
}

GameType parse_game_with_name(string &out_name)
{
    string s = prompt_line("Game (Valorant / CS:GO):");
    if (s == "") throw runtime_error("Input cannot be empty.");
    GameType g = parse_game_type(s);
    if (g == GameType::Unknown) throw runtime_error("Unsupported game. Enter Valorant or CS:GO.");
    out_name = game_type_to_string(g);
    return g;
}

string parse_outcome()
{
    string s = prompt_line("Match outcome (Win/Loss):");
    if (s == "") throw runtime_error("Input cannot be empty.");
    string tl = to_lowercase(s);
    if (tl == "win") return "Win";
    if (tl == "loss") return "Loss";
    throw runtime_error("Outcome must be Win or Loss.");
}

template <typename F>
auto prompt_until_ok(F fn) -> decltype(fn())
{
    while (true)
    {
        try { return fn(); }
        catch (const exception &e) { draw_toast(string("Invalid input: ") + e.what()); }
    }
}

// =======================================================
// Stats (lifetime)
// =======================================================
struct SummaryStats
{
    int total_matches = 0;
    int wins = 0;
    int losses = 0;
    double avg_kda = 0.0;
    double win_rate_percent = 0.0;

    int best_win_streak = 0;
    int best_loss_streak = 0;
    int best_high_kda_streak = 0;
};

SummaryStats compute_summary(const Player &p)
{
    SummaryStats st{};
    st.total_matches = (int)p.sessions.size();
    int sum_kills = 0, sum_deaths = 0, sum_assists = 0;
    int cur_win_streak = 0, cur_loss_streak = 0, cur_high_kda_streak = 0;

    for (const auto &s : p.sessions)
    {
        if (is_win(s.outcome))
        {
            st.wins++; cur_win_streak++; st.best_win_streak = max(st.best_win_streak, cur_win_streak);
            cur_loss_streak = 0;
        }
        else if (is_loss(s.outcome))
        {
            st.losses++; cur_loss_streak++; st.best_loss_streak = max(st.best_loss_streak, cur_loss_streak);
            cur_win_streak = 0;
        }
        else { cur_win_streak = cur_loss_streak = 0; }

        double k = kda_for(s);
        if (k >= 2.0) { cur_high_kda_streak++; st.best_high_kda_streak = max(st.best_high_kda_streak, cur_high_kda_streak); }
        else cur_high_kda_streak = 0;

        sum_kills += s.kills; sum_deaths += s.deaths; sum_assists += s.assists;
    }

    double avg_deaths  = st.total_matches ? sum_deaths / (double)st.total_matches : 1.0;
    if (avg_deaths <= 0) avg_deaths = 1.0;
    double avg_kills   = st.total_matches ? sum_kills  / (double)st.total_matches : 0.0;
    double avg_assists = st.total_matches ? sum_assists/ (double)st.total_matches : 0.0;

    st.avg_kda = (avg_kills + avg_assists) / avg_deaths;
    st.win_rate_percent = st.total_matches ? (st.wins * 100.0 / st.total_matches) : 0.0;
    return st;
}

int find_best_session_by_kda(const Player &p)
{
    if (p.sessions.empty()) return -1;
    double best = -1.0; int idx = -1;
    for (int i = 0; i < (int)p.sessions.size(); ++i)
    {
        double k = kda_for(p.sessions[i]);
        if (k > best) { best = k; idx = i; }
    }
    return idx;
}

// =======================================================
// Analytics helpers: dates, sorting, rolling average
// =======================================================
bool parse_date_key(const string &ymd, int &key_out)
{
    if (ymd.size() != 10 || ymd[4] != '-' || ymd[7] != '-') return false;
    string ys = ymd.substr(0, 4);
    string ms = ymd.substr(5, 2);
    string ds = ymd.substr(8, 2);
    if (!is_integer(ys) || !is_integer(ms) || !is_integer(ds)) return false;
    int y = convert_to_integer(ys);
    int m = convert_to_integer(ms);
    int d = convert_to_integer(ds);
    if (y < 1970 || y > 2100) return false;
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > 31) return false;
    key_out = y * 10000 + m * 100 + d;
    return true;
}

vector<int> build_sorted_indices_by_date(const Player &p, GameType filter)
{
    struct Row { int idx; int date_key; };
    vector<Row> rows;
    rows.reserve(p.sessions.size());

    for (int i = 0; i < (int)p.sessions.size(); ++i)
    {
        if (filter != GameType::Unknown && p.sessions[i].game_type != filter) continue;
        int key = 0;
        if (!parse_date_key(p.sessions[i].session_date, key)) key = 99999999;
        rows.push_back({i, key});
    }

    sort(rows.begin(), rows.end(), [](const Row &a, const Row &b){ return a.date_key < b.date_key; });

    vector<int> out; out.reserve(rows.size());
    for (auto &r : rows) out.push_back(r.idx);
    return out;
}

void rolling_average(const vector<double> &v, int window, vector<double> &out)
{
    out.assign(v.size(), 0.0);
    if (window <= 1 || v.empty()) { out = v; return; }

    double sum = 0.0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        sum += v[i];
        if (i >= (size_t)window) sum -= v[i - window];
        size_t denom = (i + 1 < (size_t)window) ? (i + 1) : (size_t)window;
        out[i] = sum / denom;
    }
}

// =======================================================
// Achievements (NEW)
// =======================================================
struct AchievementDef
{
    string id;
    string name;
    string desc;
};

static vector<AchievementDef> achievement_catalog()
{
    return {
        {"first_match",     "First Blood",        "Save your very first match."},
        {"ten_matches",     "Warmed Up",          "Reach 10 saved matches."},
        {"win_streak_5",    "On Fire",            "Get a best win streak of 5 or more."},
        {"kda_beast_3",     "KDA Beast",          "Hit KDA ≥ 3.0 in any single match."},
        {"avg_kda_2_last5", "Consistent Threat",  "Average KDA ≥ 2.0 over your last 5 matches."}
    };
}

static bool has_id(const vector<string> &ids, const string &id)
{
    return find(ids.begin(), ids.end(), id) != ids.end();
}

static bool check_rule(const Player &p, const string &id)
{
    if (id == "first_match") return !p.sessions.empty();

    if (id == "ten_matches") return ((int)p.sessions.size() >= 10);

    if (id == "win_streak_5")
    {
        SummaryStats st = compute_summary(p);
        return st.best_win_streak >= 5;
    }

    if (id == "kda_beast_3")
    {
        for (const auto &s : p.sessions)
            if (kda_for(s) >= 3.0) return true;
        return false;
    }

    if (id == "avg_kda_2_last5")
    {
        if (p.sessions.empty()) return false;
        vector<int> idx = build_sorted_indices_by_date(p, GameType::Unknown);
        int n = (int)idx.size();
        int start = max(0, n - 5);
        int sum_k=0, sum_d=0, sum_a=0, cnt=0;
        for (int i = start; i < n; ++i)
        {
            const auto &s = p.sessions[idx[i]];
            sum_k += s.kills; sum_d += s.deaths; sum_a += s.assists; cnt++;
        }
        if (cnt == 0) return false;
        double avg_d = max(1.0, sum_d / (double)cnt);
        double avg_k = sum_k / (double)cnt;
        double avg_a = sum_a / (double)cnt;
        double kda = (avg_k + avg_a)/avg_d;
        return kda >= 2.0;
    }

    return false;
}

void evaluate_achievements(Player &p, bool notify_new)
{
    auto defs = achievement_catalog();
    vector<string> newly;

    for (const auto &d : defs)
    {
        if (!has_id(p.achievements, d.id) && check_rule(p, d.id))
        {
            p.achievements.push_back(d.id);
            newly.push_back(d.name);
        }
    }

    if (notify_new)
    {
        for (const auto &nm : newly)
            draw_toast("Achievement unlocked: " + nm + " 🎉");
    }
}

void show_achievements_screen(const Player &p)
{
    auto defs = achievement_catalog();

    draw_bg_with_dimmer(0.45);
    draw_hud("Achievements");

    int x = 40, y = 80, h = 64, w = screen_width() - 80;

    for (const auto &d : defs)
    {
        bool unlocked = has_id(p.achievements, d.id);
        draw_card_row(x, y, w, h);
        string title = (unlocked ? "✓ " : "• ") + d.name;
        draw_ui_text(title, unlocked ? COL_OK : COL_SUB, x + 16, y + 12, 22);
        draw_ui_text(d.desc, COL_TEXT, x + 16, y + 36, 16);
        y += h + 12;
    }

    draw_ui_text("Press any key to return", COL_SUB, x, y + 8, 16);
    refresh_screen(60);

    while (!(key_typed(RETURN_KEY) || key_typed(ESCAPE_KEY) || mouse_clicked(LEFT_BUTTON)
             || key_typed(SPACE_KEY) || key_typed(A_KEY) || key_typed(B_KEY)))
    {
        process_events(); delay(10);
    }
}

// =======================================================
// JSON helpers (writer + focused parser for our schema)
// =======================================================
static string json_escape(const string &s)
{
    string out; out.reserve(s.size() + 8);
    for (char c : s)
    {
        switch (c)
        {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[7]; snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                    out += buf;
                } else out += c;
        }
    }
    return out;
}

static void skip_ws(const string &s, size_t &i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\t' || s[i] == '\r')) i++;
}

static bool match_char(const string &s, size_t &i, char want)
{
    skip_ws(s, i);
    if (i < s.size() && s[i] == want) { i++; return true; }
    return false;
}

static bool parse_string_quoted(const string &s, size_t &i, string &out)
{
    skip_ws(s, i);
    if (i >= s.size() || s[i] != '\"') return false;
    i++;
    string res;
    while (i < s.size())
    {
        char c = s[i++];
        if (c == '\"') { out = res; return true; }
        if (c == '\\')
        {
            if (i >= s.size()) return false;
            char e = s[i++];
            switch (e)
            {
                case '\"': res += '\"'; break;
                case '\\': res += '\\'; break;
                case '/':  res += '/';  break;
                case 'b':  res += '\b'; break;
                case 'f':  res += '\f'; break;
                case 'n':  res += '\n'; break;
                case 'r':  res += '\r'; break;
                case 't':  res += '\t'; break;
                case 'u':
                {
                    if (i + 4 > s.size()) return false;
                    for (int k = 0; k < 4; ++k)
                    {
                        char h = s[i++];
                        if (!isxdigit((unsigned char)h)) return false;
                    }
                    res += '?'; // skip unicode -> placeholder
                    break;
                }
                default: return false;
            }
        }
        else res += c;
    }
    return false;
}

static bool parse_integer(const string &s, size_t &i, long long &out)
{
    skip_ws(s, i);
    if (i >= s.size()) return false;
    bool neg = false;
    if (s[i] == '-') { neg = true; i++; }
    if (i >= s.size() || !isdigit((unsigned char)s[i])) return false;
    long long v = 0;
    while (i < s.size() && isdigit((unsigned char)s[i])) { v = v * 10 + (s[i] - '0'); i++; }
    out = neg ? -v : v; return true;
}

static bool parse_literal(const string &s, size_t &i, const char *lit)
{
    skip_ws(s, i);
    size_t start = i;
    for (const char *p = lit; *p; ++p)
    {
        if (i >= s.size() || s[i] != *p) { i = start; return false; }
        i++;
    }
    return true;
}

static bool skip_value(const string &s, size_t &i); // fwd
static bool skip_array(const string &s, size_t &i)
{
    if (!match_char(s, i, '[')) return false;
    skip_ws(s, i);
    if (match_char(s, i, ']')) return true;
    while (true)
    {
        if (!skip_value(s, i)) return false;
        skip_ws(s, i);
        if (match_char(s, i, ']')) return true;
        if (!match_char(s, i, ',')) return false;
    }
}
static bool skip_object(const string &s, size_t &i)
{
    if (!match_char(s, i, '{')) return false;
    skip_ws(s, i);
    if (match_char(s, i, '}')) return true;
    while (true)
    {
        string k;
        if (!parse_string_quoted(s, i, k)) return false;
        if (!match_char(s, i, ':')) return false;
        if (!skip_value(s, i)) return false;
        skip_ws(s, i);
        if (match_char(s, i, '}')) return true;
        if (!match_char(s, i, ',')) return false;
    }
}
static bool skip_value(const string &s, size_t &i)
{
    skip_ws(s, i);
    if (i >= s.size()) return false;
    char c = s[i];
    if (c == '\"') { string dummy; return parse_string_quoted(s, i, dummy); }
    if (c == '{') return skip_object(s, i);
    if (c == '[') return skip_array(s, i);
    if (c == 't') return parse_literal(s, i, "true");
    if (c == 'f') return parse_literal(s, i, "false");
    if (c == 'n') return parse_literal(s, i, "null");
    if (c == '-' || isdigit((unsigned char)c)) { long long num; return parse_integer(s, i, num); }
    return false;
}

// Parse array of sessions
static bool parse_sessions_array(const string &s, size_t &i, Player &p)
{
    if (!match_char(s, i, '[')) return false;
    skip_ws(s, i);
    if (match_char(s, i, ']')) return true;

    while (true)
    {
        if (!match_char(s, i, '{')) return false;
        Session temp{}; bool has_game_type=false, has_k=false, has_d=false, has_a=false;
        while (true)
        {
            string key;
            if (!parse_string_quoted(s, i, key)) return false;
            if (!match_char(s, i, ':')) return false;

            if (key == "game_name")
            {
                if (!parse_string_quoted(s, i, temp.game_name)) return false;
            }
            else if (key == "game_type")
            {
                long long v; if (!parse_integer(s, i, v)) return false;
                temp.game_type = static_cast<GameType>((int)v);
                has_game_type = true;
            }
            else if (key == "kills")
            {
                long long v; if (!parse_integer(s, i, v)) return false; temp.kills = (int)v; has_k=true;
            }
            else if (key == "deaths")
            {
                long long v; if (!parse_integer(s, i, v)) return false; temp.deaths = (int)v; has_d=true;
            }
            else if (key == "assists")
            {
                long long v; if (!parse_integer(s, i, v)) return false; temp.assists = (int)v; has_a=true;
            }
            else if (key == "rank")
            {
                if (!parse_string_quoted(s, i, temp.rank)) return false;
            }
            else if (key == "outcome")
            {
                if (!parse_string_quoted(s, i, temp.outcome)) return false;
            }
            else if (key == "session_date")
            {
                if (!parse_string_quoted(s, i, temp.session_date)) return false;
            }
            else
            {
                if (!skip_value(s, i)) return false;
            }

            skip_ws(s, i);
            if (match_char(s, i, '}'))
            {
                if (!has_game_type) temp.game_type = parse_game_type(temp.game_name);
                if (!has_k) temp.kills = 0;
                if (!has_d) temp.deaths = 0;
                if (!has_a) temp.assists = 0;

                if ((int)p.sessions.size() < MAX_ALLOWED) p.sessions.push_back(temp);
                break;
            }
            if (!match_char(s, i, ',')) return false;
        }

        skip_ws(s, i);
        if (match_char(s, i, ']')) return true;
        if (!match_char(s, i, ',')) return false;
    }
}

// Parse ["id","id2",...]
static bool parse_string_array(const string &s, size_t &i, vector<string> &out)
{
    if (!match_char(s, i, '[')) return false;
    skip_ws(s, i);
    if (match_char(s, i, ']')) return true;
    while (true)
    {
        string v;
        if (!parse_string_quoted(s, i, v)) return false;
        out.push_back(v);
        skip_ws(s, i);
        if (match_char(s, i, ']')) return true;
        if (!match_char(s, i, ',')) return false;
    }
}

static bool parse_player_json_string(const string &s, Player &p)
{
    size_t i = 0;
    if (!match_char(s, i, '{')) return false;

    string player_name, preferred_game;
    vector<Session> sessions_tmp;
    vector<string> achievements_tmp;

    while (true)
    {
        skip_ws(s, i);
        if (match_char(s, i, '}'))
        {
            p.player_name = player_name;
            p.preferred_game = preferred_game;
            p.sessions = std::move(sessions_tmp);
            p.achievements = std::move(achievements_tmp);
            return true;
        }

        string key;
        if (!parse_string_quoted(s, i, key)) return false;
        if (!match_char(s, i, ':')) return false;

        if (key == "player_name")
        {
            if (!parse_string_quoted(s, i, player_name)) return false;
        }
        else if (key == "preferred_game")
        {
            if (!parse_string_quoted(s, i, preferred_game)) return false;
        }
        else if (key == "sessions")
        {
            if (!parse_sessions_array(s, i, p)) return false; // writes into p.sessions
            sessions_tmp = p.sessions;                         // move into temp for final assignment
            p.sessions.clear();
        }
        else if (key == "achievements")
        {
            if (!parse_string_array(s, i, achievements_tmp)) return false;
        }
        else
        {
            if (!skip_value(s, i)) return false;
        }

        skip_ws(s, i);
        if (match_char(s, i, '}'))
        {
            p.player_name = player_name;
            p.preferred_game = preferred_game;
            p.sessions = std::move(sessions_tmp);
            p.achievements = std::move(achievements_tmp);
            return true;
        }
        if (!match_char(s, i, ',')) return false;
    }
}

// =======================================================
// Persistence (JSON + legacy TXT support) with Atomic Save
// =======================================================
static string safe_name(const string &raw)
{
    if (raw.empty()) return "default";
    string s = raw;
    for (char &c : s) if (strchr("/\\:*?\"<>|", c)) c = '_';
    return s;
}

static inline string json_filename(const string &name) { return safe_name(name) + "_data.json"; }
static inline string txt_filename (const string &name) { return safe_name(name) + "_data.txt";  }
static inline string bak_filename (const string &name) { return safe_name(name) + "_data.bak";  }
static inline string tmp_filename (const string &name) { return safe_name(name) + "_data.tmp";  }

bool profile_exists(const string &name)
{
    return fs::exists(json_filename(name)) || fs::exists(txt_filename(name));
}

static bool load_player_legacy(Player &p, const string &name)
{
    string f = txt_filename(name);
    ifstream in(f);
    if (!in) return false;

    p = Player();

    getline(in, p.player_name);
    getline(in, p.preferred_game);

    int n;
    if (!(in >> n)) { draw_toast("Legacy load failed: invalid file."); return false; }
    in.ignore();

    for (int i = 0; i < n; ++i)
    {
        Session s{}; int gtype = 0;
        if (!getline(in, s.game_name)) break;
        if (!(in >> gtype)) break; in.ignore();
        s.game_type = static_cast<GameType>(gtype);
        if (!(in >> s.kills >> s.deaths >> s.assists)) break; in.ignore();
        if (!getline(in, s.rank)) break;
        if (!getline(in, s.outcome)) break;
        if (!getline(in, s.session_date)) break;

        if ((int)p.sessions.size() < MAX_ALLOWED) p.sessions.push_back(s);
    }
    return true;
}

// Atomic save with backup
void save_player(const Player &p)
{
    if (p.player_name.empty()) return;

    string jf  = json_filename(p.player_name);
    string tf  = tmp_filename(p.player_name);
    string bak = bak_filename(p.player_name);

    // 1) Write to temp file
    {
        ofstream out(tf, ios::trunc);
        if (!out)
        {
            draw_toast("Save failed: cannot open temp file.");
            return;
        }

        out << "{\n";
        out << "  \"player_name\": \"" << json_escape(p.player_name) << "\",\n";
        out << "  \"preferred_game\": \"" << json_escape(p.preferred_game) << "\",\n";
        out << "  \"sessions\": [\n";

        for (int i = 0; i < (int)p.sessions.size(); ++i)
        {
            const Session &s = p.sessions[i];
            out << "    {\n";
            out << "      \"game_name\": \"" << json_escape(s.game_name) << "\",\n";
            out << "      \"game_type\": " << static_cast<int>(s.game_type) << ",\n";
            out << "      \"kills\": " << s.kills << ",\n";
            out << "      \"deaths\": " << s.deaths << ",\n";
            out << "      \"assists\": " << s.assists << ",\n";
            out << "      \"rank\": \"" << json_escape(s.rank) << "\",\n";
            out << "      \"outcome\": \"" << json_escape(s.outcome) << "\",\n";
            out << "      \"session_date\": \"" << json_escape(s.session_date) << "\"\n";
            out << "    }" << (i + 1 < (int)p.sessions.size() ? "," : "") << "\n";
        }

        out << "  ],\n";
        out << "  \"achievements\": [";
        for (int i = 0; i < (int)p.achievements.size(); ++i)
        {
            out << "\"" << json_escape(p.achievements[i]) << "\"";
            if (i + 1 < (int)p.achievements.size()) out << ", ";
        }
        out << "]\n";
        out << "}\n";
    }

    // 2) Backup current json to .bak (best effort)
    std::error_code ec;
    if (fs::exists(jf))
    {
        fs::copy_file(jf, bak, fs::copy_options::overwrite_existing, ec);
        // If copy fails we still proceed, but user keeps the temp file as fallback if next step fails.
    }

    // 3) Atomically replace: rename temp -> main
    fs::rename(tf, jf, ec);
    if (ec)
    {
        // Try overwrite by copy if rename failed (e.g., cross-device)
        ec.clear();
        fs::copy_file(tf, jf, fs::copy_options::overwrite_existing, ec);
        if (!ec)
        {
            fs::remove(tf); // cleanup temp
        }
        else
        {
            draw_toast("Save failed: could not replace data file.");
            return;
        }
    }

    // 4) Done
    // draw_toast("Saved.");
}

void load_player(Player &p, const string &name)
{
    // JSON first
    {
        string f = json_filename(name);
        ifstream in(f);
        if (in)
        {
            stringstream buffer; buffer << in.rdbuf();
            string content = buffer.str();

            p = Player();
            bool ok = parse_player_json_string(content, p);
            if (!ok)
            {
                draw_toast("Load failed: bad JSON. Trying legacy format...");
            }
            else
            {
                if (p.player_name.empty()) p.player_name = name;
                evaluate_achievements(p, false); // populate from data, no toasts at load
                return;
            }
        }
    }

    // Fallback: legacy TXT
    if (!load_player_legacy(p, name))
    {
        p = Player();
        p.player_name = name;
    }
    evaluate_achievements(p, false);
}

// =======================================================
// UI flows (Sessions, Analytics, Profiles)
// =======================================================
void add_session(Player &p)
{
    try
    {
        if ((int)p.sessions.size() >= MAX_ALLOWED) throw runtime_error("Cannot add more. Limit of 20 sessions reached.");

        Session s{};
        s.game_type   = prompt_until_ok([&]{ return parse_game_with_name(s.game_name); });
        s.kills       = prompt_until_ok([&]{ return parse_int_in_range("Kills (0-1000):", 0, 1000); });
        s.deaths      = prompt_until_ok([&]{ return parse_int_in_range("Deaths (0-1000):", 0, 1000); });
        s.assists     = prompt_until_ok([&]{ return parse_int_in_range("Assists (0-1000):", 0, 1000); });
        s.rank        = prompt_until_ok([&]{ return parse_nonempty("Rank (e.g., Gold 2):"); });
        s.outcome     = prompt_until_ok([&]{ return parse_outcome(); });
        s.session_date= prompt_until_ok([&]{ return parse_nonempty("Session date (YYYY-MM-DD):"); });

        p.sessions.push_back(s);
        evaluate_achievements(p, true); // toast any new unlocks
        draw_toast("Session added.");
        if (!p.player_name.empty()) save_player(p);
    }
    catch (const exception &e)
    {
        draw_toast(string("Could not add session: ") + e.what());
    }
}

void list_sessions(const Player &p)
{
    if (p.sessions.empty()) { draw_toast("No sessions recorded yet."); return; }

    // Card list with paging-ish simple pause at bottom
    draw_bg_with_dimmer(0.45);
    draw_hud("Sessions");

    int x = 24, y = 70;
    int avail_h = screen_height() - y - 40;

    for (int i = 0; i < (int)p.sessions.size(); ++i)
    {
        const Session &s = p.sessions[i];
        string head = to_string(i + 1) + ". " + s.session_date + " | " + s.game_name + " | Rank: " + s.rank;
        string kdas = "K/D/A: " + to_string(s.kills) + "/" + to_string(s.deaths) + "/" + to_string(s.assists);
        double kda = kda_for(s);

        draw_card_row(x, y, screen_width() - 48, 70);
        draw_ui_text(head, COL_TEXT, x + 8, y + 8, 18);

        // Format decimals to 2 places
        char kdastr[64];
        snprintf(kdastr, sizeof(kdastr), "KDA: %.2f", kda);

        string right = string(kdastr) + "   |   Outcome: " + s.outcome;

        draw_ui_text(kdas, COL_SUB, x + 8, y + 36, 16);
        draw_ui_text(right, (kda >= 2.0 ? COL_OK : (kda < 1.0 ? COL_BAD : COL_SUB)), x + 240, y + 36, 16);

        y += 78;
        if (y > avail_h)
        {
            string pi = "Press any key to continue...";
            int tw = ui_text_w(pi, 16);
            draw_ui_text(pi, COL_SUB, screen_width() - tw - 20, screen_height() - 28, 16);
            refresh_screen(60);
            while (!(key_typed(RETURN_KEY) || key_typed(ESCAPE_KEY) || mouse_clicked(LEFT_BUTTON)
                     || key_typed(SPACE_KEY) || key_typed(A_KEY) || key_typed(B_KEY)))
            {
                process_events(); delay(10);
            }
            draw_bg_with_dimmer(0.45);
            draw_hud("Sessions");
            y = 70;
        }
    }
    refresh_screen(60);

    while (!(key_typed(RETURN_KEY) || key_typed(ESCAPE_KEY) || mouse_clicked(LEFT_BUTTON)
             || key_typed(SPACE_KEY) || key_typed(A_KEY) || key_typed(B_KEY)))
    {
        process_events(); delay(10);
    }
}

void list_sessions_filtered(const Player &p)
{
    if (p.sessions.empty()) { draw_toast("No sessions recorded yet."); return; }

    string gname;
    GameType g = prompt_until_ok([&]{ return parse_game_with_name(gname); });

    vector<int> indices;
    for (int i = 0; i < (int)p.sessions.size(); ++i)
        if (p.sessions[i].game_type == g) indices.push_back(i);

    if (indices.empty()) { draw_toast("No sessions for " + gname + "."); return; }

    draw_bg_with_dimmer(0.45);
    draw_hud(gname + " Sessions");

    int x = 24, y = 70;
    for (int j = 0; j < (int)indices.size(); ++j)
    {
        int i = indices[j];
        const Session &s = p.sessions[i];

        draw_card_row(x, y, screen_width() - 48, 70);
        string head = to_string(j + 1) + ". " + s.session_date + " | Rank: " + s.rank;
        draw_ui_text(head, COL_TEXT, x + 8, y + 8, 18);

        double kda = kda_for(s);
        char kdastr[64];
        snprintf(kdastr, sizeof(kdastr), "KDA: %.2f", kda);

        string line = string("K/D/A: ") + to_string(s.kills) + "/" + to_string(s.deaths) + "/" + to_string(s.assists)
                    + " | Outcome: " + s.outcome + " | " + kdastr;

        draw_ui_text(line, (kda >= 2.0 ? COL_OK : (kda < 1.0 ? COL_BAD : COL_SUB)), x + 8, y + 36, 16);
        y += 78;
        if (y > screen_height() - 80) break;
    }

    refresh_screen(60);
    while (!(key_typed(RETURN_KEY) || key_typed(ESCAPE_KEY) || mouse_clicked(LEFT_BUTTON)
             || key_typed(SPACE_KEY) || key_typed(A_KEY) || key_typed(B_KEY)))
    {
        process_events(); delay(10);
    }
}

void show_summary(const Player &p)
{
    SummaryStats st = compute_summary(p);

    char kda2[64]; snprintf(kda2, sizeof(kda2), "%.2f", st.avg_kda);
    char wr2[64];  snprintf(wr2,  sizeof(wr2),  "%.2f", st.win_rate_percent);

    string line1 = "Player: " + (p.player_name.empty() ? string("(unnamed)") : p.player_name)
                 + " | Preferred: " + (p.preferred_game.empty() ? string("(unset)") : p.preferred_game) + "\n";
    string line2 = "Total Matches: " + to_string(st.total_matches)
                 + " | Wins: " + to_string(st.wins)
                 + " | Losses: " + to_string(st.losses) + "\n";
    string line3 = string("Avg KDA: ") + kda2
                 + " | Win Rate: " + wr2 + "%\n";
    string line4 = "Best Win Streak: " + to_string(st.best_win_streak)
                 + " | Best Loss Streak: " + to_string(st.best_loss_streak)
                 + " | High-KDA Streak (≥2.0): " + to_string(st.best_high_kda_streak) + "\n";

    int best_idx = find_best_session_by_kda(p);
    string line5;
    if (best_idx >= 0)
    {
        const Session &b = p.sessions[best_idx];
        char bkd[64]; snprintf(bkd, sizeof(bkd), "%.2f", kda_for(b));
        line5 = "Best Session: " + b.session_date + " | " + b.game_name
              + " | K/D/A " + to_string(b.kills) + "/" + to_string(b.deaths) + "/" + to_string(b.assists)
              + " | KDA " + bkd + "\n";
    }
    else line5 = "Best Session: N/A\n";

    show_long_text("Summary", line1 + line2 + line3 + line4 + line5);
}

// -----------------------------------------
// KDA Trend View (interactive)
// -----------------------------------------
void kda_trend_view(const Player &p)
{
    if (p.sessions.empty()) { draw_toast("No sessions to visualize."); return; }

    GameType filter = GameType::Unknown; // All
    bool use_rolling = true;
    int window = 5;

    while (true)
    {
        process_events();
        draw_bg_with_dimmer(0.35);
        draw_ui_text("KDA Trend  |  V: Filter   R: Rolling   W: Window   ESC: Exit", COL_SUB, 24, 24, 18);

        vector<int> idx = build_sorted_indices_by_date(p, filter);
        if (idx.empty())
        {
            draw_ui_text("No data for this filter.", COL_EMPH, 24, 64, 20);
            refresh_screen(60);
        }
        else
        {
            int n = (int)idx.size();
            vector<double> kdav; kdav.reserve(n);
            vector<bool> winv;  winv.reserve(n);
            double min_k = 1e9, max_k = -1e9;

            for (int i = 0; i < n; ++i)
            {
                const Session &s = p.sessions[idx[i]];
                double k = kda_for(s);
                kdav.push_back(k);
                winv.push_back(is_win(s.outcome));
                min_k = min(min_k, k);
                max_k = max(max_k, k);
            }
            if (!isfinite(min_k)) min_k = 0.0;
            if (!isfinite(max_k)) max_k = 1.0;
            if (fabs(max_k - min_k) < 1e-9) { max_k += 1.0; min_k -= 1.0; }

            vector<double> rave;
            if (use_rolling) rolling_average(kdav, window, rave);

            int left = 80, right = screen_width() - 60;
            int top = 90, bottom = screen_height() - 140;
            int w = right - left, h = bottom - top;

            draw_line(COL_SEP, left, bottom, right, bottom);
            draw_line(COL_SEP, left, top, left, bottom);

            double span = max_k - min_k;
            double step = (span > 5.0) ? 1.0 : 0.5;
            for (double yv = ceil(min_k / step) * step; yv <= max_k + 1e-9; yv += step)
            {
                int ypix = bottom - (int)round((yv - min_k) / span * h);
                draw_line(make_rgba(255,255,255,40), left, ypix, right, ypix);
                char buf[32]; snprintf(buf, sizeof(buf), "%.2f", yv);
                draw_ui_text(buf, COL_SUB, left - 60, ypix - 8, 16);
            }

            int xticks = max(2, n / 5);
            for (int i = 0; i < n; i += xticks)
            {
                int xpix = left + (int)round((double)i / (n - 1) * w);
                draw_line(make_rgba(255,255,255,40), xpix, top, xpix, bottom);
                const Session &s = p.sessions[idx[i]];
                draw_ui_text(s.session_date, COL_SUB, xpix - 40, bottom + 6, 14);
            }

            auto plot_series = [&](const vector<double> &vals, const color &col, int thickness)
            {
                if (vals.empty()) return;
                int px = left, py = bottom - (int)round((vals[0] - min_k) / span * h);
                for (int i = 1; i < (int)vals.size(); ++i)
                {
                    int x = left + (int)round((double)i / (n - 1) * w);
                    int y = bottom - (int)round((vals[i] - min_k) / span * h);
                    draw_line(col, px, py, x, y);
                    for (int t = 1; t < thickness; ++t) draw_line(col, px, py + t, x, y + t);
                    px = x; py = y;
                }
                for (int i = 0; i < (int)vals.size(); ++i)
                {
                    int x = left + (int)round((double)i / (n - 1) * w);
                    int y = bottom - (int)round((vals[i] - min_k) / span * h);
                    fill_circle(col, x, y, 2);
                }
            };

            plot_series(kdav, COL_EMPH, 1);
            if (use_rolling) plot_series(rave, COL_OK, 2);

            int stripe_top = bottom + 30;
            int stripe_h = 16;
            for (int i = 0; i < n; ++i)
            {
                int x1 = left + (int)round((double)i / (n - 1) * w);
                int x2 = left + (int)round((double)max(i,1) / (n - 1) * w);
                if (i + 1 < n) x2 = left + (int)round((double)(i + 1) / (n - 1) * w);
                int width = max(2, x2 - x1);
                color seg = winv[i] ? COL_WIN : COL_LOSS;
                fill_rectangle(seg, x1, stripe_top, width, stripe_h);
            }
            draw_ui_text("Win/Loss", COL_SUB, left - 60, stripe_top - 2, 14);

            string ft = (filter == GameType::Unknown ? "All" : (filter == GameType::Valorant ? "Valorant" : "CS:GO"));
            string tip = "Filter: " + ft + "   Rolling: " + string(use_rolling ? "ON" : "OFF") + " (W=" + to_string(window) + ")";
            draw_ui_text(tip, COL_TEXT, 24, 60, 18);

            refresh_screen(60);
        }

        if (key_typed(ESCAPE_KEY) || key_typed(RETURN_KEY) || mouse_clicked(LEFT_BUTTON)) break;
        if (key_typed(V_KEY))
        {
            if (filter == GameType::Unknown) filter = GameType::Valorant;
            else if (filter == GameType::Valorant) filter = GameType::CSGO;
            else filter = GameType::Unknown;
        }
        if (key_typed(R_KEY)) use_rolling = !use_rolling;
        if (key_typed(W_KEY)) window = (window == 3 ? 5 : window == 5 ? 7 : 3);

        delay(10);
    }
}

// ------------------------------------------------------
// Stats over the last N matches (sorted by date)
// ------------------------------------------------------
SummaryStats compute_summary_lastN(const Player &p, int N, GameType filter = GameType::Unknown)
{
    SummaryStats st{};
    vector<int> idx = build_sorted_indices_by_date(p, filter);
    if (idx.empty()) return st;

    int n = (int)idx.size();
    int start = (n > N) ? (n - N) : 0;
    int sum_k=0, sum_d=0, sum_a=0;
    int cur_win=0, cur_loss=0, cur_kda=0;

    for (int i = start; i < n; ++i)
    {
        const Session &s = p.sessions[idx[i]];
        if (is_win(s.outcome)) { st.wins++; cur_win++; st.best_win_streak = max(st.best_win_streak, cur_win); cur_loss = 0; }
        else if (is_loss(s.outcome)) { st.losses++; cur_loss++; st.best_loss_streak = max(st.best_loss_streak, cur_loss); cur_win = 0; }
        double k = kda_for(s);
        if (k >= 2.0) { cur_kda++; st.best_high_kda_streak = max(st.best_high_kda_streak, cur_kda); } else cur_kda = 0;

        sum_k += s.kills; sum_d += s.deaths; sum_a += s.assists;
        st.total_matches++;
    }

    double avg_deaths = st.total_matches ? max(1.0, sum_d / (double)st.total_matches) : 1.0;
    double avg_kills  = st.total_matches ? (sum_k / (double)st.total_matches) : 0.0;
    double avg_assists= st.total_matches ? (sum_a / (double)st.total_matches) : 0.0;

    st.avg_kda = (avg_kills + avg_assists) / avg_deaths;
    st.win_rate_percent = st.total_matches ? (st.wins * 100.0 / st.total_matches) : 0.0;
    return st;
}

// -----------------------------------------
// Quick Stats (Last N) View
// -----------------------------------------
void quick_stats_lastN_view(const Player &p)
{
    if (p.sessions.empty()) { draw_toast("No sessions recorded."); return; }

    GameType filter = GameType::Unknown;
    int N = 5;

    while (true)
    {
        process_events();
        draw_bg_with_dimmer(0.35);
        draw_ui_text("Quick Stats (Last N)  |  V: Filter   W: Window   ESC: Exit", COL_SUB, 24, 24, 18);

        vector<int> idxs = build_sorted_indices_by_date(p, filter);
        if (idxs.empty())
        {
            draw_ui_text("No data for this filter.", COL_EMPH, 24, 64, 20);
            refresh_screen(60);
        }
        else
        {
            SummaryStats stN = compute_summary_lastN(p, N, filter);

            char kda2[64]; snprintf(kda2, sizeof(kda2), "%.2f", stN.avg_kda);
            char wr2[64];  snprintf(wr2,  sizeof(wr2),  "%.2f", stN.win_rate_percent);

            string body;
            body += "Matches Considered: " + to_string(stN.total_matches)
                 + " | Wins: " + to_string(stN.wins)
                 + " | Losses: " + to_string(stN.losses) + "\n";
            body += string("Avg KDA (Last ") + to_string(N) + "): " + kda2
                 + " | Win Rate: " + wr2 + "%\n";
            body += "Best Win Streak (Last " + to_string(N) + "): " + to_string(stN.best_win_streak)
                 + " | Best Loss Streak: " + to_string(stN.best_loss_streak)
                 + " | High-KDA Streak (≥2.0): " + to_string(stN.best_high_kda_streak) + "\n";

            show_long_text("Quick Stats (Last " + to_string(N) + ") - " + (filter==GameType::Unknown? "All" : (filter==GameType::Valorant? "Valorant":"CS:GO")), body);
        }

        if (key_typed(ESCAPE_KEY) || key_typed(RETURN_KEY) || mouse_clicked(LEFT_BUTTON)) return;
        if (key_typed(V_KEY))
        {
            if (filter == GameType::Unknown) filter = GameType::Valorant;
            else if (filter == GameType::Valorant) filter = GameType::CSGO;
            else filter = GameType::Unknown;
        }
        if (key_typed(W_KEY)) N = (N == 3 ? 5 : N == 5 ? 7 : 3);

        delay(10);
    }
}

// =======================================================
// Profiles: Create / Switch / Update / List / Delete
// =======================================================
static inline string txt(double v) { char b[64]; snprintf(b, sizeof(b), "%.2f", v); return string(b); }

void create_new_profile(Player &p)
{
    while (true)
    {
        string pname = prompt_line("Enter NEW player profile name:");
        if (pname == "") { draw_toast("Creation cancelled."); return; }
        if (profile_exists(pname)) { draw_toast("Profile already exists. Choose another name."); continue; }
        p = Player();
        p.player_name = pname;
        p.preferred_game = "";
        evaluate_achievements(p, false);
        save_player(p);
        draw_toast("New profile created: " + pname);
        return;
    }
}

void do_switch_profile(Player &p)
{
    while (true)
    {
        string pname = prompt_line("Switch to player profile (must exist):");
        if (pname == "") { draw_toast("Switch cancelled."); return; }
        if (!profile_exists(pname)) { draw_toast("Profile not found. Try again."); continue; }
        load_player(p, pname);
        if (p.player_name.empty()) p.player_name = pname;
        draw_toast("Profile loaded: " + p.player_name);
        return;
    }
}

void update_player_info(Player &p)
{
    if (p.player_name.empty())
    {
        draw_toast("No active profile. Create or switch to a profile first.");
        return;
    }

    string new_name = prompt_until_ok([&]{ return parse_nonempty("Update player name:"); });
    if (new_name != p.player_name) p.player_name = new_name;
    p.preferred_game = prompt_until_ok([&]{ return parse_nonempty("Update preferred FPS game:"); });
    save_player(p);
    draw_toast("Profile info updated.");
}

void list_profiles()
{
    string report = "Existing Profiles:\n\n";
    bool found_any = false;
    unordered_set<string> names;

    for (const auto &entry : fs::directory_iterator("."))
    {
        if (!entry.is_regular_file()) continue;
        string fname = entry.path().filename().string();
        if (fname.size() > 10 && fname.substr(fname.size() - 10) == "_data.json")
        {
            string prof_name = fname.substr(0, fname.size() - 10);
            if (names.insert(prof_name).second) { report += "- " + prof_name + " (json)\n"; found_any = true; }
        }
        else if (fname.size() > 9 && fname.substr(fname.size() - 9) == "_data.txt")
        {
            string prof_name = fname.substr(0, fname.size() - 9);
            if (names.insert(prof_name).second) { report += "- " + prof_name + " (legacy)\n"; found_any = true; }
        }
    }

    if (!found_any) { draw_toast("No profiles found."); return; }
    show_long_text("Profiles", report);
}

void delete_profile(Player &p)
{
    list_profiles();

    while (true)
    {
        string target = prompt_line("Enter profile name to DELETE (Esc to cancel):");
        if (target == "") { draw_toast("Deletion cancelled."); return; }
        if (!profile_exists(target)) { draw_toast("Profile not found. Try again."); continue; }

        if (!confirm_yes("Are you sure you want to delete profile '" + target + "'? This cannot be undone."))
        {
            draw_toast("Deletion cancelled."); return;
        }

        std::error_code ec;
        bool any = false;
        string jf = json_filename(target);
        string tf = txt_filename(target);
        if (fs::exists(jf)) { any = fs::remove(jf, ec) || any; }
        if (fs::exists(tf)) { any = fs::remove(tf, ec) || any; }

        if (!any) { draw_toast("No files removed (permission?)."); return; }

        if (to_lowercase(target) == to_lowercase(p.player_name))
        {
            p = Player();
            draw_toast("Profile deleted and unloaded.");
        }
        else draw_toast("Profile deleted.");

        return;
    }
}

void delete_session(Player &p)
{
    if (p.sessions.empty()) { draw_toast("No sessions to delete."); return; }

    list_sessions(p);

    int idx1 = prompt_until_ok([&]{
        return parse_int_in_range("Enter session number to delete (1-" + to_string((int)p.sessions.size()) + "):", 1, (int)p.sessions.size());
    });

    const Session &s = p.sessions[idx1 - 1];
    string confirm_msg = "Delete session #" + to_string(idx1) + " (" + s.session_date + " | " + s.game_name + ")?";
    if (!confirm_yes(confirm_msg)) { draw_toast("Deletion cancelled."); return; }

    p.sessions.erase(p.sessions.begin() + (idx1 - 1));
    evaluate_achievements(p, false);
    draw_toast("Session deleted.");
    if (!p.player_name.empty()) save_player(p);
}

// =======================================================
// Generic Menu Renderer (title + items)
// =======================================================
int run_menu(const string &title, const vector<string> &items)
{
    while (true)
    {
        process_events();
        draw_bg_with_dimmer(0.35);

        draw_ui_text(title, COL_TEXT, 20, 20, 28);
        int y = 80;
        for (size_t i = 0; i < items.size(); ++i)
        {
            draw_card_row(32, y, screen_width() - 64, 36);
            draw_ui_text(to_string((int)i + 1) + ") " + items[i], COL_EMPH, 32, y + 8, 20);
            y += 44;
        }
        draw_ui_text("Press number key or click an option. ESC to go back.", COL_SUB, 32, y + 10, 16);
        refresh_screen(60);

        if (key_typed(ESCAPE_KEY)) return -1;
        if (key_typed(NUM_1_KEY) && items.size() >= 1) return 0;
        if (key_typed(NUM_2_KEY) && items.size() >= 2) return 1;
        if (key_typed(NUM_3_KEY) && items.size() >= 3) return 2;
        if (key_typed(NUM_4_KEY) && items.size() >= 4) return 3;
        if (key_typed(NUM_5_KEY) && items.size() >= 5) return 4;
        if (key_typed(NUM_6_KEY) && items.size() >= 6) return 5;
        if (key_typed(NUM_7_KEY) && items.size() >= 7) return 6;
        if (key_typed(NUM_8_KEY) && items.size() >= 8) return 7;
        if (key_typed(NUM_9_KEY) && items.size() >= 9) return 8;
        if (key_typed(NUM_0_KEY) && items.size() >= 10) return 9;

        if (mouse_clicked(LEFT_BUTTON))
        {
            point_2d mp = mouse_position();
            int y2 = 80;
            for (size_t i = 0; i < items.size(); ++i)
            {
                rectangle btn = rectangle_from(32, y2, screen_width() - 64, 36);
                if (point_in_rectangle(mp, btn)) return (int)i;
                y2 += 44;
            }
        }

        delay(16);
    }
}

// =======================================================
// Submenus
// =======================================================
void profile_menu(Player &player)
{
    vector<string> items = {
        "Create New Profile",
        "Switch Profile",
        "Update Player Info",
        "List Existing Profiles",
        "Delete Profile",
        "Back"
    };

    while (true)
    {
        int c = run_menu("Profile Management", items);
        if (c == -1 || c == 5) return;
        switch (c)
        {
            case 0: create_new_profile(player); break;
            case 1: do_switch_profile(player); break;
            case 2: update_player_info(player); break;
            case 3: list_profiles(); break;
            case 4: delete_profile(player); break;
        }
    }
}

void sessions_menu(Player &player)
{
    vector<string> items = {
        "Add Session",
        "List Sessions",
        "Filter Sessions by Game",
        "Delete Session",
        "Back"
    };

    while (true)
    {
        int c = run_menu("Sessions", items);
        if (c == -1 || c == 4) return;
        switch (c)
        {
            case 0: add_session(player); break;
            case 1: list_sessions(player); break;
            case 2: list_sessions_filtered(player); break;
            case 3: delete_session(player); break;
        }
    }
}

void analytics_menu(Player &player)
{
    vector<string> items = {
        "Show Summary",
        "KDA Trend (interactive)",
        "Quick Stats (Last N)",
        "Achievements",            // NEW
        "Back"
    };

    while (true)
    {
        int c = run_menu("Analytics", items);
        if (c == -1 || c == 4) return;
        switch (c)
        {
            case 0: show_summary(player); break;
            case 1: kda_trend_view(player); break;
            case 2: quick_stats_lastN_view(player); break;
            case 3: show_achievements_screen(player); break;
        }
    }
}

// =======================================================
// Window Settings (unchanged from your last good version)
// =======================================================
void reopen_window_at(int w, int h)
{
    close_all_windows();
    open_window("FPS Game Tracker", w, h);
    setup_font();
    setup_theme();
}

void window_settings_menu()
{
    vector<string> items = {
        "1280 x 720 (HD)",
        "1600 x 900",
        "1920 x 1080 (Full HD)",
        "2560 x 1440 (QHD)",
        "Custom...",
        "Back"
    };

    while (true)
    {
        int c = run_menu("Window Settings", items);
        if (c == -1 || c == 5) return;

        int w = 1280, h = 720;
        if (c == 0) { w = 1280; h = 720; }
        else if (c == 1) { w = 1600; h = 900; }
        else if (c == 2) { w = 1920; h = 1080; }
        else if (c == 3) { w = 2560; h = 1440; }
        else if (c == 4)
        {
            try
            {
                w = prompt_until_ok([&]{ return parse_int_in_range("Width (640-3840):", 640, 3840); });
                h = prompt_until_ok([&]{ return parse_int_in_range("Height (480-2160):", 480, 2160); });
            }
            catch (...) { draw_toast("Cancelled."); continue; }
        }

        reopen_window_at(w, h);
        draw_toast("Window resized to " + to_string(w) + "x" + to_string(h));
        return;
    }
}

// =======================================================
// Main
// =======================================================
int main()
{
    open_window("FPS Game Tracker", 1280, 720);
    setup_font();
    setup_theme();

    Player player;

    vector<string> main_items = {
        "Profile Management",
        "Sessions",
        "Analytics",
        "Window Settings",
        "Save & Exit"
    };

    bool running = true;
    while (running)
    {
        int choice = run_menu("Main Menu", main_items);
        if (choice == -1)
        {
            if (confirm_yes("Exit the program?")) break;
            else continue;
        }

        switch (choice)
        {
            case 0: profile_menu(player); break;
            case 1: sessions_menu(player); break;
            case 2: analytics_menu(player); break;
            case 3: window_settings_menu(); break;
            case 4: running = false; break;
            default: break;
        }
    }

    try
    {
        if (player.player_name.empty())
            draw_toast("Tip: set a profile name in 'Profile Management > Update Player Info' to save under your name.");
        save_player(player);
    }
    catch (const exception &e)
    {
        draw_toast(string("Save failed: ") + e.what());
    }

    close_all_windows();
    return 0;
}
