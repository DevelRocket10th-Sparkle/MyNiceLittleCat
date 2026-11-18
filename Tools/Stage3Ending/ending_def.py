# Stage3 엔딩 시퀀스 정의 (LI_Stage3_FinalBoss_Draft_v11 로컬 좌표, 24fps)
# - 키는 스테이지(레벨 인스턴스) 로컬 좌표: 디렉터가 GetStageTransform()을 TransformOrigin으로 넘김
# - 마을 서쪽 입구 등 LI 밖의 지점은 월드 좌표를 LI 로컬로 바꿔서 사용 (W2L)
# - ending_build.py / ending_preview.py 가 공용으로 사용
import math
from ending_common import Track, Seq, dedupe, unwrap, _yaw, FPS, PLAYER_HH, MAGE, PL

LI = (8525.0, -3935.0, -45.0)          # LI_Stage3_FinalBoss_Draft_v11 월드 위치 (회전 0)
PET_HH = 40.0                          # BP_V2_Pet 캡슐 반높이
PET = "/Game/Game/Blueprints/Pet/Animations/A_SnowWeasel_"
LOOK_AROUND = "/Game/Game/Cinematics/Stage3/AS_Stage3_Player_LookAround"
COLLAPSE = "/Game/Game/Cinematics/Stage3/AS_Stage3_Player_PuppetCollapse"  # 힘이 끊긴 인형, 1.5초
COLLAPSE_FRAMES = 36
BOSS_MESH = "/Game/Game/Blueprints/Enemy/Meshes/Character/EvilMageMesh"
BOSS_MESH_SCALE = 2.0
# 보스 기어가기 애니메이션: 아직 없음 → None이면 사망 포즈를 유지한 채 끌려가듯 이동 (자리 표시)
BOSS_CRAWL_ANIM = None

# 바닥 높이 (트레이스 확인값, LI 로컬)
F_ROOF = 2200.0
F_YARD = 150.0
F_PATH = 100.0
F_WORLD_PATH = 50.0                    # 월드: 보스 길 / 서쪽 입구 길 지면

# 중간 길 컷: 디딤돌 길이 곧장 마을 집들 사이로 이어져 마을 건물이 크게 보임 → 서쪽 입구로 들어가는 마지막 컷과 어긋나므로 기본 끔
USE_ROAD_SHOT = False

WALK = 5.0                             # 걷기 속도 (unit/frame = 120cm/s): 펫과 플레이어 동일


def W2L(x, y):
    """월드 XY -> LI 로컬 XY"""
    return (x - LI[0], y - LI[1])


def WF(z):
    """월드 Z(지면) -> LI 로컬 바닥 높이"""
    return z - LI[2]


def cut_at(t, f):
    """컷 직전 프레임(f-1)의 보간 위치를 키로 남기고 이후 키 제거 → 다음 컷에서 재배치"""
    k = t.sample(f - 1)
    t.keys = [x for x in t.keys if x[0] < f - 1] + [[f - 1] + list(k)]


def walk_to(t, f, pts, speed=WALK, turn_frames=6):
    return t.run(f, pts, speed, turn_frames=turn_frames)


# ---------------------------------------------------------------------------
# 배치 (옥상): 보스는 펫 쪽을 향해 쓰러지고, 플레이어는 보스 뒤에서 지켜봄
# ---------------------------------------------------------------------------
BOSS0 = (7300.0, 2800.0)
PET0 = (7020.0, 3420.0)
_d = (PET0[0] - BOSS0[0], PET0[1] - BOSS0[1])
_n = math.hypot(*_d)
DIR = (_d[0] / _n, _d[1] / _n)
YAW_B2P = math.degrees(math.atan2(DIR[1], DIR[0]))
PLAYER0 = (BOSS0[0] - DIR[0] * 480, BOSS0[1] - DIR[1] * 480)
ROOF_STAIR_TOP = (6700.0, 4100.0)      # 옥상 계단 출입구 (LS_Stage3_06에서 보스가 올라온 곳)
ROOF_STAIR_IN = (6650.0, 4350.0)
PET_WAIT = (6800.0, 3760.0)            # 펫이 몇 걸음 간 뒤 돌아보는 자리

# 마을 서쪽 입구 (월드): 문 기둥 x=-6703, 길 중심 y≈305, 바깥(서) -> 안(동) 으로 진행
GATE_X = -6703.0
# 문을 지나는 방향: 문 서쪽은 울타리 밖 빈 들판, 동쪽이 마을 안
# 2026-11-19 사용자 확인: 마을 문을 통해 "나가는" 장면이어야 함 → 동(마을 안)에서 서(들판)로
GATE_WALK_DIR = -1
LANE_PET = 395.0                       # 쓰러진 플레이어에 가리지 않도록 오른쪽 줄
LANE_PLAYER = 250.0
PAVING = 7.0                           # 포장석 두께 (트레이스는 지면에 맞음)


def ending():
    s = Seq("LS_Stage3_07_Ending", 0)
    s.pet = None
    s.pet_anims = []
    s.double = None          # 보스 대역 (스폰 액터)
    s.double_anims = []
    s.double_hide = 0
    s.fade = []              # (frame, value)
    s.player_end = 0         # 플레이어 애님/트랜스폼은 쓰러진 포즈를 시퀀스 끝까지 유지

    # ---------------- A. 옥상: 보스의 마지막 ----------------
    A1, A2, A3, A4 = 0, 64, 196, 250
    b = Track(0.0)           # 대역은 메시 루트 = 발밑
    by = YAW_B2P - 90        # 메시 정면이 +Y 이므로 -90
    b.key(0, BOSS0[0], BOSS0[1], F_ROOF, by)
    b.hold(70)
    # 세 번 끌어당기며 펫 쪽으로 (점점 약해짐)
    seg = [(70, 94, 85), (118, 142, 75), (160, 182, 55)]
    dist = 0.0
    for st, en, step in seg:
        b.hold(st)
        dist += step
        b.key(en, BOSS0[0] + DIR[0] * dist, BOSS0[1] + DIR[1] * dist, F_ROOF, by)
    # 손을 뻗듯 상체를 살짝 듦 (자리 표시: 기어가기 애님이 생기면 제거)
    k = b.last()
    b.keys.append([186, k[1], k[2], k[3] + 6, k[4], 0, -4])
    b.keys.append([204, k[1], k[2], k[3] + 6, k[4], 0, -4])
    b.hold(A3 + 20)
    s.double = b
    s.double_hide = A3 + 18          # 가루가 덮은 뒤 사라짐
    die = MAGE + "Die"
    s.double_anims = [(die, 0, 30, 1.0, 0)]
    if BOSS_CRAWL_ANIM:
        s.double_anims += [(die, 30, 70, 0.001, 27 / 24), (BOSS_CRAWL_ANIM, 70, s.double_hide, 1.0, 0)]
    else:
        s.double_anims += [(die, 30, s.double_hide, 0.001, 27 / 24)]
    dust = (BOSS0[0] + DIR[0] * dist, BOSS0[1] + DIR[1] * dist)

    pet = Track(PET_HH)
    pet.key(0, PET0[0], PET0[1], F_ROOF, YAW_B2P - 180)    # 보스 쪽을 봄
    pet.hold(286)
    # 사라진 자리를 잠깐 본 뒤, 몸을 계단 쪽으로
    pet.face(298, *PET_WAIT)
    pet.hold(330)
    pf = walk_to(pet, 330, [(PET_WAIT[0], PET_WAIT[1], F_ROOF)], speed=4.5)
    pet.hold(pf + 4)
    pet.face(pf + 16, *PLAYER0)          # 플레이어를 돌아봄
    look_back = pf + 16
    pet.hold(476)
    pet.face(484, *ROOF_STAIR_TOP)
    p2 = walk_to(pet, 486, [(ROOF_STAIR_TOP[0], ROOF_STAIR_TOP[1], F_ROOF), (ROOF_STAIR_IN[0], ROOF_STAIR_IN[1], F_ROOF)], speed=4.5)

    pl = Track(PLAYER_HH)
    pl.key(0, PLAYER0[0], PLAYER0[1], F_ROOF, YAW_B2P)
    pl.hold(452)
    pl.face(464, *PET_WAIT)              # 뒤늦게 펫을 봄
    pw0 = 470
    pl.hold(pw0)
    walk_to(pl, pw0, [(ROOF_STAIR_TOP[0], ROOF_STAIR_TOP[1], F_ROOF)])
    B2_END = 566
    s.mark(A1, "Ending:Begin")
    s.mark(A3 + 2, "Cue:BossDust")
    s.mark(look_back, "Pet looks back")

    # ---------------- C. 저택 외곽 철제 대문을 실제로 통과 ----------------
    C = B2_END
    # 컷 직전 프레임까지 옥상 위치 유지 → 컷 프레임에 앞마당으로 재배치 (화면 밖)
    for t in (pet, pl):
        cut_at(t, C)
    # 옥상~현관 이동은 생략. 저택/외곽 문 전경에서 안쪽 출발 → 바깥쪽 도착까지 보여준다.
    # 외곽 대문 x=-1700, 도로 지면 z=100 (LI 로컬), 두 줄의 이동 방향은 서쪽.
    pet.key(C, -1400, 1370, F_PATH, 180)
    pcf = walk_to(pet, C, [(-2600,1370,F_PATH)], turn_frames=1)
    pl.key(C, -1220, 1170, F_PATH, 180)
    walk_to(pl, C, [(-2420,1170,F_PATH)], turn_frames=1)
    C_EXIT = C + 60
    D = C + 170

    # ---------------- D. 보스 길 중간 (한 컷, 짧게) ----------------
    # 저택 외곽 문 ~ 마을 사이 디딤돌 길 (VillageGround/Paths/Entry: 4843,-1779 -> 2987,-1284)
    if USE_ROAD_SHOT:
        for t in (pet, pl):
            cut_at(t, D)
        r0 = W2L(4700, -1720)
        r1 = W2L(3300, -1340)
        dyaw = _yaw(r0, r1)
        off = (math.cos(math.radians(dyaw)), math.sin(math.radians(dyaw)))
        pet.key(D, r0[0] + off[0] * 220, r0[1] + off[1] * 220, WF(F_WORLD_PATH), dyaw)
        walk_to(pet, D, [(r1[0], r1[1], WF(F_WORLD_PATH))], turn_frames=1)
        pl.key(D, r0[0] - off[0] * 60, r0[1] - off[1] * 60, WF(F_WORLD_PATH), dyaw)
        walk_to(pl, D, [(r1[0] - off[0] * 280, r1[1] - off[1] * 280, WF(F_WORLD_PATH))], turn_frames=1)
    E = D + 84 if USE_ROAD_SHOT else D
    # 바깥에서 마을 집들/목조 문을 보여준 후, 펫만 먼저 밖으로 나온 시점에 뒤쪽 구도로.
    E_REAR = E + 124

    # ---------------- E. 마을 서쪽 입구 (뒤에서 고정 구도) ----------------
    for t in (pet, pl):
        cut_at(t, E)
    zf = WF(F_WORLD_PATH + PAVING)
    sg = GATE_WALK_DIR                         # +1: 서→동(마을 안으로), -1: 동→서(마을 밖으로)
    gyaw = 0.0 if sg > 0 else 180.0
    pet_x0 = GATE_X - sg * 500                 # 문까지의 반복 보행을 줄이고 문 통과에 집중
    pet_start = W2L(pet_x0, LANE_PET)
    pl_start = W2L(pet_x0 - sg * 180, LANE_PLAYER)   # 펫보다 한두 걸음 뒤
    pet.key(E, pet_start[0], pet_start[1], zf, gyaw)
    pl.key(E, pl_start[0], pl_start[1], zf, gyaw)
    # 문턱을 완전히 넘은 지점 + 한 박자
    over = W2L(GATE_X + sg * 90, LANE_PLAYER)
    f_over = walk_to(pl, E, [(over[0], over[1], zf)], turn_frames=1)
    beat = 20                                  # 약 0.8초
    collapse = f_over + beat
    fwd = W2L(GATE_X + sg * (90 + WALK * beat), LANE_PLAYER)
    pl.key(collapse, fwd[0], fwd[1], zf, gyaw)    # 멈추지 않고 걷다가 그대로 무너짐
    s.player_end = collapse            # 아래에서 L로 갱신 (쓰러짐은 애님으로, 트랙은 끝까지 유지)
    hold_after = 6 * FPS                       # 인식할 시간
    fade_len = 3 * FPS
    L = collapse + hold_after + fade_len + 12
    pet_end = W2L(pet_x0 + sg * (L - E) * WALK, LANE_PET)     # 같은 속도로 계속 걸어감 (멈추거나 돌아보지 않음)
    walk_to(pet, E, [(pet_end[0], pet_end[1], zf)], turn_frames=1)
    pet.hold(L)
    s.length = L
    s.fade = [(collapse + hold_after, 0.0), (collapse + hold_after + fade_len, 1.0), (L, 1.0)]
    s.mark(collapse, "Collapse:Player")
    pl.hold(L)
    s.player_end = L
    s.mark(L - 1, "Ending:End")

    s.pet = pet
    s.player = pl
    s.player_keep = True

    # 애니메이션
    s.player_anims = [(PL + "Idle", 0, 466, 1.0, 0), (PL + "Walk", 466, C, 1.0, 0),
                      (PL + "Walk", C, D, 1.0, 0.1), (PL + "Walk", D, E, 1.0, 0.4),
                      (PL + "Walk", E, collapse, 1.0, 0.2),
                      (COLLAPSE, collapse, collapse + COLLAPSE_FRAMES, 1.0, 0),
                      (COLLAPSE, collapse + COLLAPSE_FRAMES, L, 0.001, 1.49)]   # 마지막 포즈 유지
    s.pet_anims = [(PET + "Idle_A", 0, 330, 1.0, 0), (PET + "Walk", 330, pf, 1.0, 0),
                   (PET + "Idle_B", pf, 486, 1.0, 0), (PET + "Walk", 486, C, 1.0, 0),
                   (PET + "Walk", C, D, 1.0, 0.1), (PET + "Walk", D, E, 1.0, 0.2), (PET + "Walk", E, L, 1.0, 0)]

    # ---------------- 카메라 ----------------
    bx, byy = BOSS0
    side = (-DIR[1], DIR[0])                # 보스-펫 선의 왼쪽 수직
    mid = ((bx + PET0[0]) / 2, (byy + PET0[1]) / 2)
    # 세 인물이 다 보이는 옆 와이드 (플레이어-보스-펫 순서로 한 줄)
    cen = ((bx + PET0[0] + PLAYER0[0]) / 3 + DIR[0] * 90, (byy + PET0[1] + PLAYER0[1]) / 3 + DIR[1] * 90)
    s.cam("A1 Roof wide", A1, A2, 24, [
        (A1, (cen[0] + side[0] * 1650, cen[1] + side[1] * 1650, F_ROOF + 360), (cen[0], cen[1], F_ROOF + 90)),
        (A2 - 1, (cen[0] + side[0] * 1610, cen[1] + side[1] * 1610, F_ROOF + 352), (cen[0], cen[1], F_ROOF + 90)),
    ])
    # 보스 뒤 낮은 각도: 손 너머로 흐릿한 펫 (A2~A3, 소멸까지 같은 구도 유지)
    lowcam0 = (bx - DIR[0] * 300 + side[0] * 520, byy - DIR[1] * 300 + side[1] * 520, F_ROOF + 150)
    s.cam("A2 Reaching", A2, A4, 30, [
        (A2, lowcam0, (mid[0], mid[1], F_ROOF + 65)),
        (A4 - 1, (lowcam0[0] + DIR[0] * 65, lowcam0[1] + DIR[1] * 65, lowcam0[2]), (mid[0]+DIR[0]*50, mid[1]+DIR[1]*50, F_ROOF + 55)),
    ])
    # 펫 미디엄: 사라진 자리를 잠깐 봄 (펫 정면에서 약간 옆)
    pc = (PET0[0] - DIR[0] * 400 + side[0] * 200, PET0[1] - DIR[1] * 400 + side[1] * 200, F_ROOF + 90)
    s.cam("A4 Pet", A4, 318, 40, [
        (A4, pc, (PET0[0], PET0[1], F_ROOF + 40)),
        (317, pc, (PET0[0], PET0[1], F_ROOF + 40)),
    ])
    # 플레이어 어깨 너머: 빈 자리 + 배경에서 계단으로 가는 펫
    oc = (PLAYER0[0] - DIR[0] * 600 + side[0] * 180, PLAYER0[1] - DIR[1] * 600 + side[1] * 180, F_ROOF + 240)
    s.cam("B1 Player watches", 318, 470, 28, [
        (318, oc, (dust[0], dust[1], F_ROOF + 40)),
        (430, oc, (PET_WAIT[0], PET_WAIT[1], F_ROOF + 60)),
        (469, oc, (PET_WAIT[0], PET_WAIT[1], F_ROOF + 60)),
    ])
    # 플레이어 뒤를 같은 속도로 따라가는 카메라: 앞장선 펫과 계단 쪽
    su = (ROOF_STAIR_TOP[0] - PLAYER0[0], ROOF_STAIR_TOP[1] - PLAYER0[1])
    sl = math.hypot(*su)
    su = (su[0] / sl, su[1] / sl)
    def follow_camera(f):
        p, q = pl.sample(f), pet.sample(f)
        return ((p[0] - su[0]*760 + side[0]*180, p[1] - su[1]*760 + side[1]*180, F_ROOF+330),
                ((p[0]+q[0])*0.5, (p[1]+q[1])*0.5, F_ROOF+95))
    s.cam("B2 Follow to stairs", 470, C, 28, [
        (470, *follow_camera(470)),
        (C - 1, *follow_camera(C - 1)),
    ])
    # 저택의 외형과 철제 대문을 함께 보여주어 이전 장소를 명확히 닫는다.
    s.cam("C1 Mansion and iron gate", C, C_EXIT, 20, [
        (C, (-5000,-800,1500), (500,1500,700)),
        (C_EXIT-1, (-4960,-800,1490), (500,1500,700)),
    ])
    # 같은 동선의 더 가까운 외부 사선 구도. 두 캐릭터 모두 대문 밖으로 나온 뒤 컷.
    s.cam("C2 Leaving mansion gate", C_EXIT, D, 22, [
        (C_EXIT, (-3000,-1400,700), (-1600,1400,500)),
        (D-1, (-3040,-1400,690), (-1800,1370,430)),
    ])
    # 디딤돌 길: 뒤쪽 낮은 위치, 같은 방향으로 멀어짐
    if USE_ROAD_SHOT:
        rc = W2L(5250, -1930)
        rt = W2L(3400, -1360)
        s.cam("D Road", D, E, 32, [
            (D, (rc[0], rc[1], WF(190)), (rt[0], rt[1], WF(110))),
            (E - 1, (rc[0] - 120, rc[1] + 40, WF(188)), (rt[0], rt[1], WF(110))),
        ])
    # 마을 외부에서 보는 전경: 집들 → 목조 지붕/문턱 → 문 밖 빈 들판의 관계.
    # 저택과 같은 쪽의 이동 축을 유지하며 두 장소 사이의 이동 시간은 컷으로 생략한다.
    vloc = W2L(-8200,-1600)
    vtgt = W2L(-6450,305)
    s.cam("E1 Village exit exterior", E, E_REAR, 24, [
        (E, (vloc[0],vloc[1],WF(900)), (vtgt[0],vtgt[1],WF(330))),
        (E_REAR-1, (vloc[0]+60,vloc[1]+40,WF(880)), (vtgt[0],vtgt[1],WF(320))),
    ])
    # 펫이 경계를 넘은 뒤 뒷쪽으로 컷. 라쿤의 문 통과~쓰러짐은 끊지 않고 유지.
    sg = GATE_WALK_DIR
    ec = W2L(GATE_X - sg * 1150, 300)
    et = W2L(GATE_X + sg * 400, 320)
    s.cam("E2 Beyond village gate", E_REAR, L, 28, [
        (E_REAR, (ec[0], ec[1], WF(210)), (et[0], et[1], WF(140))),
        (L, (ec[0] + sg * 40, ec[1], WF(209)), (et[0], et[1], WF(140))),
    ])
    s.debug = {"collapse": collapse, "f_over": f_over, "E": E, "E_REAR": E_REAR, "D": D, "C": C, "C_EXIT": C_EXIT, "L": L, "look_back": look_back,
               "pet_wait": pf, "dust": dust, "pet_stair_in": p2}
    # 빈 자리를 지켜보는 6.3초 구간을 4.3초로 압축한다. 동작과 마커도 같은 시간축을 사용.
    def editorial_time(f):
        if f <= 318: return int(f)
        if f < 470: return int(round(318+(f-318)*104/152))
        return int(f-48)
    for track in (s.player, s.pet, s.double):
        track.keys = dedupe([[editorial_time(k[0])]+k[1:] for k in track.keys])
    for attr in ('player_anims','pet_anims','double_anims'):
        items=[]
        for path,st,en,rate,offset in getattr(s,attr):
            a,b=editorial_time(st),editorial_time(en)
            if b>a: items.append((path,a,b,rate*(en-st)/(b-a),offset))
        setattr(s,attr,items)
    s.cams=[(label,editorial_time(st),editorial_time(en),focal,
             [(editorial_time(f),loc,tgt) for f,loc,tgt in keys])
            for label,st,en,focal,keys in s.cams]
    s.marks=[(editorial_time(f),label) for f,label in s.marks]
    s.fade=[(editorial_time(f),value) for f,value in s.fade]
    s.double_hide=editorial_time(s.double_hide)
    s.player_end=editorial_time(s.player_end)
    s.length=editorial_time(s.length)
    for name in ('collapse','f_over','E','E_REAR','D','C','C_EXIT','L','look_back','pet_wait','pet_stair_in'):
        s.debug[name]=editorial_time(s.debug[name])
    return s


def all_seqs():
    return [ending()]
