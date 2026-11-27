# 聖石倖存者模組

《聖石傳說 EX》(V2.85)的注入式倖存者模組：一個 32-bit `dsound.dll` 代理，放進遊戲資料夾就會載入。

- 改模組、build、測試、數值在哪：看 skill `.claude/skills/sss-mod/SKILL.md`。
- 研究筆記與位址:`notes/`。

## 鐵律

- 不 commit 任何遊戲檔、解包素材、反編譯出來的原作程式碼全文;遊戲複本放 `game/`(已 gitignore)。
- 位址只對 V2.85;改原作位元組前先比對原本的內容。
- 測試會寫 `game/sss_meta.json`,測前備份、測完還原。
