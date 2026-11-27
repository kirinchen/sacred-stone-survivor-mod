# 量「越打越走不動」:開一局(原作操作、不升級、主角無敵),每 3 秒叫主角在兩點之間來回走(= 玩家點地板),
# 每 30 秒印:主角這段走了幾 px、夥伴走了幾 px、怪有幾成在動、fps。
# 用法:python tools/stuck_test.py 秒數 [開局後的指令 ...]      例:python tools/stuck_test.py 300 'survchase 0'
import re, sys, time, os
L = 'game/sss_mod.log'
secs = int(sys.argv[1]); extra = sys.argv[2:]
def cmd(*lines):
    open('game/sss_cmd.txt', 'w').write('\n'.join(lines) + '\n'); time.sleep(0.9)
def tail(n=4000):
    with open(L, 'rb') as f:
        f.seek(0, 2); sz = f.tell(); f.seek(max(0, sz - n * 160)); return f.read().decode('cp950', 'ignore').split('\n')
def snap():
    cmd('dump'); time.sleep(0.4)
    t = tail(); i = max(k for k, l in enumerate(t) if 'dump: mgr' in l); d = {}
    for l in t[i + 1:]:
        m = re.search(r'obj=(\w+) vt=\w+ type=\s*\d+ class=00000002 fac=(\w+) char=\s*(-?\d+) pos=\((-?\d+),(-?\d+)', l)
        if m: d[m[1]] = (m[2], int(m[3]), int(m[4]), int(m[5]))
    return d
n0 = sum(1 for l in open(L, 'rb') if b'ROGUE run started' in l)
cmd('rogue 0')
for _ in range(60):
    time.sleep(0.5)
    if sum(1 for l in tail(300) if 'ROGUE run started' in l) and sum(1 for l in open(L, 'rb') if b'ROGUE run started' in l) > n0: break
cmd('god 1', 'survxp 0', *extra) if extra else cmd('god 1', 'survxp 0')
pts = [(520, 480), (760, 540)]; k = 0; t0 = time.time(); win = []; prev = snap()
lead_px = pt_px = 0; en_mv = en_n = 0; last_print = t0
while time.time() - t0 < secs:
    k += 1; x, y = pts[k % 2]; cmd('goto %d %d' % (x, y)); time.sleep(1.2)
    cur = snap()
    for o, v in cur.items():
        if o not in prev: continue
        d = abs(v[2] - prev[o][2]) + abs(v[3] - prev[o][3])
        if v[0] in ('00000000', '10000000'):
            if v[1] == 1: lead_px += d
            else: pt_px += d
        else:
            en_n += 1; en_mv += d > 3
    prev = cur
    if time.time() - last_print >= 30:
        fps = [int(m) for m in re.findall(r'fps=(\d+) ', '\n'.join(tail(200)))[-25:] if int(m) > 0]
        print('t=%3ds lead %5dpx partner %5dpx enemies moving %3d%% (n=%d) fps %s' % (time.time() - t0, lead_px, pt_px, 100 * en_mv // max(en_n, 1), len(cur) - 2, (sum(fps) // len(fps)) if fps else '-'), flush=True)
        lead_px = pt_px = en_mv = en_n = 0; last_print = time.time()
cmd('hub'); time.sleep(2)
print([l[13:150] for l in tail(300) if 'ROGUE end' in l or 'AV #' in l][-2:])
