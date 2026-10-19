import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { releaseVersion, root, validateRelease } from './contract.mjs';
if (process.env.GITHUB_EVENT_NAME !== 'push' || !process.env.GITHUB_REF?.startsWith('refs/tags/v'))
  throw new Error('Only a pushed version tag may publish');
const version = releaseVersion(),
  tag = `v${version}`;
const repo = process.env.GITHUB_REPOSITORY;
if (repo !== 'hoowhoami/EchoMusicNext') throw new Error('Publishing repository mismatch');
const directory = path.resolve(root, 'build/release-assets');
const files = await validateRelease(directory, version);
const changelog = fs.readFileSync(path.join(root, 'CHANGELOG.md'), 'utf8');
const sections = [
  ...changelog.matchAll(
    /^##\s+\[?v?([0-9]+\.[0-9]+\.[0-9]+(?:[-+][0-9A-Za-z.+-]+)?)\]?[^\n]*\n([\s\S]*?)(?=^##\s|$(?![\s\S]))/gm,
  ),
];
const body =
  sections.find((match) => match[1] === version)?.[2]?.trim() ||
  sections.find((match) => match[1] === version.split(/[-+]/)[0])?.[2]?.trim();
if (!body) throw new Error(`Missing changelog section for ${version}`);
function gh(args, input) {
  const result = spawnSync('gh', args, {
    encoding: 'utf8',
    input,
    stdio: ['pipe', 'pipe', 'inherit'],
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`gh failed (${result.status})`);
  return result.stdout;
}
function api(method, suffix, value) {
  const result = gh(
    ['api', '--method', method, `repos/${repo}/${suffix}`, ...(value ? ['--input', '-'] : [])],
    value ? JSON.stringify(value) : undefined,
  );
  return result.trim() ? JSON.parse(result) : null;
}
// Keep incomplete uploads invisible to application update checks.
const listed = JSON.parse(
  gh(['api', '--paginate', '--slurp', `repos/${repo}/releases?per_page=100`]),
);
const existing = listed.flat().find((release) => release.tag_name === tag);
const payload = {
  tag_name: tag,
  name: `EchoMusicNext ${tag}`,
  body,
  draft: true,
  prerelease: version.split('+')[0].includes('-'),
};
const release = existing
  ? api('PATCH', `releases/${existing.id}`, payload)
  : api('POST', 'releases', payload);
for (const asset of release.assets || [])
  if (!files.includes(asset.name)) api('DELETE', `releases/assets/${asset.id}`);
gh([
  'release',
  'upload',
  tag,
  '--repo',
  repo,
  '--clobber',
  ...files.map((file) => path.join(directory, file)),
]);
const uploaded = api('GET', `releases/${release.id}`);
if (
  uploaded.assets.length !== files.length ||
  uploaded.assets.some(
    (asset) =>
      !files.includes(asset.name) ||
      asset.size !== fs.statSync(path.join(directory, asset.name)).size,
  )
)
  throw new Error('Uploaded release is incomplete; retained as draft');
api('PATCH', `releases/${release.id}`, {
  draft: false,
  make_latest: payload.prerelease ? 'false' : 'legacy',
});
console.log(`Published ${tag} with ${files.length} verified files`);
