# Stage3 보스 연출 시퀀스 정의 (LI_Stage3_FinalBoss_Draft_v8 로컬 좌표, 24fps)
# build.py / preview.py 가 공용으로 사용
import math

FPS = 24
BOSS_HH = 160.0      # EvilMage 캡슐 반높이
PLAYER_HH = 88.0     # 플레이어 캡슐 반높이

MAGE = "/Game/Game/Blueprints/Enemy/Animations/EvilMage/AS_EvilMage_"
PL = "/Game/Game/Blueprints/Player/Animations/AS_Player_"
STUMBLE = "/Game/Game/Cinematics/Stage3/AS_Stage3_Player_StumbleRecover"

# 바닥 높이 (트레이스로 확인한 값)
F_YARD = 150.0     # 정문 앞마당(x 3600~5100)
F_1F = 200.0
F_2F = 1200.0
F_ROOF = 2200.0


def _yaw(a, b):
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))


def unwrap(prev, yaw):
    while yaw - prev > 180:
        yaw -= 360
    while yaw - prev < -180:
        yaw += 360
    return yaw


class Track:
    """액터 하나의 위치/회전 키 (frame, x, y, z, yaw[, pitch, roll])"""

    def __init__(self, hh):
        self.hh = hh
        self.keys = []

    def key(self, f, x, y, floor, yaw, lift=0.0, pitch=0.0, roll=0.0):
        z = floor + self.hh + lift
        if self.keys:
            yaw = unwrap(self.keys[-1][4], yaw)
        self.keys.append([int(round(f)), float(x), float(y), float(z), float(yaw), pitch, roll])
        return f

    def last(self):
        return self.keys[-1]

    def hold(self, f):
        k = self.last()
        self.keys.append([int(round(f))] + k[1:])
        return f

    def turn(self, f, yaw):
        k = self.last()
        self.keys.append([int(round(f)), k[1], k[2], k[3], unwrap(k[4], yaw), k[5], k[6]])
        return f

    def face(self, f, x, y):
        k = self.last()
        return self.turn(f, _yaw((k[1], k[2]), (x, y)))

    def run(self, f, pts, speed, turn_frames=4, zw=0.5):
        """pts: [(x, y, floor)], speed: unit/frame. 코너마다 진행 방향으로 빠르게 회전"""
        k = self.last()
        cur = (k[1], k[2], k[3] - self.hh - 0.0)
        for i, p in enumerate(pts):
            d = math.dist(cur[:2], p[:2]) + abs(p[2] - cur[2]) * zw
            yaw = _yaw(cur, p)
            dur = max(1.0, d / speed)
            if i == 0:
                # 출발 전 제자리 회전
                self.turn(round(f + turn_frames), yaw)
                f += turn_frames
            else:
                # 이전 점 도착 직후, 이동하면서 진행 방향으로 회전
                tf = min(turn_frames, dur * 0.5)
                a = tf / dur
                self.key(f + tf, cur[0] + (p[0] - cur[0]) * a, cur[1] + (p[1] - cur[1]) * a,
                         cur[2] + (p[2] - cur[2]) * a, yaw)
            f += dur
            self.key(f, p[0], p[1], p[2], yaw)
            cur = p
        self.keys = dedupe(self.keys)
        return int(round(f))

    def jitter(self, f0, f1, amp, step=2, seed=3):
        import random
        r = random.Random(seed)
        base = self.last()
        f = f0
        while f < f1:
            f += step
            self.keys.append([f, base[1] + r.uniform(-amp, amp), base[2] + r.uniform(-amp, amp),
                              base[3] + r.uniform(-amp * .5, amp * .5), base[4] + r.uniform(-2, 2), base[5], base[6]])
        self.keys.append([f1 + step] + base[1:])
        return f1 + step

    def sample(self, f):
        ks = sorted(self.keys, key=lambda k: k[0])
        if f <= ks[0][0]:
            return ks[0][1:]
        for a, b in zip(ks, ks[1:]):
            if a[0] <= f <= b[0]:
                t = 0 if b[0] == a[0] else (f - a[0]) / (b[0] - a[0])
                return [a[i] + (b[i] - a[i]) * t for i in range(1, 7)]
        return ks[-1][1:]


def dedupe(keys):
    out = {}
    for k in keys:
        out[k[0]] = k   # 같은 프레임은 마지막 값 우선
    return [out[f] for f in sorted(out)]


class Seq:
    def __init__(self, name, length):
        self.name = name
        self.length = length
        self.cams = []      # (label, start, end, focal, [(f, loc, target)])
        self.marks = []     # (f, label)
        self.boss = None
        self.boss_anims = []   # (asset, start, end, rate, offset_sec)
        self.player = None
        self.player_anims = []
        self.player_keep = False
        self.lights = []    # (label, loc, color, intensity, radius, [(f, visible)])

    def cam(self, label, start, end, focal, keys):
        self.cams.append((label, start, end, focal, keys))

    def mark(self, f, label):
        self.marks.append((int(f), label))


