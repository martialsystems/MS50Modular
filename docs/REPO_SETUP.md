# Repo setup

Date: 2026-09-21.

Private repository. The design-pack commit contains documents and scripts. It does not contain a plugin binary, a JUCE checkout, or a scan of the Korg drawings.

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

## Private remote

GitHub, after `gh auth login`:

```bash
gh repo create MS50Modular --private --source=. --remote=origin --push
```

GitLab:

```bash
glab repo create MS50Modular --private --source=.
git push -u origin main
```

If neither CLI is logged in, create an empty private repo in the website UI and:

```bash
git remote add origin git@github.com:YOURUSER/MS50Modular.git
git push -u origin main
```

Do not use `--public`.

## Never commit

*   `.env`, keys, host licenses, account tokens.
*   DAW project files and bounces (`.logicx`, `.als`, `.ptx`, `.cpr`, `.wav`, `.aif`, `.mp3`, `.flac`).
*   Folders `samples/`, `recordings/`, `bounces/`, `reference-scans/`.
*   Korg schematic PDFs or photos of the drawings. Cite the URL in `docs/01-research.md` instead.
*   The JUCE tree, `build/`, and plugin binaries.
*   A listening recording made while following `TESTPLAN.md`. Commit the short note in `docs/listening/`, not the audio.

## How to put this on a private host

1. Read `LICENSE` and confirm you are willing to keep the repo private.
2. Run `scripts/init-repo.sh` from the project root.
3. Create the private remote with the command above. Do not flip the visibility toggle.
4. Turn on branch protection for `main`.
5. Clone the private URL on the machine that will build step 1.
6. Build the PDF once (`python3 scripts/build_design_pdf.py`) and confirm the cover date is 2026-09-21 or later if you revised it.
7. Start `feat/step-01` only after that. Step 0 is this commit.
