import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import { spawn } from 'node:child_process';
import { once } from 'node:events';
const helper = process.env.ECHO_UPDATE_HELPER_PROBE;
const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
async function waitFile(file, child) {
  for (let i = 0; i < 200; ++i) {
    if (fs.existsSync(file)) return;
    if (child.exitCode !== null) throw new Error(`Helper exited early ${child.exitCode}`);
    await sleep(20);
  }
  throw new Error(`No ${path.basename(file)} marker`);
}
async function fixture(run) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'echo-handoff-'));
  const handoff = path.join(dir, 'handoff');
  fs.mkdirSync(handoff);
  const packageFile = path.join(dir, 'package');
  fs.writeFileSync(packageFile, 'verified package');
  const target = path.join(dir, 'target');
  fs.writeFileSync(target, '#!/bin/sh\nexit 0\n');
  fs.chmodSync(target, 0o700);
  const parent = spawn(
    process.execPath,
    ['-e', 'process.stdin.resume(); process.stdin.on("end",()=>process.exit(0));'],
    { stdio: ['pipe', 'ignore', 'ignore'] },
  );
  let child;
  const start = (
    kind = 'probe',
    hash = crypto.createHash('sha512').update(fs.readFileSync(packageFile)).digest('hex'),
  ) => {
    child = spawn(helper, [
      '--parent',
      String(parent.pid),
      '--handoff',
      handoff,
      '--package',
      packageFile,
      '--sha512',
      hash,
      '--kind',
      kind,
      '--target',
      target,
      '--silent',
      '1',
    ]);
    return child;
  };
  try {
    await run({ dir, handoff, packageFile, target, parent, start });
  } finally {
    if (parent.exitCode === null) {
      parent.stdin.end();
      await once(parent, 'exit');
    }
    if (child && child.exitCode === null) {
      child.kill();
      await once(child, 'exit');
    }
    fs.rmSync(dir, { recursive: true, force: true });
  }
}
test('helper waits for committed parent exit before installation', { skip: !helper }, async () =>
  fixture(async ({ handoff, parent, start }) => {
    const child = start();
    const done = once(child, 'exit');
    await waitFile(path.join(handoff, 'ready'), child);
    fs.writeFileSync(path.join(handoff, 'commit'), '1');
    await waitFile(path.join(handoff, 'committed'), child);
    fs.writeFileSync(path.join(handoff, 'quit-authorized'), '1');
    await sleep(100);
    assert.ok(!fs.existsSync(path.join(handoff, 'installed')));
    parent.stdin.end();
    const [code] = await done;
    assert.equal(code, 0);
    assert.ok(fs.existsSync(path.join(handoff, 'installed')));
  }),
);
test('hash mismatch prevents handoff', { skip: !helper }, async () =>
  fixture(async ({ handoff, start }) => {
    const child = start('probe', '0'.repeat(128));
    const [code] = await once(child, 'exit');
    assert.notEqual(code, 0);
    assert.ok(!fs.existsSync(path.join(handoff, 'ready')));
    assert.ok(!fs.existsSync(path.join(handoff, 'installed')));
  }),
);
test(
  'changed package and missing quit authorization prevent install',
  { skip: !helper },
  async () => {
    for (const tamper of [false, true])
      await fixture(async ({ handoff, packageFile, parent, start }) => {
        const child = start();
        const done = once(child, 'exit');
        await waitFile(path.join(handoff, 'ready'), child);
        fs.writeFileSync(path.join(handoff, 'commit'), '1');
        await waitFile(path.join(handoff, 'committed'), child);
        if (tamper) {
          fs.writeFileSync(path.join(handoff, 'quit-authorized'), '1');
          fs.appendFileSync(packageFile, 'modified');
        }
        parent.stdin.end();
        const [code] = await done;
        assert.notEqual(code, 0);
        assert.ok(!fs.existsSync(path.join(handoff, 'installed')));
      });
  },
);
test(
  'AppImage replacement rolls back when new executable cannot start',
  { skip: !helper || process.platform === 'win32' },
  async () =>
    fixture(async ({ handoff, target, parent, start }) => {
      const before = fs.readFileSync(target);
      const child = start('appimage');
      const done = once(child, 'exit');
      await waitFile(path.join(handoff, 'ready'), child);
      fs.writeFileSync(path.join(handoff, 'commit'), '1');
      await waitFile(path.join(handoff, 'committed'), child);
      fs.writeFileSync(path.join(handoff, 'quit-authorized'), '1');
      parent.stdin.end();
      const [code] = await done;
      assert.notEqual(code, 0);
      assert.deepEqual(fs.readFileSync(target), before);
    }),
);
