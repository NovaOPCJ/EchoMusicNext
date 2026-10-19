import { spawnSync } from 'node:child_process';
import path from 'node:path';
import { root } from './contract.mjs';
const mode = process.argv[2];
const metadata = spawnSync('cargo', ['metadata', '--locked', '--no-deps', '--format-version=1'], {
  cwd: root,
  encoding: 'utf8',
});
if (metadata.status !== 0) throw new Error(metadata.stderr);
const data = JSON.parse(metadata.stdout);
const owned = data.packages.filter(
  (pkg) =>
    data.workspace_members.includes(pkg.id) &&
    path
      .relative(root, pkg.manifest_path)
      .split(path.sep)
      .every((part) => part !== 'vendor' && part !== '..'),
);
const packages = owned.flatMap((pkg) => ['--package', pkg.name]);
const commands = {
  format: ['fmt', '--check', ...packages],
  // Keep correctness checks blocking; style/dead-code warnings remain visible.
  lint: [
    'clippy',
    '--locked',
    '--no-deps',
    '--all-targets',
    ...packages,
    '--',
    '-D',
    'clippy::correctness',
    '-D',
    'clippy::suspicious',
  ],
  // Audio/transport probes share process-wide native configuration.
  test: ['test', '--locked', ...packages, '--', '--test-threads=1'],
};
if (!commands[mode]) throw new Error('Expected format, lint, or test');
const result = spawnSync('cargo', commands[mode], { cwd: root, stdio: 'inherit' });
if (result.error) throw result.error;
process.exitCode = result.status ?? 1;
