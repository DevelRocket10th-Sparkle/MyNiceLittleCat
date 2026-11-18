# 엔딩용 라쿤: 펫에게서 조종이 끊긴 인형의 무너짐 (물리 대신 관절 애니메이션)
# - 래그돌 대신 사용 (Rig_Raccoon Root 본 스케일 2.54 때문에 물리 래그돌에서 팔다리가 2.5배로 늘어남)
# - 걷기 포즈 → 지지력 상실 → 비대칭으로 툭 꺾임 → 앞/옆으로 낙하 → 팔·머리·꼬리 잔여 운동 정지
# - 손으로 바닥을 짚거나 다시 버티지 않는다. 기존 쓰러짐 에셋은 보존한다.
import sys, json, math, importlib
from pathlib import Path
import unreal as u
sys.path.insert(0, str(Path(u.Paths.project_dir()) / 'Tools' / 'Stage3Ending'))
import ending_def as D
importlib.reload(D)

OUT = Path(u.Paths.project_saved_dir()) / "CodexEndingWork" / "author_puppet_collapse.json"
OUT.parent.mkdir(parents=True, exist_ok=True)
FOLDER = "/Game/Game/Cinematics/Stage3"
NAME = "AS_Stage3_Player_PuppetCollapse"
MESH = u.load_asset("/Game/_ExternalAssets/Raccoon/Meshes/Rig_Raccoon")
WALK = u.load_asset("/Game/Game/Blueprints/Player/Animations/AS_Player_Walk")
E = u.AnimPoseExtensions
OPT = u.AnimPoseEvaluationOptions(optional_skeletal_mesh=MESH)
FPS = 30
N = 45                       # 1.5초 (마지막 포즈는 시퀀스에서 유지)
V, Q = u.Vector, u.Quat
Z = V(0, 0, 1)

# 지면 높이 (메시를 넘겨 평가한 포즈 공간 기준)
# - 이 공간에서는 Root가 단위 변환으로 나오고, 실제 렌더는 원본 Root(스케일 2.54)가 곱해져 월드 ≈ 1.016배
# - PIE 측정값으로 보정: 골반 98.6 ↔ 월드 121.1, 지면 월드 74.2 → (74.2 - 20.9) / 1.016 ≈ 52.5
GROUND = 52.5


def dot(a, b): return a.x * b.x + a.y * b.y + a.z * b.z
def cross(a, b): return V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x)
def length(a): return math.sqrt(dot(a, a))
def unit(a): return a * (1.0 / max(1e-6, length(a)))
def horiz(a): return V(a.x, a.y, 0)


def from_to(a, b):
    a, b = unit(a), unit(b)
    d = dot(a, b)
    if d < -0.9999:
        axis = unit(cross(a, V(1, 0, 0) if abs(a.x) < 0.9 else V(0, 1, 0)))
        return Q(axis.x, axis.y, axis.z, 0.0)
    c = cross(a, b)
    return Q(c.x, c.y, c.z, 1.0 + d).normalized()


def axis_q(axis, deg):
    ax = unit(axis)
    h = math.radians(deg) / 2
    s = math.sin(h)
    return Q(ax.x * s, ax.y * s, ax.z * s, math.cos(h))


def smooth(x):
    x = max(0.0, min(1.0, x))
    return x * x * (3 - 2 * x)


def ramp(f, f0, f1, v0, v1, ease="smooth"):
    if f <= f0:
        return v0
    if f >= f1:
        return v1
    t = (f - f0) / (f1 - f0)
    t = smooth(t) if ease == "smooth" else (t * t if ease == "in" else 1 - (1 - t) * (1 - t))
    return v0 + (v1 - v0) * t


# ---------------------------------------------------------------- 시작 포즈: 쓰러지는 순간의 걷기 포즈
sd = D.ending()
walk_len = WALK.get_editor_property("sequence_length")
walk_item = [a for a in sd.player_anims if a[0].endswith("AS_Player_Walk") and a[2] == sd.debug["collapse"]]
off = walk_item[0][4] if walk_item else 0.0
T_W = (off + (sd.debug["collapse"] - walk_item[0][1]) / 24.0) % walk_len if walk_item else 0.0
_p = E.get_anim_pose_at_time(WALK, T_W, OPT)
BONES = [str(n) for n in E.get_bone_names(_p)]
BASE = {n: E.get_bone_pose(_p, n, u.AnimPoseSpaces.LOCAL) for n in BONES}
# 메시 없이 평가한 원본 트랙: Root는 이 값(스케일 2.54 포함)을 그대로 써야 실제 크기로 렌더됨
_raw = E.get_anim_pose_at_time(WALK, T_W, u.AnimPoseEvaluationOptions())
RAW = {n: E.get_bone_pose(_raw, n, u.AnimPoseSpaces.LOCAL) for n in BONES}
RAW_DIFF = max((0.0 if n == "Root" else
                (RAW[n].translation - BASE[n].translation).length() + math.degrees(RAW[n].rotation.angular_distance(BASE[n].rotation)))
               for n in BONES)
PARENT = {n: str(MESH.get_bone_parent(n)) for n in BONES}


def copy_pose(p):
    return {n: t.copy() for n, t in p.items()}


def fk(local):
    w = {}
    for n in BONES:
        par = PARENT[n]
        w[n] = local[n].multiply(w[par]) if par in w else local[n]
    return w


def rotate_cs(pose, bone, dq):
    if bone not in pose:
        return
    w = fk(pose)
    new_world = dq.multiply(w[bone].rotation)
    par = PARENT[bone]
    t = pose[bone].copy()
    t.rotation = (w[par].rotation.inversed().multiply(new_world) if par in w else new_world).normalized()
    pose[bone] = t


def aim_bone(pose, bone, child, target_dir, weight):
    """bone→child 방향을 target_dir 쪽으로 weight만큼 돌림 (컴포넌트 공간)"""
    if weight <= 0 or bone not in pose or child not in pose:
        return
    w = fk(pose)
    cur = w[child].translation - w[bone].translation
    full = from_to(cur, target_dir)
    rotate_cs(pose, bone, nlerp_q(full, weight))


def nlerp_q(q, t):
    a = Q(0, 0, 0, 1)
    if dot(V(a.x, a.y, a.z), V(q.x, q.y, q.z)) + a.w * q.w < 0:
        q = Q(-q.x, -q.y, -q.z, -q.w)
    return Q(a.x + (q.x - a.x) * t, a.y + (q.y - a.y) * t, a.z + (q.z - a.z) * t, a.w + (q.w - a.w) * t).normalized()


# ---------------------------------------------------------------- 축: 정면(F), 왼쪽/오른쪽 바깥
BW = fk(BASE)
F = unit(horiz(BW["Character_LFoot"].translation - BW["Character_LCalf"].translation) * 0.0
         + horiz(BW["Character_FootBone001"].translation - BW["Character_LFoot"].translation)
         + horiz(BW["Character_FootBone0011"].translation - BW["Character_RFoot"].translation))
SIDE = unit(cross(Z, F))     # 정면 기준 한쪽 옆 (어느 쪽이든 회전축으로만 사용)


def lean(deg):
    """위(Z)를 정면(F) 쪽으로 deg만큼 기울이는 회전 (음수 = 뒤로)"""
    return axis_q(cross(Z, F), deg)


def out_dir(bone):
    return unit(horiz(BW[bone].translation - BW["Character_Ribcage"].translation))


# 지면 접촉 지점과 피부까지 여유 (컴포넌트 단위, 월드 = x0.4)
CONTACTS = {
    "Character_FootBone001": 3, "Character_FootBone0011": 3,
    "Character_LFoot": 4, "Character_RFoot": 4,
    "Character_LCalf": 5, "Character_RCalf": 5,
    "Character_Pelvis": 13, "Character_Spine2": 14, "Character_Ribcage": 14,
    "Character_Head": 16, "Character_LPalm": 3, "Character_RPalm": 3,
}

L_ARMS = [("Character_LUpperarm", "Character_LForearm", "Character_LPalm")]
R_ARMS = [("Character_RUpperarm", "Character_RForearm", "Character_RPalm"),
          ("Character_RUpperarm1", "Character_RForearm1", "Character_RPalm1"),
          ("Character_RUpperarm2", "Character_RForearm2", "Character_RPalm2")]


def pose_at(f):
    p = copy_pose(BASE)
    if f == 0:
        return p  # 걷기 마지막 포즈와 정확히 일치: 첫 프레임 지면 보정으로 튀지 않게 함
    loose = ramp(f, 0, 5, 0.0, 1.0)
    # 양쪽 다리가 시차를 두고 풀린다. 무릎을 꿇은 상태로 멈추지 않는다.
    left = ramp(f, 0, 7, 0, 24) - ramp(f, 9, 23, 0, 15)
    right = ramp(f, 1, 9, 0, 38) - ramp(f, 12, 26, 0, 18)
    for side, bend in (("L",left),("R",right)):
        rotate_cs(p, 'Character_'+side+'Thigh', lean(-bend))
        rotate_cs(p, 'Character_'+side+'Calf', lean(bend*1.7))
    tip = ramp(f, 2, 22, 0, 83, 'in') - ramp(f, 22, 27, 0, 3)
    roll = ramp(f, 3, 22, 0, -34, 'in') - ramp(f, 22, 31, 0, -4)
    # 목이 먼저 떨어지고 착지 후 옆으로 남는다. 스스로 일어나려는 반응은 없다.
    neck = ramp(f, 0, 6, 0, 20) - ramp(f, 16, 26, 0, 12)
    rotate_cs(p,'Character_Spine1',lean(ramp(f,1,9,0,6)))
    rotate_cs(p,'Character_Spine2',lean(ramp(f,2,10,0,7)))
    rotate_cs(p,'Character_Head',lean(neck))
    rotate_cs(p,'Character_Head',axis_q(Z,ramp(f,15,32,0,35)))

    # 발밑을 축으로 한 관성과 짧은 전진. Root 트랙은 쓰지 않는다.
    w = fk(p)
    pivot = (BW['Character_LFoot'].translation + BW['Character_RFoot'].translation)*0.5
    dq = axis_q(F,roll).multiply(lean(tip))
    r = p["Character_Pelvis"].copy()
    r.rotation = dq.multiply(r.rotation).normalized()
    r.translation = pivot + dq.rotate_vector(r.translation-pivot) + F*ramp(f,0,24,0,22) + SIDE*ramp(f,1,24,0,12)
    p["Character_Pelvis"] = r

    # 몸통 접촉을 먼저 계산한다. 손 위치에 맞춰 몸통을 들어 올리지 않는다.
    w = fk(p)
    low = min(w[b].translation.z-m for b,m in CONTACTS.items() if b in w and 'Palm' not in b)
    r = p["Character_Pelvis"].copy()
    r.translation = r.translation + Z*((GROUND-low)*ramp(f,0,3,0,1))
    p["Character_Pelvis"] = r

    # 팔은 바닥을 짚지 않고 중력으로 처지다가 옆으로 남는다.
    # 두 관절 IK는 회전만 바꿔 뼈 길이와 스케일을 유지한다.
    for arms, up, sign in ((L_ARMS,'Character_LUpperarm',1),(R_ARMS,'Character_RUpperarm',-1)):
        outward = out_dir(up)
        for shoulder, elbow, wrist in arms:
            if shoulder not in p: continue
            w=fk(p)
            root=w[shoulder].translation
            l1=length(w[elbow].translation-root)
            l2=length(w[wrist].translation-w[elbow].translation)
            fall=ramp(f,5,26,0,1)
            side_spread=(0.16+0.36*fall)*(1 if sign>0 else 0.85)
            target=root-Z*((l1+l2)*0.90)+outward*((l1+l2)*side_spread)-F*(fall*8)
            target.z=max(GROUND+4,target.z)
            target=w[wrist].translation*(1-loose)+target*loose
            solve_arm(p,shoulder,elbow,wrist,target,outward*0.7+Z*0.1-F*0.6)
            rotate_cs(p,wrist,axis_q(F,sign*ramp(f,4,28,0,18)))
    # 꼬리는 뒤늦게 풀려 뒤로 눕는다. 몸을 받치는 막대처럼 서지 않는다.
    tail_weight=ramp(f,3,30,0,0.95)
    # Unreal Vector의 단항 음수는 원본을 바꿀 수 있으므로 곱셈으로 방향 반전.
    for a,b in (('Character_Tail1','Character_Tail2'),('Character_Tail2','Character_Tail3')):
        w=fk(p)
        segment=length(w[b].translation-w[a].translation)
        drop=max(-segment*0.85,min(segment*0.85,GROUND+5-w[a].translation.z))
        horizontal=math.sqrt(max(0.01,segment*segment-drop*drop))
        tail_dir=unit((F*(-1.0)+SIDE*0.10)*horizontal+Z*drop)
        aim_bone(p,a,b,tail_dir,tail_weight)
    return p


def solve_arm(p, upper, lower, hand, target, bend_direction):
    w=fk(p)
    root=w[upper].translation
    l1=length(w[lower].translation-root)
    l2=length(w[hand].translation-w[lower].translation)
    delta=target-root
    d=max(abs(l1-l2)+0.01,min(length(delta),l1+l2-0.01))
    direction=unit(delta)
    a=(l1*l1-l2*l2+d*d)/(2*d)
    h=math.sqrt(max(0,l1*l1-a*a))
    bend=unit(bend_direction-direction*dot(bend_direction,direction))
    joint=root+direction*a+bend*h
    if joint.z<GROUND+4:
        # 팔꿈치가 지면 아래로 접히는 해 대신 위쪽 접힘 면을 선택한다.
        bend=unit(Z-direction*dot(Z,direction))
        joint=root+direction*a+bend*h
    aim_bone(p,upper,lower,joint-root,1)
    aim_bone(p,lower,hand,root+direction*d-fk(p)[lower].translation,1)


def write_asset(poses):
    path = FOLDER + "/" + NAME
    if u.EditorAssetLibrary.does_asset_exist(path):
        asset = u.EditorAssetLibrary.load_asset(path)
    else:
        fac = u.AnimSequenceFactory()
        fac.target_skeleton = MESH.get_editor_property("skeleton")
        fac.preview_skeletal_mesh = MESH
        asset = u.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, u.AnimSequence, fac)
    ctrl = asset.controller
    ctrl.open_bracket("Author " + NAME, False)
    try:
        ctrl.set_frame_rate(u.FrameRate(FPS, 1), False)
        ctrl.set_number_of_frames(u.FrameNumber(len(poses) - 1), False)
        ctrl.remove_all_bone_tracks(False)
        # 걷기 본 + 별도 꼬리 기저 관절만 기록. Root 트랙은 절대 추가하지 않는다.
        walk_tracks = {str(x).lower() for x in u.AnimationLibrary.get_animation_track_names(WALK)}
        walk_tracks.add('character_tail1')
        for b in BONES:
            if b.lower() not in walk_tracks:
                continue
            ks = [p[b] for p in poses]
            ctrl.add_bone_curve(b, False)
            ctrl.set_bone_track_keys(b, [k.translation for k in ks], [k.rotation for k in ks],
                                     [k.scale3d for k in ks], False)
    finally:
        ctrl.close_bracket(False)
    u.EditorAssetLibrary.save_loaded_asset(asset, False)
    return asset


res = {"RAW_DIFF_non_root": RAW_DIFF, "RAW_root": [RAW["Root"].translation.x, RAW["Root"].translation.y, RAW["Root"].translation.z, RAW["Root"].scale3d.x, RAW["Root"].rotation.x, RAW["Root"].rotation.y, RAW["Root"].rotation.z, RAW["Root"].rotation.w], "T_W": T_W, "F": [F.x, F.y, F.z], "ground": GROUND, "errors": []}
try:
    forward_before = (F.x,F.y,F.z)
    poses = [pose_at(f) for f in range(N + 1)]
    assert (F.x,F.y,F.z) == forward_before, 'Pose evaluation mutated the forward axis'
    head_path = [fk(p)['Character_Head'].translation for p in poses]
    res['max_head_step'] = max(length(b-a) for a,b in zip(head_path,head_path[1:]))
    assert res['max_head_step'] < 18, 'Discontinuous body trajectory'
    res['max_bone_length_change'] = max(
        abs(length(p[b].translation)-length(BASE[b].translation))
        for p in poses for b in BONES if b not in ('Root','Character_Pelvis'))
    res['max_scale_change'] = max(length(p[b].scale3d-BASE[b].scale3d) for p in poses for b in BONES)
    assert res['max_bone_length_change'] < 0.001
    assert res['max_scale_change'] < 0.001
    rows = []
    for f in (0, 7, 14, 20, 26, 31, 45):
        w = fk(poses[f])
        rows.append({"f": f, **{b: [round(w[b].translation.x), round(w[b].translation.y), round(w[b].translation.z)]
                                for b in ("Character_Head", "Character_Pelvis", "Character_LCalf", "Character_FootBone001", "Character_LPalm", "Character_Tail3")}})
    res["rows"] = rows
    a = write_asset(poses)
    res["length"] = a.get_play_length()
    res['tracks'] = [str(n) for n in u.AnimationLibrary.get_animation_track_names(a)]
    assert not any(n.lower()=='root' for n in res['tracks'])
except Exception:
    import traceback
    res["errors"].append(traceback.format_exc())
OUT.write_text(json.dumps(res, ensure_ascii=False, indent=1), encoding="utf-8")
print("AUTHOR_COLLAPSE_DONE")
if res['errors']:
    raise RuntimeError(res['errors'])
