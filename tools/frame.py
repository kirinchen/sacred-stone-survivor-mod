# 從錄影抽一幀存 PNG 並印資訊:uv run -q --with opencv-python-headless python tools/frame.py clip.mp4 out.png [秒]
import sys, cv2
c = cv2.VideoCapture(sys.argv[1])
fps = c.get(cv2.CAP_PROP_FPS); n = c.get(cv2.CAP_PROP_FRAME_COUNT)
w = int(c.get(cv2.CAP_PROP_FRAME_WIDTH)); h = int(c.get(cv2.CAP_PROP_FRAME_HEIGHT))
t = float(sys.argv[3]) if len(sys.argv) > 3 else (n / fps / 2 if fps else 0)
c.set(cv2.CAP_PROP_POS_MSEC, t * 1000)
ok, f = c.read()
print(f"{w}x{h} fps={fps:.2f} dur={n / fps if fps else 0:.1f}s frame@{t:.1f}s ok={ok} mean={f.mean() if ok else -1:.1f}")
if ok: cv2.imwrite(sys.argv[2], f)
