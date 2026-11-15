"""Validate staged source assets and story references, without Unreal or generated caches."""
import hashlib
import json
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1] / 'Content/SuperstarStory'
clips = json.loads((root / 'clips.json').read_text(encoding='utf-8'))
story = json.loads((root / 'story.json').read_text(encoding='utf-8'))
assert len(clips) >= 18
used = set()
assert {b['chapter'] for b in story['beats']} == {1, 2, 3, 4}
assert sum(bool(b['evidence']) for b in story['beats']) == 3
for beat in story['beats']:
    assert beat['duration'] > 0 and beat['title'] and beat['description']
    assert beat['scene'] in {'studio', 'outdoor', 'corridor', 'stage', 'storage', 'dressing', 'newstudio', 'newspaper'}
    for actor in beat['actors']:
        used.add(actor['clip'])
        assert actor['clip'] in clips and isinstance(actor['flip'], bool)
        assert 0 <= actor['x'] <= 1280 and 0 <= actor['to'] <= 1280
        assert not actor['name'].startswith('导演'), 'Director must use the confirmed Li Mou identity.'
        if 'walk_clip' in actor:
            used.add(actor['walk_clip'])
            assert clips[actor['walk_clip']]['frames']>1 # non-loop authored advances/retreats are also valid locomotion
            assert 0<=actor['move_start']<actor['move_end']<=beat['duration']
            assert clips[actor['walk_clip']]['screen_facing'] in ['left','right']
        if actor['x']!=actor['to']:assert 'walk_clip' in actor,'Moving actor must have stepping frames'
        last_start=0
        for part in actor.get('sequence',[]):
            assert last_start<part['at']<beat['duration'] and part['clip'] in clips
            used.add(part['clip']);last_start=part['at']
        if 'hold_frame' in actor:assert 0<=actor['hold_frame']<clips[actor['clip']]['frames']
        if 'delay' in actor:assert 0<=actor['delay']<beat['duration']
        previous = -1
        for emotion in actor.get('emotions', []):
            assert emotion['icon'] in {'Emotion_Anger', 'Emotion_Shock', 'Emotion_Care', 'Emotion_Warm'}
            assert previous <= emotion['at'] < beat['duration'] and emotion['label']
            previous = emotion['at']
    for prop in beat.get('props',[]):
        assert (root/prop['image']).is_file() and prop['size']>0
        if 'actor' in prop:assert 0<=prop['actor']<len(beat['actors'])
        if 'attach_actor' in prop:
            assert 0<=prop['attach_actor']<len(beat['actors'])
            assert len(prop['grip'])==2 and all(0<=v<=1 for v in prop['grip'])
            if 'transfer_actor' in prop:assert 0<=prop['transfer_actor']<len(beat['actors']) and 0<prop['transfer_at']<beat['duration']
    if 'evidence_image' in beat:assert (root/beat['evidence_image']).is_file()
assert used == {id for id,c in clips.items() if not c.get('retired')}, 'Every active clip, including extra idle and walk, must be reachable.'
total = 0
anchors=json.loads((root/'head_anchors.json').read_text(encoding='utf-8'))
for name, clip in clips.items():
    assert clip['fps'] == 5 and clip['frames'] > 0
    assert clip['pivot'] == [0.5, 0.890625]
    assert len(clip['sha256']) == clip['frames']
    if 'hand_anchors' in clip:assert len(clip['hand_anchors'])==clip['frames'] and all(0<=x<640 and 0<=y<640 for x,y in clip['hand_anchors'])
    assert len(anchors[name])==clip['frames']
    assert all(0<=x<640 and 0<=y<640 for x,y in anchors[name])
    for i, digest in enumerate(clip['sha256'], 1):
        file = root / clip['path'] / f'{name}_{i:06d}.png'
        assert hashlib.sha256(file.read_bytes()).hexdigest() == digest, file
        with Image.open(file) as image:
            assert image.mode == 'RGBA' and image.size == (640, 640), file
            image.verify()
        total += 1
for name in ['studio', 'outdoor', 'dressing', 'stage', 'newspaper']:
    with Image.open(root / 'References' / f'{name}.png') as image:
        image.verify()
print(f'PASS: {len(story["beats"])} beats, 4 chapters, 3 evidence gates, {len(clips)} clips, {total} intact RGBA frames, 5 reference images.')
