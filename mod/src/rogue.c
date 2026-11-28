/* rogue.c —— Roguelite 倖存者(notes/09 起):一局流程(計時 / 精英 / 頭目 / 結算)、永久進度 sss_meta.json、「倖存者」選單。
 * 由 sssmod.c 在 cmd_tick 前 #include(同一個編譯單元,直接用前面的 static)。 */

/* ---- 地圖(關卡)表 ------------------------------------------------------------------------
 * stage = stage.ini 的 id(原作可能轉成 +200 的劇情版本);頭目 = 原作角色表 def[0]&8 的角色。
 * boss_type = 頭目的圖像 type(要是原作的配對,亂配會閃退);0 = 還不知道 → 用強化精英頂替。 */
typedef struct { int stage; const wchar_t *name; int boss_chr, boss_type; } rg_map_t;
static rg_map_t RG_MAPS[] = {       /* 頭目的角色 / 圖像配對是掃圖時從場上物件抄來的(括號 = 在哪張圖看到),見 notes/09 */
    {   8, L"三重天通道",   63, 2084 },      /* 盲劍眼不見(天脊峰 214) */
    {   5, L"一重天",       57, 2085 },      /* 聾劍耳不聞(青龍樓 21) */
    {  32, L"兵器塚",       71, 2076 },      /* 劍無言(鳴劍山莊通道 266) */
    {  30, L"百草山",       58, 2095 },      /* 汗血獸(百草山 230,就是這張圖的) */
    {  38, L"李家莊",       60, 2094 },      /* 魔耳妖鳥(迷霧林 254) */
    {  35, L"冷香湖叉口",   61, 2093 },      /* 邪眼狂牛(迷霧林 255) */
    {  45, L"果報谷內部一", 70, 2014 },      /* 黑白郎君(果報谷廣場 43) */
    { 120, L"魔魁之墓內部", 67, 2109 },      /* 魔魁(神秘基地 46) */
};
#define RG_NMAP ((int)(sizeof RG_MAPS / sizeof RG_MAPS[0]))
#define RG_T_ELITE  120000
#define RG_T_BOSS   480000
#define RG_T_ENRAGE 600000

/* ---- 永久進度(game/sss_meta.json;不碰原作存檔)-------------------------------------------- */
#define RG_MAXITEM 48
#define RG_MAXPAS  16
static struct {
    int money, cleared, slots, lead, runs, wins;
    int chars[16], nchars;                          /* 擁有的角色(char id) */
    struct { int id, lv, n, af[3]; } it[RG_MAXITEM]; int nit;   /* 裝備:物品 id、等級、有幾件、附加屬性(最多 3 條,0 = 空) */
    int eq[9][4];                                   /* 每個角色各一套四格裝備(eq[角色 id][格];0 = 基本的那件) */
    int pt[9];                                      /* 每個角色出戰時帶的夥伴(角色 id;0 = 不帶) */
    int pas[RG_MAXPAS], npas;                       /* 擁有的被動技能 */
    int pas_eq[2];                                  /* [0] 主角 [1] 夥伴 裝的被動(0 = 沒裝) */
} M;
static const char *jfind(const char *b, const char *key) {
    char k[40]; const char *p; _snprintf(k, 40, "\"%s\"", key);
    p = strstr(b, k); if (!p) return NULL;
    p = strchr(p + strlen(k), ':'); return p ? p + 1 : NULL;
}
static int jint(const char *b, const char *key, int def) { const char *p = jfind(b, key); return p ? (int)strtol(p, NULL, 10) : def; }
static int jarr(const char *b, const char *key, int *out, int max) {
    const char *p = jfind(b, key); int n = 0; char *e;
    if (!p || !(p = strchr(p, '['))) return 0;
    for (p++; n < max; ) {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') p++;
        if (*p == ']' || !*p) break;
        out[n++] = (int)strtol(p, &e, 10); if (e == p) break; p = e;
    }
    return n;
}
static void meta_save(void) {
    char path[MAX_PATH], tmp[MAX_PATH]; FILE *f; int i;
    _snprintf(path, MAX_PATH, "%s\\sss_meta.json", g_dir); _snprintf(tmp, MAX_PATH, "%s\\sss_meta.json.tmp", g_dir);
    if (!(f = fopen(tmp, "w"))) { mlog("meta: cannot write %s", tmp); return; }
    fprintf(f, "{\n  \"v\": 2,\n  \"money\": %d,\n  \"cleared\": %d,\n  \"slots\": %d,\n  \"lead\": %d,\n  \"runs\": %d,\n  \"wins\": %d,\n  \"chars\": [", M.money, M.cleared, M.slots, M.lead, M.runs, M.wins);
    for (i = 0; i < M.nchars; i++) fprintf(f, "%s%d", i ? ", " : "", M.chars[i]);
    fprintf(f, "],\n  \"items\": [");
    for (i = 0; i < M.nit; i++) fprintf(f, "%s%d, %d, %d", i ? ",  " : "", M.it[i].id, M.it[i].lv, M.it[i].n);
    fprintf(f, "],\n  \"afx\": [");
    { int k_ = 0; for (i = 0; i < M.nit; i++) if (M.it[i].af[0] || M.it[i].af[1] || M.it[i].af[2]) fprintf(f, "%s%d, %d, %d, %d", k_++ ? ",  " : "", M.it[i].id, M.it[i].af[0], M.it[i].af[1], M.it[i].af[2]); }
    fprintf(f, "],\n  \"eqc\": [");
    for (i = 0; i < 32; i++) fprintf(f, "%s%d", i ? ", " : "", M.eq[1 + i / 4][i % 4]);
    fprintf(f, "],\n  \"ptc\": [");
    for (i = 0; i < 8; i++) fprintf(f, "%s%d", i ? ", " : "", M.pt[1 + i]);
    fprintf(f, "],\n  \"pas\": [");
    for (i = 0; i < M.npas; i++) fprintf(f, "%s%d", i ? ", " : "", M.pas[i]);
    fprintf(f, "],\n  \"pas_eq\": [%d, %d]\n}\n", M.pas_eq[0], M.pas_eq[1]);
    fclose(f);
    MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING);
}
static void meta_load(void) {
    static char buf[16384]; char path[MAX_PATH]; FILE *f; int n = 0, tmp[RG_MAXITEM * 3], i, k;
    memset(&M, 0, sizeof M); M.slots = 3; M.lead = 1; M.chars[0] = 1; M.nchars = 1;
    for (i = 1; i <= 8; i++) M.pt[i] = i == 2 ? 1 : 2;      /* 預設夥伴:青陽子(原作開局的夥伴);青陽子自己出戰時帶素還真 */
    _snprintf(path, MAX_PATH, "%s\\sss_meta.json", g_dir);
    if ((f = fopen(path, "r")) != NULL) { n = (int)fread(buf, 1, sizeof buf - 1, f); fclose(f); }
    buf[n] = 0;
    if (n) {
        M.money = jint(buf, "money", 0); M.cleared = jint(buf, "cleared", 0); M.slots = jint(buf, "slots", 3); M.lead = jint(buf, "lead", 1);
        M.runs = jint(buf, "runs", 0); M.wins = jint(buf, "wins", 0);
        k = jarr(buf, "chars", M.chars, 16); M.nchars = k ? k : 1; if (!k) M.chars[0] = 1;
        k = jarr(buf, "items", tmp, RG_MAXITEM * 3) / 3;
        for (i = 0; i < k; i++) { M.it[i].id = tmp[i * 3]; M.it[i].lv = tmp[i * 3 + 1]; M.it[i].n = tmp[i * 3 + 2]; } M.nit = k;
        { static int a[RG_MAXITEM * 4]; int na = jarr(buf, "afx", a, RG_MAXITEM * 4) / 4, j;
          for (i = 0; i < na; i++) for (j = 0; j < M.nit; j++) if (M.it[j].id == a[i * 4]) { M.it[j].af[0] = a[i * 4 + 1]; M.it[j].af[1] = a[i * 4 + 2]; M.it[j].af[2] = a[i * 4 + 3]; } }
        if (M.lead < 1 || M.lead > 7) M.lead = 1;
        { int e[32], q[8], ne = jarr(buf, "eqc", e, 32), nq = jarr(buf, "ptc", q, 8);
          for (i = 0; i < ne; i++) M.eq[1 + i / 4][i % 4] = e[i];
          if (!ne) jarr(buf, "eq", M.eq[M.lead], 4);      /* 舊檔(全隊共用一套)→ 算在當時出戰的角色身上 */
          for (i = 0; i < nq; i++) M.pt[1 + i] = q[i]; }
        M.npas = jarr(buf, "pas", M.pas, RG_MAXPAS); jarr(buf, "pas_eq", M.pas_eq, 2);
    }
    if (M.money < 0) M.money = 0; if (M.cleared < 0) M.cleared = 0; if (M.cleared > RG_NMAP) M.cleared = RG_NMAP;
    if (M.slots < 3) M.slots = 3; if (M.slots > 6) M.slots = 6;
    mlog("meta: %s money=%d cleared=%d slots=%d lead=%d chars=%d items=%d passives=%d runs=%d wins=%d", n ? "loaded" : "new", M.money, M.cleared, M.slots, M.lead, M.nchars, M.nit, M.npas, M.runs, M.wins);
}
static int cost_geo(int base, double mul, int n) { double v = base; while (n-- > 0) v *= mul; return (int)(v + 0.5); }
static int cost_slot(void) { return M.slots >= 6 ? 0 : cost_geo(300, 2.5, M.slots - 3); }

/* ---- 一局的狀態 ---------------------------------------------------------------------------- */
enum { RG_OFF, RG_LOAD, RG_GOTO, RG_SETUP, RG_PLAY, RG_RESULT, RG_HOME };
enum { UI_NONE, UI_RESULT, UI_HUB };
static int rg_state, rg_map, rg_ui, rg_dirty, rg_tab = 1, rg_sel_map;
static DWORD rg_t;
static int rg_elite_id[4], rg_elite_n, rg_boss_id, rg_boss_on, rg_boss_chr, rg_boss_hpmax, rg_boss_try, rg_enraged, rg_bonus, rg_tier_i;
static int rg_seen[OBJ_SLOTS];
static int rg_ranged_alive, rg_tpl_ranged[SURV_MAX_TPL], g_ranged_cap = 10;      /* 遠程怪(stat+0x78 > 0)場上最多幾隻:牠們停在射程外不靠近,主角砍不到,會越積越多把場上名額佔滿 */
static int g_boss_hpx = 22, g_elite_hpx = 5, g_mp_drop = 8, g_boss_atkx = 130;
static int g_rg_cap = 35, g_mob_atkx = 130, g_fx_cap = 60, rg_fx_alive, rg_fx_max, rg_fx_culled, rg_drop_alive;      /* 場上怪上限(原本 80;57 隻同時打主角時特效物件 183 個、44 fps)→ 少而強:上限 50、小怪攻 ×1.3 */
static short *rg_ai_p, rg_ai_cast;
static struct { int id, base; } rg_bmove[12]; static int rg_nbmove, g_boss_skill = 25;      /* 頭目的招:威力壓到 g_boss_skill%(實測一發 252,主角命 600;跟頭目的攻擊力無關),結束還原 */
static int g_hud_refresh = 1;
static struct { int win, kills, money, clear, total, unlocked, pas; DWORD ms; wchar_t drops[160]; } rg_res;
static HFONT g_font_s, g_font_b, g_font_n;

static int rg_in_stage(void) { unsigned char *gs = GAME_GS; return gs && *(void **)(gs + 0x10); }
static unsigned char *rg_obj(int id) {
    unsigned char *o = NULL;
    if (!id || !*GAME_OBJMGR) return NULL;
    __try { o = (unsigned char *)((unsigned (__fastcall *)(void *, int, int))0x44f000)(*GAME_OBJMGR, 0, id); } __except (EXCEPTION_EXECUTE_HANDLER) { o = NULL; }
    return o;
}
static void chr_name(int chr, wchar_t *out, int n) {
    unsigned char *d = NULL; out[0] = 0;
    __try { d = game_chardef(chr); if (d) MultiByteToWideChar(950, 0, (const char *)d + 4, -1, out, n); } __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
    out[n - 1] = 0;
}

/* 怪的數值正規化:不管原作這隻怪多強,都壓到「第幾張圖 × 這局過了多久」的曲線上(原作自帶的怪也一樣 → 中後期的圖不會秒殺 Lv 1 主角)。
 * kind:0 小怪 / 1 精英(血 ×5)/ 2 頭目。只對每隻做一次(rg_seen 記 slot → obj id)。 */
static void rg_norm(unsigned char *o, int kind) {
    unsigned char *sb = *(unsigned char **)(o + 0x1a4); DWORD ms = surv_ms();
    double d = (ms > RG_T_ENRAGE ? RG_T_ENRAGE : ms) / (double)RG_T_BOSS, fh, fa, hp, atk; int m = rg_map, def, lv, ex;
    if (!sb) return;
    fh = I32(sb, 0x54) / 35.0; if (fh < 0.6) fh = 0.6; if (fh > 1.6) fh = 1.6;
    fa = I32(sb, 0x138) / 30.0; if (fa < 0.75) fa = 0.75; if (fa > 1.35) fa = 1.35;
    hp = (12 + 6 * m) * (1 + 4 * d) * fh; atk = (6 + 3 * m) * (1 + 3 * d) * fa + 0.6 * (g_lv > 10 ? 10 : g_lv);      /* 等級那項封頂 10 級:右鍵連發後升級很快(實測 6:50 就 Lv 21),不封頂會把怪養到主角 50 秒內倒下 */ ;      /* 第 1 張圖:命 12 → 60、攻 6 → 24(8:00);每往後一張 +6 / +3。再加 主角等級 × 0.6:不然防禦一路疊上去,怪每下只打 1 */
    def = 2 + 2 * m + (int)(10 * d); if (I32(sb, 0x13c) < def) def = I32(sb, 0x13c);
    lv = 1 + 2 * m + (int)(8 * d); if (I32(sb, 0x40) < lv) lv = I32(sb, 0x40);
    ex = 0;      /* 原作經驗給 0:原作升級會把命上限重算回表上的值(墊高的命就沒了,實測 6 秒被判倒下),等級也不該留 → 這局的成長只靠模組的 3 選 1 */
    if (kind == 0) atk *= g_mob_atkx / 100.0;
    if (kind == 1) { hp *= g_elite_hpx; atk *= 1.3; }
    if (kind == 2) { hp *= g_boss_hpx; atk *= g_boss_atkx / 100.0; def += 4; lv += 3; }
    if (hp < 1) hp = 1; if (atk < 1) atk = 1;
    I32(sb, 0x54) = (int)hp; I32(sb, 0x50) = (int)hp; I32(sb, 0x138) = (int)atk; I32(sb, 0x60) = (int)atk;
    I32(sb, 0x13c) = def; I32(sb, 0x64) = def; I32(sb, 0x40) = lv; I32(sb, 0x44) = ex;
}
static void rg_mark(unsigned char *o) { unsigned char **t = obj_table(); int i; if (t) for (i = 1; i < OBJ_SLOTS; i++) if (t[i] == o) { rg_seen[i] = I32(o, 0xc); return; } }
static void rg_norm_scan(unsigned char **t) {
    int i, nr = 0;
    for (i = 1; i < OBJ_SLOTS; i++) { unsigned char *o = t[i]; unsigned char *sb_;
        if (!o || !IS_CHAR(o) || !(I32(o, 0x100) & 2) || !I32(o, 0xfc)) continue;
        sb_ = *(unsigned char **)(o + 0x1a4); if (sb_ && I32(sb_, 0x50) > 0 && I32(sb_, 0x78) > 0) nr++;
        if (rg_seen[i] != I32(o, 0xc)) { rg_seen[i] = I32(o, 0xc); rg_norm(o, 0); } }
    rg_ranged_alive = nr;
}

/* 擋掉一局中的換地圖(主角走進出口會被原作換圖 → 這局就斷了):掛在兩支請求函式開頭。
 * 兩支開頭都是 push esi / mov esi,ecx / call 0x427e80(相對位址)→ 跳板自己重編那個 call。 */
static void *rg_tramp_gw, *rg_tramp_xy, *rg_tramp_ex; static int rg_block_n, rg_unfreeze, rg_unstuck_n, rg_crowd_n, g_unstuck_on = 0;      /* 卡住看守:預設關 —— Kirin 實玩放招時會被它一直瞬移(放招時走路指令還在、位置不動,被誤判成卡住) */
/* 主角自己走進出口走的是第三支 0x419a40(scene, 出口索引):它直接寫排程欄位,不經過上面兩支(實測 Kirin 走到地圖邊就被換圖,blocked_exits=0)。開頭的位元組跟另外兩支一樣。 */
/* 呼叫它的 0x43e8c0 接著會把主角 / 夥伴的 obj+0x7c 設上 0x40000000(換圖中,不准動)→ 一局中每一幀都把這個旗標清掉(只清一次不夠:站在出口上每一幀都會再設),不然兩個人就凍在原地(實測)。 */
static void __fastcall rg_hook_ex(void *sc, int edx_, int idx) {
    if (g_rg) { rg_block_n++; rg_unfreeze = 1;
        __try {      /* 站在出口上原作每一幀都會再觸發一次(實測:五絕神功跳躍落在出口上,主角被釘住)→ 往地圖中央推開 32 px */
            unsigned char *gs_ = GAME_GS, *pl_ = gs_ ? *(unsigned char **)(gs_ + 8) : NULL, *st_ = pl_ ? *(unsigned char **)(pl_ + 0xac) : NULL;
            if (pl_ && st_) { int px_ = I32(pl_, 0x14) >> 16, py_ = I32(pl_, 0x18) >> 16, cx_ = I32(st_, 0x790) / 2, cy_ = I32(st_, 0x794) / 2, dx_ = cx_ - px_, dy_ = cy_ - py_; double d_ = sqrt((double)dx_ * dx_ + (double)dy_ * dy_);
                if (d_ > 1) obj_addpos(pl_, (int)(dx_ / d_ * 32 * 65536), (int)(dy_ / d_ * 32 * 65536)); }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        return; }
    ((void (__fastcall *)(void *, int, int))rg_tramp_ex)(sc, 0, idx);
}
static void __fastcall rg_hook_gw(void *sc, int edx_, int stage, int gw) {
    if (g_rg) { rg_block_n++; return; }
    ((void (__fastcall *)(void *, int, int, int))rg_tramp_gw)(sc, 0, stage, gw);
}
static void __fastcall rg_hook_xy(void *sc, int edx_, int stage, int x, int y) {
    if (g_rg) { rg_block_n++; return; }
    ((void (__fastcall *)(void *, int, int, int, int))rg_tramp_xy)(sc, 0, stage, x, y);
}
static void *rg_hook_head8(unsigned char *fn, void *hook) {
    static const unsigned char head[] = { 0x56, 0x8b, 0xf1, 0xe8 }; unsigned char *t; DWORD old; int tgt;
    if (memcmp(fn, head, 4)) { mlog("map hook: unexpected bytes at %p", fn); return NULL; }
    tgt = (int)(fn + 8) + *(int *)(fn + 4);
    t = (unsigned char *)VirtualAlloc(NULL, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!t || !VirtualProtect(fn, 8, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    t[0] = 0x56; t[1] = 0x8b; t[2] = 0xf1; t[3] = 0xe8; *(int *)(t + 4) = tgt - (int)(t + 8);
    t[8] = 0xe9; *(int *)(t + 9) = (int)(fn + 8) - (int)(t + 13);
    fn[0] = 0xe9; *(int *)(fn + 1) = (int)hook - (int)(fn + 5); fn[5] = fn[6] = fn[7] = 0x90;
    VirtualProtect(fn, 8, old, &old); FlushInstructionCache(GetCurrentProcess(), fn, 8);
    return t;
}
/* 設動作(0x440ae0 / 0x440bb0)查不到那個動作的動畫 → 原作直接讀 NULL 當掉(「圖像和數值亂配會閃退」就是這個;
 * 模組生的夥伴照青陽子的 AI 出招、但那個圖像沒有那個動作,實測當在 0x440b35)。兩個呼叫點改成:查不到就退回「站立」的動畫。 */
static int g_anim_miss;
static void *__cdecl rg_anim_find(void *set, const char *name) {
    void *a = ((void *(__cdecl *)(void *, const char *))0x477650)(set, name);
    if (!a) { if (g_anim_miss++ < 8) mlog("anim missing: \"%s\" -> stand", name ? name : "?"); a = ((void *(__cdecl *)(void *, const char *))0x477650)(set, *(const char **)0x4b5020); }
    return a;
}
static void rg_patch_call(unsigned site, unsigned expect, void *fn) {
    unsigned char *p = (unsigned char *)site; DWORD old;
    if (p[0] != 0xe8 || (unsigned)(site + 5 + *(int *)(p + 1)) != expect) { mlog("call patch: unexpected bytes at %x", site); return; }
    if (VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &old)) { *(int *)(p + 1) = (int)fn - (int)(site + 5); VirtualProtect(p, 5, old, &old); FlushInstructionCache(GetCurrentProcess(), p, 5); }
}
/* 標題畫面停太久,原作的腳本會播開場動畫(腳本指令「播動畫」0x43ec10 → app 的播放 0x422eb0;Kirin 2026-11-26:「選單停太久又會跳開場動畫」)。
 * 倖存者選單開著時不播;其他時候照原作。 */
static int g_movie_skip;
static void __fastcall rg_movie(void *app, int edx_, const char *name) {
    if (rg_ui || rg_state == RG_LOAD) { if (g_movie_skip++ < 20) mlog("movie skipped (survivor menu open / run loading): %s", name ? name : "?"); return; }
    ((void (__fastcall *)(void *, int, const char *))0x422eb0)(app, 0, name);
}
static void rg_movie_hook(void) { static int done; if (!done) { done = 1; rg_patch_call(0x43ec1c, 0x422eb0, (void *)rg_movie); } }      /* 選單一打開就要裝(標題畫面還沒開過局) */
static void rg_hooks(void) {
    static int done; if (done) return; done = 1;
    rg_patch_call(0x440b27, 0x477650, (void *)rg_anim_find); rg_patch_call(0x440be1, 0x477650, (void *)rg_anim_find);
    rg_movie_hook();
    rg_tramp_gw = rg_hook_head8((unsigned char *)0x419980, (void *)rg_hook_gw);
    rg_tramp_xy = rg_hook_head8((unsigned char *)0x4198a0, (void *)rg_hook_xy);
    rg_tramp_ex = rg_hook_head8((unsigned char *)0x419a40, (void *)rg_hook_ex);
    mlog("map-change hooks: gw=%p xy=%p exit=%p", rg_tramp_gw, rg_tramp_xy, rg_tramp_ex);
}

/* ---- 裝備(跨局保留;同一件兩件 + 錢 → 升一級)---------------------------------------------
 * 用原作的物品:武器 1xx(素還真用劍)、防具 2xx、髮簪 35x、戒指 30x;四格 = stat+0x84 / +0x88 / +0x8c / +0x90。
 * 等級:每升一級,那件裝備的數值 +25%(加在基礎數值上)。 */
static const int EQ_POOL[4][16] = {
    { 101, 102, 104, 106, 109, 112, 115, 0 },
    { 201, 202, 203, 204, 205, 206, 208, 209, 210, 211, 212, 213, 215, 0 },
    { 351, 352, 353, 354, 355, 356, 0 },
    { 301, 302, 303, 304, 305, 306, 307, 0 },
};
static const wchar_t *EQ_SLOTNAME[4] = { L"武器", L"防具", L"髮簪", L"戒指" };
static int eq_slot(int id) { return id >= 100 && id < 200 ? 0 : id >= 200 && id < 300 ? 1 : id >= 350 && id < 400 ? 2 : id >= 300 && id < 350 ? 3 : -1; }
static int it_find(int id) { int i; for (i = 0; i < M.nit; i++) if (M.it[i].id == id) return i; return -1; }
static int it_add(int id) {
    int i = it_find(id), s = eq_slot(id);
    if (i < 0) { if (M.nit >= RG_MAXITEM) return -1; i = M.nit++; M.it[i].id = id; M.it[i].lv = 1; M.it[i].n = 0; M.it[i].af[0] = M.it[i].af[1] = M.it[i].af[2] = 0; }
    M.it[i].n++;
    if (s >= 0 && !M.eq[M.lead][s]) M.eq[M.lead][s] = id;           /* 那一格還空著就直接裝上 */
    return i;
}
static int cost_item(int lv) { return lv >= 10 ? 0 : cost_geo(50, 1.8, lv - 1); }
static void it_stats(int id, int lv, int *atk, int *def, int *hp, int *mp, int *res) {
    unsigned char *d = game_itemdef(id); int k = 100 + 25 * (lv - 1);
    *atk = *def = *hp = *mp = *res = 0; if (!d) return;
    __try { *hp = I32(d, 0xb8) * k / 100; *mp = I32(d, 0xc0) * k / 100; *atk = I32(d, 0xc4) * k / 100; *def = I32(d, 0xc8) * k / 100; *res = I32(d, 0xcc) * k / 100; } __except (EXCEPTION_EXECUTE_HANDLER) { }
}
static void it_label(int id, int lv, wchar_t *out, int n) {
    wchar_t nm[40]; int a, d, h, m, r, k;
    def_name(game_itemdef, id, nm, 40); it_stats(id, lv, &a, &d, &h, &m, &r);
    k = _snwprintf(out, n, L"%s Lv%d ", nm, lv);
    if (a) k += _snwprintf(out + k, n - k, L" 攻+%d", a); if (d) k += _snwprintf(out + k, n - k, L" 防+%d", d);
    if (h) k += _snwprintf(out + k, n - k, L" 命+%d", h); if (m) k += _snwprintf(out + k, n - k, L" 內+%d", m); if (r) k += _snwprintf(out + k, n - k, L" 抗+%d", r);
    out[n - 1] = 0;
}

/* ---- 裝備頁的三種操作(Kirin 2026-11-26:「打到一大堆一樣的裝備」→ 增加刷寶樂趣;三個都要花錢)----
 * 升級:同一件 2 件 → 消耗 1 件,等級 +1(每級數值 +25%)
 * 合成:同一件 2 件 → 兩件都消耗;g_synth_pct% 變成同部位高一階的另一件裝備,不然兩件都不見
 * 加屬性:同一件 3 件 → 第 1 件保留、另外 2 件消耗,隨機多一條附加屬性(最多 3 條;只有出戰角色裝著才有效) */
#define AF_N 8
static const struct { const wchar_t *name; } AF[AF_N + 1] = { { L"" },
    { L"每秒回內力 +2" }, { L"每秒回命 +2" }, { L"攻擊 +6%" }, { L"防禦 +4" },
    { L"移動速度 +8%" }, { L"絕招冷卻 -8%" }, { L"撿到的錢 +10%" }, { L"每殺 1 隻回 1 點命" },
};
static int g_synth_pct = 60, rg_leech;      /* rg_leech:這一局「每殺回命」幾點(開局算) */
static int it_naf(int i) { return (M.it[i].af[0] != 0) + (M.it[i].af[1] != 0) + (M.it[i].af[2] != 0); }
static int it_tier(int id, int *next) {      /* 在掉落表裡第幾階;*next = 高一階的 id(最高階 = 0);不在表裡回 -1 */
    int s = eq_slot(id), t; *next = 0; if (s < 0) return -1;
    for (t = 0; EQ_POOL[s][t]; t++) if (EQ_POOL[s][t] == id) { *next = EQ_POOL[s][t + 1]; return t; }
    return -1;
}
static int cost_synth(int id) { int nx, t = it_tier(id, &nx); return (t < 0 || !nx) ? 0 : cost_geo(80, 1.5, t); }
static int cost_affix(int i) { int k = it_naf(i); return k >= 3 ? 0 : cost_geo(150, 2.0, k); }
static void it_remove(int i) {      /* 這一格的件數用完 → 從清單拿掉,所有角色身上裝著的也卸下 */
    int id = M.it[i].id, c, s;
    for (c = 0; c < 9; c++) for (s = 0; s < 4; s++) if (M.eq[c][s] == id) M.eq[c][s] = 0;
    for (; i < M.nit - 1; i++) M.it[i] = M.it[i + 1];
    M.nit--;
}
static int afx_sum(int type) {      /* 出戰角色身上四格裝備,某一種附加屬性有幾條 */
    int s, i, k, n = 0;
    for (s = 0; s < 4; s++) if (M.eq[M.lead][s] && (i = it_find(M.eq[M.lead][s])) >= 0) for (k = 0; k < 3; k++) if (M.it[i].af[k] == type) n++;
    return n;
}
static wchar_t rg_it_msg[120];
static int it_synth(int i) {      /* 回傳結果寫在 rg_it_msg */
    int id = M.it[i].id, nx, c = cost_synth(id), ok, j; wchar_t a[40], b[40];
    if (!c || M.it[i].n < 2 || M.money < c) return 0;
    it_tier(id, &nx); def_name(game_itemdef, id, a, 40); def_name(game_itemdef, nx, b, 40);
    M.money -= c; M.it[i].n -= 2; if (M.it[i].n <= 0) it_remove(i);
    ok = rand() % 100 < g_synth_pct;
    if (ok && (j = it_add(nx)) >= 0) _snwprintf(rg_it_msg, 120, L"合成成功!%s ×2 → %s", a, b);
    else { ok = 0; _snwprintf(rg_it_msg, 120, L"合成失敗……%s ×2 都不見了", a); }
    rg_it_msg[119] = 0; meta_save();
    mlog("shop: synth %d x2 -> %s %d (-%d, money %d)", id, ok ? "ok" : "lost", nx, c, M.money);
    return 1;
}
static int it_affix(int i) {
    int c = cost_affix(i), k, t; wchar_t a[40];
    if (!c || M.it[i].n < 3 || M.money < c) return 0;
    for (k = 0; k < 3 && M.it[i].af[k]; k++) ;
    t = 1 + rand() % AF_N; M.it[i].af[k] = t;
    M.money -= c; M.it[i].n -= 2;
    def_name(game_itemdef, M.it[i].id, a, 40); _snwprintf(rg_it_msg, 120, L"%s 多了一條屬性:%s(用掉 2 件)", a, AF[t].name); rg_it_msg[119] = 0;
    meta_save(); mlog("shop: affix item %d +%d (slot %d, -%d, money %d)", M.it[i].id, t, k, c, M.money);
    return 1;
}
/* 開局:把配置的裝備寫進主角四格(原作數值由 0x408680 重算),等級多出來的部分加在基礎數值上。結束由 snap_restore 整塊還原。 */
static void eq_apply(unsigned char *sb) {
    int s, i, a, d, h, m, r, a0, d0, h0, m0, r0;
    __try {
        for (s = 0; s < 4; s++) if (M.eq[M.lead][s] && it_find(M.eq[M.lead][s]) >= 0 && game_itemdef(M.eq[M.lead][s])) I32(sb, 0x84 + 4 * s) = M.eq[M.lead][s];
        for (s = 0; s < 4; s++) if (M.eq[M.lead][s] && (i = it_find(M.eq[M.lead][s])) >= 0 && M.it[i].lv > 1) {
            it_stats(M.eq[M.lead][s], M.it[i].lv, &a, &d, &h, &m, &r); it_stats(M.eq[M.lead][s], 1, &a0, &d0, &h0, &m0, &r0);
            I32(sb, 0x60) += a - a0; I32(sb, 0x64) += d - d0; I32(sb, 0x68) += r - r0; I32(sb, 0x54) += h - h0; I32(sb, 0x5c) += m - m0;      /* 基礎 攻 +0x60 / 防 +0x64 / 抗 +0x68 */
        }
        game_recalc(sb, 0); I32(sb, 0x50) = I32(sb, 0x54); I32(sb, 0x58) = I32(sb, 0x5c);
        mlog("equip: slots %d,%d,%d,%d -> atk %d/%d def %d/%d hp %d mp %d", I32(sb, 0x84), I32(sb, 0x88), I32(sb, 0x8c), I32(sb, 0x90), I32(sb, 0x60), I32(sb, 0x138), I32(sb, 0x64), I32(sb, 0x13c), I32(sb, 0x54), I32(sb, 0x5c));
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("equip: fault"); }
}
static int rg_drops[8], rg_ndrop, rg_mp_n, rg_mp_sum, rg_pick_money;
static void rg_spawn_potion(unsigned char *pl);
static void rg_drop(int bonus) {       /* 掉一件裝備:哪一格隨機,等級看第幾張圖(越後面的圖掉越好的) */
    int s = rand() % 4, n = 0, t; wchar_t nm[40], msg[80];
    while (EQ_POOL[s][n]) n++;
    t = rg_map * n / RG_NMAP + rand() % 2 + bonus; if (t >= n) t = n - 1;
    if (it_add(EQ_POOL[s][t]) < 0) return;
    if (rg_ndrop < 8) rg_drops[rg_ndrop++] = EQ_POOL[s][t];
    meta_save();
    def_name(game_itemdef, EQ_POOL[s][t], nm, 40); _snwprintf(msg, 80, L"撿到裝備:%s", nm); msg[79] = 0; toast(msg);
    mlog("ROGUE drop: item %d (slot %d tier %d)", EQ_POOL[s][t], s, t);
}

/* ---- 地上的掉落物:靠近自動撿;補內力的丹藥撿到自動喝(notes/12)-----------------------------
 * 原作的掉落物 = vtable 0x4ab0d8 的物件、obj+0x100 & 4;stat+0xc == 0x10 是物品(id = **(stat+0xb0))、0x20 是錢(stat+0x98)。
 * 原作要點它才撿(0x44a450,沒有靠近就撿),而且撿到的東西進主角的背包 —— 那是原作存檔的東西。
 * 模組不走原作的撿取:自己判斷距離,效果直接算在這一局(內力 / 命 / 這局的錢),再把物件刪掉 → 原作的背包和錢不動。 */
static int g_pick_r = 64, rg_pick_n, rg_mp_stock, rg_pot_spawned;
static void rg_sound(const char *name) { __try { unsigned char *app = GAME_APP; if (app) ((void (__cdecl *)(const char *, int, int))0x4280a0)(name, I32(app, 0x1b4), 0); } __except (EXCEPTION_EXECUTE_HANDLER) { } }
static void rg_drink(unsigned char *sb, int restore) {
    int add = restore, lo = I32(sb, 0x5c) * 30 / 100; wchar_t m[64];
    if (add < lo) add = lo;                       /* 原作的元神丹回得少,至少回內力上限的 30% */
    if (I32(sb, 0x58) + add > I32(sb, 0x5c)) add = I32(sb, 0x5c) - I32(sb, 0x58);
    I32(sb, 0x58) += add; rg_mp_n++; rg_mp_sum += add;
    _snwprintf(m, 64, L"喝下丹藥:內力 +%d", add); m[63] = 0; toast(m); rg_sound("exs_MP01.wav");
}
static void rg_spawn_potion(unsigned char *pl) {
    unsigned char *d = game_itemdef(2); int *o; double ang = (rand() % 628) / 100.0; int r = 45 + rand() % 60, x, y;
    if (!d) return;
    __try {
        x = (I32(pl, 0x14) >> 16) + (int)(r * cos(ang)); y = (I32(pl, 0x18) >> 16) + (int)(r * sin(ang) * 0.6);
        ((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)(*(void **)(pl + 0xac), 0, &x, &y, 1, I32(pl, 0x194), 12);
        o = game_create(*GAME_OBJMGR, 0, I32(pl, 0xac), 0, x << 16, y << 16, I32(pl, 0x1c), I32(d, 0x20), 0);
        if (o) { ((void (__fastcall *)(void *, int, int))0x44a360)(o, 0, 2); rg_pot_spawned++; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("potion spawn: fault %08x", GetExceptionCode()); }
}
static void rg_pickup_tick(unsigned char **t, unsigned char *pl, DWORD now) {
    static DWORD nx; unsigned char *lsb = *(unsigned char **)(pl + 0x1a4); int i, px, py;
    if (!lsb || (int)(now - nx) < 0) return;
    nx = now + 120; px = I32(pl, 0x14) >> 16; py = I32(pl, 0x18) >> 16;
    if (rg_mp_stock > 0 && I32(lsb, 0x58) < I32(lsb, 0x5c) * 35 / 100) { rg_mp_stock--; rg_drink(lsb, 0); }      /* 內力滿的時候撿到的先存著,掉到 35% 以下自動喝 */
    __try {
        for (i = 1; i < OBJ_SLOTS; i++) { unsigned char *o = t[i], *sb, *d; int dx, dy, kind, id = 0, amt;
            if (!o || *(unsigned *)o != 0x4ab0d8 || !(I32(o, 0x100) & 4) || !(sb = *(unsigned char **)(o + 0x1a4))) continue;
            dx = (I32(o, 0x14) >> 16) - px; dy = (I32(o, 0x18) >> 16) - py;
            if (dx * dx + dy * dy * 2 > g_pick_r * g_pick_r) continue;
            kind = I32(sb, 0xc);
            if (kind == 0x20) { amt = I32(sb, 0x98); amt = 1 + amt / 200; if (amt > 5) amt = 5; rg_bonus += amt; rg_pick_money += amt; }      /* 原作的錢袋:折成這局的錢 1–5 */
            else if (kind == 0x10) {
                int **b = *(int ***)(sb + 0xb0); id = (b && (unsigned char *)b != *(unsigned char **)(sb + 0xb4)) ? *(int *)b : 0;
                d = id ? game_itemdef(id) : NULL;
                if (d && I32(d, 0xbc) > 0 && I32(d, 0xbc) != I32(d, 0xc0)) { if (I32(lsb, 0x58) < I32(lsb, 0x5c)) rg_drink(lsb, I32(d, 0xbc)); else { rg_mp_stock++; toast(L"撿到丹藥(內力滿,先留著)"); } }
                else if (d && I32(d, 0xb4) > 0) { int add = I32(d, 0xb4); if (add < 40) add = 40; if (I32(lsb, 0x50) + add > I32(lsb, 0x54)) add = I32(lsb, 0x54) - I32(lsb, 0x50); I32(lsb, 0x50) += add; toast(L"撿到傷藥:命回復"); }
                else { rg_bonus += 2; rg_pick_money += 2; }                 /* 其他東西(密笈、鑰匙…):這局用不到,折成 2 錢 */
            } else continue;
            rg_pick_n++; rg_sound("exs_take01.wav");
            ((void (__fastcall *)(void *, int, int))0x44ec10)(*GAME_OBJMGR, 0, I32(o, 0xc));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("pickup: fault %08x", GetExceptionCode()); }
}

/* ---- 被動技能(打倒頭目掉 1 個;每個角色裝 1 個,同一個只有一份;跨局保留)----------------------- */
#define RG_NPAS 8
static const struct { const wchar_t *name, *desc; int holder; } RG_PAS[RG_NPAS + 1] = { { L"", L"", 0 },
    { L"嗜血",   L"每殺 1 隻回 2 點命",            0 },
    { L"疾風",   L"裝的人移動速度 +20%",           1 },
    { L"金鐘罩", L"裝的人防禦 +8、命上限 +20%",    1 },
    { L"聚氣",   L"絕招冷卻 -25%",                 0 },
    { L"貪財",   L"這局撿到的錢 +30%",             0 },
    { L"劍氣",   L"普攻範圍 +40%、一刀多打 2 隻",  0 },
    { L"怒火",   L"絕招威力 +40%",                 0 },
    { L"回春",   L"裝的人每秒多回 4 點命",         1 },
};
static int pas_has(int id) { int i; for (i = 0; i < M.npas; i++) if (M.pas[i] == id) return 1; return 0; }
static int pas_on(int id) { return (M.pas_eq[0] == id || M.pas_eq[1] == id) && pas_has(id); }
static int rg_sw_save[2], rg_sw_saved;
static void pas_apply_lead(unsigned char *pl, unsigned char *sb) {      /* 開局(surv_start 之後)套用;結束由 snap_restore / v1_end / pas_unapply 還原 */
    int id = pas_has(M.pas_eq[0]) ? M.pas_eq[0] : 0;
    if (id == 2) I32(pl, 0xf8) += I32(pl, 0xf8) / 5;
    if (id == 3) { int real = I32(sb, 0x54) - g_hpbuf; real += real / 5; if (real > 999) real = 999; I32(sb, 0x54) = real + g_hpbuf; I32(sb, 0x50) = I32(sb, 0x54); I32(sb, 0x64) += 8; recalc_keep(sb); }
    if (id == 8) g_hp_regen += 4;
    if (pas_on(4)) g_cast_ms = g_cast_ms * 3 / 4;
    if (pas_on(6)) { rg_sw_save[0] = g_swing_range; rg_sw_save[1] = g_swing_max; rg_sw_saved = 1; g_swing_range = g_swing_range * 14 / 10; g_swing_max += 2; }
    if (pas_on(7)) { int *b = *(int **)(sb + 0xa0), *e = *(int **)(sb + 0xa4); g_power_pct += 40; for (; b && b < e; b++) power_set(*b); }
    mlog("passive: lead=%d partner=%d", M.pas_eq[0], M.pas_eq[1]);
    {   /* 裝備的附加屬性(出戰角色身上四格) */
        int n1 = afx_sum(1), n2 = afx_sum(2), n3 = afx_sum(3), n4 = afx_sum(4), n5 = afx_sum(5), n6 = afx_sum(6);
        g_mp_regen += 2 * n1; g_hp_regen += 2 * n2; rg_leech = afx_sum(8);
        if (n3 || n4) { I32(sb, 0x60) += I32(sb, 0x60) * 6 * n3 / 100; I32(sb, 0x64) += 4 * n4; recalc_keep(sb); }
        if (n5) I32(pl, 0xf8) += I32(pl, 0xf8) * 8 * n5 / 100;
        while (n6-- > 0) g_cast_ms = g_cast_ms * 92 / 100;
        mlog("affix: mp+%d hp+%d atk%%+%d def+%d spd%%+%d cd x%d money%%+%d leech %d -> atk %d def %d spd %d cast_ms %d", 2 * n1, 2 * n2, 6 * n3, 4 * n4, 8 * n5, afx_sum(6), 10 * afx_sum(7), afx_sum(8), I32(sb, 0x138), I32(sb, 0x13c), I32(pl, 0xf8), g_cast_ms);
    }
}
static void pas_unapply(void) { if (rg_sw_saved) { g_swing_range = rg_sw_save[0]; g_swing_max = rg_sw_save[1]; rg_sw_saved = 0; } }

/* ---- 夥伴(照原作只有 1 個;局內由主角的 3 選 1 加入 / 升級;夥伴自己不學招、沒有 3 選 1)----------
 * 不走原作的隊伍名冊(gs+0x44:會留在記憶體裡,玩家之後存檔就多一個隊員)→ 直接生一個同陣營的物件:
 * 圖像 type = 原作隊員的配對(素還真 2001、其他 2008 + 第幾個),數值 0x444e00,跟隨 0x448fc0(obj, 主角 id),戰術 stat+0x14c = 2(跟著打)。
 * 存檔原本帶的夥伴:開局用原作的召喚 / 收回 0x4584f0 收起來,結束再叫出來。 */
static int rg_pt_id, rg_pt_chr, rg_pt_lv, rg_had_partner;
static unsigned char *rg_pt_obj(void) { unsigned char *o = rg_obj(rg_pt_id), *sb; if (!o || !IS_CHAR(o) || I32(o, 0xfc) != rg_pt_chr) return NULL; sb = *(unsigned char **)(o + 0x1a4); return (sb && I32(sb, 0x50) > 0) ? o : NULL; }
static int rg_pt_pick(void) { int c = M.pt[M.lead]; return (c >= 1 && c <= 7 && c != M.lead) ? c : 0; }      /* 夥伴在角色頁指定(Kirin 2026-11-23);劍如冰(8)沒有攻擊動畫,不能選 */
static void rg_orig_partner(int summon) {
    unsigned char *gs = GAME_GS;
    __try {
        if (!gs) return;
        if (!summon) {
            rg_had_partner = *(void **)(gs + 0xc) != NULL;
            if (rg_had_partner) { ((void (__fastcall *)(void *, int))0x4584f0)(gs, 0); mlog("partner: original partner dismissed -> gs+0xc=%p", *(void **)(gs + 0xc)); }
        } else if (rg_had_partner && !*(void **)(gs + 0xc)) { ((void (__fastcall *)(void *, int))0x4584f0)(gs, 0); mlog("partner: original partner back -> gs+0xc=%p", *(void **)(gs + 0xc)); rg_had_partner = 0; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("partner: original summon/dismiss fault %08x", GetExceptionCode()); }
}
static void rg_pt_join(int chr) {
    unsigned char *gs = GAME_GS, *pl = gs ? *(unsigned char **)(gs + 8) : NULL, *lsb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL, *sb; int *o, type = chr == 1 ? 2001 : 2008 + (chr - 1); wchar_t nm[40], msg[80];
    if (!pl || !lsb) return;
    __try {
        o = game_create(*GAME_OBJMGR, 0, I32(pl, 0xac), 0, I32(pl, 0x14) + (30 << 16), I32(pl, 0x18) + (10 << 16), I32(pl, 0x1c), type, 0);
        if (!o) { mlog("partner: create failed (type %d)", type); return; }
        I32(o, 0xbc) = I32(pl, 0xbc); game_setchar(o, 0, chr, 1);
        sb = *(unsigned char **)((unsigned char *)o + 0x1a4);
        if (sb) {       /* 原作隊員各自的加入等級差很多(傲笑紅塵 Lv 35)→ 壓到跟主角差不多 */
            int hp = 160 + 25 * g_lv, atk = I32(lsb, 0x138) * 8 / 10, def = I32(lsb, 0x13c);
            if (M.pas_eq[1] == 3 && pas_has(3)) { hp += hp / 5; def += 8; }
            I32(sb, 0x54) = hp; I32(sb, 0x50) = hp; I32(sb, 0x60) = atk; I32(sb, 0x138) = atk; I32(sb, 0x64) = def; I32(sb, 0x13c) = def; I32(sb, 0x14c) = 2;
            if (!I32(sb, 0x80)) I32(sb, 0x80) = 2;      /* AI 定義:隊員的角色表沒帶(= 0,原作 AI 不跑)→ 用原作夥伴青陽子身上的那一個(2) */
        }
        if (M.pas_eq[1] == 2 && pas_has(2)) I32(o, 0xf8) += I32(o, 0xf8) / 5;
        ((void (__fastcall *)(void *, int, int))0x448fc0)(o, 0, I32(pl, 0xc));
        rg_pt_id = I32(o, 0xc); rg_pt_chr = chr; rg_pt_lv = 1;
        chr_name(chr, nm, 40); _snwprintf(msg, 80, L"夥伴加入:%s", nm); msg[79] = 0; toast(msg);
        mlog("partner: joined char %d type %d id %d hp %d atk %d def %d ai=%d fac=%x", chr, type, rg_pt_id, sb ? I32(sb, 0x54) : 0, sb ? I32(sb, 0x138) : 0, sb ? I32(sb, 0x13c) : 0, sb ? I32(sb, 0x80) : 0, I32(o, 0x100));
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("partner: join fault %08x", GetExceptionCode()); }
}
static void rg_pt_up(void) {
    unsigned char *o = rg_pt_obj(), *sb = o ? *(unsigned char **)(o + 0x1a4) : NULL;
    if (!sb) return;
    I32(sb, 0x60) += I32(sb, 0x60) / 4 + 2; I32(sb, 0x138) = I32(sb, 0x60); I32(sb, 0x64) += 3; I32(sb, 0x13c) = I32(sb, 0x64);
    I32(sb, 0x54) += I32(sb, 0x54) * 3 / 10; I32(sb, 0x50) = I32(sb, 0x54); rg_pt_lv++;
    mlog("partner: upgraded to lv %d hp %d atk %d def %d", rg_pt_lv, I32(sb, 0x54), I32(sb, 0x138), I32(sb, 0x13c));
}
static void rg_pt_remove(void) {
    __try { if (rg_pt_id && rg_obj(rg_pt_id)) ((void (__fastcall *)(void *, int, int))0x44ec10)(*GAME_OBJMGR, 0, rg_pt_id); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    rg_pt_id = 0;
}
static int rg_upg_ok(void) { return rg_pt_obj() != NULL; }      /* 3 選 1 只出「夥伴升級」;沒帶夥伴或夥伴倒了就不出 */
static void rg_upg_label(wchar_t *out, int n) {
    wchar_t nm[40];
    if (rg_pt_obj()) { chr_name(rg_pt_chr, nm, 40); _snwprintf(out, n, L"夥伴升級:%s Lv %d → %d(攻 +25%%、命 +30%% 並回滿)", nm, rg_pt_lv, rg_pt_lv + 1); }
    else out[0] = 0;
    out[n - 1] = 0;
}
static void rg_upg_apply(void) { if (rg_pt_obj()) rg_pt_up(); }

/* ---- 換主角 / 回原本的地圖 ---------------------------------------------------------------
 * 原作的隊伍:gs+0x44[0..7] = 每個隊員的數值區塊(第 k 格 = 角色 k+1),gs+0x14 = 主角是第幾格,gs+0xa4 = 夥伴有沒有叫出來。
 * 換主角 = 換圖前把 gs+0x14 改成那一格(沒招募過就用原作的招募 0x459b80 配一塊),原作載入地圖時自己照那一格生主角(0x458040)。
 * 離開倖存者時換回原本那張圖,gs+0x14 / gs+0xa4 改回去,模組招募的格子用原作移除隊員的做法(0x406e90 + 0x45dbc0)釋放 → 原作的隊伍不留痕跡。 */
static int rg_home_stage = -1, rg_home_x, rg_home_y, rg_home_slot, rg_home_a4, rg_home_pslot, rg_recruited;
static void rg_before_goto(void) {      /* map_tick 真的送出換圖請求前呼叫(從標題開始時,這時存檔已經讀進來了) */
    unsigned char *gs = GAME_GS, *pl, *st; int k;
    if (!gs || (rg_state != RG_GOTO && rg_state != RG_HOME)) return;
    __try {
        if (rg_state == RG_HOME) { I32(gs, 0x14) = rg_home_slot; I32(gs, 0x18) = rg_home_pslot; gs[0xa4] = (unsigned char)rg_home_a4; mlog("home: lead slot -> %d, partner summoned -> %d", rg_home_slot, rg_home_a4); return; }
        pl = *(unsigned char **)(gs + 8); st = *(unsigned char **)(gs + 0x10);
        if (rg_home_stage < 0 && pl && st) {
            int sh = I32(st, 0x7d0) & 31;
            rg_home_stage = I32(st, 0x788); rg_home_x = (I32(pl, 0x14) >> 16) >> sh; rg_home_y = (I32(pl, 0x18) >> 16) >> sh; rg_home_slot = I32(gs, 0x14); rg_home_pslot = I32(gs, 0x18); rg_home_a4 = gs[0xa4];
            mlog("home: stage %d cell (%d,%d) lead slot %d partner slot %d summoned %d", rg_home_stage, rg_home_x, rg_home_y, rg_home_slot, I32(gs, 0x18), rg_home_a4);
        }
        k = M.lead - 1; if (k < 0 || k > 7) k = 0;
        if (!*(void **)(gs + 0x44 + 4 * k)) { ((void (__fastcall *)(void *, int, int))0x459b80)(gs, 0, k); if (*(void **)(gs + 0x44 + 4 * k)) rg_recruited |= 1 << k; mlog("lead: recruited slot %d -> %p", k, *(void **)(gs + 0x44 + 4 * k)); }
        if (*(void **)(gs + 0x44 + 4 * k)) I32(gs, 0x14) = k; else mlog("lead: slot %d has no stat block, staying with slot %d", k, I32(gs, 0x14));
        gs[0xa4] = 0;      /* 這局不帶存檔的夥伴(夥伴由 3 選 1 加入)。🔴 不能把 gs+0x18(選定的夥伴)改成 -1:換圖時原作 HUD 拿它查頭像,實測當掉 → HUD 右邊那格由模組蓋掉重畫 */
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("rg_before_goto: fault %08x", GetExceptionCode()); }
}
static void rg_home_cleanup(void) {
    unsigned char *gs = GAME_GS; int k;
    __try {
        for (k = 1; gs && k < 8; k++) if (rg_recruited & (1 << k)) { unsigned char *sb = *(unsigned char **)(gs + 0x44 + 4 * k);
            if (sb && I32(gs, 0x14) != k && I32(gs, 0x18) != k && rg_home_pslot != k) { ((void (__fastcall *)(void *, int))0x406e90)(sb, 0); ((void (__cdecl *)(void *))0x45dbc0)(sb); *(void **)(gs + 0x44 + 4 * k) = NULL; mlog("home: freed recruited slot %d", k); } }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("home cleanup: fault %08x", GetExceptionCode()); }
    rg_recruited = 0; rg_home_stage = -1;
}

/* ---- 一局 ---------------------------------------------------------------------------------- */
/* 從標題畫面開局要先讀一份原作存檔進遊戲(再換圖)。優先自動存檔 exsave99,沒有就找玩家自己的存檔欄;
   全新安裝一份都沒有 → 不開局,選單上提示先玩一下原作(讀不存在的存檔原作會當) */
static int g_title_slot = 0x14; static wchar_t rg_hub_msg[120];
static int title_save_slot(void) {      /* 用 exe 所在資料夾找(從 Steam 啟動時工作目錄不一定是遊戲資料夾) */
    char d[MAX_PATH], p[MAX_PATH + 32], *s; int i, n = (int)GetModuleFileNameA(NULL, d, MAX_PATH);
    if (n <= 0 || n >= MAX_PATH) d[0] = 0; else if ((s = strrchr(d, '\\')) != NULL) s[1] = 0; else d[0] = 0;
    sprintf(p, "%sSaves\\exsave99.sav", d); if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) return 0x14;
    for (i = 1; i <= 20; i++) { sprintf(p, "%sSaves\\exsave%02d.sav", d, i); if (GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES) return i - 1; }
    mlog("no save found under %sSaves", d);
    return -1;
}
static void rg_start(int map) {
    if (map < 0 || map >= RG_NMAP || (rg_state != RG_OFF && rg_state != RG_RESULT)) return;
    if (GAME_GS && !*(void **)(GAME_GS + 0x10)) {      /* 在標題畫面 */
        int s_ = title_save_slot();
        g_ng_step = 0; g_ng_busy = 0;
        if (s_ < 0) mlog("ROGUE start: no save file -> start a new game from the title first (遊戲開始 → 標準遊戲 → 普通 → 確定)");
        g_title_slot = s_;
    }
    if (M.lead < 1 || M.lead > 7) M.lead = 1;
    if (g_surv) surv_end("rogue start");
    g_surv_over = 0; rg_ui = UI_NONE; rg_map = map; rg_state = RG_LOAD; rg_t = GetTickCount();
    rg_hooks();
    /* 已經在遊戲裡就直接換圖(每局從頭靠 snap_take / snap_restore)。🔴 不重讀存檔:在遊戲中直接叫讀檔 0x4261b0,原作更新 HUD 時找不到視窗,當在 0x42bd00(實測 2 次) */
    __try { unsigned char *app = GAME_APP; if (rg_in_stage() && app && app[0x12]) v1_pause(0); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    mlog("ROGUE start requested: map %d (stage %d) in_stage=%d", map, RG_MAPS[map].stage, rg_in_stage());
}
static void rg_setup(void) {
    unsigned char *gs = GAME_GS, *pl = gs ? *(unsigned char **)(gs + 8) : NULL, *pt = gs ? *(unsigned char **)(gs + 0xc) : NULL, *st;
    if (!pl) return;
    st = *(unsigned char **)(pl + 0xac);
    __try {       /* 主角搬到地圖中央(出口 1 多半在邊上,怪只會從一側來),挪到最近的可走格 */
        int sx = I32(st, 0x790) / 2, sy = I32(st, 0x794) / 2;
        if (((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)(st, 0, &sx, &sy, (~I32(pl, 0x100) >> 28) & 1, I32(pl, 0x194), 80) & 0xff) {
            mlog("rogue: lead (%d,%d) -> centre (%d,%d) of %dx%d", I32(pl, 0x14) >> 16, I32(pl, 0x18) >> 16, sx, sy, I32(st, 0x790), I32(st, 0x794));
            obj_setpos(pl, sx, sy);
            if (pt && pt != pl) obj_setpos(pt, sx + 28, sy + 12);
        } else mlog("rogue: no walkable cell near the centre, staying at the gateway");
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("rogue: setup fault %08x", GetExceptionCode()); }
    snap_take();
    rg_orig_partner(0); rg_pt_id = 0; rg_pt_chr = 0; rg_pt_lv = 0; rg_ndrop = 0; rg_mp_n = rg_mp_sum = 0; rg_pick_n = rg_mp_stock = rg_pot_spawned = rg_pick_money = 0;
    {   /* 每局從頭:不管存檔裡這個角色練到幾級、穿什麼,一律從同一條起跑線開始(素還真 Lv 1 的數值 + 最基本的四件),結束由 snap_restore 還原 */
        unsigned char *sb_ = *(unsigned char **)(pl + 0x1a4);
        if (sb_) { I32(sb_, 0x40) = 1; I32(sb_, 0x54) = 122; I32(sb_, 0x50) = 122; I32(sb_, 0x5c) = 97; I32(sb_, 0x58) = 97; I32(sb_, 0x60) = 10; I32(sb_, 0x64) = 10; I32(sb_, 0x68) = 10;
                   I32(sb_, 0x84) = 101; I32(sb_, 0x88) = 201; I32(sb_, 0x8c) = 351; I32(sb_, 0x90) = 301; eq_apply(sb_); }
        if (gs && g_hud_refresh) __try { ((void (__fastcall *)(void *, int, int, int))0x45a9c0)(gs, 0, I32(gs, 0x80fc), 0); mlog("hud refresh ok"); } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("hud refresh: fault %08x", GetExceptionCode()); }
    }
    g_slots = M.slots; g_leash = 30000;      /* 不設牽繩(換圖已經擋掉);30000² 還在 int 範圍內 */
    memset(rg_seen, 0, sizeof rg_seen); memset(rg_tpl_ranged, 0, sizeof rg_tpl_ranged); rg_ranged_alive = 0; memset(rg_elite_id, 0, sizeof rg_elite_id);
    rg_elite_n = rg_boss_id = rg_boss_on = rg_boss_try = rg_enraged = rg_bonus = rg_tier_i = 0; rg_block_n = 0;
}
static void rg_finish(int win, const char *why) {
    DWORD ms; int i;
    if (!g_rg) return;
    if (g_menu) { g_menu = 0; v1_pause(0); }      /* 升級選單開著時結束:先收選單,時間才對 */
    ms = surv_ms(); g_rg = 0;
    mlog("ROGUE fx: peak %d effect objects, culled %d (cap %d); enemy cap %d; unstuck %d; unmarked before teleport %d; crowd unmarks %d", rg_fx_max, rg_fx_culled, g_fx_cap, g_rg_cap, rg_unstuck_n, g_unmark_n, rg_crowd_n); g_unmark_n = rg_crowd_n = 0; rg_fx_max = rg_fx_culled = rg_unstuck_n = 0;
    rg_pt_remove();
    __try { if (rg_ai_p) { rg_ai_p[5] = rg_ai_cast; rg_ai_p = NULL; } } __except (EXCEPTION_EXECUTE_HANDLER) { rg_ai_p = NULL; }
    { int i_; for (i_ = 0; i_ < rg_nbmove; i_++) { unsigned char *d_ = game_movedef(rg_bmove[i_].id); if (d_) I32(d_, 0x24) = rg_bmove[i_].base; } rg_nbmove = 0; }
    surv_end(why); g_surv_over = 0; pas_unapply();
    rg_orig_partner(1);
    memset(&rg_res, 0, sizeof rg_res); rg_res.unlocked = -1;
    if (win) { rg_bonus += 49; rg_drop(1); }
    rg_res.win = win; rg_res.ms = ms; rg_res.kills = g_surv_kills; rg_res.money = g_surv_kills + rg_bonus; rg_res.clear = win ? 100 * (rg_map + 1) : 0;
    if (pas_on(5)) rg_res.money += rg_res.money * 3 / 10;
    if (afx_sum(7)) rg_res.money += rg_res.money * afx_sum(7) / 10;
    if (win) {       /* 頭目掉 1 個還沒有的被動技能;都有了就折成錢 */
        int cand[RG_NPAS], nc = 0;
        for (i = 1; i <= RG_NPAS; i++) if (!pas_has(i)) cand[nc++] = i;
        if (nc && M.npas < RG_MAXPAS) { rg_res.pas = cand[rand() % nc]; M.pas[M.npas++] = rg_res.pas; if (!M.pas_eq[0]) M.pas_eq[0] = rg_res.pas; mlog("ROGUE passive drop: %d", rg_res.pas); }
        else rg_res.clear += 200;
    }
    {   int k_ = 0; wchar_t nm_[40];
        for (i = 0; i < rg_ndrop && k_ < 130; i++) { def_name(game_itemdef, rg_drops[i], nm_, 40); k_ += _snwprintf(rg_res.drops + k_, 159 - k_, L"%s%s", i ? L"、" : L"撿到裝備:", nm_); }
        rg_res.drops[159] = 0; }
    M.money += rg_res.money + rg_res.clear; M.runs++;
    if (win) { M.wins++; if (rg_map == M.cleared && M.cleared < RG_NMAP) { M.cleared++; if (M.cleared < RG_NMAP) rg_res.unlocked = M.cleared; } }
    rg_res.total = M.money;
    meta_save();
    for (i = 0; i < 4; i++) rg_elite_id[i] = 0; rg_boss_id = 0;
    { unsigned char *gs_ = GAME_GS, *pl_ = gs_ ? *(unsigned char **)(gs_ + 8) : NULL, *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL; if (sb_ && I32(sb_, 0x50) < 1) I32(sb_, 0x50) = 1; }
    rg_state = RG_RESULT; rg_ui = UI_RESULT; rg_dirty = 1;
    { unsigned char *app = GAME_APP; if (app && rg_in_stage() && !app[0x12]) v1_pause(1); }      /* 立刻停住:主角倒下時不讓原作跑到「進度讀取」選單 */
    mlog("ROGUE pickups: %d objects auto-picked (money +%d), potions spawned %d, drunk %d (+%d mp), left in stock %d", rg_pick_n, rg_pick_money, rg_pot_spawned, rg_mp_n, rg_mp_sum, rg_mp_stock);
    mlog("ROGUE end: %s (%s) map=%d time=%us kills=%d money=+%d clear=+%d total=%d cleared=%d blocked_exits=%d", win ? "WIN" : "LOSE", why, rg_map, ms / 1000, rg_res.kills, rg_res.money, rg_res.clear, M.money, M.cleared, rg_block_n);
}
static void rg_spawn_special(unsigned char *pl, int kind) {
    int type = 0, chr = 0, j, k;
    if (kind == 2 && RG_MAPS[rg_map].boss_type && rg_boss_try < 2) { type = RG_MAPS[rg_map].boss_type; chr = RG_MAPS[rg_map].boss_chr; }
    else { j = (kind == 2 ? g_surv_ntpl - 1 : rand() % g_surv_ntpl); type = g_surv_tpl[j].type; chr = g_surv_tpl[j].chr; }   /* 頭目沒有圖像配對 → 最後加進來(最強)的那種怪頂替 */
    rg_spawn_kind = kind; g_last_spawn = NULL;
    k = spawn_ring(pl, type, chr, 1, 120, 160);
    rg_spawn_kind = 0;
    if (kind == 2) rg_boss_try++;
    if (!k || !g_last_spawn) { mlog("rogue: special spawn failed (kind %d type %d char %d)", kind, type, chr); return; }
    g_surv_spawned += k; rg_mark(g_last_spawn);
    if (kind == 2) {
        unsigned char *sb = *(unsigned char **)(g_last_spawn + 0x1a4);
        rg_boss_id = I32(g_last_spawn, 0xc); rg_boss_chr = chr; rg_boss_hpmax = sb ? I32(sb, 0x54) : 1; rg_boss_on = 1;
        __try {      /* 頭目的 AI 定義:放招機率(short[5],盲劍是 80)壓到 25 —— 不然牠幾乎每次出手都是絕招。結束還原 */
            short *p_ = sb && I32(sb, 0x80) ? ((short *(__cdecl *)(int))0x427e60)(I32(sb, 0x80)) : NULL;
            if (p_ && !rg_ai_p) { rg_ai_p = p_; rg_ai_cast = p_[5]; if (p_[5] > 35) p_[5] = 35; mlog("boss ai def %d: cast %d -> %d", I32(sb, 0x80), rg_ai_cast, p_[5]); }
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        __try { int *b_ = sb ? *(int **)(sb + 0xa0) : NULL, *e_ = sb ? *(int **)(sb + 0xa4) : NULL;
            for (rg_nbmove = 0; b_ && b_ < e_ && rg_nbmove < 12; b_++) { unsigned char *d_ = game_movedef(*b_); if (!d_) continue;
                rg_bmove[rg_nbmove].id = *b_; rg_bmove[rg_nbmove].base = I32(d_, 0x24); I32(d_, 0x24) = I32(d_, 0x24) * g_boss_skill / 100; rg_nbmove++; }
            mlog("boss moves: %d scaled to %d%% power", rg_nbmove, g_boss_skill);
        } __except (EXCEPTION_EXECUTE_HANDLER) { }
        toast(L"頭目出場!");
        mlog("ROGUE boss: type=%d char=%d id=%d hp=%d atk=%d def=%d", type, chr, rg_boss_id, rg_boss_hpmax, sb ? I32(sb, 0x138) : 0, sb ? I32(sb, 0x13c) : 0);
    } else {
        unsigned char *sb = *(unsigned char **)(g_last_spawn + 0x1a4);
        for (j = 0; j < 4; j++) if (!rg_elite_id[j]) { rg_elite_id[j] = I32(g_last_spawn, 0xc); break; }
        rg_elite_n++; toast(L"精英出現!");
        mlog("ROGUE elite #%d: type=%d char=%d id=%d hp=%d atk=%d", rg_elite_n, type, chr, I32(g_last_spawn, 0xc), sb ? I32(sb, 0x54) : 0, sb ? I32(sb, 0x138) : 0);
    }
}
static void rg_on_elite_dead(void);
/* 特效物件(vtable 0x4ab608;type 6015 / 1051 = 受擊 / 命中的特效,被圍毆時一次上百個)超過 g_fx_cap 就把多的刪掉;順便數地上的掉落物 */
static void rg_fx_tick(unsigned char **t, DWORD now) {
    static DWORD nx; int i, n = 0, d = 0, ids[96], k = 0;
    if ((int)(now - nx) < 0) return;
    nx = now + 150;
    __try {
        for (i = 1; i < OBJ_SLOTS; i++) { unsigned char *o = t[i]; if (!o) continue;
            if (I32(o, 0) == 0x4ab0d8) d++;
            else if (I32(o, 0) == 0x4ab608 && (I32(o, 4) == 6015 || I32(o, 4) == 1051)) { n++; if (g_fx_cap && n > g_fx_cap && k < 96) ids[k++] = I32(o, 0xc); } }
        rg_fx_alive = n; rg_drop_alive = d; if (n > rg_fx_max) rg_fx_max = n;
        for (i = 0; i < k; i++) { ((void (__fastcall *)(void *, int, int))0x44ec10)(*GAME_OBJMGR, 0, ids[i]); rg_fx_culled++; }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("fx cull: fault %08x", GetExceptionCode()); g_fx_cap = 0; }
}
/* 每幀(surv_tick 裡,升級選單沒開時):正規化、精英 / 頭目進度、生怪 */
static void rg_play_tick(unsigned char **t, unsigned char *pl, DWORD now) {
    DWORD ms = surv_ms(); int i, n, k, alive, cap;
    static DWORD scan_next;
    __try {
        if ((int)(now - scan_next) >= 0) { scan_next = now + 300; rg_norm_scan(t); }
        rg_pickup_tick(t, pl, now);
        if (1) { unsigned char *gs_ = GAME_GS, *a_ = gs_ ? *(unsigned char **)(gs_ + 8) : NULL, *b_ = gs_ ? *(unsigned char **)(gs_ + 0xc) : NULL, *c_ = rg_pt_obj();
            rg_unfreeze = 0; if (a_) I32(a_, 0x7c) &= ~0x40000000; if (b_) I32(b_, 0x7c) &= ~0x40000000; if (c_) I32(c_, 0x7c) &= ~0x40000000; I32(pl, 0x7c) &= ~0x40000000; }
        rg_fx_tick(t, now);
        {   /* 擠出怪群:主角在走路指令卻 0.4 秒沒移動 → 把主角身邊 56 px 內的怪的「有人站著」記號擦掉(不搬怪),主角就算得出路徑穿過去;
             * 怪自己下一次移動時原作會重蓋記號。(實測:50 隻怪圍住時主角完全走不動 —— 原作從來沒有這麼多怪,所以沒這個問題) */
            static DWORD since2_; static int lx2_, ly2_; int px_ = I32(pl, 0x14) >> 16, py_ = I32(pl, 0x18) >> 16, i_;
            if (I32(pl, 0x134) != 1 || abs(px_ - lx2_) + abs(py_ - ly2_) > 2) { since2_ = now; lx2_ = px_; ly2_ = py_; }
            else if (now - since2_ > 400) {
                for (i_ = 1; i_ < OBJ_SLOTS; i_++) if (enemy_alive(t[i_])) { int dx_ = (I32(t[i_], 0x14) >> 16) - px_, dy_ = (I32(t[i_], 0x18) >> 16) - py_; if (dx_ * dx_ + dy_ * dy_ * 2 < 56 * 56) { obj_unmark(t[i_]); rg_crowd_n++; } }
                __try { ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, I32(pl, 0x140), I32(pl, 0x144)); } __except (EXCEPTION_EXECUTE_HANDLER) { }
                since2_ = now;
            }
        }
        {   /* 卡住看守:主角在「走路」指令(obj+0x134 == 1)卻 1.2 秒都沒移動 → 挪到最近的可走格(0x454bb0)。
             * 實測:走進地圖邊的出口區再出來,第二次起主角站在不可走的格子上,原作尋路就一直失敗(指令是走路、位置不動)。 */
            static DWORD since_; static int lx_, ly_; int px_ = I32(pl, 0x14) >> 16, py_ = I32(pl, 0x18) >> 16;
            if (I32(pl, 0x134) != 1 || I32(pl, 0x188) > 0 || abs(px_ - lx_) + abs(py_ - ly_) > 2) { since_ = now; lx_ = px_; ly_ = py_; }
            else if (g_unstuck_on && now - since_ > 1200) {
                unsigned char *st_ = *(unsigned char **)(pl + 0xac); int sx_ = px_, sy_ = py_, ok_ = 0;
                __try { ok_ = ((int (__fastcall *)(void *, int, int *, int *, int, int, int))0x454bb0)(st_, 0, &sx_, &sy_, (~I32(pl, 0x100) >> 28) & 1, I32(pl, 0x194), 80) & 0xff; } __except (EXCEPTION_EXECUTE_HANDLER) { ok_ = 0; }
                if (!ok_ || (sx_ == px_ && sy_ == py_)) { int cx_ = I32(st_, 0x790) / 2, cy_ = I32(st_, 0x794) / 2, dx_ = cx_ - px_, dy_ = cy_ - py_; double d_ = sqrt((double)dx_ * dx_ + (double)dy_ * dy_); if (d_ < 1) d_ = 1; sx_ = px_ + (int)(dx_ / d_ * 24); sy_ = py_ + (int)(dy_ / d_ * 24); }
                obj_setpos(pl, sx_, sy_); rg_unstuck_n++;
                if (rg_unstuck_n <= 20) mlog("unstuck: lead (%d,%d) -> (%d,%d)%s", px_, py_, sx_, sy_, ok_ ? "" : " (toward centre)");
                __try { ((void (__fastcall *)(void *, int, int, int))0x445a80)(pl, 0, I32(pl, 0x140), I32(pl, 0x144)); } __except (EXCEPTION_EXECUTE_HANDLER) { }
                since_ = now; lx_ = sx_; ly_ = sy_;
            }
        }
        {   static int last_k; unsigned char *sb_ = *(unsigned char **)(pl + 0x1a4); int dk = g_surv_kills - last_k;
            if (dk < 0 || dk > 50) dk = 0;
            last_k = g_surv_kills;
            for (; dk > 0; dk--) {
                if (rand() % 100 < 1) rg_drop(0);                         /* 小怪 1% 掉裝 */
                if (rg_drop_alive < 12 && rand() % 100 < g_mp_drop) rg_spawn_potion(pl);          /* 補內力的丹藥:g_mp_drop% 掉在主角附近的地上,走過去自動撿、自動喝 */
                if (pas_on(1) && sb_ && I32(sb_, 0x50) < I32(sb_, 0x54)) { I32(sb_, 0x50) += 2; if (I32(sb_, 0x50) > I32(sb_, 0x54)) I32(sb_, 0x50) = I32(sb_, 0x54); }
                if (rg_leech && sb_ && I32(sb_, 0x50) < I32(sb_, 0x54)) { I32(sb_, 0x50) += rg_leech; if (I32(sb_, 0x50) > I32(sb_, 0x54)) I32(sb_, 0x50) = I32(sb_, 0x54); }
            }
            if (M.pas_eq[1] == 8 && pas_has(8)) { static DWORD nx; unsigned char *po = rg_pt_obj(), *psb = po ? *(unsigned char **)(po + 0x1a4) : NULL;
                if (psb && (int)(now - nx) >= 0) { nx = now + 1000; I32(psb, 0x50) += 4; if (I32(psb, 0x50) > I32(psb, 0x54)) I32(psb, 0x50) = I32(psb, 0x54); } }
        }
        for (i = 0; i < 4; i++) if (rg_elite_id[i]) {
            unsigned char *o = rg_obj(rg_elite_id[i]);
            if (!o || !enemy_alive(o)) { rg_elite_id[i] = 0; rg_bonus += 9; toast(L"精英倒下!金錢 +10"); mlog("ROGUE elite dead t=%us", ms / 1000); rg_on_elite_dead(); }
        }
        if (rg_boss_id) {
            unsigned char *o = rg_obj(rg_boss_id);
            if (!o || !enemy_alive(o)) { rg_finish(1, "boss dead"); return; }
            {   /* 頭目黏著主角:離超過 36 px 就每 50 ms 往主角挪 4 px(普攻範圍 60 px、y 方向算兩倍,拉到 70 px 還是砍不到)(不然主角一走開牠就留在原地放招,實測 8:00 出場、13:44 才打倒) */
                static DWORD pull_t; int dx_ = (I32(pl, 0x14) - I32(o, 0x14)) >> 16, dy_ = (I32(pl, 0x18) - I32(o, 0x18)) >> 16; double d_ = sqrt((double)dx_ * dx_ + (double)dy_ * dy_);
                if (d_ > 36 && now - pull_t >= 50) { pull_t = now; obj_addpos(o, (int)(dx_ / d_ * 4 * 65536), (int)(dy_ / d_ * 4 * 65536)); }
            }
            if (!rg_enraged && ms >= RG_T_ENRAGE) { unsigned char *sb = *(unsigned char **)(o + 0x1a4); rg_enraged = 1; if (sb) { I32(sb, 0x138) *= 2; I32(sb, 0x60) = I32(sb, 0x138); } toast(L"頭目狂暴!攻擊 ×2"); mlog("ROGUE boss enraged"); }
        }
        while (rg_tier_i < (int)(sizeof g_tiers / sizeof g_tiers[0]) && ms >= (DWORD)g_tiers[rg_tier_i][0] * 40000) { surv_add_tpl(g_tiers[rg_tier_i][1], g_tiers[rg_tier_i][2]); rg_tier_i++; }
        if (rg_elite_n < 3 && ms >= (DWORD)(rg_elite_n + 1) * RG_T_ELITE && ms < RG_T_BOSS) rg_spawn_special(pl, 1);
        if (!rg_boss_id && ms >= RG_T_BOSS && rg_boss_try < 4) rg_spawn_special(pl, 2);
        if ((int)(now - g_surv_next) < 0) return;
        g_surv_wave++;
        alive = g_nenemy; n = 3 + ms / 30000; if (rg_boss_on) n /= 2;
        cap = 30 + ms / 8000; if (cap > g_surv_cap) cap = g_surv_cap; if (cap > g_rg_cap) cap = g_rg_cap; if (rg_boss_on && cap > 30) cap = 30;      /* 頭目戰:場上小怪壓到 45 隻以下 */
        if (alive + n > cap) n = cap - alive;
        for (i = 0, k = 0; i < n; ) {
            int c = n - i < 4 ? n - i : 4, j = (g_surv_wave + i) % g_surv_ntpl, q, got;
            for (q = 0; q < g_surv_ntpl && rg_tpl_ranged[j] && rg_ranged_alive >= g_ranged_cap; q++) j = (j + 1) % g_surv_ntpl;      /* 遠程怪滿了就換下一種 */
            g_last_spawn = NULL; got = spawn_ring(pl, g_surv_tpl[j].type, g_surv_tpl[j].chr, c, g_surv_rmin, g_surv_rmax); k += got;
            if (got && g_last_spawn) { unsigned char *sb_ = *(unsigned char **)(g_last_spawn + 0x1a4); rg_tpl_ranged[j] = sb_ && I32(sb_, 0x78) > 0; if (rg_tpl_ranged[j]) rg_ranged_alive += got; }
            i += c;
        }
        g_surv_spawned += k;
        g_surv_next = now + 5000 / (g_tscale > 0 ? g_tscale : 1);
        if (g_surv_wave % 6 == 1) { unsigned char *sb_ = *(unsigned char **)(pl + 0x1a4); mlog("ROGUE wave %d: +%d (alive %d cap %d ranged %d, kills %d, t=%us, money %d) lead hp %d/%d atk %d def %d", g_surv_wave, k, alive, cap, rg_ranged_alive, g_surv_kills, ms / 1000, g_surv_kills + rg_bonus, sb_ ? I32(sb_, 0x50) - g_hpbuf : 0, sb_ ? I32(sb_, 0x54) - g_hpbuf : 0, sb_ ? I32(sb_, 0x138) : 0, sb_ ? I32(sb_, 0x13c) : 0); }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("rogue tick: exception %08x", GetExceptionCode()); }
}
static void rg_on_elite_dead(void) { if (rand() % 100 < 60) rg_drop(0); }      /* 精英 60% 掉裝 */

/* ---- 選單(倖存者 hub / 結算)---------------------------------------------------------------
 * 自己畫在遊戲畫面上(GDI);滑鼠點 / 鍵盤都能操作。每次畫的時候把可點的區域記在 rg_hit。
 * 遊戲裡開選單 = 暫停(app+0x12),暫停時原作不出新畫面 → 內容變了就自己重畫上一幀再送一次(ui_present)。 */
enum { A_NONE, A_TAB, A_MAP, A_START, A_CLOSE, A_OK, A_BUYSLOT, A_OPEN, A_LEAD, A_BUYCHAR, A_EQUIP, A_UPITEM, A_PAGE, A_PAS, A_CHR, A_PT, A_SELIT, A_SYNTH, A_AFFIX };
#define RG_NTAB 5
static struct { RECT r; int act, arg, key; } rg_hit[64]; static int rg_nhit;
static const RECT RG_TITLE_BTN = { 452, 12, 628, 52 };
static void rg_box(HDC dc, int l, int t, int r, int b, COLORREF fill, COLORREF edge) {
    RECT rc = { l, t, r, b }; HBRUSH br = CreateSolidBrush(fill), eb = CreateSolidBrush(edge);
    FillRect(dc, &rc, br); FrameRect(dc, &rc, eb); DeleteObject(br); DeleteObject(eb);
}
static void rg_text(HDC dc, int x, int y, COLORREF c, const wchar_t *s) { SetTextColor(dc, c); TextOutW(dc, x, y, s, (int)wcslen(s)); }
/* state:0 一般 / 1 選中 / 2 不能按 */
static void rg_button(HDC dc, int x, int y, int w, int h, const wchar_t *s, int act, int arg, int key, int state) {
    rg_box(dc, x, y, x + w, y + h, state == 1 ? RGB(110, 60, 20) : state == 2 ? RGB(30, 30, 36) : RGB(36, 40, 70), state == 1 ? RGB(255, 200, 90) : RGB(110, 110, 140));
    rg_text(dc, x + 8, y + (h - 18) / 2, state == 2 ? RGB(120, 120, 120) : RGB(255, 255, 255), s);
    if (state != 2 && act && rg_nhit < 64) { rg_hit[rg_nhit].r.left = x; rg_hit[rg_nhit].r.top = y; rg_hit[rg_nhit].r.right = x + w; rg_hit[rg_nhit].r.bottom = y + h; rg_hit[rg_nhit].act = act; rg_hit[rg_nhit].arg = arg; rg_hit[rg_nhit].key = key; rg_nhit++; }
}
static void rg_fonts(void) {
    if (!g_font_s) g_font_s = CreateFontW(-16, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, L"Microsoft JhengHei");
    if (!g_font_n) g_font_n = CreateFontW(-12, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, L"Arial");
    if (!g_font_b) g_font_b = CreateFontW(-28, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, L"Microsoft JhengHei");
}
static int rg_page, rg_chr, rg_sel_chr;   /* rg_sel_chr:角色名單上的游標(0–6,↑↓ 移動)*/      /* rg_chr:角色頁點進去的那個角色(0 = 在名單) */
/* 角色頁點進一個角色:設為出戰、指定夥伴、這個角色的四格裝備、裝備升級 */
static void rg_draw_chr(HDC dc, int y) {
    static const wchar_t KEYS[] = L"QWERTYU";
    wchar_t buf[200], nm[48]; int i, c = rg_chr, per = 5, pages = (M.nit + per - 1) / per, k = 0;
    if (rg_page >= pages) rg_page = pages ? pages - 1 : 0;
    rg_button(dc, 24, y, 120, 28, L"Esc ← 名單", A_CHR, 0, VK_ESCAPE, 0);
    chr_name(c, nm, 48); rg_text(dc, 160, y + 5, RGB(255, 210, 90), nm);
    rg_button(dc, 440, y, 174, 28, c == M.lead ? L"出戰中 ✓" : L"空白鍵 設為出戰", A_LEAD, c, VK_SPACE, c == M.lead); y += 34;
    rg_text(dc, 24, y + 5, RGB(180, 200, 255), L"夥伴");
    for (i = 0; i <= 7; i++) { if (i == c) continue;
        if (i) { chr_name(i, nm, 48); _snwprintf(buf, 200, L"%c %s", KEYS[k], nm); } else _snwprintf(buf, 200, L"%c 不帶", KEYS[k]);
        buf[199] = 0; rg_button(dc, 66 + (k % 4) * 138, y + (k / 4) * 30, 134, 28, buf, A_PT, i, KEYS[k], M.pt[c] == i); k++; }
    y += 66;
    for (i = 0; i < 4; i++) { int j = M.eq[c][i] ? it_find(M.eq[c][i]) : -1; if (j >= 0) { def_name(game_itemdef, M.eq[c][i], nm, 48); _snwprintf(buf, 200, L"%s:%s Lv%d", EQ_SLOTNAME[i], nm, M.it[j].lv); } else _snwprintf(buf, 200, L"%s:(基本)", EQ_SLOTNAME[i]);
        buf[199] = 0; rg_text(dc, 24 + i * 152, y, RGB(255, 255, 160), buf); }
    y += 22;
    _snwprintf(buf, 200, L"裝備:點 = 裝上 / 卸下(升級、合成、加屬性在 F5 裝備頁)  第 %d / %d 頁", rg_page + 1, pages ? pages : 1); rg_text(dc, 24, y, RGB(180, 200, 255), buf); y += 22;
    if (!M.nit) rg_text(dc, 32, y, RGB(200, 200, 200), L"還沒有裝備。精英 60%、頭目一定掉,小怪 1%。");
    for (i = rg_page * per; i < M.nit && i < (rg_page + 1) * per; i++) {
        int s_ = eq_slot(M.it[i].id), on = s_ >= 0 && M.eq[c][s_] == M.it[i].id, key = '1' + (i - rg_page * per), na = it_naf(i);
        it_label(M.it[i].id, M.it[i].lv, nm, 48);
        if (na) _snwprintf(buf, 200, L"%c %s ×%d  屬性 %d 條%s", key, nm, M.it[i].n, na, on ? L" [裝著]" : L"");
        else _snwprintf(buf, 200, L"%c %s ×%d%s", key, nm, M.it[i].n, on ? L" [裝著]" : L"");
        buf[199] = 0; rg_button(dc, 24, y, 590, 28, buf, A_EQUIP, i, key, on); y += 31;
    }
    if (pages > 1) { rg_button(dc, 24, 396, 120, 26, L"PgUp 上一頁", A_PAGE, -1, VK_PRIOR, rg_page > 0 ? 0 : 2); rg_button(dc, 150, 396, 120, 26, L"PgDn 下一頁", A_PAGE, 1, VK_NEXT, rg_page < pages - 1 ? 0 : 2); }
}
/* F5 裝備頁:↑↓ 選一件,Q 升級 / W 合成 / E 加屬性,空白鍵 給出戰角色裝上 / 卸下 */
static int rg_sel_it;
static void rg_draw_items(HDC dc, int y) {
    wchar_t buf[200], nm[48], b2[48]; int i, k, per = 6, page, pages = (M.nit + per - 1) / per;
    if (rg_sel_it >= M.nit) rg_sel_it = M.nit ? M.nit - 1 : 0; if (rg_sel_it < 0) rg_sel_it = 0;
    page = rg_sel_it / per;
    _snwprintf(buf, 200, L"裝備:↑↓ 選、空白鍵 出戰角色裝上 / 卸下   第 %d / %d 頁", page + 1, pages ? pages : 1); rg_text(dc, 24, y, RGB(180, 200, 255), buf); y += 24;
    if (!M.nit) { rg_text(dc, 32, y, RGB(200, 200, 200), L"還沒有裝備。精英 60%、頭目一定掉,小怪 1%。"); return; }
    for (i = page * per; i < M.nit && i < (page + 1) * per; i++) {
        int s_ = eq_slot(M.it[i].id), on = s_ >= 0 && M.eq[M.lead][s_] == M.it[i].id, na = it_naf(i);
        it_label(M.it[i].id, M.it[i].lv, nm, 48);
        if (na) _snwprintf(buf, 200, L"%s ×%d  屬性 %d 條%s", nm, M.it[i].n, na, on ? L"  [出戰角色裝著]" : L"");
        else _snwprintf(buf, 200, L"%s ×%d%s", nm, M.it[i].n, on ? L"  [出戰角色裝著]" : L"");
        buf[199] = 0; rg_button(dc, 24, y, 590, 27, buf, A_SELIT, i, 0, i == rg_sel_it); y += 29;
    }
    y = 92 + 24 + per * 29 + 4;
    i = rg_sel_it;
    {   int n = 0; n += _snwprintf(buf + n, 200 - n, L"附加屬性:");
        if (!it_naf(i)) n += _snwprintf(buf + n, 200 - n, L"沒有");
        for (k = 0; k < 3; k++) if (M.it[i].af[k] >= 1 && M.it[i].af[k] <= AF_N) n += _snwprintf(buf + n, 200 - n, L"%s%s", k ? L"、" : L"", AF[M.it[i].af[k]].name);
        buf[199] = 0; rg_text(dc, 24, y, RGB(255, 200, 255), buf); y += 24; }
    {   int c1 = cost_item(M.it[i].lv), c2 = cost_synth(M.it[i].id), c3 = cost_affix(i), nx;
        if (!c1) _snwprintf(buf, 200, L"Q 升級 已滿級"); else _snwprintf(buf, 200, L"Q 升級 2 件 價 %d", c1);
        rg_button(dc, 24, y, 190, 28, buf, A_UPITEM, i, 'Q', (c1 && M.it[i].n >= 2 && M.money >= c1) ? 0 : 2);
        if (!c2) _snwprintf(buf, 200, L"W 合成 最高階了"); else _snwprintf(buf, 200, L"W 合成 2 件 價 %d", c2);
        rg_button(dc, 222, y, 190, 28, buf, A_SYNTH, i, 'W', (c2 && M.it[i].n >= 2 && M.money >= c2) ? 0 : 2);
        if (!c3) _snwprintf(buf, 200, L"E 加屬性 已 3 條"); else _snwprintf(buf, 200, L"E 加屬性 3 件 價 %d", c3);
        rg_button(dc, 420, y, 194, 28, buf, A_AFFIX, i, 'E', (c3 && M.it[i].n >= 3 && M.money >= c3) ? 0 : 2); y += 32;
        if (c2) { it_tier(M.it[i].id, &nx); def_name(game_itemdef, nx, b2, 48); _snwprintf(buf, 200, L"升級:等級 +1  合成:%d%% 變成「%s」,不然兩件都不見  加屬性:留 1 件、用掉 2 件", g_synth_pct, b2); }
        else _snwprintf(buf, 200, L"升級:等級 +1(每級 +25%%)  加屬性:留 1 件、用掉 2 件,隨機多一條屬性");
        buf[199] = 0; rg_text(dc, 24, y, RGB(180, 180, 200), buf); y += 22;
    }
    if (rg_it_msg[0]) rg_text(dc, 24, y, RGB(120, 255, 160), rg_it_msg);
}
static void rg_draw_hub(HDC dc) {
    static const wchar_t *tabs[] = { L"角色", L"地圖", L"商店", L"被動", L"裝備" };
    wchar_t buf[200], nm[48]; int i, y;
    rg_box(dc, 0, 0, 640, 480, RGB(14, 14, 30), RGB(200, 170, 80));
    SelectObject(dc, g_font_b); rg_text(dc, 20, 8, RGB(255, 210, 90), L"倖存者");
    SelectObject(dc, g_font_s);
    _snwprintf(buf, 200, L"金錢 %d    過關 %d / %d 張    出戰 %d 次", M.money, M.cleared, RG_NMAP, M.runs); rg_text(dc, 150, 18, RGB(255, 255, 160), buf);
    for (i = 0; i < RG_NTAB; i++) { _snwprintf(buf, 200, L"F%d %s", i + 1, tabs[i]); rg_button(dc, 20 + i * 120, 50, 114, 30, buf, A_TAB, i, VK_F1 + i, rg_tab == i); }
    y = 92;
    if (rg_tab == 4) rg_draw_items(dc, y);
    else if (rg_tab == 0 && rg_chr) rg_draw_chr(dc, y);
    else if (rg_tab == 0) {
        static const int all[] = { 1, 2, 3, 4, 5, 6, 7 }; int c = cost_geo(800, 2.0, M.nchars - 1), k = 0;
        rg_text(dc, 24, y, RGB(180, 200, 255), L"角色:↑↓ 選、空白鍵 進入 / 買卡(也可以點);進入 → 裝備、升級、夥伴、設為出戰"); y += 26;
        for (i = 0; i < 7; i++) {
            int own = 0, j; wchar_t pn[48]; for (j = 0; j < M.nchars; j++) if (M.chars[j] == all[i]) own = 1;
            chr_name(all[i], nm, 48); k++;
            if (own) { int p_ = M.pt[all[i]]; if (p_ >= 1 && p_ <= 7 && p_ != all[i]) chr_name(p_, pn, 48); else wcscpy(pn, L"不帶");
                _snwprintf(buf, 200, L"%d. %s   夥伴:%s%s", k, nm, pn, all[i] == M.lead ? L"   ← 出戰" : L""); buf[199] = 0; rg_button(dc, 24, y, 590, 28, buf, A_CHR, all[i], '0' + k, all[i] == M.lead); }
            else { _snwprintf(buf, 200, L"%d. %s   買角色卡 價 %d%s", k, nm, c, M.money < c ? L"(錢不夠)" : L""); rg_button(dc, 24, y, 590, 28, buf, A_BUYCHAR, all[i], '0' + k, M.money < c ? 2 : 0); }
            if (i == rg_sel_chr) { RECT rc = { 21, y - 3, 617, y + 31 }; HBRUSH hb = CreateSolidBrush(RGB(255, 230, 80)); FrameRect(dc, &rc, hb); InflateRect(&rc, -1, -1); FrameRect(dc, &rc, hb); DeleteObject(hb); }
            y += 31;
        }
    } else if (rg_tab == 1) {
        rg_text(dc, 24, y, RGB(180, 200, 255), L"地圖:打倒頭目解鎖下一張;打過的可以重選"); y += 26;
        for (i = 0; i < RG_NMAP; i++) {
            int open = i <= M.cleared; if (RG_MAPS[i].boss_type) chr_name(RG_MAPS[i].boss_chr, nm, 48); else wcscpy(nm, L"強化精英");
            _snwprintf(buf, 200, L"%d. %-8s  頭目:%s   %s", i + 1, RG_MAPS[i].name, nm, i < M.cleared ? L"已過關" : open ? L"可挑戰" : L"未解鎖");
            rg_button(dc, 24, y, 590, 28, buf, A_MAP, i, '1' + i, !open ? 2 : rg_sel_map == i); y += 32;
        }
    } else if (rg_tab == 2) {
        int c = cost_slot();
        rg_text(dc, 24, y, RGB(180, 200, 255), L"商店(角色卡在「角色」頁;裝備升級、合成、加屬性在「裝備」頁)"); y += 26;
        if (c) _snwprintf(buf, 200, L"1. 技能格 第 %d 格   價 %d%s", M.slots + 1, c, M.money < c ? L"(錢不夠)" : L""); else _snwprintf(buf, 200, L"1. 技能格 已滿(6 格)");
        rg_button(dc, 24, y, 590, 28, buf, A_BUYSLOT, 0, '1', (!c || M.money < c) ? 2 : 0); y += 34;
        _snwprintf(buf, 200, L"現在技能格:%d 格(3 格免費,最多 6 格;一局裡學到的招放進格子,滿了就不再出學招)", M.slots); rg_text(dc, 32, y, RGB(200, 200, 200), buf);
    } else {
        rg_text(dc, 24, y, RGB(180, 200, 255), L"被動技能:打倒頭目掉 1 個。主角、夥伴各裝 1 個;同一個只有一份"); y += 26;
        if (!M.npas) rg_text(dc, 32, y, RGB(200, 200, 200), L"還沒有被動技能。");
        for (i = 0; i < M.npas && i < 9; i++) { int id = M.pas[i]; if (id < 1 || id > RG_NPAS) continue;
            _snwprintf(buf, 200, L"%s:%s%s", RG_PAS[id].name, RG_PAS[id].desc, RG_PAS[id].holder ? L"" : L"(全隊)"); rg_text(dc, 28, y + 5, RGB(255, 255, 255), buf);
            rg_button(dc, 400, y, 100, 28, M.pas_eq[0] == id ? L"主角 ✓" : L"給主角", A_PAS, id, 0, M.pas_eq[0] == id);
            rg_button(dc, 508, y, 106, 28, M.pas_eq[1] == id ? L"夥伴 ✓" : L"給夥伴", A_PAS, id + 100, 0, M.pas_eq[1] == id); y += 32; }
    }
    { wchar_t pn[48]; int p_ = rg_pt_pick(); chr_name(M.lead, nm, 48); if (p_) chr_name(p_, pn, 48); else wcscpy(pn, L"不帶夥伴");
      _snwprintf(buf, 200, L"Enter 開始:%s / %s + %s", RG_MAPS[rg_sel_map].name, nm, pn); buf[199] = 0; }
    if (rg_hub_msg[0]) rg_text(dc, 24, 404, RGB(255, 170, 90), rg_hub_msg);
    rg_button(dc, 24, 430, 400, 36, buf, A_START, 0, VK_RETURN, 1);
    rg_button(dc, 440, 430, 176, 36, L"Esc 離開", A_CLOSE, 0, (rg_tab == 0 && rg_chr) ? 0 : VK_ESCAPE, 0);
}
static void rg_draw_result(HDC dc) {
    wchar_t buf[200]; int y = 120;
    rg_box(dc, 70, 90, 570, 400, RGB(14, 14, 30), RGB(200, 170, 80));
    SelectObject(dc, g_font_b);
    rg_text(dc, 250, 100, rg_res.win ? RGB(120, 255, 120) : RGB(255, 90, 90), rg_res.win ? L"過關!" : L"失敗");
    SelectObject(dc, g_font_s); y = 150;
    _snwprintf(buf, 200, L"%s    時間 %02u:%02u    擊殺 %d", RG_MAPS[rg_map].name, rg_res.ms / 60000, rg_res.ms / 1000 % 60, rg_res.kills); rg_text(dc, 92, y, RGB(255, 255, 255), buf); y += 28;
    _snwprintf(buf, 200, L"這局撿到的錢  +%d", rg_res.money); rg_text(dc, 92, y, RGB(255, 255, 160), buf); y += 24;
    if (rg_res.clear) { _snwprintf(buf, 200, L"過關獎勵      +%d", rg_res.clear); rg_text(dc, 92, y, RGB(255, 255, 160), buf); y += 24; }
    _snwprintf(buf, 200, L"金錢合計       %d", rg_res.total); rg_text(dc, 92, y, RGB(255, 255, 255), buf); y += 30;
    if (rg_res.unlocked >= 0) { _snwprintf(buf, 200, L"解鎖新地圖:%s", RG_MAPS[rg_res.unlocked].name); rg_text(dc, 92, y, RGB(120, 220, 255), buf); y += 24; }
    if (rg_res.drops[0]) { rg_text(dc, 92, y, RGB(120, 220, 255), rg_res.drops); y += 24; }
    if (rg_res.pas) { _snwprintf(buf, 200, L"頭目掉落被動技能:%s(%s)", RG_PAS[rg_res.pas].name, RG_PAS[rg_res.pas].desc); rg_text(dc, 92, y, RGB(255, 180, 255), buf); y += 24; }
    if (!rg_res.win) rg_text(dc, 92, y, RGB(200, 200, 200), L"錢和撿到的裝備都留著;等級、屬性、這局學的招不留。");
    rg_button(dc, 220, 352, 200, 36, L"Enter 回倖存者選單", A_OK, 0, VK_RETURN, 1);
}
static void ng_paint_box(HDC dc) {
    wchar_t buf[64]; HGDIOBJ of;
    rg_fonts(); of = SelectObject(dc, g_font_s); SetBkMode(dc, TRANSPARENT);
    rg_box(dc, 70, 150, 570, 300, RGB(14, 14, 30), RGB(200, 170, 80));
    rg_text(dc, 92, 166, RGB(255, 210, 90), L"第一次開局:正在幫你跳過原作開場");
    rg_text(dc, 92, 198, RGB(255, 255, 255), L"大約 2–3 分鐘,請不要動滑鼠和鍵盤");
    rg_text(dc, 92, 224, RGB(200, 200, 200), L"(畫面會自己切換、對話會自己跳過,是正常的)");
    _snwprintf(buf, 64, L"已經過 %u 秒", (GetTickCount() - g_ng_t0) / 1000); buf[63] = 0; rg_text(dc, 92, 258, RGB(160, 220, 255), buf);
    SelectObject(dc, of);
}
static void rg_draw(HDC dc) {
    HGDIOBJ of; wchar_t buf[200], nm[48]; int n;
    rg_fonts(); rg_nhit = 0;
    of = SelectObject(dc, g_font_s); SetBkMode(dc, TRANSPARENT);
    if (rg_ui == UI_HUB) rg_draw_hub(dc);
    else if (rg_ui == UI_RESULT) rg_draw_result(dc);
    else if (rg_state == RG_LOAD || rg_state == RG_GOTO || rg_state == RG_SETUP) {
        if (g_ng_busy) ng_paint_box(dc);      /* 沒存檔、自動開新遊戲中(約 2–3 分鐘) */
        else {
        rg_box(dc, 180, 200, 460, 250, RGB(14, 14, 30), RGB(200, 170, 80));
        _snwprintf(buf, 200, L"進入 %s …", RG_MAPS[rg_map].name); rg_text(dc, 200, 216, RGB(255, 255, 255), buf);
        }
    } else if (rg_state == RG_HOME) {
        rg_box(dc, 180, 200, 460, 250, RGB(14, 14, 30), RGB(200, 170, 80)); rg_text(dc, 200, 216, RGB(255, 255, 255), L"回到原本的地圖 …");
    } else if (g_rg) {
        DWORD ms = surv_ms(); int x0 = 170, w = 300, y0 = 370;
        {   /* HUD 右邊那格(原作的夥伴欄,還畫著存檔裡的夥伴)蓋掉,改畫這一局的夥伴 */
            unsigned char *po = rg_pt_obj(), *psb = po ? *(unsigned char **)(po + 0x1a4) : NULL;
            rg_box(dc, 406, 392, 588, 479, RGB(49, 57, 66), RGB(90, 100, 110));
            if (psb) { int hp = I32(psb, 0x50), mx = I32(psb, 0x54), fw; if (hp < 0) hp = 0; if (mx < 1) mx = 1; fw = hp * 150 / mx; if (fw > 150) fw = 150;
                chr_name(rg_pt_chr, nm, 48); _snwprintf(buf, 200, L"夥伴 %s  Lv %d", nm, rg_pt_lv); rg_text(dc, 416, 400, RGB(160, 255, 160), buf);
                rg_box(dc, 416, 428, 568, 440, RGB(20, 10, 10), RGB(0, 0, 0)); if (fw > 0) rg_box(dc, 417, 429, 417 + fw, 439, RGB(230, 40, 20), RGB(230, 40, 20));
                _snwprintf(buf, 200, L"命 %d / %d", hp, mx); rg_text(dc, 416, 446, RGB(255, 255, 255), buf);
            } else { rg_text(dc, 416, 412, RGB(170, 170, 170), rg_pt_chr ? L"夥伴:倒下了" : L"夥伴:沒帶"); if (!rg_pt_chr) rg_text(dc, 416, 436, RGB(140, 140, 140), L"(在角色頁指定)"); }
        }
        {   /* 主角的命墊高了 g_hpbuf → 把原作 HUD 的命條和數字蓋掉,畫真正的命 */
            unsigned char *gs_ = GAME_GS, *pl_ = gs_ ? *(unsigned char **)(gs_ + 8) : NULL, *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL;
            if (sb_ && g_hpbuf) { int hp = I32(sb_, 0x50) - g_hpbuf, mx = I32(sb_, 0x54) - g_hpbuf, fw; if (hp < 0) hp = 0; if (mx < 1) mx = 1; fw = hp * 96 / mx; if (fw > 96) fw = 96;
                rg_box(dc, 99, 422, 236, 436, RGB(49, 57, 66), RGB(49, 57, 66)); rg_box(dc, 100, 424, 197, 434, RGB(20, 10, 10), RGB(0, 0, 0));
                if (fw > 0) rg_box(dc, 100, 424, 100 + fw, 434, RGB(230, 40, 20), RGB(230, 40, 20));
                _snwprintf(buf, 200, L"%d", hp); SelectObject(dc, g_font_n); rg_text(dc, 201, 422, RGB(255, 255, 255), buf); SelectObject(dc, g_font_s); }
        }
        if (rg_boss_id) {
            unsigned char *o = rg_obj(rg_boss_id), *sb = o ? *(unsigned char **)(o + 0x1a4) : NULL; int hp = sb ? I32(sb, 0x50) : 0, fw;
            if (hp < 0) hp = 0; fw = rg_boss_hpmax > 0 ? (int)((double)hp * w / rg_boss_hpmax) : 0; if (fw > w) fw = w;
            chr_name(rg_boss_chr, nm, 48);
            rg_box(dc, x0, y0, x0 + w, y0 + 16, RGB(40, 0, 0), RGB(255, 255, 255)); if (fw > 2) rg_box(dc, x0 + 1, y0 + 1, x0 + fw - 1, y0 + 15, rg_enraged ? RGB(255, 120, 0) : RGB(220, 30, 30), rg_enraged ? RGB(255, 120, 0) : RGB(220, 30, 30));
            SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(0, 0, 0));
            n = _snwprintf(buf, 200, L"頭目 %s%s  %d / %d", nm, rg_enraged ? L"(狂暴)" : L"", hp, rg_boss_hpmax); rg_text(dc, x0, y0 - 20, RGB(255, 200, 80), buf);
        } else {
            int left = (int)(RG_T_BOSS - ms) / 1000; if (left < 0) left = 0;
            SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(0, 0, 0));
            _snwprintf(buf, 200, L"頭目 %d:%02d 後出場   精英 %d / 3", left / 60, left % 60, rg_elite_n); rg_text(dc, x0, y0 - 2, RGB(255, 200, 80), buf);
        }
    } else if (!rg_in_stage() && GAME_GS && rg_state == RG_OFF) {      /* 標題畫面:加一顆「倖存者」 */
        rg_box(dc, RG_TITLE_BTN.left, RG_TITLE_BTN.top, RG_TITLE_BTN.right, RG_TITLE_BTN.bottom, RGB(90, 20, 20), RGB(255, 210, 90));
        SelectObject(dc, g_font); rg_text(dc, RG_TITLE_BTN.left + 14, RG_TITLE_BTN.top + 9, RGB(255, 230, 140), L"倖存者(F9)");
    }
    SetBkMode(dc, OPAQUE); SetBkColor(dc, RGB(0, 0, 0));
    if (of) SelectObject(dc, of);
}
static void rg_hub_open(void) {
    if (rg_state != RG_OFF && rg_state != RG_RESULT) return;
    rg_state = RG_OFF; rg_ui = UI_HUB; rg_dirty = 1; rg_chr = 0; rg_movie_hook();
    if (rg_sel_map > M.cleared || rg_sel_map >= RG_NMAP) rg_sel_map = M.cleared < RG_NMAP ? M.cleared : RG_NMAP - 1;
    mlog("hub open (money %d, cleared %d)", M.money, M.cleared);
}
static void rg_hub_close(void) {
    unsigned char *app = GAME_APP;
    rg_ui = UI_NONE; rg_state = RG_OFF;
    __try { if (app && app[0x12]) v1_pause(0); } __except (EXCEPTION_EXECUTE_HANDLER) { }
    if (rg_home_stage >= 0 && rg_in_stage()) {      /* 玩過了:回到進倖存者之前的那張圖、那個位置,主角 / 夥伴換回原本的 */
        char a[48]; _snprintf(a, 48, "%d %d %d", rg_home_stage, rg_home_x, rg_home_y);
        rg_state = RG_HOME; goto_map_cmd(a); rg_t = GetTickCount();
    }
    mlog("hub closed");
}
static void rg_act(int act, int arg) {
    rg_dirty = 1;
    switch (act) {
    case A_TAB: rg_tab = arg; rg_chr = 0; rg_it_msg[0] = 0; rg_hub_msg[0] = 0; break;
    case A_SELIT: if (arg >= 0 && arg < M.nit) rg_sel_it = arg; break;
    case A_SYNTH: if (arg >= 0 && arg < M.nit) it_synth(arg); break;
    case A_AFFIX: if (arg >= 0 && arg < M.nit) it_affix(arg); break;
    case A_CHR: { int j, own = 0; if (arg >= 1 && arg <= 7) rg_sel_chr = arg - 1; for (j = 0; j < M.nchars; j++) if (M.chars[j] == arg) own = 1; rg_chr = own ? arg : 0; rg_page = 0; break; }
    case A_PT: if (rg_chr >= 1 && rg_chr <= 7 && arg >= 0 && arg <= 7 && arg != rg_chr) { M.pt[rg_chr] = arg; meta_save(); mlog("hub: char %d partner -> %d", rg_chr, arg); } break;
    case A_MAP: if (arg <= M.cleared && arg < RG_NMAP) rg_sel_map = arg; break;
    case A_START: rg_start(rg_sel_map); break;
    case A_CLOSE: rg_hub_close(); break;
    case A_OK: rg_sel_map = (rg_res.unlocked >= 0) ? rg_res.unlocked : rg_map; rg_hub_open(); break;
    case A_LEAD: { int j; for (j = 0; j < M.nchars; j++) if (M.chars[j] == arg) { M.lead = arg; meta_save(); mlog("hub: lead -> %d", arg); } break; }
    case A_BUYCHAR: { int c = cost_geo(800, 2.0, M.nchars - 1); if (arg >= 1 && arg <= 7) rg_sel_chr = arg - 1; if (M.money >= c && M.nchars < 16) { M.money -= c; M.chars[M.nchars++] = arg; meta_save(); mlog("shop: character card %d (-%d, money %d)", arg, c, M.money); } break; }
    case A_EQUIP: if (arg >= 0 && arg < M.nit) { int s_ = eq_slot(M.it[arg].id); int c_ = rg_chr ? rg_chr : M.lead; if (s_ >= 0) { M.eq[c_][s_] = (M.eq[c_][s_] == M.it[arg].id) ? 0 : M.it[arg].id; meta_save(); mlog("hub: char %d slot %d -> item %d", c_, s_, M.eq[c_][s_]); } } break;
    case A_UPITEM: if (arg >= 0 && arg < M.nit) { int c = cost_item(M.it[arg].lv); if (c && M.it[arg].n >= 2 && M.money >= c) { M.money -= c; M.it[arg].n--; M.it[arg].lv++; meta_save();
        { wchar_t a_[40]; def_name(game_itemdef, M.it[arg].id, a_, 40); _snwprintf(rg_it_msg, 120, L"%s 升到 Lv%d(用掉 1 件)", a_, M.it[arg].lv); rg_it_msg[119] = 0; } mlog("shop: item %d -> lv %d (-%d, -1 copy, money %d)", M.it[arg].id, M.it[arg].lv, c, M.money); } } break;
    case A_PAGE: rg_page += arg; if (rg_page < 0) rg_page = 0; break;
    case A_PAS: { int who = arg >= 100, id = arg % 100; if (pas_has(id)) { if (M.pas_eq[who] == id) M.pas_eq[who] = 0; else { M.pas_eq[who] = id; if (M.pas_eq[!who] == id) M.pas_eq[!who] = 0; } meta_save(); } break; }
    case A_BUYSLOT: { int c = cost_slot(); if (c && M.money >= c) { M.money -= c; M.slots++; meta_save(); mlog("shop: skill slot -> %d (-%d, money %d)", M.slots, c, M.money); } break; }
    }
}
static int rg_ui_open(void) { return rg_ui != 0; }
static void rg_f9(void) {
    if (rg_ui == UI_HUB) rg_hub_close();
    else if (rg_ui == UI_RESULT) rg_act(A_OK, 0);
    else if (g_rg) rg_finish(0, "abandon");
    else if (g_surv || g_surv_over) surv_toggle();
    else if (rg_state == RG_OFF) rg_hub_open();
}
/* 回傳 1 = 這個訊息模組吃掉了,不給遊戲 */
static int rg_input(UINT m, WPARAM w, LPARAM l) {
    int i, x = (short)LOWORD(l), y = (short)HIWORD(l);
    if (!rg_ui) {
        if (m == WM_LBUTTONDOWN && rg_state == RG_OFF && !g_surv && GAME_GS && !rg_in_stage() && x >= RG_TITLE_BTN.left && x < RG_TITLE_BTN.right && y >= RG_TITLE_BTN.top && y < RG_TITLE_BTN.bottom) { rg_hub_open(); return 1; }
        return 0;
    }
    switch (m) {
    case WM_KEYDOWN:
        if (l & 0x40000000) return 1;
        if (rg_ui == UI_HUB && (w == VK_RIGHT || w == VK_TAB)) { rg_act(A_TAB, (rg_tab + 1) % RG_NTAB); return 1; }
        if (rg_ui == UI_HUB && w == VK_LEFT) { rg_act(A_TAB, (rg_tab + RG_NTAB - 1) % RG_NTAB); return 1; }
        if (rg_ui == UI_HUB && rg_tab == 4 && (w == VK_UP || w == VK_DOWN) && M.nit) { rg_sel_it = (rg_sel_it + (w == VK_UP ? M.nit - 1 : 1)) % M.nit; rg_dirty = 1; return 1; }
        if (rg_ui == UI_HUB && rg_tab == 4 && w == VK_SPACE && M.nit) { rg_chr = 0; rg_act(A_EQUIP, rg_sel_it); return 1; }
        if (rg_ui == UI_HUB && rg_tab == 0 && !rg_chr && (w == VK_UP || w == VK_DOWN)) { rg_sel_chr = (rg_sel_chr + (w == VK_UP ? 6 : 1)) % 7; rg_dirty = 1; return 1; }
        if (rg_ui == UI_HUB && rg_tab == 0 && !rg_chr && w == VK_SPACE) { int j, c = rg_sel_chr + 1, own = 0; for (j = 0; j < M.nchars; j++) if (M.chars[j] == c) own = 1;
            rg_act(own ? A_CHR : A_BUYCHAR, c); rg_dirty = 1; return 1; }
        if (rg_ui == UI_HUB && rg_tab == 1 && (w == VK_UP || w == VK_DOWN)) { int s = rg_sel_map + (w == VK_UP ? -1 : 1); if (s >= 0 && s <= M.cleared && s < RG_NMAP) rg_sel_map = s; rg_dirty = 1; return 1; }
        for (i = 0; i < rg_nhit; i++) if (rg_hit[i].key == (int)w) { rg_act(rg_hit[i].act, rg_hit[i].arg); break; }
        return 1;
    case WM_KEYUP: case WM_CHAR: case WM_SYSKEYDOWN: case WM_SYSKEYUP: return 1;
    case WM_LBUTTONDOWN:
        for (i = 0; i < rg_nhit; i++) if (x >= rg_hit[i].r.left && x < rg_hit[i].r.right && y >= rg_hit[i].r.top && y < rg_hit[i].r.bottom) { rg_act(rg_hit[i].act, rg_hit[i].arg); break; }
        return 1;
    case WM_LBUTTONUP: case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK: return 1;
    }
    return 0;
}
static void rg_tick(void) {
    static int loaded; DWORD now = GetTickCount(); unsigned char *app = GAME_APP;
    if (!loaded) { loaded = 1; meta_load(); }
    __try {
        if (rg_ui && rg_in_stage() && app) {
            if (!app[0x12]) v1_pause(1);
            if (rg_dirty) { rg_dirty = 0; ui_present(); }
        }
        switch (rg_state) {
        case RG_LOAD:
            if ((int)(now - rg_t) < 0) break;
            { char a[32]; _snprintf(a, 32, "%d", RG_MAPS[rg_map].stage); goto_map_cmd(a); }
            rg_state = RG_GOTO; rg_t = GetTickCount(); break;      /* 🔴 不能用上面的 now:goto_map_cmd 裡呼叫的原作函式會動到暫存器(實測 now 變垃圾) */
        case RG_GOTO:
            if (g_goto_id < 0 && !g_goto_watch && (int)(now - rg_t) > 800) {
                unsigned char *gs = GAME_GS, *pl = gs ? *(unsigned char **)(gs + 8) : NULL, *sb = pl ? *(unsigned char **)(pl + 0x1a4) : NULL; int sid = cur_stage_id();
                if (rg_in_stage() && sb && I32(sb, 0x50) > 0 && (sid == RG_MAPS[rg_map].stage || sid % 200 == RG_MAPS[rg_map].stage % 200 || sid != g_goto_from)) { rg_state = RG_SETUP; rg_t = now + 700; }
                else { mlog("ROGUE: could not enter map %d (stage now %d)", rg_map, sid); rg_state = RG_OFF; toast(L"進場失敗"); }
            } else if ((int)(now - rg_t) > (g_title_slot < 0 ? g_ng_timeout_ms : 20000)) { mlog("ROGUE: enter timeout (now %u rg_t %u goto_id %d watch %d)", now, rg_t, g_goto_id, g_goto_watch); rg_state = RG_OFF; g_goto_id = -1;
                if (g_ng_busy) { g_ng_busy = 0; g_ng_step = 0; wcscpy(rg_hub_msg, L"自動開局失敗:請先在標題選「遊戲開始」玩到能走動(會自動存檔),再按 F9 回來開局"); rg_hub_open(); } }
            break;
        case RG_SETUP:
            if ((int)(now - rg_t) < 0) break;
            rg_setup(); g_rg = 1; surv_start();
            if (!g_surv) g_rg = 0;
            else { unsigned char *gs_ = GAME_GS, *pl_ = gs_ ? *(unsigned char **)(gs_ + 8) : NULL, *sb_ = pl_ ? *(unsigned char **)(pl_ + 0x1a4) : NULL; if (sb_) pas_apply_lead(pl_, sb_); { int c_ = rg_pt_pick(); if (c_) rg_pt_join(c_); } }      /* 夥伴:角色頁指定的那個,開局就在 */
            if (g_surv) { rg_state = RG_PLAY; g_surv_next = now + 1500; mlog("ROGUE run started: map %d \"stage %d\" slots=%d", rg_map, cur_stage_id(), g_slots); }
            else rg_state = RG_OFF;
            break;
        case RG_PLAY: if (!g_rg) rg_state = RG_OFF; break;
        case RG_HOME:
            if ((g_goto_id < 0 && !g_goto_watch && (int)(now - rg_t) > 800) || (int)(now - rg_t) > 20000) { rg_home_cleanup(); rg_state = RG_OFF; mlog("home: back at stage %d", cur_stage_id()); }
            break;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { mlog("rg_tick: exception %08x state %d", GetExceptionCode(), rg_state); rg_state = RG_OFF; }
}
static void rg_cmd_meta(void) {
    int i; mlog("meta: money=%d cleared=%d slots=%d lead=%d runs=%d wins=%d", M.money, M.cleared, M.slots, M.lead, M.runs, M.wins);
    for (i = 0; i < M.nit; i++) mlog("  item %d lv %d x%d", M.it[i].id, M.it[i].lv, M.it[i].n);
    for (i = 0; i < M.npas; i++) mlog("  passive %d", M.pas[i]);
    mlog("  lead=%d partner=%d", M.lead, M.pt[M.lead]);
    mlog("  eq=%d,%d,%d,%d pas_eq=%d,%d state=%d ui=%d tab=%d sel_map=%d", M.eq[M.lead][0], M.eq[M.lead][1], M.eq[M.lead][2], M.eq[M.lead][3], M.pas_eq[0], M.pas_eq[1], rg_state, rg_ui, rg_tab, rg_sel_map);
}
static int rg_money_now(void) { return g_surv_kills + rg_bonus; }
/* 範圍內的頭目(want_boss)或頭目 / 精英(由近到遠先頭目)的物件 id;沒有回 0 */
static int rg_special_near(int px, int py, int r2, int want_boss) {
    int ids[5], i; ids[0] = rg_boss_id; for (i = 0; i < 4; i++) ids[i + 1] = want_boss ? 0 : rg_elite_id[i];
    for (i = 0; i < 5; i++) if (ids[i]) { unsigned char *o = rg_obj(ids[i]); int dx, dy;
        if (!o || !enemy_alive(o)) continue;
        dx = (I32(o, 0x14) >> 16) - px; dy = (I32(o, 0x18) >> 16) - py;
        if (dx * dx + dy * dy * 2 <= r2) return ids[i]; }
    return 0;
}
