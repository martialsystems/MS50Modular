Copyright (c) 2026 Martial Systems LLC. All rights reserved.

Korg owns the MS-50, its name, and its circuit designs. This repository is an independent study of published drawings and papers. It is not a Korg product, it is not endorsed by Korg, and it does not license those designs.

# Repo setup

Date: 2026-09-21.

Public repository. The design-pack commits contain documents and scripts. They do not contain a plugin binary, a JUCE checkout, or a scan of the Korg drawings. Read `LICENSE` before you mirror this tree.

## Layout

Design-pack commit:

```text
MS50Modular/
  README.md
  METHODOLOGY.md
  LICENSE
  .gitignore
  scripts/init-repo.sh
  scripts/build_design_pdf.py
  docs/01-research.md
  docs/SCHEMATICS.md
  docs/BUILD_GUIDE.md
  docs/TESTPLAN.md
  docs/REPO_SETUP.md
  docs/MS50_Modular_Design_Pack.pdf
```

After step 1, also:

```text
  CMakeLists.txt
  Source/PluginProcessor.h
  Source/PluginProcessor.cpp
  Source/PluginEditor.h
  Source/PluginEditor.cpp
  Source/Modular/
  Source/UI/
  Source/Tests/
```

`build/` is gitignored. JUCE is downloaded into the build tree by CMake FetchContent. Do not add a `JUCE/` submodule unless FetchContent is impossible on that machine, and do not commit the downloaded tree.

## PDF

Regenerate the printable pack from the markdown so the PDF does not drift:

```bash
python3 -m pip install reportlab
python3 scripts/build_design_pdf.py
```

Output: `docs/MS50_Modular_Design_Pack.pdf`.

The PDF is a compiled reading copy: cover, methodology, research summary, software schematic, build guide, test plan, bibliography. The bibliography is the last section of `docs/01-research.md`. Commit the PDF with the docs when those docs change.

Page check after a PDF edit: render at least the cover, a middle page, and the last page, and read them. On a machine with `pdftoppm`:

```bash
pdftoppm -png -r 120 -f 1 -l 1 docs/MS50_Modular_Design_Pack.pdf /tmp/ms50-pdf
pdftoppm -png -r 120 -f 8 -l 8 docs/MS50_Modular_Design_Pack.pdf /tmp/ms50-pdf-mid
```

Confirm the revision date is visible, headings are not cut off, and tables stay inside the page.

## First commit

From an empty directory that already contains these files, or from this folder:

```bash
cd /path/to/MS50Modular
git init -b main
git add README.md METHODOLOGY.md LICENSE .gitignore scripts docs
git status
git commit -m "$(cat <<'EOF'
docs: add MS-50 modular design pack

Research summary, software schematic, build steps 0-20, and test plan
for a white-box VST3 FX rack. No plugin code in this commit.
EOF
)"
```

`scripts/init-repo.sh` runs that sequence. It refuses to run if `git status` would add a file under `samples/`, `recordings/`, or `reference-scans/`, or a file ending in `.wav`, `.mp3`, `.logicx`, or `.pdf` other than `docs/MS50_Modular_Design_Pack.pdf`.

## Commit subjects

*   `docs:` for markdown and the PDF only.
*   `step-XX:` for a build-guide step, one step per commit or per PR.
*   No period at the end of the subject. Body explains the stand-in ids touched, if any.

## Branches

`main` is the protected branch. Do not push feature work straight to it.

```text
main
  feat/step-00
  feat/step-01
  ...
  feat/step-20
```

Merge `feat/step-XX` only after that step's acceptance test is green. Do not open `feat/step-08` from an unmerged `feat/step-07`.

GitHub: Settings, Branches, branch protection on `main`, require a pull request. GitLab: Protected branches, `main`, no direct push.

## Public remote

The GitHub account `martialsystems` is already logged in on the machine that published this tree. Create or update the public repo with:

```bash
gh repo create MS50Modular --public --source=. --remote=origin --push
```

GitLab, if you use that host instead:

```bash
glab repo create MS50Modular --public --source=.
git push -u origin main
```

If neither CLI is logged in, create an empty public repo in the website UI and:

```bash
git remote add origin git@github.com:martialsystems/MS50Modular.git
git push -u origin main
```

## Never commit

*   `.env`, keys, host licenses, account tokens.
*   DAW project files and bounces (`.logicx`, `.als`, `.ptx`, `.cpr`, `.wav`, `.aif`, `.mp3`, `.flac`).
*   Folders `samples/`, `recordings/`, `bounces/`, `reference-scans/`.
*   Korg schematic PDFs or photos of the drawings. Cite the URL in `docs/01-research.md` instead.
*   The JUCE tree, `build/`, and plugin binaries.
*   A listening recording made while following `TESTPLAN.md`. Commit the short note in `docs/listening/`, not the audio.

## How this repo was published

1. `LICENSE` is the first legal text: Martial Systems LLC copyright, then the Korg notice.
2. The same two sentences sit at the top of every markdown file and in the running head of every PDF page.
3. `gh repo create MS50Modular --public --source=. --remote=origin --push` published `main`.
4. Branch protection on `main` is still worth turning on in the GitHub settings.
5. Later steps use `feat/step-XX`. Step 1 is the first code step.
