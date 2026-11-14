"""Validate staged source assets and story references, without Unreal or generated caches."""
import hashlib
import json
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parents[1] / 'Content/SuperstarStory'
clips = json.loads((root / 'clips.json').read_text(encoding='utf-8'))
story = json.loads((root / 'story.json').read_text(encoding='utf-8'))
assert len(clips) == 18
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
        previous = -1
        for emotion in actor.get('emotions', []):
            assert emotion['icon'] in {'Emotion_Anger', 'Emotion_Shock', 'Emotion_Care', 'Emotion_Warm'}
            assert previous <= emotion['at'] < beat['duration'] and emotion['label']
            previous = emotion['at']
assert used == clips.keys(), 'Every required clip, including extra idle and walk, must be reachable.'
total = 0
anchors=json.loads((root/'head_anchors.json').read_text(encoding='utf-8'))
for name, clip in clips.items():
    assert clip['fps'] == 5 and clip['frames'] > 0
    assert clip['pivot'] == [0.5, 0.890625]
    assert len(clip['sha256']) == clip['frames']
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
