"""Copy a reviewed 5 fps delivery into the isolated SuperstarStory content folder.
No chroma key, resampling, cropping, or transition frames are introduced.
Usage: python Tools/Import-SuperstarAnimations.py --source <delivery/final> [--replace]
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'Content/SuperstarStory'

def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--replace', action='store_true')
    args = parser.parse_args()
    clips = {}
    sources = []
    for folder in sorted(args.source.iterdir()):
        metadata = folder / 'animation.json'
        if not metadata.is_file():
            continue
        data = json.loads(metadata.read_text(encoding='utf-8-sig'))
        name = data['id']
        assert name == folder.name and name.replace('_', '').isalnum(), name
        assert data['fps'] == 5 and data['canvas'] == [640, 640], name
        frames = sorted((folder / 'frames').glob('*.png'))
        assert len(frames) == data['frame_count'] > 0, name
        for i, frame in enumerate(frames, 1):
            assert frame.name == f'{name}_{i:06d}.png', frame
            with Image.open(frame) as image:
                assert image.mode == 'RGBA' and image.size == (640, 640), frame
        clips[name] = dict(frames=len(frames), fps=5, loop=data['loop'],
                           pivot=data['anchor_normalized'], path=f'Animations/{name}',
                           sha256=[hashlib.sha256(f.read_bytes()).hexdigest() for f in frames])
        sources.append((name, frames, metadata))
    assert len(clips) == 18, f'Expected all 18 required clips, found {len(clips)}'
    if (DEST / 'clips.json').exists() and not args.replace:
        raise SystemExit('Content already exists; use --replace to explicitly replace only SuperstarStory animation files.')
    # Validate the whole delivery before writing. Never delete old files or touch other scenes.
    for name, frames, metadata in sources:
        target = DEST / 'Animations' / name
        target.mkdir(parents=True, exist_ok=True)
        for frame in frames:
            shutil.copy2(frame, target / frame.name)
        shutil.copy2(metadata, target / 'delivery-animation.json')
    (DEST / 'clips.json').write_text(json.dumps(clips, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'Imported {len(clips)} clips / {sum(c["frames"] for c in clips.values())} frames, 5 fps; original PNG bytes preserved.')

if __name__ == '__main__':
    main()
