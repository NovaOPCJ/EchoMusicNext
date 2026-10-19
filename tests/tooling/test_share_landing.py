"""Exercise the shipped standalone page, including its real click handlers."""

import json
from pathlib import Path
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ShareLandingTest(unittest.TestCase):
    def test_real_page_preserves_targets_and_requires_room_confirmation(self):
        page = (ROOT / "docs/share/index.html").read_text()
        script = re.search(r"<script>([\s\S]+)</script>", page).group(1)
        harness = r"""
const vm = require('node:vm');
const assert = require('node:assert/strict');
const source = JSON.parse(process.argv[1]);
function run(search) {
  const elements = new Map();
  const timers = [];
  const window = {location: {href: 'https://hoowhoami.github.io/EchoMusicNext/share/' + search},
                  setTimeout: (f, ms) => timers.push([f, ms])};
  let copied = '';
  const document = {title: '', getElementById: id => {
    if (!elements.has(id)) elements.set(id, {textContent: '', style: {}, hidden: true,
      classList: {add() {}}, listeners: {}, addEventListener(n, f) {this.listeners[n] = f;}});
    return elements.get(id);
  }};
  vm.runInNewContext(source, {URL, TextEncoder, window, document,
    navigator: {clipboard: {writeText: async text => {copied = text;}}}});
  return {elements, timers, window, copied: () => copied};
}
(async () => {
  for (const [type, id] of [['song', 'a'.repeat(32)], ['playlist', 'collection_3_1_2_0'],
    ['artist', '123'], ['album', '456'], ['plugin', 'native.dsp']]) {
    const native = new URL(`echomusicnext://${type}/${id}`);
    native.searchParams.set('title', '中文 A&B + 100%');
    if (type === 'plugin') native.searchParams.set('source', 'https://example.test/index?a=1&b=2');
    const state = run('?target=' + encodeURIComponent(native.href));
    const open = state.elements.get('open');
    assert.equal(open.href, native.href);
    assert.equal(state.timers.length, 1);
    open.listeners.click({preventDefault() {}});
    assert.equal(state.window.location.href, native.href);
    await state.elements.get('copy').listeners.click();
    assert.equal(state.copied(), native.href);
  }
  const room = run('?target=' + encodeURIComponent('echomusicnext://listen-together/123?roomType=0&roomName=' + encodeURIComponent('一起听 A+B')));
  assert.equal(room.timers.length, 0);
  assert.equal(room.elements.get('room-invite').hidden, false);
  assert.equal(room.elements.get('room-name').textContent, '一起听 A+B');
  assert.equal(room.elements.get('open').textContent, '在 EchoMusicNext 中查看');
  for (const query of ['?type=song&id=bad', '?target=javascript%3Aalert(1)',
    '?type=artist&id=1&id=2', '?type=listen-together&id=1&roomType=1',
    '?target=' + encodeURIComponent('echomusicnext://artist/1#fragment'),
    '?target=' + encodeURIComponent('echomusicnext://evil@artist/1')]) {
    const state = run(query);
    assert.equal(state.elements.get('title').textContent, '分享链接无效', query);
    assert.equal(state.elements.get('open').style.display, 'none');
    assert.equal(state.timers.length, 0);
  }
  console.log('Standalone share page: six targets, encoded metadata, clicks, room confirmation, invalid links passed');
})().catch(error => {console.error(error); process.exitCode = 1;});
"""
        subprocess.run(["node", "-e", harness, json.dumps(script)], cwd=ROOT, check=True)


if __name__ == "__main__":
    unittest.main()
