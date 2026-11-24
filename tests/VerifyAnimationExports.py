"""Decode native export fixtures with an independent image library (Pillow)."""
import json
import struct
import sys
import zlib
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1])

def rgba(raw):
    result = bytearray(len(raw))
    for p in range(0, len(raw), 4):
        result[p:p+4] = bytes((raw[p+2], raw[p+1], raw[p], raw[p+3]))
    return bytes(result)

def check_chunks(path):
    raw = path.read_bytes()
    assert raw[:8] == b'\x89PNG\r\n\x1a\n'
    pos = 8
    sequence = []
    while pos < len(raw):
        size = struct.unpack_from('>I', raw, pos)[0]
        kind = raw[pos+4:pos+8]
        data = raw[pos+8:pos+8+size]
        crc = struct.unpack_from('>I', raw, pos+8+size)[0]
        assert zlib.crc32(kind+data) & 0xffffffff == crc
        if kind in (b'fcTL', b'fdAT'):
            sequence.append(struct.unpack_from('>I', data)[0])
        pos += size+12
    assert pos == len(raw)
    assert sequence == list(range(len(sequence)))

expected = [(root / f'fixture-{i}.bgra').read_bytes() for i in range(4)]
check_chunks(root / 'fixture.apng')
with Image.open(root / 'fixture.apng') as image:
    assert image.n_frames == 4 and image.size == (256, 128) and image.info['loop'] == 2
    for i, duration in enumerate((1000/60, 2000/60, 50, 1500)):
        image.seek(i)
        assert abs(image.info['duration']-duration) < 0.001
        assert image.convert('RGBA').tobytes() == rgba(expected[i]), f'APNG frame {i} changed RGBA'
with Image.open(root / 'fixture.gif') as image:
    assert image.n_frames == 4 and image.size == (256, 128) and image.info['loop'] == 1
    for i, duration in enumerate((20, 30, 50, 1500)):
        image.seek(i)
        assert image.info['duration'] == duration
        actual = image.convert('RGBA').tobytes()
        reference = rgba(expected[i])
        for p in range(0, len(actual), 4):
            visible = reference[p+3] >= 128
            assert actual[p+3] == (255 if visible else 0), f'GIF frame {i} alpha/ghosting'
            if visible:
                assert actual[p:p+3] == reference[p:p+3], f'GIF frame {i} palette/LZW error'
for filename, loop in (('forever.gif', 0), ('forever.apng', 0), ('once.gif', None)):
    with Image.open(root / filename) as image:
        assert image.info.get('loop') == loop
with Image.open(root / 'quantized.gif') as image:
    assert image.size == (32, 32)
    assert all(alpha == 255 for alpha in image.convert('RGBA').tobytes()[3::4])
check_chunks(root / 'snapshot.apng')
with Image.open(root / 'snapshot.apng') as image:
    assert image.n_frames == 3 and image.size == (3, 1)
    for i in range(3):
        image.seek(i)
        pixels = [image.convert('RGBA').getpixel((x, 0)) for x in range(3)]
        assert pixels[0 if i < 2 else 2] == (255, 0, 0, 128), 'Snapshot/origin moved or changed'
        assert sum(p[3] > 0 for p in pixels) == 1
metadata = root / 'Cerebella-crouch.json'
if metadata.exists():
    data = json.loads(metadata.read_text())
    for ext in ('apng', 'gif'):
        path = root / f'Cerebella-crouch.{ext}'
        if ext == 'apng':
            check_chunks(path)
        with Image.open(path) as image:
            assert image.n_frames == data['frames'] and image.size == (data['width'], data['height'])
            total = 0
            for i, ticks in enumerate(data['ticks']):
                image.seek(i)
                total += image.info['duration']
                if ext == 'apng':
                    assert abs(image.info['duration']-ticks*1000/60) < 0.001
                assert 0 in image.convert('RGBA').tobytes()[3::4]
            assert abs(total-sum(data['ticks'])*1000/60) <= (0.01 if ext == 'apng' else 10)
print('Independent animation decode checks passed: frame pixels, alpha, origin, timing, loops, disposal, palette quantization, LZW and APNG CRC/sequence.')
