import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {
  assetName,
  matrix,
  writeManifest,
  validateRelease,
  validVersion,
  releaseVersion,
} from './contract.mjs';
test('version tags must match both declarations and strict SemVer', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'echo-version-'));
  try {
    fs.writeFileSync(
      path.join(dir, 'CMakeLists.txt'),
      'project( EchoMusicNext VERSION 0.1.0 LANGUAGES CXX)',
    );
    fs.writeFileSync(path.join(dir, 'Cargo.toml'), '[workspace.package]\nversion = "0.1.0"\n');
    assert.equal(releaseVersion(dir, 'refs/tags/v0.1.0-beta.2'), '0.1.0-beta.2');
    assert.equal(releaseVersion(dir, 'refs/heads/main'), '0.1.0');
    for (const ref of ['refs/tags/v1.0.0', 'refs/tags/0.1.0', 'refs/tags/v0.1.0-beta.01'])
      assert.throws(() => releaseVersion(dir, ref));
    fs.writeFileSync(path.join(dir, 'Cargo.toml'), 'version = "0.2.0"\n');
    assert.throws(() => releaseVersion(dir));
    for (const value of ['1.0.0', '1.0.0-beta.2+build', '999999999999999.0.0'])
      assert.ok(validVersion(value));
    for (const value of ['1.0', '01.0.0', '1.0.0-beta.00', 'v1.0.0', '../foo'])
      assert.ok(!validVersion(value));
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
test('publishing requires all six targets, exact bytes and no unexpected files', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'echo-release-'));
  const version = '0.1.0-beta.1';
  try {
    for (const target of matrix) {
      for (const kind of target.kinds)
        fs.writeFileSync(
          path.join(dir, assetName(version, target.platform, target.arch, kind)),
          `payload-${target.platform}-${target.arch}-${kind}`,
        );
      await writeManifest(dir, version, target.platform, target.arch);
    }
    assert.equal((await validateRelease(dir, version)).length, 22);
    const file = path.join(dir, assetName(version, 'Windows', 'x64', 'nsis'));
    const bytes = fs.readFileSync(file);
    fs.writeFileSync(file, Buffer.alloc(bytes.length, 0));
    await assert.rejects(validateRelease(dir, version), /verification/);
    fs.writeFileSync(file, bytes);
    fs.writeFileSync(path.join(dir, 'extra.txt'), 'unintended');
    await assert.rejects(validateRelease(dir, version), /Unexpected/);
    fs.unlinkSync(path.join(dir, 'extra.txt'));
    fs.unlinkSync(file);
    await assert.rejects(validateRelease(dir, version));
  } finally {
    fs.rmSync(dir, { recursive: true, force: true });
  }
});
