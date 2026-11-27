#!/usr/bin/env python3
"""Compare normal/widescreen gameplay through the native lockstep protocol.

Uses the user's original ADFs. No disk content is embedded or retained.
The oracle is the original 320x256 bitmap and named gameplay state from the
same executable, with the same machine ticks and input, without widescreen.
"""
import argparse
import os
from pathlib import Path
import re
import select
import subprocess
import tempfile
import time

START, END = 0x100, 0x1D400
ROOT = Path(__file__).resolve().parents[3]


class Client:
    def __init__(self, game, disks, directory, wide):
        self.log_path = directory / 'game.log'
        self.log = self.log_path.open('wb')
        self.pending = bytearray()
        self.proc = subprocess.Popen(
            [str(game), '--disks', str(disks), '--skip-intro', '--lockstep',
             '--keys', '300:40:1,305:40:0'] + (['--widescreen'] if wide else []),
            cwd=directory, env=dict(os.environ, SDL_VIDEODRIVER='dummy',
                                   SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software'),
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log,
        )

    def send(self, text):
        self.proc.stdin.write((text + '\n').encode())
        self.proc.stdin.flush()

    def receive(self, count=None):
        deadline = time.monotonic() + 30
        while (len(self.pending) < count if count is not None else b'\n' not in self.pending):
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not select.select([self.proc.stdout], [], [], remaining)[0]:
                raise TimeoutError('Game did not answer the lockstep protocol within 30 seconds')
            chunk = os.read(self.proc.stdout.fileno(), 65536)
            if not chunk:
                raise RuntimeError('Game exited before answering the lockstep protocol')
            self.pending.extend(chunk)
        size = count if count is not None else self.pending.index(b'\n') + 1
        result = bytes(self.pending[:size])
        del self.pending[:size]
        return result

    def image(self, step=False):
        self.send('step' if step else 'image')
        header = self.receive().split()
        if len(header) != 6 or header[0] != b'frame' or header[2:4] != [b'320', b'256']:
            raise RuntimeError(f'Unexpected frame header: {header!r}')
        return self.receive(320 * 256 * 3)

    def memory(self):
        self.send(f'mem {START:x} {END - START:x}')
        if self.receive() != f'mem {END - START}\n'.encode():
            raise RuntimeError('Unexpected memory reply')
        return self.receive(END - START)

    def close(self):
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=5)
        self.log.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--game', type=Path, default=ROOT / 'build/sdl/gods-sdl-dev')
    parser.add_argument('--disks', type=Path, required=True)
    args = parser.parse_args()
    game, disks = args.game.resolve(), args.disks.resolve()
    for path in (game, disks / 'gods-disk1.adf', disks / 'gods-disk2.adf'):
        if not path.is_file():
            parser.error(f'Required test input is missing: {path}')

    # Padding contains unused loaded code/scratch with native process addresses.
    # Compare all declared gameplay members in this address range, not padding.
    mask = bytearray(END - START)
    for address, size in re.findall(r'\{ 0x([0-9a-f]+), (\d+), &guest\.',
                                    (ROOT / 'src/guest_members.h').read_text()):
        first = max(START, int(address, 16))
        last = min(END, int(address, 16) + int(size))
        if last > first:
            mask[first - START:last - START] = b'\1' * (last - first)

    with tempfile.TemporaryDirectory(prefix='gods-widescreen-') as temporary:
        clients = []
        try:
            for wide in (False, True):
                directory = Path(temporary) / ('wide' if wide else 'original')
                directory.mkdir()
                clients.append(Client(game, disks, directory, wide))
            for frame in range(301):
                if frame in (1, 181):
                    for client in clients:
                        client.send('key 78 ' + ('1' if frame == 1 else '0'))
                if frame in (81, 101):
                    for client in clients:
                        client.send('key 64 ' + ('1' if frame == 81 else '0'))
                images = [client.image(step=frame != 0) for client in clients]
                states = [client.memory() for client in clients]
                differences = [hex(i + START) for i, (a, b) in enumerate(zip(*states))
                               if mask[i] and a != b]
                if images[0] != images[1] or differences:
                    raise AssertionError(f'Tick {frame}: raster equal={images[0] == images[1]}, '
                                         f'changed state addresses={differences[:20]}')
            print(f'widescreen: 300 paired ticks match the original raster and {sum(mask)} '
                  'bytes of named gameplay state (walking and firing)')
        except Exception:
            for client in clients:
                print(client.log_path.read_text(errors='replace')[-2000:])
            raise
        finally:
            for client in clients:
                client.close()


if __name__ == '__main__':
    main()
