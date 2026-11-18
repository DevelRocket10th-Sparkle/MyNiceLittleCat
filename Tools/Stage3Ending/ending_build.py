# ending_def 정의로 LS_Stage3_07_Ending 레벨 시퀀스를 생성 (다시 실행하면 같은 에셋을 덮어씀)
import sys, importlib, traceback, json
from pathlib import Path
import unreal as u
sys.path.insert(0, str(Path(u.Paths.project_dir()) / 'Tools' / 'Stage3Ending'))
import ending_def as D
importlib.reload(D)

ROOT = "/Game/Game/Cinematics/Stage3/"
ea = u.get_editor_subsystem(u.EditorActorSubsystem)
lib = u.EditorAssetLibrary
log = []

PLAYER_CLASS = u.load_class(None, "/Game/Game/Blueprints/Player/BP_V2__Player.BP_V2__Player_C")
PET_CLASS = u.load_class(None, "/Game/Game/Blueprints/Pet/BP_V2_Pet.BP_V2_Pet_C")


def set_completion(section, keep=False):
    mode = u.MovieSceneCompletionMode.KEEP_STATE if keep else u.MovieSceneCompletionMode.RESTORE_STATE
    try:
        section.set_completion_mode(mode)
        return
    except Exception:
        pass
    opts = section.get_editor_property("eval_options")
    opts.set_editor_property("completion_mode", mode)
    section.set_editor_property("eval_options", opts)


def key_transform(binding, keys, start, end, interp, keep=False):
    tr = binding.add_track(u.MovieScene3DTransformTrack)
    sec = tr.add_section()
    sec.set_range(int(start), int(end))
    set_completion(sec, keep)
    ch = sec.get_all_channels()
    for k in keys:
        f, x, y, z, yaw, pitch, roll = k
        vals = [x, y, z, roll, pitch, yaw, 1.0, 1.0, 1.0]
        for i in range(9):
            ch[i].add_key(u.FrameNumber(int(f)), float(vals[i]), interpolation=interp)
    return sec


def scale_keys(sec, s):
    ch = sec.get_all_channels()
    for i in (6, 7, 8):
        for k in ch[i].get_keys():
            k.set_value(float(s))


def possessable(seq, klass, label, tag):
    a = ea.spawn_actor_from_class(klass, u.Vector(0, 0, 60000), u.Rotator())
    try:
        b = seq.add_possessable(a)
        b.set_name(label)
        b.tag_binding(tag)
        return b
    finally:
        ea.destroy_actor(a)


def anims(seq, binding, items, end_limit=None, keep=False):
    if not items:
        return
    tr = binding.add_track(u.MovieSceneSkeletalAnimationTrack)
    tick = seq.get_tick_resolution().numerator / max(1, seq.get_tick_resolution().denominator)
    length = end_limit or seq.get_playback_end()
    for path, st, en, rate, offset in items:
        en = min(en, length)
        if en <= st:
            continue
        a = u.load_asset(path)
        assert a, path
        sec = tr.add_section()
        sec.set_range(int(st), int(en))
        set_completion(sec, keep)
        p = sec.get_editor_property("params")
        p.set_editor_property("animation", a)
        if rate != 1.0:
            p.set_editor_property("play_rate", u.MovieSceneTimeWarpExtensions.make_time_warp(rate))
        if offset:
            p.set_editor_property("first_loop_start_frame_offset", u.FrameNumber(int(offset * tick)))
        sec.set_editor_property("params", p)
        sec.set_range(int(st), int(en))
    return tr


def camera(seq, cuts, label, st, en, focal, keys):
    loc0 = keys[0][1]
    cam = ea.spawn_actor_from_class(u.CineCameraActor, u.Vector(*loc0), u.Rotator())
    try:
        cam.set_actor_label(label)
        cc = cam.get_cine_camera_component()
        cc.set_editor_property("current_focal_length", float(focal))
        fs = cc.get_editor_property("focus_settings")
        fs.set_editor_property("focus_method", u.CameraFocusMethod.DISABLE)
        cc.set_editor_property("focus_settings", fs)
        b = seq.add_spawnable_from_instance(cam)
        b.set_name(label)
    finally:
        ea.destroy_actor(cam)
    tk = []
    prev = None
    for f, loc, tgt in keys:
        r = u.MathLibrary.find_look_at_rotation(u.Vector(*loc), u.Vector(*tgt))
        yaw = r.yaw if prev is None else D.unwrap(prev, r.yaw)
        prev = yaw
        tk.append([f, loc[0], loc[1], loc[2], yaw, r.pitch, 0.0])
    key_transform(b, tk, st, en, u.MovieSceneKeyInterpolation.AUTO)
    sec = cuts.add_section()
    sec.set_range(int(st), int(en))
    sec.set_camera_binding_id(seq.get_binding_id(b))
    # 카메라 스폰 구간을 컷 구간으로 제한
    sp = b.find_tracks_by_type(u.MovieSceneSpawnTrack)
    if sp:
        for s in sp[0].get_sections():
            s.set_range(int(st), int(en))


def boss_double(seq, sd):
    """보스 대역: 실제 보스(사망 처리·래그돌·제거)와 분리된 스폰 스켈레탈 메시"""
    mesh = u.load_asset(D.BOSS_MESH)
    a = ea.spawn_actor_from_class(u.SkeletalMeshActor, u.Vector(0, 0, 60000), u.Rotator())
    try:
        a.set_actor_label("Boss double")
        c = a.skeletal_mesh_component
        c.set_skinned_asset_and_update(mesh)
        c.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", True)
        b = seq.add_spawnable_from_instance(a)
        b.set_name("Boss double")
    finally:
        ea.destroy_actor(a)
    end = sd.double_hide + 2
    sec = key_transform(b, D.dedupe(sd.double.keys), 0, end, u.MovieSceneKeyInterpolation.LINEAR)
    scale_keys(sec, D.BOSS_MESH_SCALE)
    anims(seq, b, sd.double_anims, end_limit=end)
    vt = b.add_track(u.MovieSceneVisibilityTrack)
    vs = vt.add_section()
    vs.set_range(0, end)
    ch = vs.get_all_channels()[0]
    ch.add_key(u.FrameNumber(0), True)
    ch.add_key(u.FrameNumber(int(sd.double_hide)), False)
    sp = b.find_tracks_by_type(u.MovieSceneSpawnTrack)
    if sp:
        for s in sp[0].get_sections():
            s.set_range(0, end)


def fade(seq, keys, length):
    tr = seq.add_track(u.MovieSceneFadeTrack)
    sec = tr.add_section()
    sec.set_range(0, int(length))
    # 끝난 뒤에도 검은 화면 유지 (RestoreState면 레벨 전환 전에 화면이 한 번 밝아짐)
    set_completion(sec, keep=True)
    ch = sec.get_all_channels()[0]
    ch.add_key(u.FrameNumber(0), 0.0, interpolation=u.MovieSceneKeyInterpolation.LINEAR)
    for f, v in keys:
        ch.add_key(u.FrameNumber(int(f)), float(v), interpolation=u.MovieSceneKeyInterpolation.LINEAR)


def build(sd):
    # Fail before touching the sequence if an animation is missing or an edit left a cut gap.
    assert sd.cams[0][1] == 0 and sd.cams[-1][2] == sd.length
    assert all(a[2] == b[1] for a, b in zip(sd.cams, sd.cams[1:])), 'Camera cut gap/overlap'
    for path, st, en, rate, offset in sd.player_anims + sd.pet_anims + sd.double_anims:
        assert en > st and rate > 0 and offset >= 0, (path, st, en)
        assert u.load_asset(path), 'Missing animation: ' + path
    path = ROOT + sd.name
    seq = lib.load_asset(path)
    if seq is None:
        seq = u.AssetToolsHelpers.get_asset_tools().create_asset(sd.name, ROOT[:-1], u.LevelSequence, u.LevelSequenceFactoryNew())
    for t in seq.get_tracks():
        seq.remove_track(t)
    for b in list(seq.get_bindings()):
        b.remove()
    seq.delete_marked_frames()
    seq.set_display_rate(u.FrameRate(24, 1))
    seq.set_playback_start(0)
    seq.set_playback_end(sd.length)
    cuts = seq.add_track(u.MovieSceneCameraCutTrack)
    for c in sd.cams:
        camera(seq, cuts, *c)

    boss_double(seq, sd)

    # Root 스케일 문제 때문에 물리 전환 없이 쓰러짐 애니메이션을 끝까지 유지.
    p = possessable(seq, PLAYER_CLASS, "Player (runtime)", "Player")
    pk = [k for k in D.dedupe(sd.player.keys) if k[0] <= sd.player_end]
    key_transform(p, pk, 0, sd.player_end, u.MovieSceneKeyInterpolation.LINEAR, keep=True)
    anims(seq, p, sd.player_anims, end_limit=sd.player_end, keep=True)

    pt = possessable(seq, PET_CLASS, "Pet (runtime)", "Pet")
    key_transform(pt, D.dedupe(sd.pet.keys), 0, sd.length, u.MovieSceneKeyInterpolation.LINEAR, keep=True)
    anims(seq, pt, sd.pet_anims, keep=True)

    fade(seq, sd.fade, sd.length)

    tr = seq.get_tick_resolution()
    ticks_per_frame = (tr.numerator / tr.denominator) / 24.0
    for f, label in sorted(sd.marks):
        m = u.MovieSceneMarkedFrame()
        m.set_editor_property("frame_number", u.FrameNumber(int(round(f * ticks_per_frame))))
        m.set_editor_property("label", label)
        seq.add_marked_frame(m)
    lib.set_metadata_tag(seq, "Stage3Generated", "codex-ending-2")
    assert lib.save_loaded_asset(seq, only_if_is_dirty=False), sd.name
    log.append({"name": sd.name, "length": sd.length, "cams": len(sd.cams), "debug": sd.debug,
                "bindings": [str(b.get_name()) for b in seq.get_bindings()],
                "marks": sorted(sd.marks)})


build_error = None
try:
    for sd in D.all_seqs():
        build(sd)
except Exception:
    build_error = traceback.format_exc()
    log.append(build_error)
report_dir = Path(u.Paths.project_saved_dir()) / 'CodexEndingWork'
report_dir.mkdir(parents=True, exist_ok=True)
(report_dir / 'build.json').write_text(json.dumps(log, indent=1, ensure_ascii=False), encoding='utf8')
if build_error:
    raise RuntimeError(build_error)
print("ENDING_BUILD_DONE", log)
