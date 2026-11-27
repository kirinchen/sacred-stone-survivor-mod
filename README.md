# 聖石倖存者模組(Sacred Stone Survivor mod)

《聖石傳說 EX》的**非官方**倖存者(割草)模組：在原作裡多一個「倖存者」模式，用原作的角色、招式、隊友，一局 10 分鐘，打怪、升級、3 選 1 學招，最後打頭目；局外有角色、地圖、商店、被動、裝備(升級 / 合成 / 加屬性)。

> ⚠️ **聲明**
> - 這個 repo **不含任何遊戲檔**。要玩請自備 Steam 正版《聖石傳說 EX》。
> - 非官方、跟遊戲版權方無關。
> - **只支援 V2.85**(遊戲資料夾裡 `version.ini` 寫的版本)。其他版本位址不同，裝了可能閃退。

---

## 怎麼裝(玩家)

1. 在 Steam 買《聖石傳說 EX》並安裝好。
2. 到本 repo 的 [Releases](../../releases/latest) 下載 **`dsound.dll`**(只要這一個檔)。瀏覽器可能警告「不常下載」(Edge:SmartScreen),點下載項目旁的「…」→「保留」→「仍要保留」。
3. Steam 收藏庫 → 對《聖石傳說 EX》按右鍵 → **管理** → **瀏覽本機檔案**,會打開遊戲資料夾。
4. 把 `dsound.dll` 放進這個資料夾(跟 `WUJIN.exe` 同一層)。
5. 開遊戲。標題畫面**右上角**多一顆「**倖存者**」按鈕(或按 **F9**)→ 選地圖 → **Enter** 開一局。

> **第一次玩**:倖存者模式要借原作的存檔進遊戲。全新安裝、還沒玩過原作的話，先在標題選「遊戲開始」,玩到角色能走動(遊戲會自動存檔),再按 F9 開倖存者。已經有存檔就可以直接從標題開。
> 啟動器的「全螢幕」與各種解析度都可以。

操作跟原作一樣：點地板走、點怪打、右鍵放絕招，數字鍵 1–9 選技能格的招。升級時跳 3 選 1,用滑鼠或 1 / 2 / 3 選。倖存者選單裡 F1–F5 換頁,Esc 離開;一局中按 F9 是放棄這局。

模組會在遊戲資料夾多寫兩個檔:`sss_meta.json`(倖存者模式的進度：錢、解鎖、裝備)、`sss_mod.log`(紀錄，回報問題時附上)。

## 怎麼移除

刪掉遊戲資料夾裡的 `dsound.dll` 就好。想連進度一起清掉，再刪 `sss_meta.json`、`sss_mod.log`。原作的存檔不會被動到。

---

## 怎麼自己改(用 Claude Code 之類的 agent)

這個 repo 附了一份給 agent 用的 skill:[`.claude/skills/sss-mod/`](.claude/skills/sss-mod/SKILL.md),裡面寫好怎麼 build、怎麼測、數值在哪裡改、位址表在哪、哪些不能動。用 Claude Code 開這個資料夾，直接用講的就能改。

### 需要先裝
- Windows 10 / 11
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/),安裝時勾「**使用 C++ 的桌面開發**」(要有 32-bit / x86 的 MSVC)
- [Git](https://git-scm.com/)(附 Git Bash)
- [Claude Code](https://docs.anthropic.com/claude-code)(或其他會改程式的 agent)

### 步驟
1. 在 GitHub 按右上角 **Fork**,把這個 repo 複製一份到你的帳號。
2. 把你的 fork clone 到電腦:`git clone https://github.com/<你的帳號>/sacred-stone-survivor-mod.git`
3. 把 Steam 的整個遊戲資料夾**複製一份**到 repo 裡的 `game/`(`game/WUJIN.exe` 要存在)。`game/` 已在 `.gitignore`,不會被 commit。用複本測，就不會動到 Steam 那份。
4. 在 repo 資料夾開 Claude Code,用講的,例如:「**把怪的上限從 35 改成 50**」。
5. agent 會改 `mod/src/` 的程式、跑 `mod\build.cmd` 重新產生 `dsound.dll`(在 `mod/build/`),再用 `tools/relaunch.sh` 放進 `game/` 開遊戲。
6. 進遊戲試玩，確認改對了。
7. 想分享回來：請 agent commit、push 到你的 fork,再到 GitHub 從你的 fork **開 Pull Request** 回這個 repo。

自己手動 build:在 repo 根目錄跑 `mod\build.cmd`,成功會印 `BUILD OK`。

## 資料夾

| 路徑 | 是什麼 |
|---|---|
| `mod/src/sssmod.c` | 注入的主程式(dsound.dll 代理、掛勾、戰鬥、技能、測試指令) |
| `mod/src/rogue.c` | 倖存者模式：一局流程、選單、地圖 / 頭目表、裝備、被動、存檔 |
| `mod/build.cmd` | build 腳本(VS2022 Build Tools,32-bit) |
| `notes/` | 研究筆記：逆向找到的位址、每個功能怎麼做、量測結果 |
| `tools/` | 測試腳本(Git Bash):送指令、重開遊戲、腳本代玩 |
| `.claude/skills/sss-mod/` | 給 agent 的改模組說明 |

## 授權

本專案自己寫的程式碼採 [MIT](LICENSE)。遊戲本體與原作素材的版權屬原權利人，不包含在本 repo,也不在 MIT 範圍內。
