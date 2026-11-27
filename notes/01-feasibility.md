# 01 可行性(①)

> 2026-11-09。遊戲 V2.85(`version.ini`),WUJIN.exe sha256 `a91e1636…c8240`。位址都只對這個版本。

## 結論

**可行。** 注入式模組進得去,在原作戰鬥地圖一次生 50 隻敵人會正常參戰(圍攻主角、被隊友打死、隊友升級),FPS 幾乎不掉。

| 場上敵人(洞內混戰,god 模式) | 平均 FPS | 最低 | 最慢一幀 |
|---|---|---|---|
| 0(基準) | 60.2 | 60 | 32 ms |
| 10 | 60.4 | 60 | 32 ms |
| **50** | **58.1** | **54** | 47 ms |
| ~120 | 57.5 | 50 | 47 ms |
| ~230(生了 350,邊打邊死) | 50.2 | 44 | 52 ms |

- 遊戲本身鎖 60 fps;生怪一次性成本 ≈ 1 ms / 隻(50 隻 41 ms)
- 量法:DLL 掛 `IDirectDrawSurface::Blt`,每次 Blt 到主畫面算一幀;每秒統計,`bench N` 取 N 秒平均 / 最低

## 引擎

- **不是 Unity**。宇峻自家 2D 引擎(PDB `OdinGame.pdb`),DirectDraw + DirectSound,跟大富翁4 同代
- `WUJIN.exe`:x86、imagebase 0x400000、**無 ASLR**、無 DirectInput(鍵鼠全走 WndProc)
- `GameLoader.exe`:Steam 初始化 + 把桌面切 16-bit 色深再開 WUJIN。**WUJIN 可直接開**:`WUJIN.exe -window`
- 資料:`sstone.pak` 等大包;`Script/*.so`、`Stage/*/map*.so` 是編譯過的腳本(magic `Smart Script`),不是物件表

## 注入方式

照 rich4-study:`dsound.dll` 代理(`mod/src/sssmod.c`,`mod/build.cmd` 用 VS2022 BuildTools 32-bit 建),只放進 `game/`,所有匯出轉發到 `SysWOW64\dsound.dll`。不改 exe、不用 CreateRemoteThread。

IAT 掛勾:

| 匯入 | IAT | 用途 |
|---|---|---|
| `RegisterClassA` | 0x4aa23c | 包 WndProc(類別名 `HolyStone`,原 WndProc 0x48e290) |
| `PeekMessageA` | 0x4aa278 | 主執行緒 tick:讀指令檔、生怪、god、統計 |
| `DirectDrawCreate` | 0x4aa048 | 掛 `CreateSurface`(vt 6)→ surface `Blt`(5)/`Flip`(11):量幀 + 畫 overlay + 截圖 |

### 坑:直接開 WUJIN 會花屏

遊戲寫 RGB555 像素,但桌面是 32-bit,離屏 surface 跟著變 32-bit → 畫面錯位變色。原版靠 GameLoader 改桌面色深。
解法:`CreateSurface` 掛勾裡,沒指定像素格式的離屏 surface 強制成 RGB555(主畫面不動),DirectDraw Blt 時自己轉色。**不改使用者的螢幕設定**。

### 不搶螢幕的操作方式

測試機當時在鎖定畫面,所以:
- 截圖:DLL 在 Blt 前 `Lock` back buffer 讀像素存 BMP(`shot` 指令),螢幕關 / 鎖都抓得到
- 輸入:DLL 直接呼叫原 WndProc 送 `WM_KEYDOWN` / 滑鼠訊息,不動實體鍵鼠
- 指令:寫 `game/sss_cmd.txt`,DLL 每 0.25 秒讀一次(`tools/sss.sh`)

## 遊戲內掛點(V2.85)

| 位址 | 是什麼 |
|---|---|
| `*(void**)0x4c31f0` | 物件管理器;`+0xc` 物件表(槽 1..0x800) |
| `0x44d7d0` thiscall | 生物件 `create(mgr, stage, parent, x, y, z, type, wait)`,座標 16.16 |
| `0x444b60` thiscall | 陣營 `obj+0x100 \|= set`:0 我方、1 友方 NPC、2 敵 |
| `0x444e00` thiscall | 套角色數值 `setChar(obj, charId, 1)`;怪物會依難度加成、掛 AI / 招式 |
| `0x452780` | 關卡物件載入器(16 bytes / 筆);生怪流程照它抄 |
| `0x477610(type)` | 類型表查詢;表 `*(int**)0x4c7b60`,數量 `*(int*)0x4d3fc0`,每筆 32 bytes,`[6]` = class |
| `0x427df0(charId)` | 角色數值表;`def[0]` 旗標(`&3` 隊員、`4` 雜兵、`8` 頭目),`def+4` 名字(Big5) |

物件欄位:`+4` type、`+8` class、`+0x14/+0x18/+0x1c` x/y/z(16.16)、`+0xac` stage、`+0xbc` 方向、`+0xfc` charId、`+0x100` 陣營、`+0x1a4` 數值區塊(`+0x50/+0x54` 命 現/上限、`+0x58/+0x5c` 內)。
角色類物件 = `class & 0x23e306`(小物件只有 0xf8 bytes,不能讀 `+0x100`)。

實測 ID:素還真 type 2001 / char 1、青陽子 2009 / 2;怪物圖像 type 約 2071–2115;蝙蝠 2071 / 11、黃刀手 2077 / 13。

## 隊友切換 / 放招(e,只做到一半)

遊戲內熱鍵(遊戲的「熱鍵說明」):

- **Q 更換戰友**、E 收 / 放戰友、R 戰友攻擊模式(跟隨 / 保護 / 獨立)
- F1–F4、Z / X、滾輪:切主角絕招;W/A/S/D:八卦迷蹤步;Ctrl:原地攻擊;Shift:點擊施展
- 移動 / 攻擊:按住左鍵(實測)

DLL 已能把按鍵送進 WndProc(動畫用 Esc 跳過實測有效),所以**最簡單的掛法是模擬這些鍵**。
還沒做:找 WndProc 裡處理 Q / F1–F4 的函式(要程式直接呼叫時用);這個進度只有一位戰友,Q 無法實測。

## 還沒做 / 已知限制

- 生在城鎮地圖的怪不會動(城鎮不開戰鬥);野外 / 地城才會開打
- 錄影(OBS)沒做;素材只有截圖
- 沒存檔:每次重開要重跑劇情到「琉璃仙洞」(`tools/to_cave.sh`,約 4 分鐘,隨機探索)
