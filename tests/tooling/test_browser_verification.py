"""Exercise the shipped page's lifecycle with generated SDK control callbacks.

These are JavaScript control tests, never a valid provider proof or online sample.
"""

from pathlib import Path
import shutil
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]


class BrowserVerificationPageTest(unittest.TestCase):
    @unittest.skipUnless(shutil.which("node"), "Node is required for development page checks")
    def test_automatic_challenge_return_errors_and_stale_callbacks(self):
        source = ROOT / "services/account/web/browser-verification.html"
        program = r"""
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const script = fs.readFileSync(process.argv[1], 'utf8').match(/<script>([\s\S]*?)<\/script>/)[1];
function page({reject = false, hash = '#' + 'a'.repeat(43)} = {}) {
  const nodes = new Map();
  const timers = new Map();
  const calls = [];
  const instances = [];
  let closed = 0;
  const node = id => {
    if (!nodes.has(id)) nodes.set(id, {hidden:false, textContent:'', remove(){}});
    return nodes.get(id);
  };
  const document = {
    body: {dataset:{}},
    querySelector: node,
    createElement: () => ({remove(){}}),
    head: {appendChild: script => script.onload()},
  };
  const context = {
    document,
    location: {hash, pathname:'/verification'},
    history: {replaceState(){}},
    window: {menubar:{visible:false}, resizeTo(){}, close(){closed++;}},
    AbortSignal: {timeout(){}},
    setTimeout: (callback, duration) => { const id = {}; timers.set(id, {callback,duration}); return id; },
    clearTimeout: id => timers.delete(id),
    fetch: async (_, options) => {
      const body = JSON.parse(options.body);
      calls.push(body);
      return {ok:!reject, json:async () => body.ret === 2 ? {cancelled:true} : {accepted:true}};
    },
    TencentCaptcha: function(element, appId, callback, options) {
      assert.equal(element, node('#challenge'));
      assert.equal(options.type, 'embed');
      const instance = {callback, ready:options.ready, show:options.ready, destroy(){}};
      instances.push(instance);
      return instance;
    },
  };
  vm.runInNewContext(script, context);
  return {document, nodes, timers, calls, instances, get closed(){return closed;},
    async flush(){for(let i=0;i<12;i++) await Promise.resolve();},
    fire(duration){for(const [id,t] of timers) if(t.duration === duration) {timers.delete(id); t.callback();}},
  };
}
(async () => {
  const valid = page();
  assert.equal(valid.instances.length, 1); // No second Start click.
  assert.equal(valid.document.body.dataset.phase, 'challenge');
  await valid.flush();
  assert.deepEqual(valid.calls, [{event:'ready'}]);
  const proof = {ret:0,ticket:'generated-control-only',randstr:'generated-rand'};
  valid.instances[0].callback(proof);
  valid.instances[0].callback(proof); // Duplicate SDK callback cannot submit twice.
  await valid.flush();
  assert.equal(valid.calls.filter(x => x.ticket).length, 1);
  assert.equal(valid.calls[1].randstr, 'generated-rand');
  assert.equal(valid.document.body.dataset.phase, 'sent');
  valid.fire(700);
  assert.equal(valid.closed, 1);
  assert.equal(valid.timers.size, 0);

  const error = page();
  error.instances[0].callback({ret:0,errorCode:1007,ticket:'trerror_control'});
  await error.flush();
  assert.equal(error.document.body.dataset.phase, 'error');
  assert.equal(error.nodes.get('#description').hidden, false);
  assert.equal(error.nodes.get('#status').hidden, true);
  assert.equal(error.calls.some(x => x.ticket), false);
  assert.equal(error.closed, 0);
  error.nodes.get('#retry').onclick();
  assert.equal(error.instances.length, 2);
  assert.equal(error.nodes.get('#description').hidden, true);
  assert.equal(error.nodes.get('#status').hidden, false);
  error.instances[0].callback(proof); // Retired instance cannot affect new attempt.
  await error.flush();
  assert.equal(error.calls.some(x => x.ticket), false);
  assert.equal(error.document.body.dataset.phase, 'challenge');
  error.fire(180000);
  assert.equal(error.document.body.dataset.phase, 'expired');
  error.instances[1].callback(proof);
  await error.flush();
  assert.equal(error.calls.some(x => x.ticket), false);

  const cancelled = page();
  cancelled.instances[0].callback({ret:2});
  await cancelled.flush();
  assert.equal(cancelled.document.body.dataset.phase, 'cancelled');
  assert.equal(cancelled.closed, 0);

  const revoked = page({reject:true});
  await revoked.flush();
  assert.equal(revoked.document.body.dataset.phase, 'expired');
  assert.equal(revoked.closed, 0);
  const invalid = page({hash:''});
  assert.equal(invalid.instances.length, 0);
  assert.equal(invalid.calls.length, 0);
  assert.equal(invalid.document.body.dataset.phase, 'expired');
})().catch(error => { console.error(error); process.exitCode = 1; });
"""
        result = subprocess.run(
            [shutil.which("node"), "-e", program, str(source)],
            capture_output=True,
            text=True,
            timeout=10,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
