"""PIE pose inspection in the real ending; execute before starting PIE.
Synthetic defeat and timeline scrubbing only; no persistent actors or level edits.
"""
import unreal as u, time, json, traceback, sys
from pathlib import Path
sys.path.insert(0,str(Path(u.Paths.project_dir())/'Tools/Stage3Ending'))
import ending_def
sd_preview=ending_def.ending()
collapse_preview=sd_preview.debug['collapse']
preview_out=Path(u.Paths.project_saved_dir())/'CodexEndingWork'/globals().pop('POSE_PREVIEW_NAME','puppet_poses')
preview_out.mkdir(parents=True,exist_ok=True)
preview_close=globals().pop('POSE_PREVIEW_CLOSE',False)
preview_frames=[collapse_preview+i for i in [-1,0,3,6,10,14,18,22,26,30,36,60]]
preview_state={'phase':0,'start':time.time(),'i':0,'ticks':0,'rows':[],'errors':[]}
preview_perf=u.get_default_object(u.load_class(None,'/Script/UnrealEd.EditorPerformanceSettings'))
preview_throttle=preview_perf.get_editor_property('bThrottleCPUWhenNotForeground')
preview_perf.set_editor_property('bThrottleCPUWhenNotForeground',False)
def preview_finish():
    preview_perf.set_editor_property('bThrottleCPUWhenNotForeground',preview_throttle)
    u.unregister_slate_post_tick_callback(preview_handle[0])
    (preview_out/'report.json').write_text(json.dumps(preview_state,indent=2,default=str),encoding='utf8')
    print('PUPPET_PREVIEW_DONE',preview_out)
def preview_tick(dt):
    try:
        world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
        if not world:return
        if time.time()-preview_state['start']>90:
            preview_state['errors'].append('timeout');preview_finish();return
        pc=u.GameplayStatics.get_player_character(world,0)
        if preview_state['phase']==0:
            if time.time()-preview_state['start']<3 or not pc:return
            director=u.GameplayStatics.get_all_actors_of_class(world,u.Stage3BossDirector)[0]
            boss=director.get_current_boss()
            manager=director.get_editor_property('encounter')
            assert manager.start_encounter(pc)
            for brain in boss.get_controller().get_components_by_class(u.BrainComponent):brain.stop_logic('Pose inspection')
            boss.set_actor_tick_enabled(False)
            manager.call_method('HandleBossDefeated',(boss,))
            preview_state['phase']=1
            return
        if preview_state['phase']==1:
            seqs=u.GameplayStatics.get_all_actors_of_class(world,u.LevelSequenceActor)
            active=[a for a in seqs if a.sequence_player and a.sequence_player.is_playing()]
            if not active:return
            preview_state['player']=active[-1].sequence_player
            preview_state['player'].pause()
            preview_state['phase']=2
        if preview_state['i']>=len(preview_frames):preview_finish();return
        frame=preview_frames[preview_state['i']]
        if preview_state['ticks']==0:
            params=u.MovieSceneSequencePlaybackParams(frame=u.FrameTime(u.FrameNumber(frame)),position_type=u.MovieScenePositionType.FRAME,update_method=u.UpdatePositionMethod.JUMP)
            preview_state['player'].set_playback_position(params)
            pc.character_movement.disable_movement()
            if preview_close:
                cam=pc.get_controller().get_view_target()
                p=pc.get_actor_location()
                loc=u.Vector(p.x+450,p.y+500,p.z+160)
                target=u.Vector(p.x-50,p.y,p.z-30)
                cam.set_actor_location_and_rotation(loc,u.MathLibrary.find_look_at_rotation(loc,target),False,False)
                cam.get_cine_camera_component().set_editor_property('current_focal_length',32)
        preview_state['ticks']+=1
        # HighResShot은 비동기다. 다음 요청이 파일명을 덮어쓰지 않도록 저장 완료 후 진행.
        if preview_state.get('pending'):
            if not Path(preview_state['pending']).exists():return
            if time.time()-preview_state['shot_time']<0.2:return
            preview_state.pop('pending')
            preview_state['i']+=1
            preview_state['ticks']=0
            return
        if preview_state['ticks']<5:return
        mesh=pc.mesh
        preview_state['rows'].append({'frame':frame,'bones':{bone:str(mesh.get_socket_location(bone)) for bone in ['Character_Head','Character_Pelvis','Character_LPalm','Character_RPalm','Character_LCalf','Character_RCalf','Character_Tail3']}})
        fn=str((preview_out/('f%04d.png'%frame)).resolve()).replace('\\','/')
        u.SystemLibrary.execute_console_command(world,'HighResShot 960x540 filename="'+fn+'"')
        preview_state['pending']=fn
        preview_state['shot_time']=time.time()
    except Exception:
        preview_state['errors'].append(traceback.format_exc());preview_finish()
preview_handle=[u.register_slate_post_tick_callback(preview_tick)]
print('PUPPET_PREVIEW_STARTED')
