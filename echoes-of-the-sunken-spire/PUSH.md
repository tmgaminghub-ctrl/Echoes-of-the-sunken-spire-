# Publishing this repository

The repository is already initialised and committed on branch `main`. There is
just no remote yet, because creating one needs your GitHub account.

## Option A — GitHub CLI (fastest)

```bash
# once
gh auth login

cd echoes-of-the-sunken-spire
gh repo create echoes-of-the-sunken-spire --public --source=. --remote=origin --push
```

`--private` instead of `--public` if you want it hidden. Add
`--description "A low-poly action-adventure: browser, single-file HTML and native C++ Windows builds"`.

## Option B — create the repo on the website, then push

1. Go to <https://github.com/new>
2. Name it `echoes-of-the-sunken-spire`, **do not** add a README, .gitignore or
   licence (this repo already has them).
3. Then:

```bash
cd echoes-of-the-sunken-spire
git remote add origin https://github.com/YOUR_USER/echoes-of-the-sunken-spire.git
git push -u origin main
```

## If you would rather publish it under your own name

The commit is currently authored as `Echoes of the Sunken Spire
<dev@example.invalid>`. To re-attribute it:

```bash
git config user.name  "Your Name"
git config user.email "you@example.com"
git commit --amend --reset-author --no-edit
```

## After pushing — two things worth adding

**A release with the Windows exe.** `builds/` is git-ignored on purpose (a 20 MB
binary does not belong in git history). Attach it to a release instead:

```bash
cd native && ./build_exe.sh          # -> ../builds/Echoes-of-the-Sunken-Spire.exe
gh release create v1.0.0 \
   ../builds/Echoes-of-the-Sunken-Spire.exe \
   --title "v1.0.0" --notes "First release. Windows x64 portable exe, assets embedded."
```

**A repository description and topics** so people can find it — `three.js`,
`cpp`, `game`, `software-renderer`, `gltf`, `low-poly`, `kaykit`.

## Note on repository size

The committed tree is ~22 MB, almost all of it the KayKit models and textures in
`web/public/assets/`. That is fine for GitHub, but if you ever want it smaller,
the art could move to a release asset or Git LFS. Nothing else in the repo is
large: the source is a few hundred KB.
