---
name: sss-mod
description: 改《聖石傳說 EX》倖存者模組(這個 repo)—— 怎麼 build、怎麼放進遊戲測、數值在哪改(怪的上限、頭目血量、掉落、裝備、技能)、測試指令、位址表在哪、不能動什麼。使用者說「把怪的上限改成 50」「頭目太強」「加一種裝備屬性」「build 一下」「進遊戲試」這類跟這個模組有關的話時用。
---

# 改聖石倖存者模組

這個模組是一個 32-bit 的 `dsound.dll`:遊戲 `WUJIN.exe` 啟動時會載入它，它把所有 DirectSound 呼叫轉給系統的 `dsound.dll`,同時掛勾遊戲、在原作裡加上倖存者模式。**不改 exe、不改遊戲檔。**

## 先確認

- `game/WUJIN.exe` 存在(使用者自己從 Steam 複製來的遊戲資料夾，在 `.gitignore` 裡)。沒有就請使用者照 README「怎麼自己改」第 3 步放好。
- 遊戲版本是 **V2.85**(`game/version.ini`)。所有位址只對這個版本，別的版本不要硬改位址。
- 有 VS2022 Build Tools(`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars32.bat` 存在)。裝在別的路徑就改 `mod/build.cmd` 第 4 行。

## Build → 測

| 要做的 | 指令(在 repo 根目錄,Git Bash) |
|---|---|
| 只 build | `cmd //c "mod\\build.cmd"` → 印 `BUILD OK`,產物 `mod/build/dsound.dll` |
| build + 放進 `game/` + 重開遊戲(視窗模式) | `tools/relaunch.sh` |
| 對執行中的遊戲送測試指令 | `tools/sss.sh '指令' ['指令' …]`(寫進 `game/sss_cmd.txt`,遊戲每 0.25 秒讀一次,印最後幾行 log) |
| 截遊戲畫面 | `tools/sss.sh shot 名字` → `shots/local/名字.png`(需要 `uv`) |
| 腳本代玩一局 | `tools/rogue_test.sh 地圖編號 秒數 'bot 3'` |
| 看發生什麼 | `game/sss_mod.log`(每局開始 / 結束、升級、頭目、錯誤都會記) |

- build 有 error 就停下來修，不要拿舊的 DLL 去測。
- 改完一定進遊戲看一次，log 裡找對應的行確認(例如改怪的上限 → 開一局後看 `ROGUE wave … cap N`)。
- 測試會改 `game/sss_meta.json`(倖存者進度)。測之前先備份，測完還原。

## 常改的數值在哪

都在 `mod/src/rogue.c` 開頭附近(`/* ---- 一局的狀態` 那段)與 `mod/src/sssmod.c`:

| 想改 | 變數 | 位置 | 測試時可以不重 build 直接試的指令 |
|---|---|---|---|
| 場上怪的上限(預設 35) | `g_rg_cap` | `rogue.c` | `rgcap 50` |
| 頭目 / 精英血量倍率 | `g_boss_hpx`(22)、`g_elite_hpx`(5) | `rogue.c` | `rghpx 頭目 精英` |
| 頭目攻擊倍率(%) | `g_boss_atkx`(130) | `rogue.c` | `rgatkx N` |
| 小怪攻擊倍率(%) | `g_mob_atkx`(130) | `rogue.c` | |
| 遠程怪上限 | `g_ranged_cap`(10) | `rogue.c` | |
| 地圖與頭目 | `RG_MAPS[]` | `rogue.c` 最上面 | `rgboss 地圖 圖像 角色` |
| 裝備掉落的階級表 | `EQ_POOL[4][16]` | `rogue.c` | `rgitem 物品id 件數` |
| 裝備附加屬性(8 種) | `AF[]`、套用在 `pas_apply_lead` | `rogue.c` | |
| 合成成功率(%) | `g_synth_pct`(60) | `rogue.c` | `rgsynth N` |
| 招式冷卻 | `g_cd_base`(2500 ms)、`g_cd_per_mp` | `sssmod.c` | |
| 「學新招」給哪些招 | `g_learn_ids[]` | `sssmod.c` | |
| 一局的時間、波次 | 搜 `ROGUE wave` 附近 | `rogue.c` | `survtime 秒`、`survtscale N` |
| 錢、過關數(測試用) | — | — | `meta money N`、`meta cleared N` |

完整測試指令表:`notes/09-roguelite-run.md`「測試指令」段，和 `mod/src/sssmod.c` 檔頭註解。

## 位址表在哪

- `mod/src/sssmod.c` 檔頭：掛勾點、物件管理器、建物件 / 設陣營 / 設角色的函式位址、物件結構的偏移。
- `notes/`:每份筆記開頭寫做了什麼、位址與量測結果。
  - 01 可行性 / 注入方式;03 自動放招與升級選單;05 怪的走路;07 直接載入地圖;08 邊砍邊走、技能格、CD
  - 09 一局流程與選單;10 裝備、被動、夥伴;12 自動撿取、頭目配對;13 技能樹、掉幀、WASD
- 改原作程式碼用 `rg_patch_call(位址, 原本的呼叫目標, 新函式)`(改 E8 call)或直接寫位元組;**一定要先比對原本的位元組**,不符就不要改(可能是別的版本)。

## 不能動 / 要小心

- 🔴 **不要 commit 任何遊戲檔**(`game/` 整個在 `.gitignore`)、解包出來的素材、反編譯出來的原作程式碼全文。筆記裡引用原作只留位址和必要的幾行。
- 不要改 `WUJIN.exe` 或 Steam 資料夾裡的檔；測試一律用 `game/` 的複本。
- `RG_MAPS` 的頭目「圖像 / 角色」要是原作裡真的配對，亂配會閃退(見 notes/12)。
- 主角不會真的死：原作的死亡流程會讓之後換圖當掉，所以模組把命墊高、掉到墊高值以下算倒下。別拿掉這個機制。
- 一局中重讀原作存檔會當，開局 / 結束是靠記下隊伍數值再還原。
- 遊戲是 32-bit、MSVC `/MT`,不要加需要額外 DLL 的相依。

## 改完要交出去

1. `git add` 只加改到的原始碼 / 筆記(不要 `game/`、`mod/build/`、截圖)。
2. commit 訊息寫「改了什麼 + 為什麼 + 怎麼測的」。
3. push 到使用者自己的 fork,再到 GitHub 從 fork 開 Pull Request 回原 repo;PR 內文附測試結果(log 的那幾行)。
