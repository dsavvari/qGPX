# Releasing qGPX

Releases use [semantic versioning](https://semver.org) and tags named `vMAJOR.MINOR.PATCH`
(for example `v0.1.1`). While the major version is `0`, use a patch bump for
backwards-compatible fixes and a minor bump for new features or API-breaking changes.

Run all commands from the repository root.

## 1. Prepare the release branch

```sh
git switch main
git pull
git switch -c release/vX.Y.Z
```

## 2. Bump the version

Update the version in:

- `CMakeLists.txt`: `project(qGPX VERSION X.Y.Z ...)`
- `viewer/package.json` and `viewer/package-lock.json`:

  ```sh
  npm --prefix viewer version X.Y.Z --no-git-tag-version
  ```

If the **minor** version changed, also update the `find_package(qGPX X.Y ...)` examples in
`README.md` and `test/integration/CMakeLists.txt`.

## 3. Verify

Run the checks from [AGENTS.md](../AGENTS.md) and CI:

```sh
cmake -S . -B build -DBUILD_TESTS=ON -DBUILD_VIEWER=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
npm --prefix viewer ci
npm --prefix viewer test
npm --prefix viewer run build
```

## 4. Open and merge the release PR

```sh
git add -A
git commit -m "Prepare release vX.Y.Z"
git push -u origin release/vX.Y.Z
```

Open a pull request into `main`, wait for CI to pass, then merge it.

## 5. Tag the merge commit

Tag `main` after the merge, never the release branch:

```sh
git switch main
git pull
git tag -a vX.Y.Z -m "qGPX vX.Y.Z"
git push origin vX.Y.Z
```

## 6. Publish the GitHub release

On GitHub, go to **Releases → Draft a new release**, choose the `vX.Y.Z` tag, click
**Generate release notes**, review them, and publish.

## Fixing a mistake

If a tag was pushed to the wrong commit and nobody has used it yet, delete it and recreate it:

```sh
git tag -d vX.Y.Z
git push origin :refs/tags/vX.Y.Z
```

Otherwise leave it in place and release the next patch version.
