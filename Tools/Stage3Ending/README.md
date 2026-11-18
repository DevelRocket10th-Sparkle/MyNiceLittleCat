# Stage3 ending cinematic

Canonical source for `/Game/Game/Cinematics/Stage3/LS_Stage3_07_Ending`.
Keep edits in these scripts: rebuilding replaces the sequence's tracks and bindings.
The former `Saved/ClaudeEndingWork` scripts are compatibility entry points in this workspace;
`Saved` is ignored by Git, so use the versioned scripts here for future work.

## Build in the Unreal Python console

```python
import unreal
from pathlib import Path
exec((Path(unreal.Paths.project_dir()) / 'Tools/Stage3Ending/ending_build.py').read_text(encoding='utf-8'))
```

- `ending_def.py`: shots, blocking, animation sections, markers and final editorial timing.
- `ending_common.py`: self-contained track/sampling helpers; no dependency on other Saved scripts.
- `ending_build.py`: preflight validation, generated sequence and `Saved/CodexEndingWork/build.json`.
- `author_collapse.py`: generates the puppet collapse `AS_Stage3_Player_PuppetCollapse`.
  The former `AS_Stage3_Player_Collapse` asset is preserved. Re-run the generator when
  changing the collapse or the incoming walk phase, then run `ending_build.py`.
- `preview_collapse.py`: execute before PIE for a frame-by-frame inspection of the collapse
  in the real sequence. `POSE_PREVIEW_CLOSE=True` provides a temporary inspection camera;
  it does not change the saved sequence. Stop PIE after inspection.
- `verify_runtime.py`: execute before starting PIE on `Lv_MainGame_Draft_v15_Ending`.
  It starts the Stage3 encounter, stops the real boss AI, and invokes the reflected defeat callback.
  It captures the actual game viewport and records a JSON report under `Saved/CodexEndingWork`.
  It restores background throttling when finished. Stop PIE after reading the result.
  This checks the post-defeat sequence and title transition, **not an actual combat kill**.

## Current edit

24 fps, 1091 frames (45.46 seconds), seven contiguous camera cuts.
Collapse marker: 863; fully black: 1079; sequence end: 1091.
The initial roof wide includes all three actors. The reaching shot and pet reaction stay
on the same side of the action axis. The shoulder/follow shots leave room for the player.
The courtyard camera stays clear of the building, with separate walking lanes so the pet
does not disappear behind the player. The gate approach is shorter; the final camera
continues to hold both the fallen player and departing pet.

Gate direction remains `-1` (village to outside), as confirmed in the handoff.
The final editorial remap shortens the player-watching beat by 48 frames; all transforms,
camera sections, markers, animation timing and fade are remapped together.

## Constraints and remaining art

- Keep actor transforms in the existing level-instance local coordinate system.
- Do not use `Die()` or physical ragdoll for the cinematic collapse.
- Do not write a Root track to the raccoon collapse asset (existing import scale issue).
- The puppet collapse keeps the incoming walking pose, loses joint support immediately,
  lands asymmetrically in about 0.7 seconds and settles within the 1.5-second animation.
  Arms hang instead of bracing, and the tail falls after the torso. The last pose is held
  without idle motion. It uses the walking bone tracks plus Tail1, with no Root track.
  Generation checks unchanged bone lengths/scales and a continuous head trajectory.
- The boss still uses the existing death-pose crawl placeholder; a dedicated crawl animation
  remains an art task. `BOSS_CRAWL_ANIM` is the replacement hook.
- BossDust uses the existing level cue. No replacement VFX, BGM or footsteps were authored.
- Posed editor proxies can fail to update animations. Judge the finished cinematic in PIE.

Before-change backup: `Saved/CodexEndingWork/LS_Stage3_07_Ending.before.uasset`.

Puppet animation generation:

```python
exec((Path(unreal.Paths.project_dir()) / 'Tools/Stage3Ending/author_collapse.py').read_text(encoding='utf-8'))
```

Reports and PIE captures for this animation are in `Saved/CodexEndingWork/author_puppet_collapse.json`,
`puppet_v3_close/` (pose inspection), and `puppet_final/` (normal playback).
