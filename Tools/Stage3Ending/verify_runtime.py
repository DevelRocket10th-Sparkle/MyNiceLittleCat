"""PIE smoke test using a synthetic Stage3 defeat, never changes saved level state.

Execute before starting PIE. Captures real game frames and restores editor throttling.
This validates the defeat callback path; it does not replace a complete boss fight.
"""
import unreal as u, time, json, traceback, sys
from pathlib import Path
sys.path.insert(0,str(Path(u.Paths.project_dir())/'Tools/Stage3Ending'))
import ending_def
test_collapse_frame=ending_def.ending().debug['collapse']
test_out=Path(u.Paths.project_saved_dir())/'CodexEndingWork'/globals().pop('RUNTIME_NAME','runtime_final')
test_out.mkdir(exist_ok=True)
test_state={'start':time.time(),'phase':0,'samples':[],'events':[],'last_sample':-100}
test_perf=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
test_state['throttle']=test_perf.get_editor_property('bThrottleCPUWhenNotForeground')
test_perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def test_finish():
    test_perf.set_editor_property('bThrottleCPUWhenNotForeground',test_state['throttle'])
    u.unregister_slate_post_tick_callback(test_handle[0])
    (test_out/'report.json').write_text(json.dumps(test_state,indent=2,default=str),encoding='utf8')
    print('CODEX_RUNTIME_DONE')
def test_tick(dt):
    try:
        w=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not w:return
        elapsed=time.time()-test_state['start']
        if elapsed>100: test_state['events'].append('timeout');test_finish();return
        if 'TitleScreen' in w.get_name():
            test_state['events'].append('title_reached');test_finish();return
        pc=u.GameplayStatics.get_player_character(w,0)
        if test_state['phase']==0:
            if elapsed<3 or not pc:return
            director=u.GameplayStatics.get_all_actors_of_class(w,u.Stage3BossDirector)[0]
            boss=director.get_current_boss()
            manager=director.get_editor_property('encounter')
            test_state['events'].append(['start_encounter',manager.start_encounter(pc)])
            for brain in boss.get_controller().get_components_by_class(u.BrainComponent): brain.stop_logic('Cinematic validation')
            boss.set_actor_tick_enabled(False)
            manager.call_method('HandleBossDefeated',(boss,))
            test_state['phase']=1
            test_state['trigger']=time.time()
            return
        sequences=u.GameplayStatics.get_all_actors_of_class(w,u.LevelSequenceActor)
        active=[a for a in sequences if a.sequence_player and a.sequence_player.is_playing()]
        if not active:return
        player=active[-1].sequence_player
        f=player.get_current_time().time.frame_number.value
        sample_step=12 if test_collapse_frame-24<=f<=test_collapse_frame+48 else 48
        if f-test_state['last_sample']<sample_step:return
        test_state['last_sample']=f
        pets=u.GameplayStatics.get_all_actors_of_class(w,u.PetCharacter)
        pet=pets[0] if pets else None
        camera=u.GameplayStatics.get_player_camera_manager(w,0)
        sample={'f':f,'player':str(pc.get_actor_location()),'player_head':str(pc.mesh.get_socket_location('Character_Head')),'pet':str(pet.get_actor_location()) if pet else None,'pet_anim':str(pet.mesh.get_anim_instance()) if pet else None,'camera':str(camera.get_camera_location())}
        test_state['samples'].append(sample)
        # Capture the actual game viewport, including cinematic fade.
        filename=str((test_out/('f%04d.png'%f)).resolve()).replace('\\','/')
        u.SystemLibrary.execute_console_command(w,'HighResShot 960x540 filename="'+filename+'"')
    except Exception:
        test_state['events'].append(traceback.format_exc());test_finish()
test_handle=[u.register_slate_post_tick_callback(test_tick)]
print('CODEX_RUNTIME_STARTED')
