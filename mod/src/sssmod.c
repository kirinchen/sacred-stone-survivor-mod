/* sssmod — 聖石倖存者模組 ① 可行性驗證用的注入 DLL。
 *
 * 載入方式:建成 dsound.dll 放進 game/(只放複本,不動 Steam 原版)。WUJIN.exe 只用 DSOUND #1,
 * 所有匯出轉發到 C:\Windows\SysWOW64\dsound.dll。不改 exe、不用遠端注入。
 *
 * 掛勾(WUJIN.exe 匯入表,imagebase 0x400000、無 ASLR):
 *   RegisterClassA   0x4aa23c  → 包 WndProc(保留給之後的鍵盤 / 隊友切換)
 *   PeekMessageA     0x4aa278  → 主執行緒 tick:讀指令檔、做生怪、備援幀數
 *   DirectDrawCreate 0x4aa048  → 掛 IDirectDraw::CreateSurface → 掛 Surface::Blt/Flip 量幀數 + 畫 overlay
 *
 * 遊戲內位址(逆向自 WUJIN.exe V2.85,見 notes/01-feasibility.md、03-survivor-v1.md):
 *   *(void**)0x4c31f0                 物件管理器;+0xc = 物件表(0x801 槽)
 *   0x44d7d0 __thiscall create(mgr, stage, parent, x, y, z, type, wait) → obj
 *   0x444b60 __thiscall setFaction(obj, set, clear)   obj+0x100:1 綠(友)/ 2 紅(敵)
 *   0x444e00 __thiscall setChar(obj, charId, 1)       套角色數值;obj+0xfc = charId
 *   obj: +4 type  +8 class  +0x14/+0x18/+0x1c x/y/z (16.16)  +0xac stage  +0xbc dir
 *
 * 指令:在 game/ 放 sss_cmd.txt(每行一條),每 ~0.25 秒讀一次後刪掉:
 *   spawn N        複製場上第一隻敵人,在玩家(或該敵人)周圍生 N 隻
 *   dump           物件表寫進 sss_mod.log
 *   key VK [ms]    對遊戲 WndProc 送 WM_KEYDOWN/UP(十進位或 0x 開頭)
 *   overlay 0|1
 *   surv           = 按 F9:倖存者模式開 / 關(v1 起任何地圖都行;自動出手 + 升級選單,見 notes/03-survivor-v1.md)
 *   pick N / auto 0|1 / castms N / leash N / super 0|1 / learn ID / moves A B / items A B / goto X Y / row … / peek / poke
 *   post M W L     PostMessage 給遊戲視窗(會經過 WndProc 掛勾)
 */
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- dsound proxy -------------------------------------------------------------------- */
static HMODULE g_real_ds;
static FARPROC ds_proc(const char *name) {
    if (!g_real_ds) {
        char p[MAX_PATH];
        GetSystemWow64DirectoryA(p, MAX_PATH);           /* 32-bit 系統 dll 目錄 */
        if (!p[0]) GetSystemDirectoryA(p, MAX_PATH);
        strcat(p, "\\dsound.dll");
        g_real_ds = LoadLibraryA(p);
    }
    return g_real_ds ? GetProcAddress(g_real_ds, name) : NULL;
}
#define FWD(name, ord) \
    __declspec(naked) void __stdcall fwd_##name(void) { \
        static FARPROC f; \
        __asm { mov eax, f } \
        __asm { test eax, eax } \
        __asm { jnz go } \
        __asm { push offset s_##name } \
        __asm { call ds_proc } \
        __asm { add esp, 4 } \
        __asm { mov f, eax } \
        __asm { go: jmp eax } \
    }
static const char s_DirectSoundCreate[] = "DirectSoundCreate";
static const char s_DirectSoundEnumerateA[] = "DirectSoundEnumerateA";
static const char s_DirectSoundEnumerateW[] = "DirectSoundEnumerateW";
static const char s_DllCanUnloadNow[] = "DllCanUnloadNow";
static const char s_DllGetClassObject[] = "DllGetClassObject";
static const char s_DirectSoundCaptureCreate[] = "DirectSoundCaptureCreate";
static const char s_DirectSoundCaptureEnumerateA[] = "DirectSoundCaptureEnumerateA";
static const char s_DirectSoundCaptureEnumerateW[] = "DirectSoundCaptureEnumerateW";
static const char s_GetDeviceID[] = "GetDeviceID";
static const char s_DirectSoundFullDuplexCreate[] = "DirectSoundFullDuplexCreate";
static const char s_DirectSoundCreate8[] = "DirectSoundCreate8";
static const char s_DirectSoundCaptureCreate8[] = "DirectSoundCaptureCreate8";
FWD(DirectSoundCreate, 1)
FWD(DirectSoundEnumerateA, 2)
FWD(DirectSoundEnumerateW, 3)
FWD(DllCanUnloadNow, 4)
FWD(DllGetClassObject, 5)
FWD(DirectSoundCaptureCreate, 6)
FWD(DirectSoundCaptureEnumerateA, 7)
FWD(DirectSoundCaptureEnumerateW, 8)
FWD(GetDeviceID, 9)
FWD(DirectSoundFullDuplexCreate, 10)
FWD(DirectSoundCreate8, 11)
FWD(DirectSoundCaptureCreate8, 12)
#pragma comment(linker, "/export:DirectSoundCreate=_fwd_DirectSoundCreate@0,@1")
#pragma comment(linker, "/export:DirectSoundEnumerateA=_fwd_DirectSoundEnumerateA@0,@2")
#pragma comment(linker, "/export:DirectSoundEnumerateW=_fwd_DirectSoundEnumerateW@0,@3")
#pragma comment(linker, "/export:DllCanUnloadNow=_fwd_DllCanUnloadNow@0,@4,PRIVATE")
#pragma comment(linker, "/export:DllGetClassObject=_fwd_DllGetClassObject@0,@5,PRIVATE")
#pragma comment(linker, "/export:DirectSoundCaptureCreate=_fwd_DirectSoundCaptureCreate@0,@6")
#pragma comment(linker, "/export:DirectSoundCaptureEnumerateA=_fwd_DirectSoundCaptureEnumerateA@0,@7")
#pragma comment(linker, "/export:DirectSoundCaptureEnumerateW=_fwd_DirectSoundCaptureEnumerateW@0,@8")
#pragma comment(linker, "/export:GetDeviceID=_fwd_GetDeviceID@0,@9")
#pragma comment(linker, "/export:DirectSoundFullDuplexCreate=_fwd_DirectSoundFullDuplexCreate@0,@10")
#pragma comment(linker, "/export:DirectSoundCreate8=_fwd_DirectSoundCreate8@0,@11")
#pragma comment(linker, "/export:DirectSoundCaptureCreate8=_fwd_DirectSoundCaptureCreate8@0,@12")

/* ---- log ----------------------------------------------------------------------------- */
static FILE *g_log;
static char g_dir[MAX_PATH];
static void mlog(const char *fmt, ...) {
    va_list ap; SYSTEMTIME t;
    if (!g_log) return;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}

/* 當機紀錄:存取違規時把位址、暫存器、堆疊上看起來像原作回傳位址的值寫進 log(模組自己 __try 接住的也會記,最多 40 筆) */
static LONG CALLBACK crash_veh(EXCEPTION_POINTERS *e) {
    static int n; EXCEPTION_RECORD *r = e->ExceptionRecord; CONTEXT *c = e->ContextRecord;
    if (r->ExceptionCode == 0xc0000005 && n < 40) {
        char b[400]; int k = 0, i; DWORD *sp = (DWORD *)c->Esp;
        n++;
        for (i = 0; i < 160 && k < 360; i++) { DWORD v; if (IsBadReadPtr(sp + i, 4)) break; v = sp[i]; if (v >= 0x401000 && v < 0x4aa000) k += sprintf(b + k, " %x", v); }
        b[k] = 0;
        mlog("AV #%d eip=%08x %s %08x eax=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x | stack:%s", n, c->Eip, r->ExceptionInformation[0] ? "write" : "read", (unsigned)r->ExceptionInformation[1],
             c->Eax, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, b);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ---- IAT patch ----------------------------------------------------------------------- */
static void *patch_ptr(void **slot, void *val) {
    DWORD old; void *prev = *slot;
    VirtualProtect(slot, sizeof(void *), PAGE_EXECUTE_READWRITE, &old);
    *slot = val;
    VirtualProtect(slot, sizeof(void *), old, &old);
    return prev;
}

/* ---- game ---------------------------------------------------------------------------- */
#define GAME_OBJMGR   ((unsigned char **)0x4c31f0)
#define OBJ_SLOTS     0x801
/* __thiscall == __fastcall 且 EDX 不用:C 裡用 fastcall 呼叫 */
typedef int *(__fastcall *create_t)(void *mgr, int edx_, int stage, int parent, int x, int y, int z, int type, int wait);
typedef void (__fastcall *faction_t)(void *obj, int edx_, unsigned set, unsigned clear);
typedef int  (__fastcall *setchar_t)(void *obj, int edx_, int charId, int flag);
static const create_t  game_create  = (create_t)0x44d7d0;
static const faction_t game_faction = (faction_t)0x444b60;
static const setchar_t game_setchar = (setchar_t)0x444e00;
#define I32(o, off) (*(int *)((unsigned char *)(o) + (off)))
/* 角色類物件(>= 0x1a8 bytes,才有 +0x100 陣營 / +0xfc 數值);遮罩與原載入器 0x452780 相同 */
#define IS_CHAR(o)  ((I32(o, 8) & 0x23e306) != 0)

static unsigned char **obj_table(void) {
    unsigned char *mgr = *GAME_OBJMGR;
    if (!mgr) return NULL;
    return *(unsigned char ***)(mgr + 0xc);
}

static int g_nobj, g_nenemy;
static void count_objs(void) {
    unsigned char **t = obj_table(); int i, n = 0, e = 0;
    if (t) for (i = 1; i < OBJ_SLOTS; i++) {
        unsigned char *o = t[i];
        if (!o) continue;
        n++;
        if (IS_CHAR(o) && (I32(o, 0x100) & 2) && I32(o, 0xfc)) e++;
    }
    g_nobj = n; g_nenemy = e;
}

static void do_dump(void) {
    unsigned char **t = obj_table(); int i;
    mlog("dump: mgr=%p table=%p", *GAME_OBJMGR, t);
    if (!t) return;
    __try {
        for (i = 1; i < OBJ_SLOTS; i++) {
            unsigned char *o = t[i];
            if (!o) continue;
            mlog("  slot %4d obj=%p vt=%08x type=%5d class=%08x fac=%08x char=%5d pos=(%d,%d,%d) stage=%08x",
                 i, o, I32(o, 0), I32(o, 4), I32(o, 8), IS_CHAR(o) ? I32(o, 0x100) : -1, IS_CHAR(o) ? I32(o, 0xfc) : -1,
                 I32(o, 0x14) >> 16, I32(o, 0x18) >> 16, I32(o, 0x1c) >> 16, I32(o, 0xac));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("dump: exception %08x", GetExceptionCode()); }
}

/* 玩家隊伍:陣營 0 且有角色數值(實測:素還真 type 2001/char 1、青陽子 2009/2;陣營 1 是友方 NPC) */
static unsigned char *find_player(unsigned char **t) {
    int i;
    { unsigned char *gs = *(unsigned char **)0x4c3214, *pl = (gs && *(void **)(gs + 0x10)) ? *(unsigned char **)(gs + 8) : NULL;       /* 原作記的主角物件(場上可能有模組生的同陣營夥伴,不能只看陣營) */
      if (pl) for (i = 1; i < OBJ_SLOTS; i++) if (t[i] == pl) return pl; }
    for (i = 1; i < OBJ_SLOTS; i++) {
        unsigned char *o = t[i];
        if (o && IS_CHAR(o) && !(I32(o, 0x100) & 7) && I32(o, 0xfc)) return o;
    }
    return NULL;
}
static unsigned char *find_enemy(unsigned char **t) {
    int i;
    for (i = 1; i < OBJ_SLOTS; i++) {
        unsigned char *o = t[i];
        if (o && IS_CHAR(o) && (I32(o, 0x100) & 2) && I32(o, 0xfc)) return o;
    }
    return NULL;
}

typedef unsigned char *(__cdecl *chardef_t)(int id);
static const chardef_t game_chardef = (chardef_t)0x427df0;      /* 角色數值表;def[0] 旗標(&0xc 怪物、&3 隊員),def+4 名字 Big5 */
static void big5_to_utf8(const char *in, char *out, int n) {
    wchar_t w[128];
    int k = MultiByteToWideChar(950, 0, in, -1, w, 128);
    if (k <= 0) { out[0] = 0; return; }
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, n, NULL, NULL);
}
static void do_chars(void) {
    int id, n = 0; char name[256];
    for (id = 1; id < 20000; id++) {
        unsigned char *d = NULL;
        __try { d = game_chardef(id); } __except (EXCEPTION_EXECUTE_HANDLER) { d = NULL; }
        if (!d) continue;
        __try { big5_to_utf8((const char *)d + 4, name, sizeof name); } __except (EXCEPTION_EXECUTE_HANDLER) { strcpy(name, "?"); }
        mlog("  char %5d flags=%08x hp=%d name=%s", id, I32(d, 0), I32(d, 0x28), name);
        n++;
    }
    mlog("chars: %d entries", n);
}
static void do_types(void) {
    int *t = *(int **)0x4c7b60, n = *(int *)0x4d3fc0, i;
    mlog("types: %d entries at %p", n, t);
    for (i = 0; t && i < n; i++) {
        int *e = t + i * 8;
        if (e[6] & 0x23e306)
            mlog("  type %5d class=%08x f1=%08x f2=%08x f3=%08x f4=%08x f5=%08x f7=%08x", e[0], e[6], e[1], e[2], e[3], e[4], e[5], e[7]);
    }
}
static int g_type_override, g_char_override;
static void do_spawn(int n) {
    unsigned char **t = obj_table(); unsigned char *tpl, *pl, *ctr;
    int i, ok = 0, type, chr, fac, stage, dir, cx, cy, cz;
    LARGE_INTEGER f, a, b;
    if (!t) { mlog("spawn: no object table"); return; }
    tpl = find_enemy(t); pl = find_player(t);
    if (!tpl && !(g_type_override && pl)) { mlog("spawn: no enemy on stage to copy (use: type T char C)"); return; }
    if (g_type_override) tpl = NULL;
    type = tpl ? I32(tpl, 4) : g_type_override;
    chr = tpl ? I32(tpl, 0xfc) : g_char_override;
    fac = tpl ? (I32(tpl, 0x100) & 7) : 2;
    ctr = pl ? pl : tpl;
    stage = I32(ctr, 0xac); dir = I32(ctr, 0xbc);
    cx = I32(ctr, 0x14); cy = I32(ctr, 0x18); cz = I32(ctr, 0x1c);
    mlog("spawn %d: tpl=%p type=%d char=%d fac=%d stage=%08x center=%s(%d,%d,%d)",
         n, tpl, type, chr, fac, stage, pl ? "player" : "enemy", cx >> 16, cy >> 16, cz >> 16);
    QueryPerformanceFrequency(&f); QueryPerformanceCounter(&a);
    for (i = 0; i < n; i++) {
        double ang = i * 2.399963;                  /* 黃金角,平均散開 */
        int r = 70 + (i % 5) * 25;
        int x = cx + ((int)(r * cos(ang)) << 16), y = cy + ((int)(r * sin(ang) * 0.6) << 16);
        int *o = NULL;
        __try {
            o = game_create(*GAME_OBJMGR, 0, stage, 0, x, y, cz, type, 0);
            if (o) {
                I32(o, 0xbc) = dir;
                game_faction(o, 0, fac, 0);
                game_setchar(o, 0, chr, 1);
                ok++;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            mlog("spawn: exception %08x at #%d", GetExceptionCode(), i); break;
        }
    }
    QueryPerformanceCounter(&b);
    count_objs();
    mlog("spawn done: %d/%d ok in %.2f ms; objects=%d enemies=%d", ok, n,
         (b.QuadPart - a.QuadPart) * 1000.0 / f.QuadPart, g_nobj, g_nenemy);
}

/* ---- 倖存者模式 v0(F9 開關)--------------------------------------------------------------
 * 在「目前這張戰鬥地圖」上割草:波次定時在玩家外圍一圈生怪,越後面越多、越快;
 * 統計存活時間 / 擊殺;主角命歸零或換地圖就結算。怪物種類 = 開始時場上看到的敵人(沒有就用蝙蝠)。 */
static int g_rg, rg_spawn_kind, g_tscale = 1, g_toff;       /* Roguelite(rogue.c):g_rg = 一局進行中 */
static unsigned char *g_last_spawn;
static DWORD surv_ms(void);
static void rg_norm(unsigned char *o, int kind);
static void rg_finish(int win, const char *why);
static void rg_play_tick(unsigned char **t, unsigned char *pl, DWORD now);
static void rg_draw(HDC dc);
static int  rg_money_now(void);
static int  rg_input(UINT m, WPARAM w, LPARAM l);
static int g_title_slot;      /* 從標題開局時讀哪一格存檔(rogue.c 的 title_save_slot 選) */
static void rg_f9(void);
static int  rg_ui_open(void);
static int  ui_menu_open(void);
static int g_mx = -100, g_my = -100, g_cur_dirty, g_cur_saved, g_cur_sx, g_cur_sy;      /* 選單裡自己畫的滑鼠游標(見 cur_draw) */
static void ui_present(void);
static int  rg_upg_ok(void);
static void rg_before_goto(void);
static unsigned char *lead_obj(void);
static void recalc_keep(unsigned char *sb);
static int  rg_special_near(int px, int py, int r2, int want_boss);
static void rg_upg_label(wchar_t *out, int n);
static void rg_upg_apply(void);
static int  v1_paused(void);
static void v1_tick(unsigned char **t, unsigned char *pl, DWORD now, int kills);
static void v1_reset(void);
static void v1_end(void);
#define SURV_MAX_TPL   12
#define SURV_CAP       80          /* 場上敵人上限(bench:~120 隻仍 57 fps) */
/* 生成半徑:200–300 太遠,怪會停在索敵範圍外不過來(實測);可用指令 survr MIN MAX 調 */
static int g_god, g_hpbuf, g_hitcap = 15, g_last_hp, g_maxhit, g_capped, g_fixmax, g_dpscap = 12, g_dpscapped;
static int g_surv_cap = SURV_CAP;   /* survcap N(最多 240)*/
static int g_surv_rmin = 100, g_surv_rmax = 170;
/* 原作怪物索敵範圍很小,主角不動就不會過來(實測);先每 50 ms 把遠的敵人往主角推 —— 會用滑的,沒有走路動畫。survpull 0..N 調速度(px/步) */
static int g_surv_pull = 2;
static int g_surv, g_surv_over;     /* g_surv:進行中;g_surv_over:已結算(畫面保留成績) */
static DWORD g_surv_t0, g_surv_next, g_surv_end_ms;
static int g_surv_wave, g_surv_spawned, g_surv_base_enemy, g_surv_kills, g_surv_lead, g_surv_stage;
static struct { int type, chr; } g_surv_tpl[SURV_MAX_TPL]; static int g_surv_ntpl, g_surv_ntpl0;
/* 後面幾波加進來的怪:{第幾波起, 圖像 type, 數值 char}。🔴 圖像和數值要是原作的配對:亂配(例:弓手圖像 + 刀手數值)第 4 波直接閃退(實測) */
static const int g_tiers[][3] = { {1, 2071, 11}, {2, 2072, 12}, {3, 2073, 13}, {5, 2075, 15}, {6, 2074, 14}, {8, 2073, 213}, {9, 2072, 212}, {11, 2077, 313} };   /* 蝙蝠 蜘蛛 黃刀手 蟾蜍 黃弓手 → 強化版 */
/* ---- 追擊方式(指令 survchase N)--------------------------------------------------------
 * 0 關 / 1 推(v0:每 50 ms 改座標,滑步、穿牆)/ 2 原作走路(定時對每隻下「走到」0x445a80,有動畫、原作尋路)
 * / 3 放大原作 AI 的索敵範圍(AI 定義表 0x427e60(stat+0x80) 的 short[3] 索敵、[2] 放棄距離,單位 = 格),讓原作 AI 自己追。
 * 2 的規則:離主角 g_walk_hand px 內、或原作 AI 已鎖定目標(obj+0x150 & 2)就不再下指令,交還原作 AI;
 *          忙(obj+0x188 硬直 / 指令 5 攻擊中)不打斷;每隻錯開 g_walk_ms 重下一次;目的地 = 主角周圍散開的點。
 *          🔴 不呼叫原作 can-act 0x446cf0:它對某些類型會寫 obj+0x18c(有副作用),直接讀 +0x188。 */
static int g_fps;
static int g_spawn_snap = 1, g_snap_moved, g_snap_fail;
static int g_chase = 2, g_walk_ms = 400, g_walk_hand = 48, g_aggro = 60, g_chase_log = 1;
static struct { int id; DWORD born, next; short lx, ly, bd; unsigned char stuck, arrived; } g_ch[OBJ_SLOTS];
static struct { int n_arr; unsigned sum_arr, sum_bd; int calls, stuck, nudge, seen; } g_cs;
static struct { short *p; short s2, s3; int id; } g_ai_sav[16]; static int g_ai_nsav;
static void aggro_track(unsigned char *o) {
    unsigned char *sb = *(unsigned char **)(o + 0x1a4); int id, i; short *p = NULL;
    if (!sb || !(id = I32(sb, 0x80))) return;
    for (i = 0; i < g_ai_nsav; i++) if (g_ai_sav[i].id == id) return;
    if (g_ai_nsav >= 16) return;
    __try { p = ((short *(__cdecl *)(int))0x427e60)(id); } __except (EXCEPTION_EXECUTE_HANDLER) { p = NULL; }
    if (!p) return;
    g_ai_sav[g_ai_nsav].p = p; g_ai_sav[g_ai_nsav].id = id; g_ai_sav[g_ai_nsav].s2 = p[2]; g_ai_sav[g_ai_nsav].s3 = p[3]; g_ai_nsav++;
    mlog("ai def %d (char %d): wander=%d/%d leash=%d aggro=%d [4]=%d cast=%d [6..8]=%d,%d,%d keep=%d swap=%d call=%d  (cell=%dpx)", id, I32(o, 0xfc),
         p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11], 1 << (I32(I32(o, 0xac), 0x7d0) & 31));
}
/* 搬角色(直接改座標)前一定要先叫這個 —— 原作每個角色在地圖格子上蓋「有人站著」的記號(格子 byte 的 0x80,範圍記在 obj+0x198 的遮罩),
 * 尋路會繞開;原作擦記號 0x449a30 → 0x4543f0 是照「目前位置」擦。直接改座標的話舊位置的記號永遠擦不掉 = 隱形牆,越積越多,
 * 實測一局 1.5–2 分鐘後主角、夥伴、怪全部走不動(Kirin 2026-11-24 實玩;同一個遊戲行程連開幾局,牆還留著)。 */
static int g_unmark_n;
static void obj_unmark(unsigned char *o) {
    __try { unsigned char *st = *(unsigned char **)(o + 0xac); int sh;
        if (!st || !(I32(o, 0x198) | I32(o, 0x19c))) return;
        sh = I32(st, 0x7d0) & 31;
        ((void (__fastcall *)(void *, int, int, int, int, void *))0x449a30)(o, 0, (I32(o, 0x14) >> 16) >> sh, (I32(o, 0x18) >> 16) >> sh, I32(o, 0x194), o + 0x198);
        g_unmark_n++;
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
static void obj_setpos(unsigned char *o, int x, int y) { obj_unmark(o); I32(o, 0x14) = x << 16; I32(o, 0x18) = y << 16; }
static void obj_addpos(unsigned char *o, int dx16, int dy16) { obj_unmark(o); I32(o, 0x14) += dx16; I32(o, 0x18) += dy16; }
static void aggro_sync(int on) {
    int i;
    __try { for (i = 0; i < g_ai_nsav; i++) {
        short w2 = on ? (short)g_aggro : g_ai_sav[i].s2, w3 = on ? (short)g_aggro : g_ai_sav[i].s3;
        if (g_ai_sav[i].p[2] != w2 || g_ai_sav[i].p[3] != w3) { g_ai_sav[i].p[2] = w2; g_ai_sav[i].p[3] = w3; mlog("ai def %d: leash/aggro -> %d/%d", g_ai_sav[i].id, w2, w3); }
    } } __except (EXCEPTION_EXECUTE_HANDLER) { g_ai_nsav = 0; }
}
static void chase_reset(void) { aggro_sync(0); g_ai_nsav = 0; memset(g_ch, 0, sizeof g_ch); memset(&g_cs, 0, sizeof g_cs); g_snap_moved = g_snap_fail = 0; }
static void chase_report(const char *tag) {
    mlog("CHASE %s: mode=%d seen=%d arrived=%d avg_arrive=%ums from_dist=%upx walk_calls=%d stuck=%d nudge=%d snap=%d snapfail=%d", tag, g_chase, g_cs.seen, g_cs.n_arr,
         g_cs.n_arr ? g_cs.sum_arr / g_cs.n_arr : 0, g_cs.n_arr ? g_cs.sum_bd / g_cs.n_arr : 0, g_cs.calls, g_cs.stuck, g_cs.nudge, g_snap_moved, g_snap_fail);
}
static void chase_tick(unsigned char **t, unsigned char *pl, DWORD now) {
    static DWORD last, last_log; int i, budget = 24, px, py, n = 0, nw = 0, na = 0, nb = 0, ntg = 0, nnear = 0, nme = 0, pid = I32(pl, 0xc); double sd = 0;
    if (now - last < 50) return;
    last = now; px = I32(pl, 0x14) >> 16; py = I32(pl, 0x18) >> 16;
    __try {
        for (i = 1; i < OBJ_SLOTS; i++) {
            unsigned char *o = t[i]; int x, y, dx, dy, cmd; double d;
            if (!o || !IS_CHAR(o) || !(I32(o, 0x100) & 2) || !I32(o, 0xfc)) continue;
            x = I32(o, 0x14) >> 16; y = I32(o, 0x18) >> 16; dx = px - x; dy = py - y;
            d = sqrt((double)dx * dx + (double)dy * dy); cmd = I32(o, 0x134);
            if (g_ch[i].id != I32(o, 0xc)) {
                memset(&g_ch[i], 0, sizeof g_ch[i]); g_ch[i].id = I32(o, 0xc); g_ch[i].born = now; g_ch[i].next = now + i * 53 % 300;
                g_ch[i].lx = (short)x; g_ch[i].ly = (short)y; g_ch[i].bd = (short)d; g_cs.seen++; aggro_track(o);
            }
            n++; sd += d; if (cmd == 1) nw++; else if (cmd == 5) na++;
            if (I32(o, 0x150) & 2) ntg++; if (cmd == 5 && I32(o, 0x13c) == pid) nme++;
            if (!g_ch[i].arrived && d < 100) { g_ch[i].arrived = 1; g_cs.n_arr++; g_cs.sum_arr += now - g_ch[i].born; g_cs.sum_bd += g_ch[i].bd; }
            if (d < 1) continue;
            if (g_chase == 1) {
                if (!g_surv_pull || d < 34 + (i * 37 % 60)) continue;        /* 每隻停的距離不同,不然全疊在同一點 */
                obj_unmark(o); I32(o, 0x14) += (int)(dx / d * g_surv_pull * 65536);
                I32(o, 0x18) += (int)(dy / d * g_surv_pull * 65536);
            } else if (g_chase == 2) {
                int moved, wx, wy, r; double ang;
                if (d < g_walk_hand + (i * 37 % 40)) { nnear++; continue; }                   /* 夠近:交還原作 AI */
                if (I32(o, 0x15c) && (I32(o, 0x150) & 2)) continue;              /* 原作 AI 已鎖定目標在追 */
                if (I32(o, 0x188) || cmd == 5) { nb++; continue; }                          /* 硬直 / 攻擊中:不打斷 */
                if ((int)(now - g_ch[i].next) < 0 || budget <= 0) continue;
                moved = abs(x - g_ch[i].lx) + abs(y - g_ch[i].ly);
                if (moved < 3) { if (g_ch[i].stuck < 250) g_ch[i].stuck++; if (g_ch[i].stuck == 3) g_cs.stuck++; } else g_ch[i].stuck = 0;
                g_ch[i].lx = (short)x; g_ch[i].ly = (short)y;
                ang = i * 2.399963 + g_ch[i].stuck * 1.3; r = 24 + (i * 29 % 28);           /* 散開點;卡住就換角度 */
                wx = px + (int)(r * cos(ang)); wy = py + (int)(r * sin(ang) * 0.6);
                if (g_ch[i].stuck >= 8) {                                                    /* 連 8 次(約 4 秒)沒動:推一把(= v0 的做法)脫困 */
                    obj_addpos(o, (int)(dx / d * 12 * 65536), (int)(dy / d * 12 * 65536));
                    g_ch[i].stuck = 3; g_cs.nudge++;
                }
                ((void (__fastcall *)(void *, int, int, int))0x445a80)(o, 0, wx, wy);
                g_cs.calls++; budget--;
                g_ch[i].next = now + g_walk_ms + (i * 53 % 150);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("chase: exception %08x in mode %d, off", GetExceptionCode(), g_chase); g_chase = 0; }
    aggro_sync(g_chase == 3);
    {   /* 主角狀態每秒一行(Kirin 2026-11-25「最後那個技能害我卡住」現場沒留下:之後卡住可以從 log 看是哪一種) */
        static DWORD lead_log; unsigned char *lo = lead_obj();
        if (lo && now - lead_log >= 1000) { unsigned char *sb_ = *(unsigned char **)(lo + 0x1a4); lead_log = now;
            __try { mlog("LEAD pos=(%d,%d) cmd=%d dest=(%d,%d) f7c=%08x f100=%08x lock188=%d back190=%d fail1cc=%d hp=%d mp=%d",
                I32(lo, 0x14) >> 16, I32(lo, 0x18) >> 16, I32(lo, 0x134), I32(lo, 0x140), I32(lo, 0x144), I32(lo, 0x7c), I32(lo, 0x100), I32(lo, 0x188), I32(lo, 0x190), I32(lo, 0x1cc), sb_ ? I32(sb_, 0x50) : -1, sb_ ? I32(sb_, 0x58) : -1); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    }
    if (g_chase_log && now - last_log >= 3000) {
        last_log = now;
        mlog("chase m%d: n=%d walking=%d attacking=%d(me %d) ai_target=%d busy=%d near=%d avg_dist=%d | arrived=%d avg=%ums calls=%d stuck=%d nudge=%d fps=%d",
             g_chase, n, nw, na, nme, ntg, nb, nnear, n ? (int)(sd / n) : 0, g_cs.n_arr, g_cs.n_arr ? g_cs.sum_arr / g_cs.n_arr : 0, g_cs.calls, g_cs.stuck, g_cs.nudge, g_fps);
    }
}
static unsigned char *find_char(unsigned char **t, int chr) {
    int i;
    for (i = 1; i < OBJ_SLOTS; i++) {
        unsigned char *o = t[i];
        if (o && IS_CHAR(o) && !(I32(o, 0x100) & 7) && I32(o, 0xfc) == chr) return o;
    }
    return NULL;
}
static void surv_add_tpl(int type, int chr) {
    int i;
    for (i = 0; i < g_surv_ntpl; i++) if (g_surv_tpl[i].type == type && g_surv_tpl[i].chr == chr) return;
    if (g_surv_ntpl < SURV_MAX_TPL) { g_surv_tpl[g_surv_ntpl].type = type; g_surv_tpl[g_surv_ntpl].chr = chr; g_surv_ntpl++; }
}
/* 在 ctr 外圍 rmin..rmax 的環上生 n 隻;回傳成功數 */
static int spawn_ring(unsigned char *ctr, int type, int chr, int n, int rmin, int rmax) {
    int i, ok = 0, stage = I32(ctr, 0xac), cx = I32(ctr, 0x14), cy = I32(ctr, 0x18), cz = I32(ctr, 0x1c);
    double a0 = (GetTickCount() % 6283) / 1000.0;
    for (i = 0; i < n; i++) {
        double ang = a0 + i * 2.399963;
        int r = rmin + (int)((rmax - rmin) * ((i * 37 % 100) / 100.0));
        int x = cx + ((int)(r * cos(ang)) << 16), y = cy + ((int)(r * sin(ang) * 0.6) << 16);
        int *o = NULL;
        __try {
            o = game_create(*GAME_OBJMGR, 0, stage, 0, x, y, cz, type, 0);
            if (o) {
                /* 面向玩家:方向值沿用場上敵人的,實測 0..7 */
                I32(o, 0xbc) = I32(ctr, 0xbc);
                game_faction(o, 0, 2, 0);
                game_setchar(o, 0, chr, 1);
                if (g_spawn_snap) {   /* 生在牆 / 樹裡就走不出來(實測:通道地圖 260–380 px 的環大半不能走)→ 用原作「找最近可走格」0x454bb0 挪到可走的地方 */
                    int sx = x >> 16, sy = y >> 16, ok2;
                    ok2 = ((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)((void *)stage, 0, &sx, &sy, (~I32(o, 0x100) >> 28) & 1, I32(o, 0x194), 24) & 0xff;
                    if (ok2) { if (sx != (x >> 16) || sy != (y >> 16)) g_snap_moved++; obj_setpos((unsigned char *)o, sx, sy); } else g_snap_fail++;
                }
                g_last_spawn = (unsigned char *)o;
                if (g_rg) rg_norm((unsigned char *)o, rg_spawn_kind);
                else {   /* 難度曲線:前幾波的怪打折(攻 25%、命 30%),每波各 +4%,最高 120% */
                    unsigned char *sb = *(unsigned char **)((unsigned char *)o + 0x1a4);
                    int ap = 25 + 4 * g_surv_wave, hp = 30 + 4 * g_surv_wave;
                    if (ap > 120) ap = 120; if (hp > 120) hp = 120;
                    if (sb && i == 0) mlog("enemy base: type=%d char=%d lv=%d exp=%d hp=%d atk=%d def=%d res=%d hit=%d spd=%d (scale atk %d%% hp %d%%)", type, chr, I32(sb, 0x40), I32(sb, 0x44), I32(sb, 0x54), I32(sb, 0x138), I32(sb, 0x13c), I32(sb, 0x140), I32(sb, 0x144), I32(o, 0xf8), ap, hp);
                    if (sb) { I32(sb, 0x138) = I32(sb, 0x138) * ap / 100; if (I32(sb, 0x138) < 1) I32(sb, 0x138) = 1;
                              I32(sb, 0x54) = I32(sb, 0x54) * hp / 100; if (I32(sb, 0x54) < 1) I32(sb, 0x54) = 1; I32(sb, 0x50) = I32(sb, 0x54); }
                }
                ok++;
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("surv spawn: exception %08x", GetExceptionCode()); break; }
    }
    return ok;
}
static void surv_start(void) {
    unsigned char **t = obj_table(), *pl; int i;
    if (!t || !(pl = find_player(t))) { mlog("surv: no player on stage"); return; }
    g_surv_ntpl = 0;
    for (i = 1; i < OBJ_SLOTS; i++) {
        unsigned char *o = t[i];
        if (o && IS_CHAR(o) && (I32(o, 0x100) & 2) && I32(o, 0xfc)) surv_add_tpl(I32(o, 4), I32(o, 0xfc));
    }
    surv_add_tpl(2077, 13);                         /* 黃刀手:蝙蝠幾乎不打人,一定要混一種會打的 */
    g_surv_ntpl0 = g_surv_ntpl;
    count_objs();
    g_surv = 1; g_surv_over = 0; g_toff = 0;
    g_surv_t0 = GetTickCount(); g_surv_next = g_surv_t0 + 1500;
    g_surv_wave = 0; g_surv_spawned = 0; g_surv_kills = 0; g_surv_base_enemy = g_nenemy;
    g_surv_lead = I32(pl, 0xfc); g_surv_stage = I32(pl, 0xac);
    v1_reset(); chase_reset();
    mlog("SURV start: lead char=%d stage=%08x templates=%d base_enemies=%d map=%dx%d pos=(%d,%d) chase=%d", g_surv_lead, g_surv_stage, g_surv_ntpl, g_surv_base_enemy,
         I32(g_surv_stage, 0x790), I32(g_surv_stage, 0x794), I32(pl, 0x14) >> 16, I32(pl, 0x18) >> 16, g_chase);
}
static void surv_end(const char *why) {
    v1_end(); chase_report(why); aggro_sync(0);
    g_surv_end_ms = surv_ms(); g_surv = 0; g_surv_over = 1;
    mlog("SURV end (%s): time=%us wave=%d kills=%d spawned=%d", why, g_surv_end_ms / 1000, g_surv_wave, g_surv_kills, g_surv_spawned);
}
static DWORD surv_ms(void) { return (GetTickCount() - g_surv_t0) * (g_tscale > 0 ? g_tscale : 1) + g_toff; }      /* 這局過了多久(survtscale / survtime 可以加速、跳時間,測試用) */
static void surv_toggle(void) {
    if (g_surv) surv_end("F9");
    else if (g_surv_over) g_surv_over = 0;          /* 成績畫面再按一次 F9 = 收起 */
    else surv_start();
}
static int g_menu, g_wasd, g_evo_to, g_wasd_n, g_atk_walk, g_atk_drop;
/* 操作模式(Kirin 2026-11-24:改成原作的操作,不要自動施放):g_manual = 1 → 模組不自動普攻、不自動放招;
 * 移動 / 普攻 / 絕招全部照原作:點地板走、點怪打、F1–F4 / 滾輪選招、右鍵放;W/A/S/D 的迷蹤步也照原作。
 * 模組只留:技能格 + CD 的把關(my_cast)、一局中不給原作 Q(換戰友)/ E(收放戰友)—— 那兩個會把這一局的主角 / 夥伴弄亂。`manual 0` 切回全自動。 */
static int g_manual = 1, g_man_cast, g_man_fail, g_key_blocked;
/* 原作操作下(Kirin 2026-11-24):數字鍵 1–9 選技能格的招,右鍵照原作施放。每招的冷卻保留(原作本來沒有計時冷卻);
 * 右鍵時選的那招還在冷卻 → 自動換成下一格冷卻好的招放出去(連按右鍵 = 一招接一招),選的招也跟著換過去。
 * 原作「目前選的招」= stat+0x2c[ stat+0x28 ](存的是招式清單裡第幾招;右鍵 0x4492d8 讀它去叫 0x446e80)→ 選招 = 把那一格寫成技能格那招的位置。
 * 這兩個欄位不在 snap 的範圍內,開局記下來、結束寫回去。 */
#define CD_TIMED 1
static int g_slot_id[8], g_slot_n;
static int g_sel_slot, g_hot_saved, g_hot_cur, g_hot_val, g_sel_n, g_auto_next_n;
static const char MAN_KEYS[] = "QE";
static struct { int from, to; } g_evo_map[16]; static int g_evo_nmap;      /* 這一局進化過的招:玩家選的還是舊招時,右鍵改放進化後的那招 */
static DWORD g_lc_t; static int g_lc_id, g_lc_n, g_cast_unlock = 600, g_lc_walk, g_lc_wx, g_lc_wy;      /* 放招後主角被動作鎖(obj+0x188)多久;g_cast_unlock ms 後強制解開(0 = 不解)。實測蒼龍一吼破雲關一次鎖 3–4.5 秒(龍氣劍 0.4–0.8 秒),每 5.3 秒放一次 → 主角大半時間動不了;0.6 秒解開後 30 秒擊殺 49(不解 55),傷害沒有不見。放招前正在走的話,解開後接著走原本的目的地。 */
/* 原作的 W/A/S/D = 「八卦迷蹤步」熱鍵:0x44920c 直接看鍵盤掃描碼位元表 0x4c78a0(W = bit 17、A / S = bit 30 / 31、D = 下一個 dword 的 bit 0),
 * 有按就跳去 0x449253 放那一招 —— 不走視窗訊息,攔 WM_KEYDOWN 沒用(Kirin 實玩:按 W 就一直轉、不能控制)。
 * 倖存者模式開著時把 0x44920c 的 `mov eax,[0x4c78a0]` 換成 `jmp 0x44922b`(跳過那三個判斷),結束換回來。 */
static void wasd_patch(int on) {
    static const unsigned char orig[5] = { 0xa1, 0xa0, 0x78, 0x4c, 0x00 }; static int patched;
    unsigned char *p = (unsigned char *)0x44920c, jmp[5] = { 0xe9, 0, 0, 0, 0 }; DWORD old;
    *(int *)(jmp + 1) = 0x44922b - (0x44920c + 5);
    __try {
        if (on == patched) return;
        if (memcmp(p, on ? orig : jmp, 5)) { mlog("wasd patch: unexpected bytes at 44920c"); return; }
        if (!VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) return;
        memcpy(p, on ? jmp : orig, 5); VirtualProtect(p, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), p, 5);
        patched = on; mlog("wasd patch: original W/A/S/D dodge hotkey %s", on ? "off" : "back on");
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("wasd patch: fault"); }
}
static void surv_tick(void) {
    unsigned char **t, *pl, *sb; DWORD now; int alive, n, k, i;
    if (!g_surv || v1_paused()) return;
    now = GetTickCount();
    if (!(t = obj_table())) { if (g_rg) rg_finish(0, "no table"); else surv_end("no table"); return; }
    pl = find_char(t, g_surv_lead);
    if (!pl || I32(pl, 0xac) != g_surv_stage) {
        unsigned char *gs_ = *(unsigned char **)0x4c3214;
        __try { mlog("lead lost: pl=%p stage=%x (run stage %x) | gs+8=%p gs+0xc=%p lead slot=%d partner slot=%d a4=%d stage obj=%p", pl, pl ? I32(pl, 0xac) : 0, g_surv_stage, gs_ ? *(void **)(gs_ + 8) : NULL, gs_ ? *(void **)(gs_ + 0xc) : NULL, gs_ ? I32(gs_, 0x14) : -1, gs_ ? I32(gs_, 0x18) : -1, gs_ ? gs_[0xa4] : -1, gs_ ? *(void **)(gs_ + 0x10) : NULL); } __except (EXCEPTION_EXECUTE_HANDLER) { }
        if (g_rg) rg_finish(0, "stage changed"); else surv_end("stage changed"); return; }
    sb = *(unsigned char **)(pl + 0x1a4);
    if (g_manual) {      /* 把原作「目前選的招」對到模組選的那一格 */
        int cur_ = I32(sb, 0x28), *b_ = *(int **)(sb + 0xa0), *e_ = *(int **)(sb + 0xa4), i_, want_ = -1;
        if (g_sel_slot >= g_slot_n) g_sel_slot = 0;
        if (cur_ >= 0 && cur_ < 4 && g_slot_n > 0) {
            if (!g_hot_saved) { g_hot_saved = 1; g_hot_cur = cur_; g_hot_val = I32(sb, 0x2c + 4 * cur_); mlog("hotbar: saved cur=%d value=%d", g_hot_cur, g_hot_val); }
            for (i_ = 0; b_ && b_ + i_ < e_; i_++) if (b_[i_] == g_slot_id[g_sel_slot]) { want_ = i_; break; }
            if (want_ >= 0 && I32(sb, 0x2c + 4 * cur_) != want_) { unsigned char *gs_ = *(unsigned char **)0x4c3214;
                I32(sb, 0x2c + 4 * cur_) = want_; mlog("select: slot %d move %d (list index %d) -> hotbar[%d]", g_sel_slot + 1, g_slot_id[g_sel_slot], want_, cur_);
                if (gs_) __try { ((void (__fastcall *)(void *, int, int, int))0x45a9c0)(gs_, 0, I32(gs_, 0x80fc), 0); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
        }
    }
    {   /* WASD:方向一變就用原作的「走到」0x445a80 往那個方向 240 px 外(挪到最近的可走格)走,按住時每 500 ms 補一次;放開 → 走到自己腳下 = 停。
         * 🔴 不能每 120 ms 叫一次:實測主角 84–99% 的時間卡在動作鎖(obj+0x188)裡,看起來像一直在轉。按住時普攻只結算不播動作(不然每一刀都打斷走路)。 */
        static DWORD nx_, lock_t_; static int last_; int dx_ = ((g_wasd & 8) ? 1 : 0) - ((g_wasd & 2) ? 1 : 0), dy_ = ((g_wasd & 1) ? 1 : 0) - ((g_wasd & 4) ? 1 : 0) /* 物件座標的 y 往上是正的(實測:點畫面上方 y 變大)→ W = +y */, px_ = I32(pl, 0x14) >> 16, py_ = I32(pl, 0x18) >> 16, dir_ = dx_ * 3 + dy_;
        if (g_menu) { g_wasd = 0; last_ = 0; }
        else if (dir_) {
            if (I32(pl, 0x188) <= 0) lock_t_ = now; else if (now - lock_t_ > 350) { I32(pl, 0x188) = 0; lock_t_ = now; nx_ = now; }      /* 按著方向鍵卻被動作鎖住超過 0.35 秒 → 解開,馬上再走 */
            if (dir_ != last_ || (int)(now - nx_) >= 0) {
                unsigned char *st_ = *(unsigned char **)(pl + 0xac); int tx_ = px_ + dx_ * 240, ty_ = py_ + dy_ * 160, ok_ = 0;
                __try {
                    if (tx_ < 16) tx_ = 16; if (ty_ < 16) ty_ = 16; if (tx_ > I32(st_, 0x790) - 16) tx_ = I32(st_, 0x790) - 16; if (ty_ > I32(st_, 0x794) - 16) ty_ = I32(st_, 0x794) - 16;
                    ok_ = ((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)(st_, 0, &tx_, &ty_, (~I32(pl, 0x100) >> 28) & 1, I32(pl, 0x194), 60) & 0xff;
                    if (!ok_) { tx_ = px_ + dx_ * 70; ty_ = py_ + dy_ * 46; }
                    ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, tx_, ty_);
                } __except (EXCEPTION_EXECUTE_HANDLER) { }
                nx_ = now + 500; last_ = dir_; g_wasd_n++;
            }
        }
        else if (last_) { last_ = 0; __try { ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, px_, py_); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    }
    if (g_god && sb) I32(sb, 0x50) = I32(sb, 0x54);          /* god 1:測試用,命鎖滿 */
    if (sb && g_hpbuf) {
        /* 主角的命由模組看著:(1) 命上限突然掉一半以上或暴增(原作重算、頭目的狀態效果 —— 實測頭目出場後十幾秒命上限被改掉、主角被判倒下,沒有任何一下大的扣血)→ 改回模組記得的值;
         * (2) 單次受傷上限:一下扣超過命上限的 g_hitcap% 就把多的補回去。 */
        static int real_max; int mx = I32(sb, 0x54) - g_hpbuf, hp_ = I32(sb, 0x50), cap_;
        if (!g_last_hp) real_max = mx;
        if (mx < real_max / 2 || mx > real_max * 3 + 200) {
            if (g_fixmax++ < 12) mlog("lead max hp changed by the game: %d -> %d (hp %d, last %d) - restored", real_max, mx, hp_ - g_hpbuf, g_last_hp - g_hpbuf);
            I32(sb, 0x54) = real_max + g_hpbuf;
            if (hp_ <= g_hpbuf || hp_ > real_max + g_hpbuf) hp_ = g_last_hp > g_hpbuf ? g_last_hp : real_max + g_hpbuf;
            I32(sb, 0x50) = hp_;
        } else real_max = mx;
        cap_ = real_max * g_hitcap / 100;
        if (g_hitcap > 0 && g_last_hp && cap_ > 0 && g_last_hp - hp_ > cap_) { if (g_last_hp - hp_ > g_maxhit) g_maxhit = g_last_hp - hp_; g_capped++; hp_ = g_last_hp - cap_; I32(sb, 0x50) = hp_; }
        {   /* 每秒受傷上限:一秒內最多掉命上限的 g_dpscap%(頭目連續放招,每一下都不大但很密,實測命 995 的主角 13 秒倒下) */
            static DWORD sec_t; static int sec_hp; DWORD n_ = GetTickCount();
            if (!g_last_hp || n_ - sec_t >= 1000 || hp_ > sec_hp) { sec_t = n_; sec_hp = hp_; }
            else if (g_dpscap > 0 && sec_hp - hp_ > real_max * g_dpscap / 100) { hp_ = sec_hp - real_max * g_dpscap / 100; I32(sb, 0x50) = hp_; g_dpscapped++; }
        }
        g_last_hp = hp_;
    }
    if (sb && I32(sb, 0x54) > 0 && I32(sb, 0x50) <= g_hpbuf) { if (g_rg) rg_finish(0, "dead"); else surv_end("dead"); return; }
    chase_tick(t, pl, now);
    count_objs();
    k = g_surv_base_enemy + g_surv_spawned - g_nenemy;
    if (k > g_surv_kills) g_surv_kills = k;
    v1_tick(t, pl, now, g_surv_kills);
    if (v1_paused()) return;
    if (g_rg) { rg_play_tick(t, pl, now); return; }
    if ((int)(now - g_surv_next) < 0) return;
    /* 第 w 波:6 + 3w 隻,間隔 8 秒遞減到 3 秒 */
    g_surv_wave++;
    for (i = 0; i < (int)(sizeof g_tiers / sizeof g_tiers[0]); i++) if (g_tiers[i][0] == g_surv_wave) surv_add_tpl(g_tiers[i][1], g_tiers[i][2]);
    alive = g_nenemy;
    n = 4 + 2 * g_surv_wave;
    if (alive + n > g_surv_cap) n = g_surv_cap - alive;
    if (n < 0) n = 0;
    k = 0;
    for (i = 0; i < n; ) {
        int c = n - i < 4 ? n - i : 4, j = (g_surv_wave + i) % g_surv_ntpl;
        k += spawn_ring(pl, g_surv_tpl[j].type, g_surv_tpl[j].chr, c, g_surv_rmin, g_surv_rmax);
        i += c;
    }
    g_surv_spawned += k;
    g_surv_next = now + (g_surv_wave < 8 ? 8000 - g_surv_wave * 500 : 4000);
    mlog("SURV wave %d: +%d (alive %d, kills %d, t=%us)", g_surv_wave, k, alive, g_surv_kills, (now - g_surv_t0) / 1000);
}

/* ---- 倖存者 v1:經驗 / 升級選單 / 自動放招 ------------------------------------------------
 * 經驗 = 擊殺數(模組自己算,不用原作的經驗);升級時整個遊戲停住(在 PeekMessage 掛勾裡自己轉訊息迴圈),
 * overlay 列 3 個選項,數字鍵 1–3 選。選項直接改原作的數值區塊(obj+0x1a4)。 */
#define UPG_N 13
enum { UPG_ATK, UPG_HP, UPG_HEAL, UPG_SPEED, UPG_SKILL, UPG_CAST, UPG_ALLY, UPG_WEAPON, UPG_ARMOR, UPG_POWER, UPG_PARTNER, UPG_RANK, UPG_EVOLVE };
static int g_lv = 1, g_xp, g_xp_next = 5, g_xp_kills0;
static int g_upg_lv[UPG_N];
static int g_menu, g_menu_drawn, g_menu_opt[3], g_menu_pick;     /* g_menu:選單開著(遊戲停住) */
static DWORD g_pause_ms;                                         /* 選單停掉的總時間(計時要扣掉) */
static wchar_t g_toast[96]; static DWORD g_toast_until;
static void upg_label(int id, wchar_t *out, int n);
static void upg_apply(int id);
static int  upg_ok(int id);
static void growth_roll(void);
static void toast(const wchar_t *s) { wcsncpy(g_toast, s, 95); g_toast_until = GetTickCount() + 2500; }
static void menu_open(void) {
    int i, j, k, tries;
    growth_roll();
    for (i = 0; i < 3; i++) {
        for (tries = 0; tries < 60; tries++) {
            k = rand() % UPG_N;
            for (j = 0; j < i; j++) if (g_menu_opt[j] == k) break;
            if (j == i && upg_ok(k)) break;
        }
        g_menu_opt[i] = k;
    }
    {   /* 每次 3 選 1 至少保證 1 個技能選項(學新招 / 升階 / 解鎖下一階),有得選的話 */
        static const int sk[3] = { UPG_SKILL, UPG_RANK, UPG_EVOLVE }; int has = 0, av[3], na = 0;
        for (i = 0; i < 3; i++) for (j = 0; j < 3; j++) if (g_menu_opt[i] == sk[j]) has = 1;
        for (j = 0; j < 3; j++) if (upg_ok(sk[j])) av[na++] = sk[j];
        if (!has && na) g_menu_opt[rand() % 3] = av[rand() % na];
        if (g_evo_to) { for (i = 0; i < 3; i++) if (g_menu_opt[i] == UPG_EVOLVE) break; if (i == 3) { for (i = 0; i < 3; i++) if (g_menu_opt[i] != UPG_SKILL && g_menu_opt[i] != UPG_RANK) break; g_menu_opt[i < 3 ? i : 0] = UPG_EVOLVE; } }      /* 有招到 Lv 3:保證出「進化」 */
    }
    g_menu = 1; g_menu_drawn = 0; g_menu_pick = 0;
    mlog("LEVELUP lv=%d options=%d,%d,%d", g_lv, g_menu_opt[0], g_menu_opt[1], g_menu_opt[2]);
}
static int g_xp_on = 1;      /* survxp 0:不升級(測追擊用,選單不會跳出來停住遊戲) */
static void xp_tick(int kills) {
    if (g_menu || !g_xp_on) return;
    g_xp = kills - g_xp_kills0;
    if (g_xp >= g_xp_next) {
        g_xp_kills0 += g_xp_next; g_xp -= g_xp_next;
        g_lv++; g_xp_next = 4 + g_lv * 3;
        menu_open();
    }
}

/* 原作函式(逆向自 V2.85,見 notes/03):thiscall 用 fastcall 呼叫,都在主執行緒 */
#define GAME_APP (*(unsigned char **)0x4c31ec)      /* 遊戲視窗物件;+0x12 = 暫停旗標(主迴圈跳過邏輯、照樣畫) */
typedef void (__fastcall *g_attack_t)(void *obj, int edx_, int targetId, int wx, int wy, int hasTarget);
typedef void (__fastcall *g_cast_t)(void *obj, int edx_, int moveIdx, int targetId, int wx, int wy);
typedef int  (__fastcall *g_sb1_t)(void *sb, int edx_, int id);
typedef void (__fastcall *g_sb0_t)(void *sb, int edx_);
typedef void (__cdecl *g_void_t)(void);
static const g_attack_t game_attack = (g_attack_t)0x447230;   /* 普攻:走過去打 targetId(= obj+0xc) */
static const g_cast_t   game_cast   = (g_cast_t)0x446e80;     /* 放第 moveIdx 招;忙 / 冷卻 / 內力不夠會默默失敗 */
static const g_sb1_t    game_learn  = (g_sb1_t)0x407ee0;      /* 學絕招 learn(stat, moveId) → 1 = 新學會 */
static const g_sb1_t    game_forget = (g_sb1_t)0x407f70;      /* 忘掉絕招(thiscall, ret 4) */
static const g_sb0_t    game_recalc = (g_sb0_t)0x408680;      /* 重算有效數值(+0x138.. = 基礎 + 四格裝備) */
static const g_void_t   game_timer_reset = (g_void_t)0x476490;
/* 招式表 / 物品表是 std::map(VC6 紅黑樹:節點 +0 左 +4 父 +8 右 +0xc key +0x10 值)。
 * 🔴 原作的查表函式 0x427e30 / 0x427e10 是 operator[]:查不存在的 id 會「插入一筆垃圾」,不能拿來掃 id。自己走樹。 */
typedef unsigned char *(*def_t)(int id);
static unsigned char *map_find(unsigned char *head, unsigned char *nil, int id) {
    unsigned char *n; int guard = 64;
    if (!head || !nil) return NULL;
    __try {
        for (n = *(unsigned char **)(head + 4); n != nil && guard--; ) {
            int k = I32(n, 0xc);
            if (id == k) return n + 0x10;
            n = *(unsigned char **)(n + (id < k ? 0 : 8));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    return NULL;
}
static unsigned char *game_movedef(int id) {       /* 招式:+0 名字、+0x24 威力、+0x28 內力 */
    unsigned char *m = *(unsigned char **)0x4c320c;
    return m ? map_find(*(unsigned char **)(m + 8), *(unsigned char **)0x4c2fec, id) : NULL;
}
static unsigned char *game_itemdef(int id) {       /* 物品:+0 旗標、+0xc4 攻、+0xc8 防 */
    unsigned char *m = *(unsigned char **)0x4c3208;
    return m ? map_find(*(unsigned char **)(m + 4), *(unsigned char **)0x4c2fdc, id) : NULL;
}
static int g_anchor_x, g_anchor_y, g_leash = 230, g_stage_flag = -1;   /* 開始位置;只追離它 g_leash px 內的敵人(不然會追進出口換地圖) */
static int g_hp_regen = 2, g_super = 1, g_unstick_n;
static int g_auto = 1, g_cast_ms = 4000, g_mp_regen = 3, g_cast_n, g_atk_n;
static DWORD g_auto_next, g_cast_next, g_regen_next;
/* 技能格 + 每招 CD(notes/08):原作沒有每招計時 CD → 模組自己記。技能格 g_slots 格(3 起),只有格子裡的招放得出來。 */
#define SLOT_MAX 8
static int g_slots = 3, g_slot_id[SLOT_MAX], g_slot_n, g_cd_on = 1, g_cd_block, g_cd_base = 2500, g_cd_per_mp = 50;
static DWORD g_slot_ready[SLOT_MAX];
/* 技能成長(notes/13):招式等級(升級:每級 威力 +35%、冷卻 -8%,最高 Lv 5);升到 Lv 3(Kirin 2026-11-23 定)後下一次 3 選 1 保證出「進化」= 換成同組下一階,同一格,等級回 Lv 1 */
static struct { int id, lv; } g_rank[16]; static int g_rank_n, g_rank_pick, g_evo_from, g_evo_to, g_wasd;
static DWORD g_pick_t;
static int rank_of(int id) { int i; for (i = 0; i < g_rank_n; i++) if (g_rank[i].id == id) return g_rank[i].lv; return 1; }
static void rank_set(int id, int lv) { int i; for (i = 0; i < g_rank_n; i++) if (g_rank[i].id == id) { g_rank[i].lv = lv; return; } if (g_rank_n < 16) { g_rank[g_rank_n].id = id; g_rank[g_rank_n++].lv = lv; } }
static int g_run_learn[16], g_run_learn_n;
static void power_set(int id);
static int mp_base(int id);
static int g_mp_cost_pct = 60, g_mp_cost_cap = 30, g_mp_maxx = 250, g_mp_regen_rg = 8;      /* Roguelite 的內力(Kirin:內力太少):招的消耗 ×60%、最多 30;內力上限 ×2.5;每秒回 8(原本 3) */          /* 這一局學的招:結束時忘掉(技能不保留) */
static struct { int id, ms; } g_cd_ovr[16]; static int g_cd_novr;
static int g_atk_mode = 1, g_swing_ms = 550, g_swing_range = 60, g_swing_max = 3, g_swing_anim = 1, g_swing_hit = 1, g_swing_tgt = 0;
static int g_sw_n, g_sw_hit, g_sw_kill, g_sw_miss, g_sw_slide;
static struct { int n, walk, lock, px, lx, ly; } g_mv;       /* 量「邊砍邊走」:走了幾 px、幾成時間在走 / 被動作鎖住 */
static void slots_init(unsigned char *sb);
static void cast_hook_install(void);
static void atk_hook_install(void);
/* 可調:升級可學的招、可換的武器(id 由 moves / items 指令列出來後挑的) */
static int g_learn_ids[8] = { 7, 4, 11, 38 }, g_learn_n = 4;              /* 「學新招」只給各組的低階(Kirin 2026-11-25:原本直接給天地根 = 天地組末階,一開始就拿到最強):
                                                                                  石破天驚混元掌(掌組第 2 階;第 1 階青光連擊威力 10 太弱)、旋空斬(刀組起點)、昊陽真火(火組起點)、九州生氣恃風雷(天地組起點);技能格裡已經有同組的招就不給 */
static int g_weapon_ids[8] = { 102, 104, 106, 109, 112, 115 }, g_weapon_n = 6, g_weapon_i;  /* 鋼劍 8 → 明月劍 18 → 凌雲劍 30 → 烈陽劍 45 → 擎天神劍 62 → 寰宇神劍 68 */
static void v1_pause(int on);
/* 暫停旗標會連畫面一起停 → 等選單畫出一幀(g_menu_drawn)才真的停,畫面就停在有選單的那一幀 */
static int v1_paused(void) { if (g_menu && g_menu_drawn) v1_pause(1); return g_menu; }
static void v1_pause(int on) {
    static DWORD t0;
    unsigned char *app = GAME_APP;
    if (!app) return;
    __try {
        if (on) { if (!app[0x12]) { app[0x12] = 1; t0 = GetTickCount(); } }
        else if (app[0x12]) {
            DWORD d = GetTickCount() - t0;
            app[0x12] = 0; game_timer_reset();
            g_surv_t0 += d; g_surv_next += d; g_cast_next += d; g_pause_ms += d;
            { int i_; for (i_ = 0; i_ < SLOT_MAX; i_++) g_slot_ready[i_] += d; }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("pause: exception"); }
}
static void menu_pick(int n) {
    wchar_t lab[64], msg[96];
    if (!g_menu || n < 1 || n > 3) return;
    upg_label(g_menu_opt[n - 1], lab, 64);
    if (g_rg) { unsigned char *pl_ = lead_obj(), *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL;      /* Roguelite 不拿原作經驗 → 每升一級自帶 攻 +1 防 +1 命上限 +8 */
            if (sb_) { I32(sb_, 0x60) += 1; I32(sb_, 0x64) += 1; if (I32(sb_, 0x54) - g_hpbuf < 985) { I32(sb_, 0x54) += 12; I32(sb_, 0x50) += 12; } if (I32(sb_, 0x5c) < 990) I32(sb_, 0x5c) += 6; recalc_keep(sb_); } }
    upg_apply(g_menu_opt[n - 1]); g_upg_lv[g_menu_opt[n - 1]]++;
    _snwprintf(msg, 96, L"升級!%s", lab); toast(msg);
    mlog("PICK %d -> upg %d (now lv %d)", n, g_menu_opt[n - 1], g_upg_lv[g_menu_opt[n - 1]]);
    g_menu = 0; v1_pause(0);
}
/* 每局從頭:開始時把隊伍每個人的數值區塊(stat+0x3c..+0x93:等級 / 經驗 / 命 / 內 / 攻防 / 裝備)和會的招記下來,結束整塊還原、多學的招忘掉。
 * 隊伍的數值區塊是 gs+0x44[0..7](原作自己配的,換地圖不會變)。 */
#define SNAP_LO 0x3c
#define SNAP_N  0x58
static struct { unsigned char *sb; unsigned char raw[SNAP_N]; int moves[24], nmoves; } g_snap[8]; static int g_snap_on;
static void snap_take(void) {
    unsigned char *gs = *(unsigned char **)0x4c3214; int k;
    if (!gs) return;
    __try { for (k = 0; k < 8; k++) { unsigned char *sb = *(unsigned char **)(gs + 0x44 + 4 * k); int *b, *e;
        g_snap[k].sb = sb; g_snap[k].nmoves = 0; if (!sb) continue;
        memcpy(g_snap[k].raw, sb + SNAP_LO, SNAP_N);
        for (b = *(int **)(sb + 0xa0), e = *(int **)(sb + 0xa4); b && b < e && g_snap[k].nmoves < 24; b++) g_snap[k].moves[g_snap[k].nmoves++] = *b; }
        g_snap_on = 1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_snap_on = 0; mlog("snap: fault"); }
}
static void snap_restore(void) {
    unsigned char *gs = *(unsigned char **)0x4c3214; int k, i, j, n = 0, extra[24], ne;
    if (!g_snap_on || !gs) return;
    g_snap_on = 0;
    __try { for (k = 0; k < 8; k++) { unsigned char *sb = *(unsigned char **)(gs + 0x44 + 4 * k); int *b, *e;
        if (!sb || sb != g_snap[k].sb) continue;
        memcpy(sb + SNAP_LO, g_snap[k].raw, SNAP_N);
        for (ne = 0, b = *(int **)(sb + 0xa0), e = *(int **)(sb + 0xa4); b && b < e && ne < 24; b++) { for (j = 0; j < g_snap[k].nmoves; j++) if (g_snap[k].moves[j] == *b) break; if (j == g_snap[k].nmoves) extra[ne++] = *b; }
        for (i = 0; i < ne; i++) game_forget(sb, 0, extra[i]);
        game_recalc(sb, 0); n++;
        memcpy(sb + 0x50, g_snap[k].raw + (0x50 - SNAP_LO), 16);      /* 重算可能被還留在身上的狀態效果帶歪(實測命上限變成 418635)→ 命 / 內四個欄位再寫一次 */
        mlog("snap: slot %d restored (lv %d hp %d/%d atk %d def %d, forgot %d moves)", k, I32(sb, 0x40), I32(sb, 0x50), I32(sb, 0x54), I32(sb, 0x60), I32(sb, 0x64), ne); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("snap restore: fault"); }
}
/* 一局中重算主角數值:原作的重算 0x408680 會把命上限壓回 999(實測:第一次升級後墊高的 30000 就沒了,補救程式又把命上限補成 999)→ 先把墊高的拿掉、算完再加回去 */
static void recalc_keep(unsigned char *sb) {
    int b = (g_hpbuf && I32(sb, 0x54) >= g_hpbuf) ? g_hpbuf : 0;
    I32(sb, 0x54) -= b; I32(sb, 0x50) -= b; game_recalc(sb, 0); I32(sb, 0x54) += b; I32(sb, 0x50) += b;
}
static int g_save[7], g_saved;                  /* 開始時主角的 命上限 / 內上限 / 攻 / 防 / 武器 / 速度,結束還原 */
static unsigned char *lead_obj(void);
static void v1_reset(void) {
    unsigned char *pl = lead_obj(), *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
    if (!g_snap_on) snap_take();
    g_last_hp = 0; g_maxhit = 0; g_capped = 0; g_fixmax = 0; g_dpscapped = 0;
    g_hpbuf = 0;
    if (sb) {
        g_save[0] = I32(sb, 0x54); g_save[1] = I32(sb, 0x5c); g_save[2] = I32(sb, 0x60); g_save[3] = I32(sb, 0x64); g_save[4] = I32(sb, 0x84); g_save[5] = I32(pl, 0xf8); g_save[6] = I32(sb, 0x3c); g_saved = 1;
        mlog("lead stats: lv=%d hp=%d mp=%d atk=%d/%d def=%d/%d res=%d hit=%d wpn=%d,%d,%d,%d spd=%d movebonus=%d", I32(sb, 0x40), I32(sb, 0x54), I32(sb, 0x5c), I32(sb, 0x60), I32(sb, 0x138),
             I32(sb, 0x64), I32(sb, 0x13c), I32(sb, 0x140), I32(sb, 0x144), I32(sb, 0x84), I32(sb, 0x88), I32(sb, 0x8c), I32(sb, 0x90), I32(pl, 0xf8), I32(sb, 0x3c));
        g_anchor_x = I32(pl, 0x14) >> 16; g_anchor_y = I32(pl, 0x18) >> 16;
        /* stage+0x79c bit 2 = 這張地圖可戰鬥(原作普攻 / 放招都先查它);非戰鬥地圖也硬開,結束還原 */
        { unsigned char *st = *(unsigned char **)(pl + 0xac); g_stage_flag = I32(st, 0x79c); I32(st, 0x79c) |= 2; mlog("stage flag %x -> %x, map %dx%d", g_stage_flag, I32(st, 0x79c), I32(st, 0x790), I32(st, 0x794)); }
        I32(sb, 0x54) *= 3; if (I32(sb, 0x54) > 999) I32(sb, 0x54) = 999; I32(sb, 0x50) = I32(sb, 0x54);   /* 倖存者主角要耐打:命上限 ×3 */
        /* Roguelite:主角不能真的死(原作會跳「進度讀取」選單,那之後換圖 / 讀檔都會當)→ 命上限和現在的命都墊高 g_hpbuf,掉到墊高值以下 = 倒下。畫面上的命條模組自己蓋掉重畫 */
        if (g_rg) { g_hpbuf = 30000; I32(sb, 0x54) += g_hpbuf; I32(sb, 0x50) += g_hpbuf; }
        if (g_rg) { I32(sb, 0x5c) = I32(sb, 0x5c) * g_mp_maxx / 100; if (I32(sb, 0x5c) > 999) I32(sb, 0x5c) = 999; I32(sb, 0x58) = I32(sb, 0x5c); g_mp_regen = g_mp_regen_rg; mlog("rogue mp: max %d regen %d/s, move cost x%d%% (cap %d)", I32(sb, 0x5c), g_mp_regen, g_mp_cost_pct, g_mp_cost_cap); }
    }
    g_lv = 1; g_xp = 0; g_xp_next = 5; g_xp_kills0 = 0; g_menu = 0; g_pause_ms = 0;
    if (sb) slots_init(sb); cast_hook_install(); atk_hook_install();
    g_sw_n = g_sw_hit = g_sw_kill = g_sw_miss = g_sw_slide = 0; memset(&g_mv, 0, sizeof g_mv);
    memset(g_upg_lv, 0, sizeof g_upg_lv); g_rank_n = 0; g_wasd = 0; g_evo_nmap = 0; g_sel_slot = 0; g_hot_saved = 0; wasd_patch(!g_manual);
    g_cast_ms = 4000; g_mp_regen = g_rg ? g_mp_regen_rg : 3; g_hp_regen = 2; g_cast_n = g_atk_n = 0; g_weapon_i = 0;
    g_auto_next = g_cast_next = g_regen_next = GetTickCount() + 1000;
    srand(GetTickCount());
}
static void power_restore(void);
static void v1_end(void) {
    unsigned char *pl = lead_obj(), *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
    if (g_menu) { g_menu = 0; v1_pause(0); }
    if (sb && g_saved) __try {
        if (g_stage_flag >= 0 && I32(pl, 0xac) == g_surv_stage) I32(*(unsigned char **)(pl + 0xac), 0x79c) = g_stage_flag;
        I32(sb, 0x54) = g_save[0]; I32(sb, 0x5c) = g_save[1]; I32(sb, 0x60) = g_save[2]; I32(sb, 0x64) = g_save[3]; I32(sb, 0x84) = g_save[4]; I32(pl, 0xf8) = g_save[5]; I32(sb, 0x3c) = g_save[6];
        if (I32(sb, 0x50) > I32(sb, 0x54)) I32(sb, 0x50) = I32(sb, 0x54);
        if (I32(sb, 0x58) > I32(sb, 0x5c)) I32(sb, 0x58) = I32(sb, 0x5c);
        if (g_hot_saved) { I32(sb, 0x2c + 4 * g_hot_cur) = g_hot_val; g_hot_saved = 0; mlog("hotbar: restored cur=%d value=%d", g_hot_cur, g_hot_val); }
        game_recalc(sb, 0);
        { int i_; for (i_ = 0; i_ < g_run_learn_n; i_++) mlog("forget move %d (learned this run) -> %d", g_run_learn[i_], game_forget(sb, 0, g_run_learn[i_])); g_run_learn_n = 0; }   /* 技能不保留 */
    } __except (EXCEPTION_EXECUTE_HANDLER) { }
    if (g_hpbuf) mlog("hit cap %d%%: capped %d hits, biggest single hit %d; max hp restored %d times; per-second cap %d%% kicked in %d ticks", g_hitcap, g_capped, g_maxhit, g_fixmax, g_dpscap, g_dpscapped);
    snap_restore(); g_hpbuf = 0; wasd_patch(0);
    g_saved = 0; power_restore();
    mlog("input: manual=%d (Q/E blocked %d, casts %d, cd/slot blocked %d, number-key selects %d, auto-switched to next ready %d) | wasd walk orders %d | original attack orders on the lead: %d turned into walk (left button held), %d dropped", g_manual, g_key_blocked, g_cast_n, g_cd_block, g_sel_n, g_auto_next_n, g_wasd_n, g_atk_walk, g_atk_drop); g_wasd_n = g_atk_walk = g_atk_drop = g_key_blocked = g_sel_n = g_auto_next_n = 0;
    mlog("v1 end: lv=%d casts=%d attacks=%d unstick=%d pause=%ums | atk mode %d: swings=%d hit=%d miss=%d kill=%d slide=%d | slots %d/%d cd_blocked=%d | moved=%dpx walking=%d%% locked=%d%%", g_lv, g_cast_n, g_atk_n, g_unstick_n, g_pause_ms,
         g_atk_mode, g_sw_n, g_sw_hit, g_sw_miss, g_sw_kill, g_sw_slide, g_slot_n, g_slots, g_cd_block, g_mv.px, g_mv.n ? g_mv.walk * 100 / g_mv.n : 0, g_mv.n ? g_mv.lock * 100 / g_mv.n : 0);
}
static int enemy_alive(unsigned char *o) {
    unsigned char *sb;
    if (!o || !IS_CHAR(o) || !(I32(o, 0x100) & 2) || !I32(o, 0xfc)) return 0;
    sb = *(unsigned char **)(o + 0x1a4);
    return sb && I32(sb, 0x50) > 0 && !(I32(o, 0x7c) & 0x80000000);
}
/* ---- 技能格 / CD ------------------------------------------------------------------------- */
static struct { int id, raw; } g_evo_cd[16]; static int g_evo_ncd;      /* 進化後冷卻上限(還沒乘 g_cast_ms / 升階的原始 ms) */
static int move_cd_ms(int id) {
    unsigned char *d; int i, ms;
    for (i = 0; i < g_cd_novr; i++) if (g_cd_ovr[i].id == id) return g_cd_ovr[i].ms;
    d = game_movedef(id);
    ms = g_cd_base + (d ? mp_base(id) : 40) * g_cd_per_mp;      /* 預設:2.5 秒 + 內力消耗 × 0.05 秒(內力 60 的招 = 5.5 秒) */
    if (ms < 1500) ms = 1500; if (ms > 20000) ms = 20000;
    for (i = 0; i < g_evo_ncd; i++) if (g_evo_cd[i].id == id && ms > g_evo_cd[i].raw) ms = g_evo_cd[i].raw;      /* 進化後冷卻不比進化前長太多 */
    ms = ms * g_cast_ms / 4000;
    return ms * (100 - 8 * (rank_of(id) - 1)) / 100;                 /* 升階:每級冷卻 -8% */
                                      /* 升級「絕招冷卻 -20%」= 所有 CD ×0.8 */
}
static int slot_of(int id) { int i; for (i = 0; i < g_slot_n && i < g_slots; i++) if (g_slot_id[i] == id) return i; return -1; }
static int slot_add(int id) { if (slot_of(id) >= 0) return 1; if (g_slot_n >= g_slots || g_slot_n >= SLOT_MAX) return 0; g_slot_ready[g_slot_n] = GetTickCount(); g_slot_id[g_slot_n++] = id; power_set(id); return 1; }
static void slots_init(unsigned char *sb) {
    int *b = *(int **)(sb + 0xa0), *e = *(int **)(sb + 0xa4);
    g_slot_n = 0; g_run_learn_n = 0; g_cd_block = 0;
    for (; b && b < e; b++) slot_add(*b);                          /* 開局:原本會的招照順序放進格子,放不下的這局不能用 */
}
/* 掛在原作放招 0x446e80 開頭(inline hook):主角在倖存者模式裡放招 → 不在技能格 / CD 沒好就擋掉。
 * 模組自動放招、玩家右鍵、原作「收招後自動放」三條路都呼叫 0x446e80,都會經過這裡。 */
static g_cast_t g_cast_tramp;
static int g_refuse_n, g_castfix = 1, g_lockbreak_n;
static void __fastcall my_cast(void *obj, int edx_, int idx, int tid, int wx, int wy) {
    unsigned char *pl = (unsigned char *)obj, *sb; int id = 0, s, mp0; DWORD now = GetTickCount();
    if (!g_surv || !g_cd_on || I32(pl, 0xfc) != g_surv_lead || !(sb = *(unsigned char **)(pl + 0x1a4))) { g_cast_tramp(obj, 0, idx, tid, wx, wy); return; }
    { int *b = *(int **)(sb + 0xa0), *e = *(int **)(sb + 0xa4); if (b && idx >= 0 && b + idx < e) id = b[idx]; }
    s = slot_of(id);
    if (s < 0) {      /* 選的是進化前的舊招(原作的招式清單裡還在)→ 改放進化後的那一招 */
        int i_, k_, n_ = id, *b_ = *(int **)(sb + 0xa0), *e_ = *(int **)(sb + 0xa4);
        for (k_ = 0; k_ < 8; k_++) { for (i_ = 0; i_ < g_evo_nmap; i_++) if (g_evo_map[i_].from == n_) { n_ = g_evo_map[i_].to; break; } if (i_ == g_evo_nmap) break; }
        if (n_ != id && slot_of(n_) >= 0) for (i_ = 0; b_ && b_ + i_ < e_; i_++) if (b_[i_] == n_) { idx = i_; id = n_; s = slot_of(n_); break; }
    }
    if (s >= 0 && g_manual && (int)(now - g_slot_ready[s]) < 0) {      /* 還在冷卻:換下一格冷卻好的 */
        int j_, i_, *b_ = *(int **)(sb + 0xa0), *e_ = *(int **)(sb + 0xa4);
        for (j_ = 1; j_ < g_slot_n; j_++) { int s2_ = (s + j_) % g_slot_n, f_ = -1;
            if ((int)(now - g_slot_ready[s2_]) < 0) continue;
            for (i_ = 0; b_ && b_ + i_ < e_; i_++) if (b_[i_] == g_slot_id[s2_]) { f_ = i_; break; }
            if (f_ >= 0) { s = s2_; idx = f_; id = g_slot_id[s2_]; g_sel_slot = s2_; g_auto_next_n++; break; } }
    }
    if (s < 0 || (CD_TIMED && (int)(now - g_slot_ready[s]) < 0)) { g_cd_block++; return; }
    mp0 = I32(sb, 0x58);
    g_cast_tramp(obj, 0, idx, tid, wx, wy);
    if (I32(sb, 0x58) >= mp0 && g_castfix && I32(pl, 0x188) > 0 && !(I32(pl, 0xb0) >= 0xc && I32(pl, 0xb0) <= 0x10)) {
        /* 被動作鎖擋下(受創 / 普攻收招中;不是上一招還在放)→ 解開動作鎖再放一次。
         * Kirin 2026-11-26:被一群怪圍著打時主角幾乎一直在受創 / 出手的動作鎖裡,原作每次都拒絕放招 → 看起來像技能卡住 */
        int lk_ = I32(pl, 0x188), b0_ = I32(pl, 0xb0);
        I32(pl, 0x188) = 0; if (I32(pl, 0xb4) == 0x13) I32(pl, 0xb4) = 0;
        g_cast_tramp(obj, 0, idx, tid, wx, wy);
        if (I32(sb, 0x58) < mp0) { g_lockbreak_n++; if (g_manual) mlog("PLAYER cast broke lock: move=%d lock188=%d b0=%d", id, lk_, b0_); }
        else { int f_ = 0, m_ = 1, *fb_ = *(int **)(pl + 0x1d8), *fe_ = *(int **)(pl + 0x1dc), k_; char fl_[64] = ""; int n_ = 0;
            __try { f_ = ((int (__fastcall *)(void *, int, int))0x442e30)(pl, 0, id) & 0xff; m_ = ((int (__fastcall *)(void *, int, int))0x446e10)(pl, 0, id) & 0xff;
                for (k_ = 0; fb_ && fb_ + k_ < fe_ && k_ < 8; k_++) n_ += _snprintf(fl_ + n_, 64 - n_, "%d,", fb_[k_]); } __except (EXCEPTION_EXECUTE_HANDLER) { }
            if (g_manual) mlog("PLAYER lock break did not help: move=%d idx=%d field=%d mpok=%d b0=%d b4=%d active=[%s]", id, idx, f_, m_, I32(pl, 0xb0), I32(pl, 0xb4), fl_);
            I32(pl, 0x188) = lk_; }      /* 還是沒放出來(別的原因)→ 動作鎖放回去 */
    }
    if (I32(sb, 0x58) < mp0) { g_slot_ready[s] = now + move_cd_ms(id); if (g_manual) { g_cast_n++; mlog("PLAYER cast move=%d mp %d->%d cd=%dms", id, mp0, I32(sb, 0x58), move_cd_ms(id)); } }
    else if (g_manual) {      /* 原作沒放出來:照 0x446fc0 的檢查順序記原因(lock = 動作鎖 +0x188;b0 4–16 是出手中 → 原作排隊收招後放;hit = 受創 b4 0x13;field = 同一招還在場上 +0x1d8;mp = 內力不夠) */
        int b0_ = I32(pl, 0xb0), b4_ = I32(pl, 0xb4), lk_ = I32(pl, 0x188), fld_ = 0, mpok_ = 1; const char *why_;
        __try { fld_ = ((int (__fastcall *)(void *, int, int))0x442e30)(pl, 0, id) & 0xff; mpok_ = ((int (__fastcall *)(void *, int, int))0x446e10)(pl, 0, id) & 0xff; } __except (EXCEPTION_EXECUTE_HANDLER) { }
        why_ = lk_ ? (b0_ >= 4 && b0_ <= 16 ? "lock-attacking" : "lock") : b4_ == 0x13 ? "hit" : fld_ ? "field" : !mpok_ ? "mp" : "other";
        g_refuse_n++;
        if (g_castfix && s >= 0) { g_slot_ready[s] = now + 400; }      /* 這一招現在放不出來 → 0.4 秒內先換別格,不要一直卡在同一招 */
        mlog("PLAYER cast refused by game: move=%d why=%s b0=%d b4=%d lock188=%d kind184=%d mp=%d cmd=%d f7c=%08x", id, why_, b0_, b4_, lk_, I32(pl, 0x184), mp0, I32(pl, 0x134), I32(pl, 0x7c));
    }      /* 原作沒放出來(Kirin 2026-11-25 卡住那段沒有紀錄) */
}
static void *hook_inline(unsigned char *fn, void *hook, int n) {     /* n = 開頭要搬走的完整指令長度(≥ 5,不能含相對位址) */
    unsigned char *t = (unsigned char *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); DWORD old; int i;
    if (!t || !VirtualProtect(fn, n, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    memcpy(t, fn, n); t[n] = 0xe9; *(int *)(t + n + 1) = (int)(fn + n) - (int)(t + n + 5);
    fn[0] = 0xe9; *(int *)(fn + 1) = (int)hook - (int)(fn + 5); for (i = 5; i < n; i++) fn[i] = 0x90;
    VirtualProtect(fn, n, old, &old); FlushInstructionCache(GetCurrentProcess(), fn, n);
    return t;
}
static const unsigned char CAST_HEAD[] = { 0x8b, 0x44, 0x24, 0x04, 0x56, 0x8b, 0xf1, 0x50 };   /* mov eax,[esp+4] / push esi / mov esi,ecx / push eax */
static void cast_hook_install(void) {
    unsigned char *fn = (unsigned char *)0x446e80;
    if (g_cast_tramp) return;
    if (memcmp(fn, CAST_HEAD, sizeof CAST_HEAD)) { mlog("cast hook: unexpected bytes at 0x446e80 (%02x %02x %02x %02x %02x) - CD only for auto-cast", fn[0], fn[1], fn[2], fn[3], fn[4]); return; }
    g_cast_tramp = (g_cast_t)hook_inline(fn, (void *)my_cast, sizeof CAST_HEAD);
    mlog("cast hook %s", g_cast_tramp ? "installed at 0x446e80" : "FAILED");
}

/* 原作的普攻指令 0x447230 thiscall (obj, 目標 id, x, y, flag):主角走到目標旁邊站定揮刀(玩家點到怪、按住左鍵掃過怪都會叫)。
 * 倖存者的普攻是模組自動結算的,這個指令只會讓主角停下來(Kirin 實玩:「普攻會停下來打」)→ 主角的這個指令改成「走到點的那個位置」;
 * 不是玩家按著左鍵叫的(原作自己下的)就直接不做。目標 id = 0(Ctrl 原地攻擊)照舊。 */
typedef void (__fastcall *atk_t)(void *, int, int, int, int, int);
static atk_t g_atk_tramp; static int g_atk_walk, g_atk_drop;
static const unsigned char ATK_HEAD[] = { 0x83, 0xec, 0x0c, 0x53, 0x55 };   /* sub esp,0xc / push ebx / push ebp */
static void __fastcall my_atk(void *obj, int edx_, int tid, int x, int y, int flag) {
    unsigned char *pl = (unsigned char *)obj, *gs = *(unsigned char **)0x4c3214;
    if (g_surv && !g_manual && g_atk_mode == 1 && tid && gs && *(unsigned char **)(gs + 8) == pl) {
        if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) { g_atk_walk++; ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, x, y); }
        else g_atk_drop++;
        return;
    }
    g_atk_tramp(obj, 0, tid, x, y, flag);
}
static void atk_hook_install(void) {
    unsigned char *fn = (unsigned char *)0x447230;
    if (g_atk_tramp) return;
    if (memcmp(fn, ATK_HEAD, sizeof ATK_HEAD)) { mlog("atk hook: unexpected bytes at 0x447230"); return; }
    g_atk_tramp = (atk_t)hook_inline(fn, (void *)my_atk, sizeof ATK_HEAD);
    mlog("atk hook %s", g_atk_tramp ? "installed at 0x447230" : "FAILED");
}

/* ---- 邊砍邊走(指令 survatk N)--------------------------------------------------------------
 * 0 原作普攻 0x447230:主角走到目標旁站定揮刀(v1 的做法,會搶走玩家的走路指令)
 * 1 做法 A(預設):主角照玩家點的走;模組每 g_swing_ms 對 g_swing_range px 內最近的 g_swing_max 隻敵人
 *   直接呼叫原作普攻結算 0x43d780(命中判定、傷害數字、硬直、死亡、經驗都在裡面),再播一次攻擊動作當視覺(0x440ae0,不鎖指令)。
 * 2 做法 B:射程內有敵人就叫原作起手 0x4441a0(真的揮刀動畫 + 命中幀),揮刀期間模組寫速度 +0x2c/+0x30 讓主角繼續往目的地滑,
 *   並把目的地排隊(0x445a80 在動作鎖內只記下來,收招自動接著走)。 */
typedef int (__cdecl *g_hit_t)(int atkId, int tgtId, int scratch);
typedef void (__fastcall *g_setact_t)(void *obj, int edx_, int action);
static const g_hit_t    game_hit    = (g_hit_t)0x43d780;       /* 回傳 0 無效 / miss、1 命中、2 命中 + 硬直、3 擊殺。🔴 第三個參數槽會被拿去當暫存,一定要給 */
static const g_setact_t game_setact = (g_setact_t)0x440ae0;    /* 設動作:0 站 2 走 4–7 Attack1–4;跟目前動作相同就不重播 */
static const g_setact_t game_setact1 = (g_setact_t)0x440bb0;   /* 一次性覆蓋動作(+0xb4),可強制重播 */
static DWORD g_swing_next;
static struct { int walking, dx, dy; } g_sb;
static void swing_tick(unsigned char **t, unsigned char *pl, DWORD now) {
    static DWORD last; int ids[8], ds[8], k = 0, i, j, px, py, pid, r2, kmax, cmd;
    unsigned char *sb = *(unsigned char **)(pl + 0x1a4);
    if (!sb || now - last < 15) return;
    last = now; px = I32(pl, 0x14) >> 16; py = I32(pl, 0x18) >> 16; pid = I32(pl, 0xc); cmd = I32(pl, 0x134);
    { int d_ = abs(px - g_mv.lx) + abs(py - g_mv.ly); if (g_mv.n && d_ < 40) g_mv.px += d_; g_mv.lx = px; g_mv.ly = py; g_mv.n++; if (cmd == 1) g_mv.walk++; if (I32(pl, 0x188) > 0) g_mv.lock++; }
    if (!g_auto || !g_atk_mode || g_manual) return;
    r2 = g_swing_range * g_swing_range; kmax = g_atk_mode == 2 ? 1 : (g_swing_max > 8 ? 8 : g_swing_max < 1 ? 1 : g_swing_max);
    __try {
        if (I32(pl, 0x7c) & 0x80000000) return;
        if (g_atk_mode == 2 && I32(pl, 0x184) == 1 && I32(pl, 0x188) > 0) {            /* B:揮刀中 → 繼續往目的地滑 */
            if (g_sb.walking) {
                double ddx = g_sb.dx - px, ddy = g_sb.dy - py, d = sqrt(ddx * ddx + ddy * ddy);
                if (d > 6) { int spd = I32(pl, 0xf8); I32(pl, 0x2c) = (int)(ddx / d * spd); I32(pl, 0x30) = (int)(ddy / d * spd); g_sw_slide++;
                             if (!pl[0x18e]) ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, g_sb.dx, g_sb.dy); }
            }
            return;
        }
        if (g_atk_mode == 1 && cmd == 1 && I32(pl, 0xb0) == 0 && !I32(pl, 0x188)) game_setact(pl, 0, 2);   /* A:攻擊動作播完原作掉回站立 → 還在走就切回走路動作 */
        if ((int)(now - g_swing_next) < 0 || I32(pl, 0x188)) return;
        for (i = 1; i < OBJ_SLOTS; i++) if (enemy_alive(t[i])) {
            int dx = (I32(t[i], 0x14) >> 16) - px, dy = (I32(t[i], 0x18) >> 16) - py, d = dx * dx + dy * dy * 2;     /* 畫面是斜視角,y 方向算遠一點 */
            if (d > r2) continue;
            j = k; if (k == kmax) { if (d >= ds[k - 1]) continue; j = k - 1; } else k++;       /* 由近到遠留前 kmax 隻 */
            for (; j > 0 && ds[j - 1] > d; j--) { ds[j] = ds[j - 1]; ids[j] = ids[j - 1]; }
            ds[j] = d; ids[j] = I32(t[i], 0xc);
        }
        if (g_rg && g_atk_mode == 1) {      /* 頭目 / 精英在範圍內就一定砍到(被小怪圍住時最近的 3 隻輪不到牠) */
            int sid = rg_special_near(px, py, r2, 0);
            if (sid) { for (j = 0; j < k; j++) if (ids[j] == sid) break; if (j == k) { if (k < kmax) k++; ids[k - 1] = sid; } }
        }
        if (!k) return;
        if (g_atk_mode == 2) {
            g_sb.walking = cmd == 1; g_sb.dx = I32(pl, 0x140); g_sb.dy = I32(pl, 0x144);
            I32(pl, 0x13c) = ids[0];
            if (((int (__fastcall *)(void *, int, int))0x4441a0)(pl, 0, ids[0]) & 0xff) { g_sw_n++; g_atk_n++; g_swing_next = now + 60; }
            else g_swing_next = now + 100;
            return;
        }
        g_swing_next = now + g_swing_ms; g_sw_n++; g_atk_n++;
        if (!g_swing_hit) {      /* 探測用(survswing 的 hit=0):只播動作不結算,看原作的動畫幀腳本會不會自己扣血 → 記上一刀目標的命 */
            static int pid_, php_; unsigned char *o_ = pid_ ? (unsigned char *)((unsigned (__fastcall *)(void *, int, int))0x44f000)(*GAME_OBJMGR, 0, pid_) : NULL;
            if (o_ && *(unsigned char **)(o_ + 0x1a4)) { int h_ = I32(*(unsigned char **)(o_ + 0x1a4), 0x50); if (h_ != php_) g_sw_hit++; else g_sw_miss++; }
            o_ = (unsigned char *)((unsigned (__fastcall *)(void *, int, int))0x44f000)(*GAME_OBJMGR, 0, ids[0]);
            pid_ = ids[0]; php_ = o_ && *(unsigned char **)(o_ + 0x1a4) ? I32(*(unsigned char **)(o_ + 0x1a4), 0x50) : 0;
        }
        if (g_swing_anim && !g_wasd) {      /* 按著 WASD 時只結算不播揮砍動作 */
            static int combo; int nc = I32(sb, 0x74); if (nc > 2) nc = 2; if (nc < 1) nc = 1;      /* 只用 Attack1/2:Attack3/4 原作會 ×1.2 */
            if (g_swing_tgt) I32(pl, 0x13c) = ids[0];
            (g_swing_anim == 2 ? game_setact1 : game_setact)(pl, 0, 4 + combo % nc); combo++;
        }
        if (g_swing_hit) for (i = 0; i < k; i++) {
            int r = game_hit(pid, ids[i], 0);
            if (r == 3) g_sw_kill++; if (r) g_sw_hit++; else g_sw_miss++;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("swing: exception %08x in mode %d, back to mode 0", GetExceptionCode(), g_atk_mode); g_atk_mode = 0; }
}

static void cast_lock_tick(unsigned char *pl, DWORD now) {
    if (g_lc_t) { int el_ = (int)(now - g_lc_t), done_ = 0;
            if (el_ > 150 && I32(pl, 0x188) <= 0) { if (g_lc_n++ < 40) mlog("cast lock: move %d held the lead for %d ms", g_lc_id, el_); done_ = 1; }
            else if (g_cast_unlock && el_ > g_cast_unlock) { if (g_lc_n++ < 40) mlog("cast lock: move %d still locked after %d ms -> released", g_lc_id, el_); I32(pl, 0x188) = 0; done_ = 1; }
            else if (el_ > 6000) done_ = 1;
            if (done_) { g_lc_t = 0;
                if (g_lc_walk && !g_wasd && I32(pl, 0x134) != 1 && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) __try { ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, g_lc_wx, g_lc_wy); } __except (EXCEPTION_EXECUTE_HANDLER) { } } }
}
/* 測試用的代玩(指令 bot 1;只在原作操作下):模仿玩家的輸入,不走模組的自動出手 ——
 * 每 400 ms 對最近的敵人下原作的普攻指令 0x447230(= 玩家左鍵點怪);每 600 ms 對牠放「原作目前選的招」(呼叫右鍵會叫的 0x446e80,
 * 一樣經過 my_cast 的冷卻 / 自動換招)。送真的右鍵訊息不行:右鍵要點在怪身上才放,固定點一個位置實測一整局 0 次。 */
static int g_bot, g_bot_atk, g_bot_rc;
static HWND g_hwnd;
static void bot_tick(unsigned char **t, unsigned char *pl, unsigned char *sb, DWORD now) {
    static DWORD na, nr; int i, best = 500 * 500, px = I32(pl, 0x14) >> 16, py = I32(pl, 0x18) >> 16; unsigned char *tg = NULL;
    if ((g_bot == 1 || g_bot == 3) && (int)(now - na) >= 0) { na = now + 400;      /* bot 2:只放招、不下普攻指令(量走路用);bot 3:跟 1 一樣,但放招不管動作鎖、每 500 ms 按一次(= 玩家一直按右鍵) */
        for (i = 1; i < OBJ_SLOTS; i++) if (enemy_alive(t[i])) { int dx = (I32(t[i], 0x14) >> 16) - px, dy = (I32(t[i], 0x18) >> 16) - py, d = dx * dx + dy * dy; if (d < best) { best = d; tg = t[i]; } }
        if (tg && (I32(pl, 0x134) != 5 || I32(pl, 0x13c) != I32(tg, 0xc)) && I32(pl, 0x188) <= 0) __try { game_attack(pl, 0, I32(tg, 0xc), 0, 0, 1); g_bot_atk++; } __except (EXCEPTION_EXECUTE_HANDLER) { } }
    if ((int)(now - nr) >= 0) { int cur_ = I32(sb, 0x28); nr = now + (g_bot == 3 ? 500 : 600);
        if (!tg) for (i = 1; i < OBJ_SLOTS; i++) if (enemy_alive(t[i])) { int dx = (I32(t[i], 0x14) >> 16) - px, dy = (I32(t[i], 0x18) >> 16) - py, d = dx * dx + dy * dy; if (d < best) { best = d; tg = t[i]; } }
        if (tg && cur_ >= 0 && cur_ < 4 && (g_bot == 3 || I32(pl, 0x188) <= 0)) __try { game_cast(pl, 0, I32(sb, 0x2c + 4 * cur_), I32(tg, 0xc), 0, 0); g_bot_rc++; } __except (EXCEPTION_EXECUTE_HANDLER) { } }
}
/* 自動出手:沒按滑鼠時普攻最近的敵人;每 g_cast_ms 對「周圍最密」的敵人放絕招(會的招輪流用) */
static void auto_tick(unsigned char **t, unsigned char *pl, DWORD now) {
    unsigned char *near_o = NULL, *dense_o = NULL, *sb = *(unsigned char **)(pl + 0x1a4);
    int i, j, px, py, best = 0x7fffffff, bestn = -1, ne = 0;
    static unsigned char *es[240 + 64];
    if (!g_auto || !sb) return;
    if ((int)(now - g_regen_next) >= 0) {             /* 內力回復(原作回很慢,自動放招撐不住) */
        g_regen_next = now + 1000;
        if (I32(sb, 0x50) > 0 && I32(sb, 0x50) < I32(sb, 0x54)) { I32(sb, 0x50) += g_hp_regen; if (I32(sb, 0x50) > I32(sb, 0x54)) I32(sb, 0x50) = I32(sb, 0x54); }
        if (I32(sb, 0x58) < I32(sb, 0x5c)) { I32(sb, 0x58) += g_mp_regen; if (I32(sb, 0x58) > I32(sb, 0x5c)) I32(sb, 0x58) = I32(sb, 0x5c); }
    }
    if (g_manual) { if (g_bot) bot_tick(t, pl, sb, now); return; }      /* 原作操作:模組不自動放招、不自動普攻 */
    if ((int)(now - g_auto_next) < 0) return;
    g_auto_next = now + 200;
    px = I32(pl, 0x14) >> 16; py = I32(pl, 0x18) >> 16;
    __try {
        for (i = 1; i < OBJ_SLOTS && ne < 240 + 64; i++) if (enemy_alive(t[i])) {
            int dx = (I32(t[i], 0x14) >> 16) - px, dy = (I32(t[i], 0x18) >> 16) - py, d = dx * dx + dy * dy;
            if (d > 420 * 420) continue;
            { int ax = (I32(t[i], 0x14) >> 16) - g_anchor_x, ay = (I32(t[i], 0x18) >> 16) - g_anchor_y; if (ax * ax + ay * ay > g_leash * g_leash) continue; }
            es[ne++] = t[i];
            if (d < best) { best = d; near_o = t[i]; }
        }
        if (!near_o) return;
        if ((int)(now - g_cast_next) >= 0) {
            int cnt = I32(sb, 0xa0) ? (I32(sb, 0xa4) - I32(sb, 0xa0)) >> 2 : 0, mp0 = I32(sb, 0x58);
            for (i = 0; i < ne; i++) {
                int n = 0, ex = I32(es[i], 0x14) >> 16, ey = I32(es[i], 0x18) >> 16;
                for (j = 0; j < ne; j++) { int dx = (I32(es[j], 0x14) >> 16) - ex, dy = (I32(es[j], 0x18) >> 16) - ey; if (dx * dx + dy * dy < 90 * 90) n++; }
                if (n > bestn) { bestn = n; dense_o = es[i]; }
            }
            if (g_rg) { static int alt_; int bid = rg_special_near(px, py, 300 * 300, 1); unsigned char *bo = bid ? (unsigned char *)((unsigned (__fastcall *)(void *, int, int))0x44f000)(*GAME_OBJMGR, 0, bid) : NULL;
                if (bo && (alt_++ % 3)) dense_o = bo; }      /* 頭目在附近:絕招三次有兩次對著頭目放 */
            if (cnt > 0 && dense_o) {
                static int rr; int idx = rr % cnt, s, id = 0;
                if (g_cd_on) {                        /* 技能格裡 CD 好了的招輪流放 */
                    int *b = *(int **)(sb + 0xa0);
                    idx = -1;
                    for (j = 0; j < g_slot_n && idx < 0; j++) {
                        s = (rr + j) % g_slot_n;
                        if ((int)(now - g_slot_ready[s]) < 0) continue;
                        for (i = 0; i < cnt; i++) if (b[i] == g_slot_id[s]) { idx = i; id = b[i]; rr = s; break; }
                    }
                }
                if (idx >= 0) {
                    if (g_super && !(g_atk_mode == 2 && I32(pl, 0x184) == 1)) I32(pl, 0x188) = 0;      /* 霸體:被圍毆時 obj+0x188(動作 / 硬直剩餘)永遠不歸零,原作的 can-act 0x446cf0 就一直擋 → 放招前清掉 */
                    g_lc_walk = I32(pl, 0x134) == 1; g_lc_wx = I32(pl, 0x140); g_lc_wy = I32(pl, 0x144);
                    game_cast(pl, 0, idx, I32(dense_o, 0xc), 0, 0);
                    if (I32(sb, 0x58) < mp0) { rr++; g_cast_n++; g_cast_next = now + (g_cd_on ? 700 : g_cast_ms);
                        if (g_cd_on && !g_cast_tramp) { s = slot_of(id); if (s >= 0) g_slot_ready[s] = now + move_cd_ms(id); }
                        g_lc_t = now; g_lc_id = id;
                        mlog("AUTO cast idx=%d/%d move=%d cd=%dms target=%d (density %d) mp %d->%d", idx, cnt, id, id ? move_cd_ms(id) : g_cast_ms, I32(dense_o, 0xc), bestn, mp0, I32(sb, 0x58)); return; }
                    g_cast_next = now + 400;              /* 沒放出來 → 0.4 秒後再試,先普攻 */
                }
            }
        }
        cast_lock_tick(pl, now);
        { static DWORD busy_since; if (!I32(pl, 0x188)) busy_since = now; else if (g_super && now - busy_since > 1200 && !(g_atk_mode == 2 && I32(pl, 0x184) == 1)) { I32(pl, 0x188) = 0; busy_since = now; g_unstick_n++; } }
        if (!g_atk_mode && !(*(int *)0x4c790c & 3) && (I32(pl, 0x134) != 5 || I32(pl, 0x13c) != I32(near_o, 0xc))) {
            game_attack(pl, 0, I32(near_o, 0xc), 0, 0, 1); g_atk_n++;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_auto = 0; mlog("auto: exception %08x, off", GetExceptionCode()); }
}
static void v1_tick(unsigned char **t, unsigned char *pl, DWORD now, int kills) {
    xp_tick(kills);
    if (g_menu) return;
    swing_tick(t, pl, now);
    auto_tick(t, pl, now);
}

/* 升級選項:直接改原作數值區塊 stat = obj+0x1a4(欄位見 notes/03) */
static unsigned char *lead_obj(void) { unsigned char **t = obj_table(); return t ? find_char(t, g_surv_lead) : NULL; }
static int g_power_pct; static struct { int id, base, mp, eb; } g_power[24]; static int g_power_n;      /* eb:進化墊底後這局用的基礎威力(0 = 照原作) */
static int mp_base(int id) { int i; unsigned char *d; for (i = 0; i < g_power_n; i++) if (g_power[i].id == id) return g_power[i].mp; d = game_movedef(id); return d ? I32(d, 0x28) : 40; }
static void power_set(int id) {
    unsigned char *d = game_movedef(id); int i;
    if (!d) return;
    for (i = 0; i < g_power_n; i++) if (g_power[i].id == id) break;
    if (i == g_power_n) { if (g_power_n >= 24) return; g_power[i].id = id; g_power[i].base = I32(d, 0x24); g_power[i].mp = I32(d, 0x28); g_power[i].eb = 0; g_power_n++; }
    if (g_rg) { int c = g_power[i].mp * g_mp_cost_pct / 100; if (c > g_mp_cost_cap) c = g_mp_cost_cap; if (c < 1 && g_power[i].mp > 0) c = 1; I32(d, 0x28) = c; }
    I32(d, 0x24) = (g_power[i].eb ? g_power[i].eb : g_power[i].base) * (100 + g_power_pct + 35 * (rank_of(id) - 1)) / 100;
}
static void power_restore(void) { int i; for (i = 0; i < g_power_n; i++) { unsigned char *d = game_movedef(g_power[i].id); if (d) { I32(d, 0x24) = g_power[i].base; I32(d, 0x28) = g_power[i].mp; } } g_power_n = 0; g_power_pct = 0; g_evo_ncd = 0; }
static int knows_move(unsigned char *sb, int id) {
    int *b = *(int **)(sb + 0xa0), *e = *(int **)(sb + 0xa4);
    for (; b && b < e; b++) if (*b == id) return 1;
    return 0;
}
static int g_learn_pick;      /* 這次 3 選 1 的「學新絕招」學哪一招:每次開選單時從還沒學的裡面隨機挑(原本照清單順序,每局第一招都是玄子神功 —— Kirin 2026-11-24) */
static int tree_in_slots(int id);
static int next_learn(unsigned char *sb) { int i; if (g_learn_pick && !knows_move(sb, g_learn_pick)) return g_learn_pick; for (i = 0; i < g_learn_n; i++) if (!knows_move(sb, g_learn_ids[i]) && !tree_in_slots(g_learn_ids[i])) return g_learn_ids[i]; return 0; }
static int tree_of(int id);
static int tree_in_slots(int id) { int i, g = tree_of(id); if (g < 0) return 0; for (i = 0; i < g_slot_n; i++) if (tree_of(g_slot_id[i]) == g) return 1; return 0; }
static void learn_roll(unsigned char *sb) { int i, c[8], n = 0; for (i = 0; i < g_learn_n; i++) if (!knows_move(sb, g_learn_ids[i]) && !tree_in_slots(g_learn_ids[i])) c[n++] = g_learn_ids[i]; g_learn_pick = n ? c[rand() % n] : 0; }
/* 技能樹:notes/04 的分組,每組照威力由低到高排(頭目專用那組不放)。同組的低階招練到 Lv 3 → 3 選 1 保證出現「進化」(沒有下一階的招可以繼續升到 Lv 5)。 */
static const int SK_TREE[][12] = {      /* 2026-11-25 依實際每秒傷害(威力 ÷ 冷卻)由低到高重排,數值完全一樣的重複階拿掉(82 山嵐劍氣、76 長老鬼風刀、80 紫電閃擊、77 破擊爆威、83 怒咆震天) */
    { 16, 18, 2, 39, 19, 0 },                         /* 龍 */
    { 41, 34, 24, 43, 42, 46, 0 },                    /* 劍氣 */
    { 28, 74, 68, 26, 0 },                            /* 紅塵劍 */
    { 48, 22, 0 },                                    /* 劍法 */
    { 71, 25, 72, 0 },                                /* 劍陣 */
    { 4, 30, 15, 0 },                                 /* 刀 */
    { 81, 23, 0 },                                    /* 剪 / 網 */
    { 79, 7, 58, 35, 3, 0 },                          /* 掌 / 指 / 拳 */
    { 20, 14, 0 },                                    /* 爆 / 擊 */
    { 13, 78, 0 },                                    /* 暗器 */
    { 11, 54, 1, 55, 0 },                             /* 火 */
    { 45, 44, 47, 0 },                                /* 四靈 */
    { 5, 31, 0 },                                     /* 分身 */
    { 38, 40, 33, 17, 29, 32, 10, 36, 9, 0 },         /* 天地 / 道 / 佛(27 氣傲天蒼 實測 11 秒內放不出來,先不放) */
};
static int tree_of(int id) { int g, i; for (g = 0; g < (int)(sizeof SK_TREE / sizeof SK_TREE[0]); g++) for (i = 0; SK_TREE[g][i]; i++) if (SK_TREE[g][i] == id) return g; return -1; }
static int g_evo_lv = 3, g_tree_skip[16], g_tree_nskip;
static int tree_next(int id) {      /* 同組的下一階(跳過標成不能用的);沒有回 0 */
    int g, i, j;
    for (g = 0; g < (int)(sizeof SK_TREE / sizeof SK_TREE[0]); g++) for (i = 0; SK_TREE[g][i]; i++) if (SK_TREE[g][i] == id) {
        for (i++; SK_TREE[g][i]; i++) { for (j = 0; j < g_tree_nskip; j++) if (g_tree_skip[j] == SK_TREE[g][i]) break; if (j == g_tree_nskip && game_movedef(SK_TREE[g][i])) return SK_TREE[g][i]; }
        return 0; }
    return 0;
}
static void growth_roll(void) {      /* 開 3 選 1 前先決定:升階升哪一招、解鎖哪一招 */
    int i, c[SLOT_MAX], n = 0, r;
    g_rank_pick = g_evo_from = g_evo_to = 0;
    { unsigned char *pl_ = lead_obj(), *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL; if (sb_) learn_roll(sb_); }
    for (i = 0; i < g_slot_n && i < g_slots; i++) if (rank_of(g_slot_id[i]) < 5) c[n++] = g_slot_id[i];
    if (n) g_rank_pick = c[rand() % n];
    r = g_slot_n ? rand() % g_slot_n : 0;
    for (i = 0; i < g_slot_n && i < g_slots; i++) { int id = g_slot_id[(r + i) % g_slot_n], nx = rank_of(id) >= g_evo_lv ? tree_next(id) : 0;
        while (nx && slot_of(nx) >= 0) nx = tree_next(nx);
        if (nx) { g_evo_from = id; g_evo_to = nx; break; } }
}
static void evolve_apply(unsigned char *sb, int from, int to) {
    int s = slot_of(from), i, r_ = 1;
    if (s < 0 || !to) return;
    if (!knows_move(sb, to)) { r_ = game_learn(sb, 0, to); if (r_ && g_run_learn_n < 16) g_run_learn[g_run_learn_n++] = to; }
    if (!r_) { mlog("evolve: could not learn move %d", to); return; }
    {   /* 進化一定變強(Kirin 2026-11-25:原本進化回 Lv 1,13 步裡 11 步單發變弱):進化後 Lv 1 的單發威力、每秒傷害都至少是進化前的 1.1 倍,冷卻最多 1.2 倍 */
        unsigned char *da = game_movedef(from), *db = game_movedef(to); int pa, ca, cb, need, k, rb = rank_of(to);
        power_set(from); power_set(to);
        if (da && db) { pa = I32(da, 0x24); ca = move_cd_ms(from); cb = move_cd_ms(to);
            if (cb > ca * 12 / 10 && g_evo_ncd < 16) { g_evo_cd[g_evo_ncd].id = to; g_evo_cd[g_evo_ncd++].raw = (ca * 12 / 10) * 4000 / g_cast_ms * 100 / (100 - 8 * (rb - 1)) + 1; cb = move_cd_ms(to); }
            need = (int)ceil(pa * 1.1 * (cb > ca ? (double)cb / ca : 1.0));
            for (k = 0; k < g_power_n; k++) if (g_power[k].id == to) { int m_ = 100 + g_power_pct + 35 * (rb - 1), eb = (need * 100 + m_ - 1) / m_; if (eb > g_power[k].base) g_power[k].eb = eb; }
            power_set(to);
            mlog("EVO %d Lv%d power %d cd %dms (%.1f/s) -> %d Lv%d power %d cd %dms (%.1f/s)  single x%.2f dps x%.2f", from, rank_of(from), pa, ca, pa * 1000.0 / ca,
                 to, rb, I32(db, 0x24), cb, I32(db, 0x24) * 1000.0 / cb, I32(db, 0x24) / (double)pa, (I32(db, 0x24) * 1000.0 / cb) / (pa * 1000.0 / ca)); }
    }
    g_slot_id[s] = to; g_slot_ready[s] = GetTickCount();
    for (i = 0; i < g_rank_n; i++) if (g_rank[i].id == from) g_rank[i].lv = 1;
    power_set(from); power_set(to);
    if (g_evo_nmap < 16) { g_evo_map[g_evo_nmap].from = from; g_evo_map[g_evo_nmap++].to = to; }
    mlog("evolve: slot %d move %d -> %d", s + 1, from, to);
}
static void def_name(def_t f, int id, wchar_t *out, int n) {
    unsigned char *d = NULL; out[0] = 0;
    __try { d = f(id); if (d) MultiByteToWideChar(950, 0, (const char *)d + (f == game_itemdef ? 4 : 0), -1, out, n); } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
}
static int upg_ok(int id) {
    unsigned char *pl = lead_obj(), *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
    if (!sb) return 0;
    if (id == UPG_SKILL) return next_learn(sb) != 0 && (!g_cd_on || g_slot_n < g_slots);     /* 技能格滿了就不出這個選項 */
    if (id == UPG_PARTNER) return g_rg && rg_upg_ok();
    if (id == UPG_RANK) return g_rank_pick != 0;
    if (id == UPG_EVOLVE) return g_evo_to != 0;
    if (id == UPG_ALLY) return !g_rg;                       /* Roguelite:夥伴只有 1 個,不再召喚刀手 */
    if (id == UPG_WEAPON) return !g_rg && g_weapon_i < g_weapon_n;      /* Roguelite:武器是打到的裝備(跨局保留),不在升級選項裡 */
    if (id == UPG_ATK) return I32(sb, 0x60) < 240;
    if (id == UPG_ARMOR) return I32(sb, 0x64) < 240;
    if (id == UPG_HP) return I32(sb, 0x54) - g_hpbuf < 900;
    if (id == UPG_CAST) return CD_TIMED && g_cast_ms > 800;      /* 沒有計時冷卻時不出這個選項 */
    return 1;
}
static void upg_label(int id, wchar_t *out, int n) {
    unsigned char *pl = lead_obj(), *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
    wchar_t nm[48];
    if (!sb) { out[0] = 0; return; }
    switch (id) {
    case UPG_ATK:    _snwprintf(out, n, L"攻擊 +5(現在 %d)", I32(sb, 0x138)); break;
    case UPG_ARMOR:  _snwprintf(out, n, L"防禦 +4(現在 %d)", I32(sb, 0x13c)); break;
    case UPG_HP:     _snwprintf(out, n, L"命上限 +25%% 並回滿(現在 %d)", I32(sb, 0x54) - g_hpbuf); break;
    case UPG_HEAL:   _snwprintf(out, n, L"命、內全滿,每秒回復 命 +2 內 +2"); break;
    case UPG_POWER:  _snwprintf(out, n, L"絕招威力 +30%%(現在 +%d%%)", g_power_pct); break;
    case UPG_SPEED:  _snwprintf(out, n, L"移動速度 +15%%"); break;
    case UPG_CAST:   _snwprintf(out, n, L"絕招冷卻 -20%%(現在 ×%.2f)", g_cast_ms / 4000.0); break;
    case UPG_ALLY:   _snwprintf(out, n, L"召喚一名刀手助戰"); break;
    case UPG_PARTNER: rg_upg_label(out, n); break;
    case UPG_SKILL:  def_name(game_movedef, next_learn(sb), nm, 48); _snwprintf(out, n, L"學新絕招:%s(技能格 %d/%d)", nm, g_slot_n, g_slots); break;
    case UPG_RANK:   def_name(game_movedef, g_rank_pick, nm, 48); _snwprintf(out, n, CD_TIMED ? L"升級:%s Lv %d → %d(威力 +35%%、冷卻 -8%%)" : L"升級:%s Lv %d → %d(威力 +35%%)", nm, rank_of(g_rank_pick), rank_of(g_rank_pick) + 1); break;
    case UPG_EVOLVE: { wchar_t n2[48]; def_name(game_movedef, g_evo_from, nm, 48); def_name(game_movedef, g_evo_to, n2, 48); _snwprintf(out, n, L"進化:%s Lv %d → %s(同一格,等級回 Lv 1)", nm, rank_of(g_evo_from), n2); break; }
    case UPG_WEAPON: def_name(game_itemdef, g_weapon_ids[g_weapon_i < g_weapon_n ? g_weapon_i : 0], nm, 48); _snwprintf(out, n, L"換武器:%s", nm); break;
    default:         _snwprintf(out, n, L"(%d)", id);
    }
    out[n - 1] = 0;
}
static void upg_apply(int id) {
    unsigned char *pl = lead_obj(), *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
    if (!sb) return;
    __try {
        switch (id) {
        case UPG_ATK:   I32(sb, 0x60) += 5; recalc_keep(sb); break;
        case UPG_ARMOR: I32(sb, 0x64) += 4; recalc_keep(sb); break;
        case UPG_HP:    I32(sb, 0x54) += (I32(sb, 0x54) - g_hpbuf) / 4; if (I32(sb, 0x54) - g_hpbuf > 999) I32(sb, 0x54) = 999 + g_hpbuf; I32(sb, 0x50) = I32(sb, 0x54); break;
        case UPG_HEAL:  I32(sb, 0x50) = I32(sb, 0x54); I32(sb, 0x58) = I32(sb, 0x5c); g_mp_regen += 2; g_hp_regen += 2; break;
        case UPG_POWER: {       /* stat+0x3c 每次放招都被原作重設(0x44704a)→ 改招式表的威力 moveDef+0x24(結束還原) */
            int *b = *(int **)(sb + 0xa0), *e = *(int **)(sb + 0xa4);
            g_power_pct += 30;
            for (; b && b < e; b++) power_set(*b);
            break; }
        case UPG_SPEED: I32(pl, 0xf8) += I32(pl, 0xf8) * 15 / 100; break;
        case UPG_CAST:  g_cast_ms = g_cast_ms * 4 / 5; if (g_cast_ms < 800) g_cast_ms = 800; break;
        case UPG_SKILL: { int m = next_learn(sb); if (m) { int r_ = game_learn(sb, 0, m); mlog("learn move %d -> %d", m, r_); if (r_ && g_run_learn_n < 16) g_run_learn[g_run_learn_n++] = m; slot_add(m); if (g_power_pct) power_set(m); } break; }
        case UPG_WEAPON: if (g_weapon_i < g_weapon_n) { I32(sb, 0x84) = g_weapon_ids[g_weapon_i++]; recalc_keep(sb); } break;
        case UPG_PARTNER: rg_upg_apply(); break;
        case UPG_RANK: if (g_rank_pick && rank_of(g_rank_pick) < 5) { rank_set(g_rank_pick, rank_of(g_rank_pick) + 1); power_set(g_rank_pick); mlog("rank: move %d -> lv %d", g_rank_pick, rank_of(g_rank_pick)); } break;
        case UPG_EVOLVE: evolve_apply(sb, g_evo_from, g_evo_to); break;
        case UPG_ALLY: {
            int *o = game_create(*GAME_OBJMGR, 0, I32(pl, 0xac), 0, I32(pl, 0x14) + (30 << 16), I32(pl, 0x18), I32(pl, 0x1c), 2077, 0);
            if (o) { I32(o, 0xbc) = I32(pl, 0xbc); game_faction(o, 0, 1, 0); game_setchar(o, 0, 13, 1); }
            break; }
        }
        mlog("upg %d applied: atk=%d/%d def=%d/%d hp=%d/%d mp=%d/%d spd=%d wpn=%d", id, I32(sb, 0x60), I32(sb, 0x138), I32(sb, 0x64), I32(sb, 0x13c),
             I32(sb, 0x50), I32(sb, 0x54), I32(sb, 0x58), I32(sb, 0x5c), I32(pl, 0xf8), I32(sb, 0x84));
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("upg %d: exception", id); }
}
/* 探索用:列招式表 / 物品表 */
static void do_defs(def_t f, int lo, int hi, const char *tag) {
    int id, n = 0; char name[256];
    for (id = lo; id <= hi; id++) {
        unsigned char *d = NULL;
        __try { d = f(id); } __except (EXCEPTION_EXECUTE_HANDLER) { d = NULL; }
        if (!d) continue;
        __try {
            if (f == game_movedef) { big5_to_utf8((const char *)d, name, sizeof name);
                mlog("  %s %5d name=%s elem=%d f20=%d power=%d mp=%d f2c=%d", tag, id, name, I32(d, 0x1c), I32(d, 0x20), I32(d, 0x24), I32(d, 0x28), I32(d, 0x2c)); }
            else { big5_to_utf8((const char *)d + 4, name, sizeof name);
                mlog("  %s %5d flags=%08x name=%s hp=%d mp=%d atk=%d def=%d res=%d hit=%d move=%d lv=%d | %08x %08x %08x %08x", tag, id, I32(d, 0), name, I32(d, 0xb8), I32(d, 0xc0), I32(d, 0xc4), I32(d, 0xc8), I32(d, 0xcc), I32(d, 0xd0), I32(d, 0xe0), I32(d, 0xe4), I32(d, 0x24), I32(d, 0x28), I32(d, 0x2c), I32(d, 0x30)); }
        } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("  %s %d: fault", tag, id); }
        n++;
    }
    mlog("%s: %d entries in %d..%d", tag, n, lo, hi);
}

/* ---- window / input ------------------------------------------------------------------ */
static WNDPROC g_orig_wndproc;
static int g_msglog = 1;
static HWND g_hwnd;
/* 啟動器選了 640×480 以外的解析度(或全螢幕)時,視窗 client 區比原作的 640×480 大;模組的選單、按鈕、游標都用 640×480 座標,
   滑鼠座標先換算回 640×480 再給模組用(原作收到的還是原值) */
static LPARAM mouse_lp640(HWND h, LPARAM l) {
    RECT rc; int x, y;
    if (!GetClientRect(h, &rc) || rc.right <= 0 || rc.bottom <= 0 || (rc.right == 640 && rc.bottom == 480)) return l;
    x = (short)LOWORD(l) * 640 / rc.right; y = (short)HIWORD(l) * 480 / rc.bottom;
    return MAKELPARAM(x, y);
}
static LRESULT CALLBACK my_wndproc(HWND h, UINT m, WPARAM w, LPARAM l_orig) {
    static int logged;
    LPARAM l = (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && m != WM_MOUSEWHEEL) ? mouse_lp640(h, l_orig) : l_orig;
    if (!g_hwnd) g_hwnd = h;
    if (m == WM_KEYDOWN && w == VK_F9) { if (!(l & 0x40000000)) rg_f9(); return 0; }   /* F9 不給遊戲:倖存者選單 / 放棄這一局 */
    if (m == WM_KEYUP && w == VK_F9) return 0;
    if (rg_input(m, w, l)) return 0;
    if (m == WM_KILLFOCUS) g_wasd = 0;
    if (g_surv && !g_manual && (m == WM_KEYDOWN || m == WM_KEYUP) && (w == 'W' || w == 'A' || w == 'S' || w == 'D')) {      /* WASD 走路:不給原作 */
        int b = w == 'W' ? 1 : w == 'A' ? 2 : w == 'S' ? 4 : 8; if (m == WM_KEYDOWN) g_wasd |= b; else g_wasd &= ~b; return 0; }
    if (m == WM_MOUSEMOVE) { int nx_ = (short)LOWORD(l), ny_ = (short)HIWORD(l); if (nx_ != g_mx || ny_ != g_my) { g_mx = nx_; g_my = ny_; if (ui_menu_open()) g_cur_dirty = 1; } }
    if (g_menu && g_menu_drawn && m == WM_LBUTTONDOWN) { int x_ = (short)LOWORD(l), y_ = (short)HIWORD(l), i_;      /* 升級選單用滑鼠點:三行各 36 px 高(跟畫的位置一樣) */
        for (i_ = 0; i_ < 3; i_++) if (x_ >= 100 && x_ < 550 && y_ >= 174 + i_ * 36 && y_ < 174 + i_ * 36 + 34) { menu_pick(i_ + 1); break; }
        return 0; }
    if (g_menu && (m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN || m == WM_RBUTTONUP)) return 0;
    if (g_menu && (m == WM_KEYDOWN || m == WM_KEYUP) && w >= '1' && w <= '3') { if (m == WM_KEYDOWN && g_menu_drawn) menu_pick((int)w - '0'); return 0; }
    if (g_msglog && logged < 300 && m != WM_MOUSEMOVE && m != WM_TIMER && m != WM_SETCURSOR && m != WM_NCHITTEST) {
        logged++; mlog("wndproc msg=%04x w=%x l=%x", m, (unsigned)w, (unsigned)l);
    }
    return CallWindowProcA(g_orig_wndproc, h, m, w, l_orig);
}
typedef ATOM (WINAPI *regclass_t)(const WNDCLASSA *);
static regclass_t g_orig_regclass;
static ATOM WINAPI my_regclass(const WNDCLASSA *wc) {
    WNDCLASSA c = *wc;
    if (!g_orig_wndproc) {
        g_orig_wndproc = c.lpfnWndProc;
        c.lpfnWndProc = my_wndproc;
        mlog("RegisterClassA '%s' wndproc=%p wrapped", c.lpszClassName ? c.lpszClassName : "?", g_orig_wndproc);
    }
    return g_orig_regclass(&c);
}

/* 排程的按鍵:到時間送 UP */
static struct { int vk; DWORD up_at; } g_keys[16];
static void send_key(int vk, int ms) {
    int i;
    if (!g_hwnd || !g_orig_wndproc) { mlog("key: no hwnd yet"); return; }
    CallWindowProcA(g_orig_wndproc, g_hwnd, WM_KEYDOWN, vk, 1);
    for (i = 0; i < 16; i++) if (!g_keys[i].vk) { g_keys[i].vk = vk; g_keys[i].up_at = GetTickCount() + ms; break; }
    mlog("key %d down (%d ms)", vk, ms);
}
static LPARAM g_click_lp; static int g_click_step; static DWORD g_click_t;
static void click_tick(void) {
    DWORD now = GetTickCount();
    if (!g_click_step || now - g_click_t < 80) return;
    g_click_t = now;
    if (g_click_step == 1) { CallWindowProcA(g_orig_wndproc, g_hwnd, WM_MOUSEMOVE, 0, g_click_lp); g_click_step = 2; }
    else if (g_click_step == 2) { CallWindowProcA(g_orig_wndproc, g_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, g_click_lp); g_click_step = 3; }
    else { CallWindowProcA(g_orig_wndproc, g_hwnd, WM_LBUTTONUP, 0, g_click_lp); g_click_step = 0; }
}
static DWORD g_hold_until; static LPARAM g_hold_lp; static UINT g_hold_dn = WM_LBUTTONDOWN;
static void hold_tick(void) {
    if (!g_hold_until) return;
    CallWindowProcA(g_orig_wndproc, g_hwnd, WM_MOUSEMOVE, g_hold_dn == WM_LBUTTONDOWN ? MK_LBUTTON : MK_RBUTTON, g_hold_lp);
    if ((int)(GetTickCount() - g_hold_until) >= 0) {
        CallWindowProcA(g_orig_wndproc, g_hwnd, g_hold_dn + 1, 0, g_hold_lp);
        g_hold_until = 0;
    }
}
static void key_tick(void) {
    click_tick(); hold_tick();
    int i; DWORD now = GetTickCount();
    for (i = 0; i < 16; i++) if (g_keys[i].vk && (int)(now - g_keys[i].up_at) >= 0) {
        CallWindowProcA(g_orig_wndproc, g_hwnd, WM_KEYUP, g_keys[i].vk, 0xC0000001);
        g_keys[i].vk = 0;
    }
}

/* ---- frames / overlay ---------------------------------------------------------------- */
static int g_overlay = 1;
static int g_dbgline = 0;      /* 左上「[SSS mod] FPS …」除錯字:預設不顯示(錄影會入鏡),指令 dbgline 1 打開 */
static LARGE_INTEGER g_qf, g_sec_start, g_last_frame;
static int g_frames_sec, g_src_is_blt;
static double g_max_ms_sec, g_max_ms_shown;
static int g_peek_idle_sec, g_peek_fps = -1;

static void frame_mark(void) {
    LARGE_INTEGER now; double dt;
    QueryPerformanceCounter(&now);
    if (g_last_frame.QuadPart) {
        dt = (now.QuadPart - g_last_frame.QuadPart) * 1000.0 / g_qf.QuadPart;
        if (dt > g_max_ms_sec) g_max_ms_sec = dt;
    }
    g_last_frame = now;
    g_frames_sec++;
}

static HFONT g_font;
/* 選單開著時自己畫滑鼠游標(Kirin 2026-11-26:選單裡滑鼠不見)。原作的游標畫在遊戲畫面裡:選單整塊蓋上去 / 暫停時不出新畫面 → 看不到。
 * 暫停中重畫是畫在同一張上一幀上 → 先把游標底下那一小塊存起來,下次畫之前貼回去,才不會留下殘影。 */
static HDC g_cur_mdc; static HBITMAP g_cur_bmp;
#define CUR_W 14
#define CUR_H 22
static int ui_menu_open(void) { return g_menu || rg_ui_open(); }
static int ui_paused(void) { unsigned char *app = GAME_APP; int p = 0; __try { p = app && app[0x12]; } __except (EXCEPTION_EXECUTE_HANDLER) { } return p; }
static void cur_restore(HDC dc) {
    if (g_cur_saved && ui_paused() && g_cur_mdc) BitBlt(dc, g_cur_sx, g_cur_sy, CUR_W, CUR_H, g_cur_mdc, 0, 0, SRCCOPY);
    g_cur_saved = 0;
}
static void cur_draw(HDC dc) {
    static const POINT shape[] = { {0,0}, {0,16}, {4,12}, {7,19}, {10,18}, {7,11}, {12,11} };
    POINT pt[7]; int i; HGDIOBJ ob, op; HPEN pen;
    if (!ui_menu_open() || g_mx < 0 || g_my < 0) return;
    if (!g_cur_mdc) { g_cur_mdc = CreateCompatibleDC(dc); g_cur_bmp = CreateCompatibleBitmap(dc, CUR_W, CUR_H); if (g_cur_mdc && g_cur_bmp) SelectObject(g_cur_mdc, g_cur_bmp); }
    if (g_cur_mdc && g_cur_bmp) { g_cur_sx = g_mx; g_cur_sy = g_my; BitBlt(g_cur_mdc, 0, 0, CUR_W, CUR_H, dc, g_cur_sx, g_cur_sy, SRCCOPY); g_cur_saved = 1; }
    for (i = 0; i < 7; i++) { pt[i].x = g_mx + shape[i].x; pt[i].y = g_my + shape[i].y; }
    pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0)); op = SelectObject(dc, pen); ob = SelectObject(dc, GetStockObject(WHITE_BRUSH));
    Polygon(dc, pt, 7);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(pen);
}
static void cur_tick(void) {      /* 暫停中滑鼠動了 → 重送一幀(最多每 30 ms 一次) */
    static DWORD last; DWORD now = GetTickCount();
    if (!g_cur_dirty || (int)(now - last) < 30) return;
    g_cur_dirty = 0; last = now;
    if (ui_menu_open() && ui_paused()) ui_present();
}
static void draw_overlay(HDC dc) {
    wchar_t buf[160]; int n; HGDIOBJ of = NULL;
    cur_restore(dc);
    n = _snwprintf(buf, 160, L"[SSS mod] FPS %d  max %.0fms  obj %d  enemy %d",
                   g_fps, g_max_ms_shown, g_nobj, g_nenemy);
    SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(0, 0, 0)); SetTextColor(dc, RGB(255, 255, 0));
    if (g_dbgline) TextOutW(dc, 4, 4, buf, n);
    if (!g_font) g_font = CreateFontW(-20, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, L"Microsoft JhengHei");
    if (g_font) of = SelectObject(dc, g_font);
    if (g_surv || g_surv_over) {
        DWORD ms = g_surv ? surv_ms() : g_surv_end_ms;
        static DWORD frozen;
        if (g_menu) ms = frozen; else frozen = ms;
        if (g_rg) n = _snwprintf(buf, 160, L"倖存者 %02u:%02u / 10:00  擊殺 %d  Lv %d  經驗 %d/%d  錢 +%d",
                       ms / 60000, ms / 1000 % 60, g_surv_kills, g_lv, g_xp, g_xp_next, rg_money_now());
        else n = _snwprintf(buf, 160, L"倖存者 %02u:%02u  第 %d 波  擊殺 %d  Lv %d  經驗 %d/%d",
                       ms / 60000, ms / 1000 % 60, g_surv_wave, g_surv_kills, g_lv, g_xp, g_xp_next);
        SetTextColor(dc, g_surv ? RGB(255, 80, 80) : RGB(255, 255, 255));
        TextOutW(dc, 4, 24, buf, n);
        if (g_surv_over) {
            n = _snwprintf(buf, 160, L"結束!存活 %u 秒  擊殺 %d   (F9 收起)", ms / 1000, g_surv_kills);
            TextOutW(dc, 120, 60, buf, n);
        }
        if (g_surv && g_cd_on) {            /* 技能格:名字 + CD 剩幾秒 */
            int i, x = 4, yy_ = 48; wchar_t nm[32]; DWORD tn = g_menu ? 0 : GetTickCount();
            for (i = 0; i < g_slots && i < SLOT_MAX; i++) {
                int left = (i < g_slot_n && tn && CD_TIMED) ? (int)(g_slot_ready[i] - tn) : 0; SIZE sz;
                if (i < g_slot_n) { def_name(game_movedef, g_slot_id[i], nm, 32); { int lv_ = rank_of(g_slot_id[i]); if (lv_ > 1) { wchar_t t_[8]; _snwprintf(t_, 8, L" Lv%d", lv_); t_[7] = 0; wcsncat(nm, t_, 31 - wcslen(nm)); } } { wchar_t kc_ = (wchar_t)(L'1' + i); if (left > 0) n = _snwprintf(buf, 160, L"[%c %s %.1f]", kc_, nm, left / 1000.0); else n = _snwprintf(buf, 160, (g_manual && i == g_sel_slot) ? L"▶[%c %s]" : L"[%c %s]", kc_, nm); } }
                else n = _snwprintf(buf, 160, L"[%d 空]", i + 1);
                SetTextColor(dc, i >= g_slot_n ? RGB(140, 140, 140) : left > 0 ? RGB(255, 170, 60) : (g_manual && i == g_sel_slot) ? RGB(255, 255, 120) : RGB(120, 220, 255));
                GetTextExtentPoint32W(dc, buf, n, &sz); if (x + sz.cx > 636 && x > 4) { x = 4; yy_ += 22; }      /* 技能格多的時候一行放不下(6 格實測最後一格被切掉)→ 換行 */
                TextOutW(dc, x, yy_, buf, n); x += sz.cx + 6;
            }
        }
        if (g_toast[0] && (int)(GetTickCount() - g_toast_until) < 0 && !g_menu) {
            SetTextColor(dc, RGB(120, 255, 120)); TextOutW(dc, 4, g_rg ? 96 : 72, g_toast, (int)wcslen(g_toast));
        }
        if (g_menu) {
            int i; RECT r = { 90, 130, 550, 300 }; wchar_t lab[64];
            HBRUSH br = CreateSolidBrush(RGB(16, 16, 40)); FillRect(dc, &r, br); DeleteObject(br);
            FrameRect(dc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
            SetBkColor(dc, RGB(16, 16, 40)); SetTextColor(dc, RGB(255, 220, 80));
            n = _snwprintf(buf, 160, L"升級!Lv %d —— 按 1 / 2 / 3 或點一下", g_lv);
            TextOutW(dc, 106, 140, buf, n);
            SetTextColor(dc, RGB(255, 255, 255));
            for (i = 0; i < 3; i++) {
                upg_label(g_menu_opt[i], lab, 64);
                n = _snwprintf(buf, 160, L"%d. %s", i + 1, lab);
                TextOutW(dc, 116, 180 + i * 36, buf, n);
            }
            g_menu_drawn = 1;
        }
    }
    rg_draw(dc);
    cur_draw(dc);
    if (of) SelectObject(dc, of);
}

/* IDirectDrawSurface* vtable:5 Blt, 11 Flip, 14 GetCaps, 17 GetDC, 26 ReleaseDC(各版本相同) */
typedef HRESULT (__stdcall *blt_t)(void *self, RECT *dr, void *src, RECT *sr, DWORD fl, void *fx);
typedef HRESULT (__stdcall *flip_t)(void *self, void *ovr, DWORD fl);
typedef HRESULT (__stdcall *bltfast_t)(void *self, DWORD x, DWORD y, void *src, RECT *sr, DWORD fl);
typedef HRESULT (__stdcall *getdc_t)(void *self, HDC *dc);
typedef HRESULT (__stdcall *reldc_t)(void *self, HDC dc);
typedef HRESULT (__stdcall *csurf_t)(void *self, void *desc, void **out, void *outer);
#define VT(o) (*(void ***)(o))
static void *g_primary;
static int g_force565 = 1;
typedef HRESULT (__stdcall *gas_t)(void *self, void *caps, void **out);
typedef HRESULT (__stdcall *lock_t2)(void *self, RECT *r, void *desc, DWORD fl, void *ev);
typedef HRESULT (__stdcall *unlock_t2)(void *self, void *p);
static struct { void **vt; blt_t blt; flip_t flip; bltfast_t bltfast; gas_t gas; lock_t2 lock; unlock_t2 unlock; } g_svt[8];
static void *g_backbuf; static int g_gas_log, g_lock_log, g_blt_seen, g_in_ovl, g_fs_log;
static void hook_surface(void *s);
static int g_blt_log, g_bfast_log, g_flip_log;      /* 前幾次送上主畫面的參數寫進 log(全螢幕 / 別的解析度怎麼畫,查問題用) */
static struct { void **vt; csurf_t cs; } g_dvt[8];

static int svt_find(void **vt) { int i; for (i = 0; i < 8; i++) if (g_svt[i].vt == vt) return i; return -1; }

/* 截圖:直接讀遊戲要送上螢幕的 back buffer(螢幕鎖定 / 關閉時也抓得到) */
static char g_shot[MAX_PATH];
static void *g_last_src;
static struct { RECT dr, sr; int has_dr, has_sr; DWORD fl; } g_pb;      /* 上一次 Blt 到主畫面的參數(ui_present 重送用) */
static void save_bmp(HDC src, int w, int h, const char *path) {
    BITMAPINFO bi; void *bits; HDC mem; HBITMAP bmp, old; FILE *f; BITMAPFILEHEADER fh;
    memset(&bi, 0, sizeof bi);
    bi.bmiHeader.biSize = sizeof bi.bmiHeader; bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 24; bi.bmiHeader.biCompression = BI_RGB;
    mem = CreateCompatibleDC(src);
    bmp = CreateDIBSection(src, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!mem || !bmp) { mlog("shot: GDI alloc failed"); return; }
    old = (HBITMAP)SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, src, 0, 0, SRCCOPY);
    GdiFlush();
    bi.bmiHeader.biSizeImage = ((w * 3 + 3) & ~3) * h;
    memset(&fh, 0, sizeof fh);
    fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof bi.bmiHeader; fh.bfSize = fh.bfOffBits + bi.bmiHeader.biSizeImage;
    if ((f = fopen(path, "wb")) != NULL) {
        fwrite(&fh, sizeof fh, 1, f); fwrite(&bi.bmiHeader, sizeof bi.bmiHeader, 1, f);
        fwrite(bits, bi.bmiHeader.biSizeImage, 1, f); fclose(f);
        mlog("shot saved %s (%dx%d)", path, w, h);
    }
    SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem);
}
/* 用 Lock 讀原始像素,依像素格式的遮罩轉 24-bit(GDI 對這種 surface 的視角不可靠) */
typedef HRESULT (__stdcall *lock_t)(void *self, RECT *r, void *desc, DWORD fl, HANDLE ev);
typedef HRESULT (__stdcall *unlock_t)(void *self, void *p);
static void overlay_on_dc(void *surf, RECT *sr);
static int mask_shift(DWORD m) { int s = 0; if (!m) return 0; while (!(m & 1)) { m >>= 1; s++; } return s; }
static int mask_bits(DWORD m) { int n = 0; while (m) { n += m & 1; m >>= 1; } return n; }
static int save_locked(void *surf, const char *path) {
    unsigned char desc[124]; HRESULT hr; int w, h, pitch, bpp, x, y, rowb; DWORD rm, gm, bm;
    unsigned char *px, *row; FILE *f; BITMAPFILEHEADER fh; BITMAPINFOHEADER ih;
    memset(desc, 0, sizeof desc); *(DWORD *)desc = 108;          /* DDSURFACEDESC(v1 介面) */
    hr = ((lock_t)VT(surf)[25])(surf, NULL, desc, 1 /*WAIT*/ | 0x800 /*READONLY*/, NULL);
    if (hr) { mlog("shot: Lock failed %08x", hr); return 0; }
    h = *(int *)(desc + 8); w = *(int *)(desc + 12); pitch = *(int *)(desc + 16); px = *(unsigned char **)(desc + 36);
    bpp = *(int *)(desc + 84); rm = *(DWORD *)(desc + 88); gm = *(DWORD *)(desc + 92); bm = *(DWORD *)(desc + 96);
    mlog("shot: surface %dx%d pitch=%d bpp=%d masks r=%x g=%x b=%x", w, h, pitch, bpp, rm, gm, bm);
    rowb = (w * 3 + 3) & ~3;
    row = (unsigned char *)malloc(rowb);
    memset(&ih, 0, sizeof ih); ih.biSize = sizeof ih; ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 24;
    ih.biSizeImage = rowb * h;
    memset(&fh, 0, sizeof fh); fh.bfType = 0x4d42; fh.bfOffBits = sizeof fh + sizeof ih; fh.bfSize = fh.bfOffBits + ih.biSizeImage;
    if ((f = fopen(path, "wb")) != NULL && row) {
        int rs = mask_shift(rm), gs = mask_shift(gm), bs = mask_shift(bm);
        int rb = mask_bits(rm), gb = mask_bits(gm), bb = mask_bits(bm);
        fwrite(&fh, sizeof fh, 1, f); fwrite(&ih, sizeof ih, 1, f);
        for (y = 0; y < h; y++) {
            unsigned char *s = px + y * pitch;
            memset(row, 0, rowb);
            for (x = 0; x < w; x++) {
                DWORD v;
                if (bpp == 16 && rm == 0x7c00) { unsigned short q = ((unsigned short *)s)[x];
                    row[x*3] = (unsigned char)((q & 0x1f) << 3); row[x*3+1] = (unsigned char)(((q >> 5) & 0x1f) << 3); row[x*3+2] = (unsigned char)(((q >> 10) & 0x1f) << 3); continue; }
                v = bpp == 16 ? ((unsigned short *)s)[x] : bpp == 32 ? ((DWORD *)s)[x]
                        : (DWORD)(s[x * 3] | s[x * 3 + 1] << 8 | s[x * 3 + 2] << 16);
                row[x * 3 + 0] = bb ? (unsigned char)((((v & bm) >> bs) * 255) / ((1 << bb) - 1)) : 0;
                row[x * 3 + 1] = gb ? (unsigned char)((((v & gm) >> gs) * 255) / ((1 << gb) - 1)) : 0;
                row[x * 3 + 2] = rb ? (unsigned char)((((v & rm) >> rs) * 255) / ((1 << rb) - 1)) : 0;
            }
            fwrite(row, rowb, 1, f);
        }
        fclose(f);
        mlog("shot saved %s", path);
    }
    free(row);
    ((unlock_t)VT(surf)[32])(surf, NULL);
    return 1;
}
/* rec SECS FPS NAME:模組自己把遊戲畫面(含 overlay)連續存成 game/NAME.raw,tools/raw2mp4.py 轉 MP4。
 * 為什麼要有:筆電鎖屏時 OBS 視窗擷取整段全黑(實測),這條從 DirectDraw surface 直接讀,不受鎖屏影響。
 * 檔案:6 個 int(w, h, bpp, rmask, gmask, bmask),之後每幀 = DWORD 毫秒 + w*h*bpp/8 原始像素。 */
static FILE *g_rec_f; static DWORD g_rec_end, g_rec_next; static int g_rec_iv, g_rec_n;
static void rec_frame(void *surf) {
    unsigned char desc[124]; DWORD now = GetTickCount(); int w, h, pitch, bpp, y; unsigned char *px;
    if (!g_rec_f || !surf) return;
    if ((int)(now - g_rec_end) >= 0) { fclose(g_rec_f); g_rec_f = NULL; mlog("rec done: %d frames", g_rec_n); return; }
    if ((int)(now - g_rec_next) < 0) return;
    g_rec_next += g_rec_iv; if ((int)(now - g_rec_next) > 0) g_rec_next = now + g_rec_iv;
    memset(desc, 0, sizeof desc); *(DWORD *)desc = 108;
    if (((lock_t)VT(surf)[25])(surf, NULL, desc, 1 | 0x800, NULL)) return;
    h = *(int *)(desc + 8); w = *(int *)(desc + 12); pitch = *(int *)(desc + 16); px = *(unsigned char **)(desc + 36); bpp = *(int *)(desc + 84);
    if (!g_rec_n) { int hd[6]; hd[0] = w; hd[1] = h; hd[2] = bpp; hd[3] = *(int *)(desc + 88); hd[4] = *(int *)(desc + 92); hd[5] = *(int *)(desc + 96); fwrite(hd, sizeof hd, 1, g_rec_f); }
    fwrite(&now, 4, 1, g_rec_f);
    for (y = 0; y < h; y++) fwrite(px + y * pitch, w * bpp / 8, 1, g_rec_f);
    ((unlock_t)VT(surf)[32])(surf, NULL);
    g_rec_n++;
}
static void overlay_on(void *surf, RECT *sr) {
    if (g_shot[0] && surf) {
        /* 先畫 overlay 再抓,截圖裡才看得到 */
        HDC dc;
        if (g_overlay && ((getdc_t)VT(surf)[17])(surf, &dc) == 0) { draw_overlay(dc); ((reldc_t)VT(surf)[26])(surf, dc); }
        save_locked(surf, g_shot); g_shot[0] = 0;
        return;
    }
    overlay_on_dc(surf, sr);
    rec_frame(surf);
}
static void overlay_on_dc(void *surf, RECT *sr) {
    HDC dc;
    if ((!g_overlay && !g_shot[0]) || !surf) return;
    if (((getdc_t)VT(surf)[17])(surf, &dc) == 0) {
        if (g_overlay) draw_overlay(dc);
        if (g_shot[0]) {
            int w = sr ? sr->right - sr->left : 640, h = sr ? sr->bottom - sr->top : 480;
            save_bmp(dc, w, h, g_shot); g_shot[0] = 0;
        }
        ((reldc_t)VT(surf)[26])(surf, dc);
    }
}
static HRESULT __stdcall my_blt(void *self, RECT *dr, void *src, RECT *sr, DWORD fl, void *fx) {
    int i = svt_find(VT(self));
    if (self == g_primary && src && g_blt_log < 4) { g_blt_log++; mlog("blt->primary dr=%ld,%ld,%ld,%ld sr=%ld,%ld,%ld,%ld fl=%08lx", dr ? dr->left : -1, dr ? dr->top : -1, dr ? dr->right : -1, dr ? dr->bottom : -1, sr ? sr->left : -1, sr ? sr->top : -1, sr ? sr->right : -1, sr ? sr->bottom : -1, fl); }
    if (self == g_primary && src) {
        g_blt_seen = 1; g_last_src = src; g_pb.has_dr = dr != NULL; g_pb.has_sr = sr != NULL; if (dr) g_pb.dr = *dr; if (sr) g_pb.sr = *sr; g_pb.fl = fl;
        overlay_on(src, sr); frame_mark(); g_src_is_blt = 1;
    }
    return g_svt[i].blt(self, dr, src, sr, fl, fx);
}
/* 暫停中原作不出新畫面:把 overlay 重畫到上一幀的 back buffer,再照上一次的參數 Blt 一次 */
static void ui_present(void) {
    int i; HDC dc;
    if (g_primary && !g_blt_seen && !g_in_ovl) {      /* 全螢幕:直接畫在主畫面 */
        g_in_ovl = 1;
        __try { if (((getdc_t)VT(g_primary)[17])(g_primary, &dc) == 0) { draw_overlay(dc); ((reldc_t)VT(g_primary)[26])(g_primary, dc); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
        g_in_ovl = 0; return;
    }
    if (!g_primary || !g_last_src) return;
    __try {
        if (((getdc_t)VT(g_last_src)[17])(g_last_src, &dc) == 0) { draw_overlay(dc); ((reldc_t)VT(g_last_src)[26])(g_last_src, dc); }
        if ((i = svt_find(VT(g_primary))) >= 0) g_svt[i].blt(g_primary, g_pb.has_dr ? &g_pb.dr : NULL, g_last_src, g_pb.has_sr ? &g_pb.sr : NULL, g_pb.fl, NULL);
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("ui_present: fault"); }
}
/* 全螢幕時原作用 BltFast 把畫面貼上主畫面(視窗模式才用 Blt):一樣先把 overlay 畫進來源 */
static HRESULT __stdcall my_bltfast(void *self, DWORD x, DWORD y, void *src, RECT *sr, DWORD fl) {
    int i = svt_find(VT(self));
    if (self == g_primary && src && g_bfast_log < 4) { g_bfast_log++; mlog("bltfast->primary at %lu,%lu sr=%ld,%ld,%ld,%ld fl=%08lx", x, y, sr ? sr->left : -1, sr ? sr->top : -1, sr ? sr->right : -1, sr ? sr->bottom : -1, fl); }
    if (self == g_primary && src) { overlay_on(src, sr); frame_mark(); g_src_is_blt = 1; }
    return g_svt[i].bltfast(self, x, y, src, sr, fl);
}
/* 全螢幕(-fullscreen)時主畫面是 flip chain:原作拿 back buffer 來畫,畫完 Flip */
static HRESULT __stdcall my_gas(void *self, void *caps, void **out) {
    int i = svt_find(VT(self)); HRESULT hr = g_svt[i].gas(self, caps, out);
    if (g_gas_log < 4) { g_gas_log++; mlog("GetAttachedSurface self=%p primary=%p caps=%08lx -> hr=%08lx %p", self, g_primary, caps ? *(DWORD *)caps : 0, hr, out ? *out : NULL); }
    if (hr == 0 && self == g_primary && out && *out) { g_backbuf = *out; hook_surface(*out); }
    return hr;
}
static HRESULT __stdcall my_lock(void *self, RECT *r, void *desc, DWORD fl, void *ev) {
    int i = svt_find(VT(self));
    if (g_lock_log < 6) { g_lock_log++; mlog("Lock self=%p primary=%p back=%p", self, g_primary, g_backbuf); }
    return g_svt[i].lock(self, r, desc, fl, ev);
}
/* 全螢幕:原作直接 Lock 主畫面寫像素(不經 Blt / Flip)→ 在它 Unlock 之後把 overlay 畫上主畫面 */
static HRESULT __stdcall my_unlock(void *self, void *p) {
    int i = svt_find(VT(self)); HRESULT hr = g_svt[i].unlock(self, p);
    if (self == g_primary && !g_blt_seen && !g_in_ovl) {
        g_in_ovl = 1;
        __try { HDC dc; if (g_overlay && ((getdc_t)VT(self)[17])(self, &dc) == 0) {
            if (g_fs_log < 1) { RECT cb; g_fs_log++; GetClipBox(dc, &cb); mlog("fullscreen overlay: primary %ldx%ld", cb.right, cb.bottom); }
            draw_overlay(dc); ((reldc_t)VT(self)[26])(self, dc); }
            if (g_shot[0]) { save_locked(self, g_shot); g_shot[0] = 0; }
            frame_mark();
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        g_in_ovl = 0;
    }
    return hr;
}
static HRESULT __stdcall my_flip(void *self, void *ovr, DWORD fl) {
    int i = svt_find(VT(self));
    if (g_flip_log < 4) { g_flip_log++; mlog("flip self=%p primary=%p", self, g_primary); }
    if (self == g_primary) { frame_mark(); }
    return g_svt[i].flip(self, ovr, fl);
}
static void hook_surface(void *s) {
    void **vt = VT(s); int i;
    if (svt_find(vt) >= 0) return;
    for (i = 0; i < 8; i++) if (!g_svt[i].vt) {
        g_svt[i].vt = vt;
        g_svt[i].blt = (blt_t)patch_ptr(&vt[5], (void *)my_blt);
        g_svt[i].flip = (flip_t)patch_ptr(&vt[11], (void *)my_flip);
        g_svt[i].bltfast = (bltfast_t)patch_ptr(&vt[7], (void *)my_bltfast);
        g_svt[i].gas = (gas_t)patch_ptr(&vt[12], (void *)my_gas);
        g_svt[i].lock = (lock_t2)patch_ptr(&vt[25], (void *)my_lock);
        g_svt[i].unlock = (unlock_t2)patch_ptr(&vt[32], (void *)my_unlock);
        mlog("hooked surface vtable %p", vt);
        return;
    }
}
static HRESULT __stdcall my_createsurface(void *self, void *desc, void **out, void *outer) {
    int i; csurf_t real = NULL; HRESULT hr;
    unsigned char d2[124];
    for (i = 0; i < 8; i++) if (g_dvt[i].vt == VT(self)) real = g_dvt[i].cs;
    mlog("CreateSurface flags=%08x caps=%08x %dx%d", *(DWORD *)((char *)desc + 4), *(DWORD *)((char *)desc + 104),
         *(int *)((char *)desc + 12), *(int *)((char *)desc + 8));
    /* 遊戲假設桌面 16-bit(原版靠 GameLoader 切色深)。離屏 surface 沒指定像素格式時強制 RGB555,
       主畫面維持桌面格式,由 DirectDraw 在 Blt 時轉色 —— 不必改 Kirin 的螢幕設定。 */
    if (g_force565 && !(*(DWORD *)((char *)desc + 104) & 0x200) && !(*(DWORD *)((char *)desc + 4) & 0x1000)) {
        DWORD sz = *(DWORD *)desc; if (sz > sizeof d2) sz = sizeof d2;
        memcpy(d2, desc, sz);
        *(DWORD *)(d2 + 4) |= 0x1000;                                  /* DDSD_PIXELFORMAT */
        memset(d2 + 72, 0, 32);
        *(DWORD *)(d2 + 72) = 32; *(DWORD *)(d2 + 76) = 0x40;          /* DDPF_RGB */
        *(DWORD *)(d2 + 84) = 16; *(DWORD *)(d2 + 88) = 0x7c00; *(DWORD *)(d2 + 92) = 0x3e0; *(DWORD *)(d2 + 96) = 0x1f;
        hr = real(self, d2, out, outer);
        mlog("  forced RGB555 -> hr=%08x", hr);
        if (hr != 0) hr = real(self, desc, out, outer);
    } else
    hr = real(self, desc, out, outer);
    if (hr == 0 && out && *out) {
        DWORD caps = *(DWORD *)((char *)desc + 104);       /* DDSURFACEDESC(2).ddsCaps.dwCaps */
        hook_surface(*out);
        if (caps & 0x200) { g_primary = *out; mlog("primary surface %p", g_primary); }
    }
    return hr;
}
static void hook_ddraw(void *dd) {
    void **vt = VT(dd); int i;
    for (i = 0; i < 8; i++) if (g_dvt[i].vt == vt) return;
    for (i = 0; i < 8; i++) if (!g_dvt[i].vt) {
        g_dvt[i].vt = vt;
        g_dvt[i].cs = (csurf_t)patch_ptr(&vt[6], (void *)my_createsurface);
        mlog("hooked ddraw vtable %p", vt);
        return;
    }
}
/* IDirectDraw::QueryInterface 也掛,遊戲若升級成 IDirectDraw2/4 一樣接得到 */
typedef HRESULT (__stdcall *qi_t)(void *self, const GUID *iid, void **out);
static qi_t g_orig_qi;
static HRESULT __stdcall my_qi(void *self, const GUID *iid, void **out) {
    HRESULT hr = g_orig_qi(self, iid, out);
    if (hr == 0 && out && *out && VT(*out)[0] != (void *)my_qi) {
        /* 只對 DirectDraw 介面再掛 CreateSurface;surface 介面的 vtable[6] 是 BltBatch,不能動 */
        static const GUID dd2 = {0xB3A6F3E0,0x2B43,0x11CF,{0xA2,0xDE,0x00,0xAA,0x00,0xB9,0x33,0x56}};
        static const GUID dd4 = {0x9c59509a,0x39bd,0x11d1,{0x8c,0x4a,0x00,0xc0,0x4f,0xd9,0x30,0xc5}};
        static const GUID dd7 = {0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0xa0,0xc9,0x69,0x68,0xa8}};
        if (!memcmp(iid, &dd2, 16) || !memcmp(iid, &dd4, 16) || !memcmp(iid, &dd7, 16)) {
            mlog("ddraw QueryInterface -> new interface");
            hook_ddraw(*out);
        }
    }
    return hr;
}
typedef HRESULT (WINAPI *ddc_t)(GUID *, void **, void *);
static ddc_t g_orig_ddc;
static HRESULT WINAPI my_ddcreate(GUID *g, void **dd, void *outer) {
    HRESULT hr = g_orig_ddc(g, dd, outer);
    mlog("DirectDrawCreate hr=%08x dd=%p", hr, dd ? *dd : NULL);
    if (hr == 0 && dd && *dd) {
        hook_ddraw(*dd);
        if (!g_orig_qi) g_orig_qi = (qi_t)patch_ptr(&VT(*dd)[0], (void *)my_qi);
    }
    return hr;
}

/* ---- per-tick (main thread) ---------------------------------------------------------- */
/* god:我方(陣營 0)角色每幀補滿 —— statblock = obj+0x1a4;+0x50/+0x54 命(現/上限)、+0x58/+0x5c 內(依 0x444e00 推得) */
static int g_god;
static void god_tick(void) {
    unsigned char **t; int i;
    if (!g_god || !(t = obj_table())) return;
    __try {
        for (i = 1; i < OBJ_SLOTS; i++) {
            unsigned char *o = t[i], *sb;
            if (!o || !IS_CHAR(o) || (I32(o, 0x100) & 7) || !I32(o, 0xfc)) continue;
            sb = *(unsigned char **)(o + 0x1a4);
            if (sb && g_god == 2 && I32(sb, 0x54) < 30000) I32(sb, 0x54) = 30000;      /* god 2:掃圖用,命上限也墊高(god 1 擋不住一擊超過命上限的傷害) */
            if (sb && I32(sb, 0x54) > 0) I32(sb, 0x50) = I32(sb, 0x54);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { g_god = 0; mlog("god: exception, off"); }
}
/* bench N:接下來 N 秒的 FPS 統計 */
static int g_bench_left, g_bench_n, g_bench_min = 9999, g_bench_sum; static double g_bench_maxms;
static void second_tick(void) {
    LARGE_INTEGER now; double el;
    QueryPerformanceCounter(&now);
    el = (now.QuadPart - g_sec_start.QuadPart) / (double)g_qf.QuadPart;
    if (el < 1.0) return;
    g_fps = (int)(g_frames_sec / el + 0.5);
    g_peek_fps = (int)(g_peek_idle_sec / el + 0.5);
    g_max_ms_shown = g_max_ms_sec;
    count_objs();
    mlog("fps=%d (src=%s) max_frame=%.1fms peek_idle/s=%d obj=%d enemy=%d",
         g_fps, g_src_is_blt ? "blt" : (g_fps > 0 ? "flip" : "none"), g_max_ms_sec, g_peek_fps, g_nobj, g_nenemy);
    if (g_bench_left > 0) {
        g_bench_n++; g_bench_sum += g_fps; if (g_fps < g_bench_min) g_bench_min = g_fps;
        if (g_max_ms_sec > g_bench_maxms) g_bench_maxms = g_max_ms_sec;
        if (--g_bench_left == 0)
            mlog("BENCH %ds: avg=%.1f min=%d worst_frame=%.1fms enemies=%d objects=%d", g_bench_n,
                 g_bench_sum / (double)g_bench_n, g_bench_min, g_bench_maxms, g_nenemy, g_nobj);
    }
    g_frames_sec = 0; g_peek_idle_sec = 0; g_max_ms_sec = 0; g_sec_start = now;
}

/* ---- 地圖:列表 / 直接換到指定地圖(notes/07)--------------------------------------------
 * 原作換地圖是排程式的:請求函式寫 scene+0x15c..(scene = *(模式管理器 0x4c31f8)+0x14),主迴圈淡出 → 拆舊圖 → 載新圖 → 淡入。
 *   0x4198a0 thiscall(scene, stage, 格x, 格y) / 0x419980 thiscall(scene, stage, 出口 id);gs->stage == 0(標題)或正在換時靜默不做。
 * 淡入結束會設 scene+0x1e4 = 1 → 0x416530 寫自動存檔 exsave99。模組換圖期間把 0x4197cb 的立即數 1 改 0(到站後改回),不蓋存檔。
 * stage.ini 在 pak 裡:0x46ecf0 open / 0x46ed20 get(查不到回 -1,不會插入)/ 0x46ec00 close,都是 cdecl。 */
#define GAME_GS    (*(unsigned char **)0x4c3214)
#define GAME_MODE  (*(unsigned char **)0x4c31f8)
static unsigned char *game_scene(void) { unsigned char *m = GAME_MODE; return m ? *(unsigned char **)(m + 0x14) : NULL; }
/* 🔴 這幾支原作函式會弄壞 ebx / esi / edi(實測:呼叫完回到模組,暫存器裡的變數變垃圾、接著 c0000005)→ 一律經過這個會自己存 / 還原暫存器的跳板 */
static __declspec(naked) int __cdecl safe_cdecl(void *fn, int a, int b, int c, int d) {
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        push dword ptr [ebp + 24]
        push dword ptr [ebp + 20]
        push dword ptr [ebp + 16]
        push dword ptr [ebp + 12]
        call dword ptr [ebp + 8]
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}
static int ini_open(void) { return safe_cdecl((void *)0x46ecf0, (int)"stage.ini", 0, 0, 0) != -1; }
static int ini_get(const char *key, char *out) { out[0] = 0; return safe_cdecl((void *)0x46ed20, 0, (int)key, 1, (int)out) != -1; }
static void ini_close(void) { safe_cdecl((void *)0x46ec00, 0, 0, 0, 0); }
static void do_maps(int lo, int hi) {
    int id, n = 0, g; char key[64], raw[1024], name[256], type[256], gw[512], gv[128], path[MAX_PATH]; FILE *f;
    _snprintf(path, MAX_PATH, "%s\\sss_maps.tsv", g_dir);
    f = fopen(path, "w");
    __try {
        if (!ini_open()) { mlog("maps: cannot open stage.ini"); if (f) fclose(f); return; }
        for (id = lo; id <= hi; id++) {
            int k = 0;
            sprintf(key, "stage%03d\\Name", id); if (!ini_get(key, raw)) continue;
            big5_to_utf8(raw, name, sizeof name);
            sprintf(key, "stage%03d\\Type", id); ini_get(key, raw); strncpy(type, raw, 255); type[255] = 0;
            gw[0] = 0;
            for (g = 1; g <= 32; g++) { sprintf(key, "stage%03d\\Gateway%03d", id, g); if (!ini_get(key, gv)) break; k += _snprintf(gw + k, sizeof gw - k - 1, "%s%d>%s", g > 1 ? " " : "", g, gv); if (k > 480) break; }
            mlog("map %d\t%s\ttype=%s\tgw=%s", id, name, type, gw);
            if (f) fprintf(f, "%d\t%s\t%s\t%s\n", id, name, type, gw);
            n++;
        }
        ini_close();
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("maps: exception %08x at id %d", GetExceptionCode(), id); }
    if (f) fclose(f);
    mlog("maps: %d stages in %d..%d -> sss_maps.tsv", n, lo, hi);
}
static int g_rgb3;
static int g_goto_id = -1, g_goto_a, g_goto_b, g_goto_xy, g_goto_loaded, g_goto_watch, g_goto_from; static DWORD g_goto_at, g_goto_t0;
static void autosave_patch(int off) {      /* 0x4197c5: mov byte [reg+0x1e4], 1 —— 立即數在 0x4197cb */
    unsigned char *p = (unsigned char *)0x4197c5; DWORD old;
    __try {
        if (p[0] != 0xc6 || p[2] != 0xe4 || p[3] != 0x01 || p[4] || p[5] || p[6] != (off ? 1 : 0)) { if (p[6] != (off ? 0 : 1)) mlog("autosave patch: unexpected bytes %02x %02x %02x .. %02x", p[0], p[1], p[2], p[6]); return; }
        if (VirtualProtect(p, 8, PAGE_EXECUTE_READWRITE, &old)) { p[6] = off ? 0 : 1; VirtualProtect(p, 8, old, &old); mlog("autosave %s", off ? "OFF (mod map change)" : "restored"); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("autosave patch: fault"); }
}
static int cur_stage_id(void) { unsigned char *gs = GAME_GS, *st = gs ? *(unsigned char **)(gs + 0x10) : NULL; return st ? I32(st, 0x788) : -1; }
static void map_report(const char *tag) {
    unsigned char *gs = GAME_GS, *st = gs ? *(unsigned char **)(gs + 0x10) : NULL, **t = obj_table(), *pl = t ? find_player(t) : NULL; char nm[128] = "";
    if (!st) { mlog("%s: no stage (title?)", tag); return; }
    __try { big5_to_utf8((char *)st + 0x7a0, nm, sizeof nm); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    count_objs();
    if (t) { int i_, k_ = 0, seen_[64][2], ns_ = 0, j_; char b_[900];
        for (i_ = 1; i_ < OBJ_SLOTS && ns_ < 64; i_++) { unsigned char *o_ = t[i_]; if (!o_ || !IS_CHAR(o_) || !I32(o_, 0xfc)) continue;
            for (j_ = 0; j_ < ns_; j_++) if (seen_[j_][0] == I32(o_, 4) && seen_[j_][1] == I32(o_, 0xfc)) break;
            if (j_ < ns_) continue; seen_[ns_][0] = I32(o_, 4); seen_[ns_][1] = I32(o_, 0xfc); ns_++;
            if (k_ < 860) k_ += sprintf(b_ + k_, " %d:%d/f%x", I32(o_, 4), I32(o_, 0xfc), I32(o_, 0x100) & 7); }
        if (ns_) mlog("PAIRS stage %d:%s", I32(st, 0x788), b_); }
    mlog("%s: stage %d \"%s\" %dx%d cell=%dpx flags=%x player=%s(%d,%d) obj=%d enemy=%d", tag, I32(st, 0x788), nm, I32(st, 0x790), I32(st, 0x794), 1 << (I32(st, 0x7d0) & 31), I32(st, 0x79c),
         pl ? "" : "none", pl ? I32(pl, 0x14) >> 16 : 0, pl ? I32(pl, 0x18) >> 16 : 0, g_nobj, g_nenemy);
}
static void map_tick(void) {
    unsigned char *gs = GAME_GS, *sc = game_scene(); DWORD now = GetTickCount();
    __try {
        if (g_goto_watch && sc) {
            if (I32(sc, 0x15c)) g_goto_watch = 2;
            else if (g_goto_watch == 2) {
                unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL;
                autosave_patch(0); g_goto_watch = 0;
                if (pl && g_goto_xy) {       /* 座標版:給的格子不一定能站 → 挪到最近可走格 */
                    int sx = I32(pl, 0x14) >> 16, sy = I32(pl, 0x18) >> 16, x0 = sx, y0 = sy;
                    if (((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)(*(void **)(pl + 0xac), 0, &sx, &sy, (~I32(pl, 0x100) >> 28) & 1, I32(pl, 0x194), 40) & 0xff) {
                        if (sx != x0 || sy != y0) { obj_setpos(pl, sx, sy); mlog("goto_map: player snapped (%d,%d) -> (%d,%d)", x0, y0, sx, sy); }
                    } else mlog("goto_map: no walkable cell near (%d,%d)", x0, y0);
                }
                mlog("goto_map: %s in %ums (from stage %d)", cur_stage_id() == g_goto_from ? "SAME STAGE?" : "arrived", now - g_goto_t0, g_goto_from);
                map_report("goto_map");
            } else if (now - g_goto_t0 > 3000) { autosave_patch(0); g_goto_watch = 0; mlog("goto_map: request ignored by the game (scene flags stayed 0)"); }
        }
        if (g_goto_id < 0 || !gs || !sc) return;
        if (!*(void **)(gs + 0x10)) {            /* 標題畫面:先讀自動存檔進遊戲,再換圖 */
            if (!g_goto_loaded) { g_goto_loaded = 1; mlog("goto_map: at title -> load slot 0x%x first", g_title_slot); ((void (__fastcall *)(void *, int, int))0x4261b0)(GAME_APP, 0, g_title_slot); }
            g_goto_at = now + 2500; return;
        }
        if ((int)(now - g_goto_at) < 0) return;
        if (I32(sc, 0x15c)) {            /* 原作正在淡出 / 換圖(主角死掉的結束畫面也算)→ 等 5 秒,還沒好就放棄 */
            if (now - g_goto_at > 5000) { mlog("goto_map: dropped - scene busy for 5s (flags=%x; fading / game over?)", I32(sc, 0x15c)); g_goto_id = -1; }
            return;
        }
        {   /* 主角死了(原作跳「進度讀取」選單)還換圖會當掉(實測 2 次)→ 不換 */
            unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL, *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
            if (!sb || I32(sb, 0x50) <= 0) { mlog("goto_map: refused - no live player (dead / game over?)"); g_goto_id = -1; return; }
        }
        rg_before_goto();
        g_goto_from = cur_stage_id(); g_goto_t0 = now; g_goto_watch = 1; g_goto_loaded = 0;
        autosave_patch(1);
        if (g_goto_xy) ((void (__fastcall *)(void *, int, int, int, int))0x4198a0)(sc, 0, g_goto_id, g_goto_a, g_goto_b);
        else           ((void (__fastcall *)(void *, int, int, int))0x419980)(sc, 0, g_goto_id, g_goto_a);
        mlog("goto_map: requested stage %d %s %d,%d (from %d)", g_goto_id, g_goto_xy ? "cell" : "gateway", g_goto_a, g_goto_b, g_goto_from);
        g_goto_id = -1;
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("goto_map: exception %08x", GetExceptionCode()); g_goto_id = -1; g_goto_watch = 0; autosave_patch(0); }
}
/* goto_map ID            → 目標圖有 Gateway001 就從出口 1 進,沒有就放 (10,10) 再挪到可走格
 * goto_map ID gw N       → 從出口 N 進;goto_map ID X Y → 放在格 (X,Y) */
static void goto_map_cmd(const char *arg) {
    int id = -1, a = 0, b = 0, n; char key[64], v[128];
    if (sscanf(arg, "%d gw %d", &id, &a) == 2) g_goto_xy = 0;
    else if ((n = sscanf(arg, "%d %d %d", &id, &a, &b)) == 3) g_goto_xy = 1;
    else if (n == 1) {
        int has = 0;
        __try { if (ini_open()) { sprintf(key, "stage%03d\\Gateway001", id); has = ini_get(key, v); ini_close(); } } __except (EXCEPTION_EXECUTE_HANDLER) { }
        if (has) { g_goto_xy = 0; a = 1; } else { g_goto_xy = 1; a = b = 10; }
    } else { mlog("goto_map: usage goto_map ID [gw N | X Y]"); return; }
    if (!safe_cdecl((void *)0x450590, id, 0, 0, 0)) { mlog("goto_map: stage %d not in stage.ini", id); return; }
    g_goto_id = id; g_goto_a = a; g_goto_b = b; g_goto_at = GetTickCount(); g_goto_loaded = 0;
    mlog("goto_map: queued stage %d (%s %d,%d)", id, g_goto_xy ? "cell" : "gateway", a, b);
}

#include "rogue.c"

static void cmd_tick(void) {
    static DWORD last; char path[MAX_PATH], line[256]; FILE *f;
    DWORD now = GetTickCount();
    if (now - last < 250) return;
    last = now;
    _snprintf(path, MAX_PATH, "%s\\sss_cmd.txt", g_dir);
    if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) return;
    f = fopen(path, "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        int a = 0, b = 0;
        if (sscanf(line, "spawn %i", &a) == 1) do_spawn(a);
        else if (!strncmp(line, "dump", 4)) do_dump();
        else if (sscanf(line, "rogue %i", &a) == 1) rg_start(a);
        else if (!strncmp(line, "hub", 3)) rg_f9();
        else if (!strncmp(line, "rgwin", 5)) rg_finish(1, "debug rgwin");
        else if (!strncmp(line, "rgstat ", 7)) { int at_ = 0, df_ = 0, hp_ = 0; unsigned char *pl_ = lead_obj(), *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL;      /* 測試:把主角設成某個強度(有效攻 / 防、命上限),用來單獨測頭目戰 */
            if (sb_ && sscanf(line + 7, "%i %i %i", &at_, &df_, &hp_) == 3) { I32(sb_, 0x60) += at_ - I32(sb_, 0x138); I32(sb_, 0x64) += df_ - I32(sb_, 0x13c); I32(sb_, 0x54) = hp_ + g_hpbuf; I32(sb_, 0x50) = hp_ + g_hpbuf; recalc_keep(sb_); g_last_hp = 0; mlog("rgstat: atk %d def %d hp %d", I32(sb_, 0x138), I32(sb_, 0x13c), I32(sb_, 0x54) - g_hpbuf); } }
        else if (sscanf(line, "rgevo %i %i", &a, &b) == 2) { unsigned char *pl_ = lead_obj(), *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL; if (sb_ && a >= 1 && a <= g_slot_n) evolve_apply(sb_, g_slot_id[a - 1], b); }      /* 測試:第 a 格換成招 b */
        else if (sscanf(line, "rgslot %i %i", &a, &b) == 2) { unsigned char *pl_ = lead_obj(), *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL;      /* 測試:第 a 格直接換成招 b(不算進化、不墊底) */
            if (sb_ && a >= 1 && a <= g_slot_n + 1 && a <= SLOT_MAX && (knows_move(sb_, b) || game_learn(sb_, 0, b))) { if (a > g_slot_n) slot_add(b); else g_slot_id[a - 1] = b; power_set(b); mlog("slot %d = move %d (slots %d)", a, b, g_slot_n); } }      /* a = 目前格數 + 1 → 加一格 */
        else if (!strncmp(line, "rgreset", 7)) { int i_; power_restore(); g_rank_n = 0; g_evo_nmap = 0; for (i_ = 0; i_ < g_slot_n; i_++) power_set(g_slot_id[i_]); mlog("rgreset: ranks / evolve floors cleared"); }      /* 測試:清掉這局的升階與進化墊底 */
        else if (sscanf(line, "rgrank %i %i", &a, &b) == 2) { if (a >= 1 && a <= g_slot_n) { rank_set(g_slot_id[a - 1], b); power_set(g_slot_id[a - 1]); mlog("rank: move %d -> lv %d", g_slot_id[a - 1], b); } }
        else if (sscanf(line, "rgcap %i", &a) == 1) { g_rg_cap = a; mlog("rogue cap %d", a); }
        else if (sscanf(line, "rgfx %i", &a) == 1) { g_fx_cap = a; mlog("fx cap %d", a); }
        else if (sscanf(line, "wasd %i", &a) == 1) { g_wasd = a; mlog("wasd %d", a); }
        else if (sscanf(line, "castfix %i", &a) == 1) { g_castfix = a; mlog("castfix=%d (refused %d, lock broken %d)", a, g_refuse_n, g_lockbreak_n); }
        else if (sscanf(line, "bot %i", &a) == 1) { g_bot = a; mlog("bot=%d (attack orders %d, right clicks %d)", a, g_bot_atk, g_bot_rc); }
        else if (sscanf(line, "manual %i", &a) == 1) { g_manual = a; mlog("manual mode=%d", a); }
        else if (sscanf(line, "castunlock %i", &a) == 1) { g_cast_unlock = a; mlog("cast unlock after %d ms", a); }
        else if (sscanf(line, "rghitcap %i %i", &a, &b) >= 1) { g_hitcap = a; if (b) g_dpscap = b; mlog("hit cap %d%% per-second cap %d%%", g_hitcap, g_dpscap); }
        else if (sscanf(line, "rgpt %i", &a) == 1) { rg_pt_remove(); g_anim_miss = 0; rg_pt_join(a); }      /* 測試:直接讓角色 a 當夥伴 */
        else if (sscanf(line, "rgdrop %i", &a) == 1) { while (a-- > 0) rg_drop(0); }
        else if (sscanf(line, "rgitem %i %i", &a, &b) == 2) { while (b-- > 0) it_add(a); meta_save(); rg_dirty = 1; mlog("rgitem: item %d now x%d", a, it_find(a) >= 0 ? M.it[it_find(a)].n : 0); }      /* 測試:直接給 b 件物品 a */
        else if (sscanf(line, "rgsynth %i", &a) == 1) { g_synth_pct = a; mlog("synth success %d%%", a); }
        else if (!strncmp(line, "meta", 4)) { if (sscanf(line + 4, " money %i", &a) == 1) M.money = a; else if (sscanf(line + 4, " cleared %i", &a) == 1) M.cleared = a; else if (sscanf(line + 4, " slots %i", &a) == 1) M.slots = a; else if (sscanf(line + 4, " lead %i", &a) == 1) { int j_; for (j_ = 0; j_ < M.nchars && M.chars[j_] != a; j_++); if (j_ == M.nchars && M.nchars < 16) M.chars[M.nchars++] = a; M.lead = a; } else { rg_cmd_meta(); continue; } meta_save(); rg_dirty = 1; rg_cmd_meta(); }
        else if (sscanf(line, "survtscale %i", &a) == 1) { DWORD ms_ = g_surv ? surv_ms() : 0; g_tscale = a < 1 ? 1 : a; if (g_surv) g_toff += ms_ - surv_ms(); mlog("time scale x%d", g_tscale); }
        else if (sscanf(line, "survtime %i", &a) == 1) { if (g_surv) { g_toff += a * 1000 - (int)surv_ms(); mlog("run clock -> %ds", a); } }
        else if (sscanf(line, "rgboss %i %i %i", &a, &b, &g_rgb3) == 3) { if (a >= 0 && a < RG_NMAP) { RG_MAPS[a].boss_type = b; RG_MAPS[a].boss_chr = g_rgb3; mlog("map %d boss: type %d char %d", a, b, g_rgb3); } }
        else if (sscanf(line, "rghpx %i %i", &a, &b) == 2) { g_boss_hpx = a; g_elite_hpx = b; mlog("boss hp x%d elite hp x%d", a, b); }
        else if (sscanf(line, "rgatkx %i", &a) == 1) { g_boss_atkx = a; mlog("boss atk x%d%%", a); }
        else if (sscanf(line, "cdef %i", &a) == 1) { unsigned char *d_ = NULL; char b_[1024]; int k_ = 0, i_; __try { d_ = game_chardef(a); for (i_ = 0; d_ && i_ < 80; i_++) { if (i_ % 8 == 0) { if (k_) mlog("%s", b_); k_ = sprintf(b_, "  cdef %d +%03x:", a, i_ * 4); } k_ += sprintf(b_ + k_, " %08x", I32(d_, i_ * 4)); } if (k_) mlog("%s", b_); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("cdef: fault"); } }
        else if (!strncmp(line, "goto_map ", 9)) goto_map_cmd(line + 9);
        else if (!strncmp(line, "maps", 4)) { a = 0; b = 999; sscanf(line + 4, "%i %i", &a, &b); do_maps(a, b); }
        else if (!strncmp(line, "where", 5)) map_report("where");
        else if (sscanf(line, "survatk %i", &a) == 1) { g_atk_mode = a; mlog("atk mode=%d (0 original stand / 1 walk+mod hit / 2 original swing+slide)", a); }
        else if (!strncmp(line, "survswing ", 10)) { sscanf(line + 10, "%i %i %i %i %i %i", &g_swing_ms, &g_swing_range, &g_swing_max, &g_swing_anim, &g_swing_hit, &g_swing_tgt);
            mlog("swing every %dms range %dpx max %d targets anim=%d hit=%d set_target=%d", g_swing_ms, g_swing_range, g_swing_max, g_swing_anim, g_swing_hit, g_swing_tgt); }
        else if (sscanf(line, "survslots %i", &a) == 1) { g_slots = a < 1 ? 1 : a > SLOT_MAX ? SLOT_MAX : a; mlog("skill slots=%d", g_slots); }
        else if (sscanf(line, "survcdbase %i %i", &a, &b) == 2) { g_cd_base = a; g_cd_per_mp = b; mlog("cd = %dms + mp*%dms", a, b); }
        else if (sscanf(line, "survcd %i %i", &a, &b) == 2) {      /* survcd 0 0 = 關掉 CD(回到 v1 的固定間隔);survcd 1 0 = 開;survcd ID MS = 指定某招的 CD */
            if (a <= 1 && !b) g_cd_on = a; else { int i_; for (i_ = 0; i_ < g_cd_novr && g_cd_ovr[i_].id != a; i_++); if (i_ < 16) { g_cd_ovr[i_].id = a; g_cd_ovr[i_].ms = b; if (i_ == g_cd_novr) g_cd_novr++; } }
            mlog("cd on=%d override move %d = %dms", g_cd_on, a, b); }
        else if (sscanf(line, "cast %i", &a) == 1) {      /* 測 CD 用:對最近的敵人放「會的第 a 招」(跟玩家右鍵走同一支原作函式) */
            unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL, *e = t ? find_enemy(t) : NULL, *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL;
            if (sb && e) __try { int mp0 = I32(sb, 0x58), bl = g_cd_block; I32(pl, 0x188) = 0; game_cast(pl, 0, a, I32(e, 0xc), 0, 0);
                mlog("cast %d: mp %d->%d %s", a, mp0, I32(sb, 0x58), g_cd_block > bl ? "BLOCKED (cooldown / not in a slot)" : I32(sb, 0x58) < mp0 ? "cast" : "not cast (busy / mp)"); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("cast: fault"); } }
        else if (!strncmp(line, "slots", 5)) { int i_; for (i_ = 0; i_ < g_slot_n; i_++) mlog("  slot %d: move %d cd=%dms ready_in=%dms", i_ + 1, g_slot_id[i_], move_cd_ms(g_slot_id[i_]), (int)(g_slot_ready[i_] - GetTickCount())); mlog("  slots %d/%d blocked=%d", g_slot_n, g_slots, g_cd_block); }
        else if (sscanf(line, "survpull %i", &a) == 1) { g_surv_pull = a; if (a) g_chase = 1; mlog("surv pull %d (chase=%d)", a, g_chase); }   /* survpull N = 切回推力版(v0) */
        else if (sscanf(line, "survchase %i", &a) == 1) { chase_report("switch"); memset(&g_cs, 0, sizeof g_cs); g_chase = a; mlog("chase mode=%d (0 off 1 push 2 walk 3 aggro)", a); }
        else if (sscanf(line, "survwalk %i %i", &a, &b) == 2) { g_walk_ms = a < 100 ? 100 : a; g_walk_hand = b; mlog("walk every %dms, handoff %dpx", g_walk_ms, g_walk_hand); }
        else if (sscanf(line, "survsnap %i", &a) == 1) { g_spawn_snap = a; mlog("spawn snap=%d", a); }
        else if (!strncmp(line, "chasedump", 9)) { unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL; int i, k = 0;
            for (i = 1; t && pl && i < OBJ_SLOTS && k < 40; i++) { unsigned char *o = t[i];
                if (!o || !IS_CHAR(o) || !(I32(o, 0x100) & 2) || !I32(o, 0xfc)) continue; k++;
                mlog("  e%d char=%d pos=(%d,%d) d=(%d,%d) cmd=%x tgt=%d ai=%x/%d dest=(%d,%d) busy=%d stuck=%d f7c=%x", i, I32(o, 0xfc), I32(o, 0x14) >> 16, I32(o, 0x18) >> 16,
                     (I32(pl, 0x14) - I32(o, 0x14)) >> 16, (I32(pl, 0x18) - I32(o, 0x18)) >> 16, I32(o, 0x134), I32(o, 0x13c), I32(o, 0x150), I32(o, 0x15c), I32(o, 0x140), I32(o, 0x144), I32(o, 0x188), g_ch[i].stuck, I32(o, 0x7c)); }
            mlog("  player id=%d pos=(%d,%d)", pl ? I32(pl, 0xc) : 0, pl ? I32(pl, 0x14) >> 16 : 0, pl ? I32(pl, 0x18) >> 16 : 0); }
        else if (sscanf(line, "survcap %i", &a) == 1) { g_surv_cap = a > 240 ? 240 : a; mlog("cap=%d", g_surv_cap); }
        else if (sscanf(line, "god %i", &a) == 1) { g_god = a; mlog("god=%d", a); }
        else if (sscanf(line, "survxp %i", &a) == 1) { g_xp_on = a; mlog("xp=%d", a); }
        else if (sscanf(line, "survaggro %i", &a) == 1) { g_aggro = a; mlog("aggro=%d cells", a); }
        else if (sscanf(line, "survr %i %i", &a, &b) == 2) { g_surv_rmin = a; g_surv_rmax = b; mlog("surv radius %d..%d", a, b); }
        else if (sscanf(line, "pick %i", &a) == 1) menu_pick(a);
        else if (!strncmp(line, "row ", 4)) {      /* row T0 STEP N CHAR:一排不同 type 的怪(認圖像用) */
            int t0 = 0, st = 1, n = 0, ch = 13, i; unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL;
            sscanf(line + 4, "%i %i %i %i", &t0, &st, &n, &ch);
            for (i = 0; pl && i < n; i++) __try {
                int *o = game_create(*GAME_OBJMGR, 0, I32(pl, 0xac), 0, I32(pl, 0x14) + ((-240 + (i % 8) * 68) << 16), I32(pl, 0x18) + ((i / 8 ? -90 : 90) << 16), I32(pl, 0x1c), t0 + i * st, 0);
                if (o) { game_faction(o, 0, 2, 0); game_setchar(o, 0, ch, 1); }
                mlog("row %d: type %d -> %p", i, t0 + i * st, o);
            } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("row %d: exception", i); }
        }
        else if (sscanf(line, "leash %i", &a) == 1) { g_leash = a; mlog("leash=%d", a); }
        else if (sscanf(line, "goto %i %i", &a, &b) == 2) { unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL;   /* 原作的「走到」0x445a80(wx, wy) */
            if (pl) __try { ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, a, b); mlog("goto %d,%d", a, b); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("goto: fault"); } }
        else if (sscanf(line, "tp %i %i", &a, &b) == 2) { unsigned char **t = obj_table(), *pl = t ? find_player(t) : NULL; if (pl) { obj_setpos(pl, (a << 16) >> 16, (b << 16) >> 16); mlog("tp %d,%d", a, b); } }
        else if (sscanf(line, "super %i", &a) == 1) { g_super = a; mlog("super=%d", a); }
        else if (sscanf(line, "auto %i", &a) == 1) { g_auto = a; mlog("auto=%d", a); }
        else if (sscanf(line, "castms %i", &a) == 1) { g_cast_ms = a; mlog("cast ms=%d", a); }
        else if (sscanf(line, "moves %i %i", &a, &b) == 2) do_defs(game_movedef, a, b, "move");
        else if (sscanf(line, "items %i %i", &a, &b) == 2) do_defs(game_itemdef, a, b, "item");
        else if (sscanf(line, "learn %i", &a) == 1) { unsigned char *pl = lead_obj(); if (!pl) { unsigned char **t = obj_table(); pl = t ? find_player(t) : NULL; }
            if (pl) __try { mlog("learn %d -> %d", a, game_learn(*(void **)(pl + 0x1a4), 0, a)); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("learn: fault"); } }
        else if (!strncmp(line, "surv", 4)) surv_toggle();
        else if (!strncmp(line, "post ", 5) && g_hwnd) {   /* 走真正的訊息佇列(會經過 my_wndproc),測熱鍵用 */
            unsigned m = 0, w = 0, l = 0; sscanf(line + 5, "%i %i %i", &m, &w, &l); PostMessageA(g_hwnd, m, w, l); mlog("post %x %x %x", m, w, l);
        }
        else if (!strncmp(line, "chars", 5)) do_chars();
        else if (sscanf(line, "god %i", &a) == 1) { g_god = a; mlog("god=%d", a); }
        else if (sscanf(line, "bench %i", &a) == 1) { g_bench_left = a; g_bench_n = 0; g_bench_sum = 0; g_bench_min = 9999; g_bench_maxms = 0; mlog("bench %d s start", a); }
        else if (!strncmp(line, "hp", 2)) { unsigned char **t = obj_table(); int i;
            for (i = 1; t && i < OBJ_SLOTS; i++) { unsigned char *o = t[i], *sb;
                if (!o || !IS_CHAR(o) || !I32(o, 0xfc) || !(sb = *(unsigned char **)(o + 0x1a4))) continue;
                mlog("  hp slot %d char=%d fac=%x hp=%d/%d mp=%d/%d", i, I32(o, 0xfc), I32(o, 0x100), I32(sb, 0x50), I32(sb, 0x54), I32(sb, 0x58), I32(sb, 0x5c)); } }
        else if (!strncmp(line, "types", 5)) do_types();
        else if (!strncmp(line, "peek ", 5)) {            /* peek ADDR N:印 N 個 dword(探索用) */
            unsigned ad = 0, n = 16, i; char buf[1024]; int k = 0;
            sscanf(line + 5, "%i %i", &ad, &n); if (n > 96) n = 96;
            __try { for (i = 0; i < n; i++) { if (i % 8 == 0) { if (k) mlog("%s", buf); k = sprintf(buf, "  %08x:", ad + i * 4); }
                        k += sprintf(buf + k, " %08x", *(unsigned *)(ad + i * 4)); } if (k) mlog("%s", buf);
            } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("peek: fault at %08x", ad); }
        }
        else if (!strncmp(line, "poke ", 5)) {
            unsigned ad = 0, v = 0; sscanf(line + 5, "%i %i", &ad, &v);
            __try { *(unsigned *)ad = v; mlog("poke %08x = %08x", ad, v); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("poke: fault"); }
        }
        else if (!strncmp(line, "who", 3)) { unsigned char **t = obj_table(); int i;
            for (i = 1; t && i < OBJ_SLOTS; i++) { unsigned char *o = t[i];
                if (o && IS_CHAR(o) && I32(o, 0xfc)) mlog("  who slot %d obj=%p sb=%p type=%d char=%d fac=%x class=%x pos=(%d,%d)", i, o, *(void **)(o + 0x1a4), I32(o, 4), I32(o, 0xfc), I32(o, 0x100), I32(o, 8), I32(o, 0x14) >> 16, I32(o, 0x18) >> 16); } }
        else if (sscanf(line, "key %i %i", &a, &b) >= 1) send_key(a, b ? b : 120);
        else if (sscanf(line, "click %i %i", &a, &b) == 2 && g_hwnd) {
            LPARAM lp = MAKELPARAM(a, b);
            CallWindowProcA(g_orig_wndproc, g_hwnd, WM_MOUSEMOVE, 0, lp);
            g_click_lp = lp; g_click_step = 1; mlog("click %d,%d", a, b);
        }
        else if ((!strncmp(line, "hold ", 5) || !strncmp(line, "rhold ", 6)) && g_hwnd) {
            int x = 0, y = 0, ms = 1000; sscanf(line + (line[0] == 'r' ? 6 : 5), "%i %i %i", &x, &y, &ms);
            g_hold_dn = line[0] == 'r' ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
            g_hold_lp = MAKELPARAM(x, y);
            CallWindowProcA(g_orig_wndproc, g_hwnd, WM_MOUSEMOVE, 0, g_hold_lp);
            CallWindowProcA(g_orig_wndproc, g_hwnd, g_hold_dn, g_hold_dn == WM_LBUTTONDOWN ? MK_LBUTTON : MK_RBUTTON, g_hold_lp);
            g_hold_until = GetTickCount() + ms; mlog("%s %d,%d %dms", line[0] == 'r' ? "rhold" : "hold", x, y, ms);
        }
        else if (!strncmp(line, "msg ", 4) && g_hwnd) {
            unsigned m = 0, w = 0, l = 0; sscanf(line + 4, "%i %i %i", &m, &w, &l);
            mlog("msg %x %x %x -> %x", m, w, l, (unsigned)CallWindowProcA(g_orig_wndproc, g_hwnd, m, w, l));
        }
        else if (sscanf(line, "move %i %i", &a, &b) == 2 && g_hwnd) CallWindowProcA(g_orig_wndproc, g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(a, b));
        else if (sscanf(line, "overlay %i", &a) == 1) g_overlay = a;
        else if (sscanf(line, "dbgline %i", &a) == 1) { g_dbgline = a; mlog("debug line %s", a ? "on" : "off"); }
        else if (!strncmp(line, "rec ", 4)) { int secs = 20, fps = 15; char nm[64] = "rec", path2[MAX_PATH];
            sscanf(line + 4, "%d %d %63s", &secs, &fps, nm); if (fps < 1) fps = 15;
            if (g_rec_f) { fclose(g_rec_f); g_rec_f = NULL; }
            _snprintf(path2, MAX_PATH, "%s\\%s.raw", g_dir, nm);
            if ((g_rec_f = fopen(path2, "wb")) != NULL) { setvbuf(g_rec_f, NULL, _IOFBF, 1 << 22); g_rec_n = 0; g_rec_iv = 1000 / fps; g_rec_next = GetTickCount(); g_rec_end = g_rec_next + secs * 1000; mlog("rec %ds @%dfps -> %s", secs, fps, path2); } }
        else if (!strncmp(line, "shot ", 5)) { char *e; _snprintf(g_shot, MAX_PATH, "%s\\%s", g_dir, line + 5);
                                               for (e = g_shot; *e; e++) if (*e == '\r' || *e == '\n') *e = 0; }
        else if (sscanf(line, "type %i char %i", &a, &b) == 2) { g_type_override = a; g_char_override = b; mlog("override type=%d char=%d", a, b); }
        else if (line[0] > ' ') mlog("cmd? %s", line);
    }
    fclose(f);
    DeleteFileA(path);
    if (g_shot[0] && g_menu && g_menu_drawn && g_last_src) { save_locked(g_last_src, g_shot); g_shot[0] = 0; }
    if (g_shot[0] && rg_ui && g_last_src && GAME_APP && GAME_APP[0x12]) { ui_present(); save_locked(g_last_src, g_shot); g_shot[0] = 0; }   /* 暫停中沒有新的一幀,直接讀上一幀 */
}

typedef BOOL (WINAPI *peek_t)(MSG *, HWND, UINT, UINT, UINT);
static peek_t g_orig_peek;
static BOOL WINAPI my_peek(MSG *m, HWND h, UINT a, UINT b, UINT rm) {
    BOOL r = g_orig_peek(m, h, a, b, rm);
    if (!r) g_peek_idle_sec++;
    /* 原作在訊息迴圈裡自己把 WM_KEYDOWN 的掃描碼記進 0x4c78a0(0x46f240),熱鍵都看那張表 → 一局中 Q(換戰友)/ E(收放戰友)在這裡先改成 WM_NULL,原作就看不到。 */
    if (r && m && (rm & PM_REMOVE) && g_surv && g_manual && !g_menu && (m->message == WM_KEYDOWN || m->message == WM_KEYUP)) {
        const char *q = m->wParam ? strchr(MAN_KEYS, (int)m->wParam) : NULL;
        if (q) { g_key_blocked++; m->message = WM_NULL; }
        else if (m->wParam >= '1' && m->wParam <= '9' && !rg_ui) { if (m->message == WM_KEYDOWN && (int)m->wParam - '1' < g_slot_n) { g_sel_slot = (int)m->wParam - '1'; g_sel_n++; } m->message = WM_NULL; }      /* 數字鍵:選技能格的招 */
    }
    key_tick();
    god_tick();
    rg_tick();
    cur_tick();
    surv_tick();
    map_tick();
    cmd_tick();
    second_tick();
    return r;
}

/* ---- 視野放大(實驗;notes/11)--------------------------------------------------------------
 * game/sss_view.txt 寫「寬 高」(例 800 600)才啟用。原作畫面大小散在四組變數 + 一些寫死的立即數,這裡改掉戰鬥地圖用得到的那幾處。
 * 每一處先比對原本的位元組,不對就整組不改。 */
static int g_view_w, g_view_h;
static int view_put(unsigned addr, const unsigned char *expect, int n, int off, unsigned val, int apply) {
    unsigned char *p = (unsigned char *)addr; DWORD old;
    if (memcmp(p, expect, n)) { mlog("view: unexpected bytes at %x", addr); return 0; }
    if (!apply) return 1;
    if (!VirtualProtect(p, n + 4, PAGE_EXECUTE_READWRITE, &old)) return 0;
    *(unsigned *)(p + off) = val; VirtualProtect(p, n + 4, old, &old);
    return 1;
}
static void view_patch(void) {
    char path[MAX_PATH]; FILE *f; int W = 0, H = 0, pass, ok = 1;
    static const unsigned char e_push280[] = { 0x68, 0x80, 0x02, 0x00, 0x00 }, e_push1e0[] = { 0x68, 0xe0, 0x01, 0x00, 0x00 }, e_push186[] = { 0x68, 0x86, 0x01, 0x00, 0x00 },
        e_push140[] = { 0x68, 0x40, 0x01, 0x00, 0x00 }, e_push11d[] = { 0x68, 0x1d, 0x01, 0x00, 0x00 }, e_buf[] = { 0x68, 0x00, 0x60, 0x09, 0x00 }, e_cnt[] = { 0xb9, 0x00, 0x58, 0x02, 0x00 },
        e_appw[] = { 0xc7, 0x05, 0x18, 0x43, 0x4b, 0x00, 0x80, 0x02, 0x00, 0x00 }, e_apph[] = { 0xc7, 0x05, 0x1c, 0x43, 0x4b, 0x00, 0xe0, 0x01, 0x00, 0x00 },
        e_inw[] = { 0xc7, 0x05, 0x30, 0x7e, 0x4b, 0x00, 0x80, 0x02, 0x00, 0x00 }, e_inh[] = { 0xc7, 0x05, 0x34, 0x7e, 0x4b, 0x00, 0xe0, 0x01, 0x00, 0x00 },
        e_cullw[] = { 0x81, 0xc6, 0x48, 0x03, 0x00, 0x00 }, e_cullh[] = { 0x05, 0xa8, 0x02, 0x00, 0x00 }, e_w[] = { 0x80, 0x02, 0x00, 0x00 }, e_h[] = { 0xe0, 0x01, 0x00, 0x00 };
    _snprintf(path, MAX_PATH, "%s\\sss_view.txt", g_dir);
    if (!(f = fopen(path, "r"))) return;
    fscanf(f, "%d %d", &W, &H); fclose(f);
    if (W < 640 || H < 480 || W > 1600 || H > 960 || (W == 640 && H == 480)) { mlog("view: ignored %dx%d (640x480 .. 1600x960)", W, H); return; }
    for (pass = 0; pass < 2 && ok; pass++) {        /* 第一輪只比對,全部對才真的改 */
        ok &= view_put(0x423586, e_push280, 5, 1, W, pass);          ok &= view_put(0x423581, e_push1e0, 5, 1, H, pass);
        ok &= view_put(0x4235c4, e_appw, 10, 6, W, pass);            ok &= view_put(0x4235ce, e_apph, 10, 6, H, pass);
        ok &= view_put(0x4b83e8, e_w, 4, 0, W, pass);                ok &= view_put(0x4b83ec, e_h, 4, 0, H, pass);
        ok &= view_put(0x47fc21, e_buf, 5, 1, W * H * 2, pass);      ok &= view_put(0x47fc30, e_cnt, 5, 1, W * H / 2, pass);
        ok &= view_put(0x46f393, e_inw, 10, 6, W, pass);             ok &= view_put(0x46f39d, e_inh, 10, 6, H, pass);
        ok &= view_put(0x41b125, e_push280, 5, 1, W, pass);          ok &= view_put(0x41b120, e_push186, 5, 1, H - 90, pass);
        ok &= view_put(0x43c4f3, e_push140, 5, 1, W / 2, pass);      ok &= view_put(0x43c4e9, e_push11d, 5, 1, (H - 90) / 2 + 90, pass);
        ok &= view_put(0x454e49, e_cullw, 6, 2, W + 200, pass);      ok &= view_put(0x454e57, e_cullh, 5, 1, H + 200, pass);
        ok &= view_put(0x4aaba4, e_w, 4, 0, W, pass);                ok &= view_put(0x4aaba8, e_h, 4, 0, H, pass);
    }
    if (ok) { g_view_w = W; g_view_h = H; }
    mlog("view: %dx%d %s", W, H, ok ? "patched (18 places)" : "NOT applied");
}

/* ---- init ---------------------------------------------------------------------------- */
BOOL WINAPI DllMain(HINSTANCE inst, DWORD why, LPVOID r) {
    if (why == DLL_PROCESS_ATTACH) {
        char exe[MAX_PATH], *s, logp[MAX_PATH];
        DisableThreadLibraryCalls(inst);
        GetModuleFileNameA(NULL, exe, MAX_PATH);
        strcpy(g_dir, exe);
        if ((s = strrchr(g_dir, '\\')) != NULL) *s = 0;
        _snprintf(logp, MAX_PATH, "%s\\sss_mod.log", g_dir);
        g_log = fopen(logp, "a");
        mlog("==== sssmod loaded into %s", exe);
        AddVectoredExceptionHandler(1, crash_veh);
        QueryPerformanceFrequency(&g_qf); QueryPerformanceCounter(&g_sec_start);
        s = strrchr(exe, '\\');
        if (s && !_stricmp(s + 1, "WUJIN.exe")) {
            /* 確認 IAT 槽真的指向預期的函式再改,避免版本不同亂寫 */
            void **rc = (void **)0x4aa23c, **pk = (void **)0x4aa278, **dc = (void **)0x4aa048;
            HMODULE u = GetModuleHandleA("user32.dll");
            if (*rc == (void *)GetProcAddress(u, "RegisterClassA") && *pk == (void *)GetProcAddress(u, "PeekMessageA")) {
                g_orig_regclass = (regclass_t)patch_ptr(rc, (void *)my_regclass);
                g_orig_peek = (peek_t)patch_ptr(pk, (void *)my_peek);
                g_orig_ddc = (ddc_t)patch_ptr(dc, (void *)my_ddcreate);
                mlog("IAT hooks installed (RegisterClassA, PeekMessageA, DirectDrawCreate)");
                view_patch();
            } else {
                mlog("IAT mismatch: rc=%p pk=%p — wrong WUJIN version? hooks NOT installed", *rc, *pk);
            }
        }
    }
    return TRUE;
}
