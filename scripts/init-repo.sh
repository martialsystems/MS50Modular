#!/bin/sh
# Create the design-pack git repo and the first commit.
# Does not create a remote and does not push.
set -eu
cd "$(dirname "$0")/.."

if [ -d .git ]; then
  echo "Already a git repo. Refusing to re-init." >&2
  exit 1
fi

# Do not add samples, bounces, or Korg scans. find|while would hide a failure.
for f in $(find . -type f ! -path './.git/*'); do
  case "$f" in
    ./docs/MS50_Modular_Design_Pack.pdf) ;;
    *.wav|*.aif|*.aiff|*.mp3|*.flac|*.ogg|*.logicx|*.als|*.ptx|*.cpr)
      echo "blocked $f" >&2
      exit 1
      ;;
    ./samples/*|./recordings/*|./bounces/*|./reference-scans/*)
      echo "blocked $f" >&2
      exit 1
      ;;
  esac
done

git init -b main
git add README.md METHODOLOGY.md LICENSE .gitignore scripts docs
git status --short
git commit -m "$(cat <<'EOF'
docs: add MS-50 modular design pack

Research summary, software schematic, build steps 0-20, and test plan
for a white-box VST3 FX rack. No plugin code in this commit.
EOF
)"
echo "Committed on main. Add a private remote when you are ready. See docs/REPO_SETUP.md."
