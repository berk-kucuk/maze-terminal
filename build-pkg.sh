#!/usr/bin/env bash
# =============================================================================
#  Maze Terminal — local Arch package builder
#
#  Builds maze-terminal-<version>-<rel>-<arch>.pkg.tar.zst straight from the
#  current working tree (uncommitted changes included), using packaging/PKGBUILD.
#  The package version is read from MAZE_TERMINAL_VERSION in CMakeLists.txt so
#  it always matches the code being built. makepkg runs the tests (check()).
#
#  Building needs no privileges and this script asks for none: it runs makepkg
#  and stops. Installing is a separate, explicit step you run yourself — the
#  command is printed at the end.
#
#  Build dependencies (once):
#    sudo pacman -S --needed cmake qt6-tools qt6-svg \
#                            layer-shell-qt libcanberra ttf-hack
#
#  Usage:
#    ./build-pkg.sh              build the package into ./dist-pkg/
#    ./build-pkg.sh --clean      remove ./dist-pkg/ and exit
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PKG_SRC="$ROOT/packaging"
OUT="$ROOT/dist-pkg"

if [[ -t 1 ]]; then
  GREEN='\033[0;32m'; BLUE='\033[0;34m'; YELLOW='\033[1;33m'; RED='\033[0;31m'; RESET='\033[0m'
else
  GREEN=''; BLUE=''; YELLOW=''; RED=''; RESET=''
fi
info() { echo -e "${BLUE}[*]${RESET} $*"; }
ok()   { echo -e "${GREEN}[✓]${RESET} $*"; }
warn() { echo -e "${YELLOW}[!]${RESET} $*"; }
die()  { echo -e "${RED}[✗]${RESET} $*" >&2; exit 1; }

for arg in "$@"; do
  case "$arg" in
    --clean)   rm -rf "$OUT"; ok "Removed $OUT"; exit 0 ;;
    -h|--help) sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^#\s\?//'; exit 0 ;;
    *)         die "Unknown option: $arg" ;;
  esac
done

command -v makepkg >/dev/null 2>&1 || die "makepkg not found — run this on Arch (pacman)."
[[ -f "$PKG_SRC/PKGBUILD" ]] || die "Missing $PKG_SRC/PKGBUILD"

missing=()
for p in cmake qt6-tools qt6-svg layer-shell-qt libcanberra; do
  pacman -Qq "$p" >/dev/null 2>&1 || missing+=("$p")
done
if ((${#missing[@]})); then
  die "Missing build dependencies — install them first:
    sudo pacman -S --needed ${missing[*]}"
fi

# ── Version comes from the source of truth ────────────────────────────────────
VER="$(sed -n 's/^set(MAZE_TERMINAL_VERSION "\(.*\)")/\1/p' "$ROOT/CMakeLists.txt")"
[[ -n "$VER" ]] || die "Could not read MAZE_TERMINAL_VERSION from CMakeLists.txt"
info "Building maze-terminal ${VER} from working tree"

# ── Fresh build directory ─────────────────────────────────────────────────────
BUILD="$OUT/build"
rm -rf "$BUILD"
mkdir -p "$BUILD"

# ── Source tarball from the working tree ──────────────────────────────────────
info "Creating source tarball"
tar czf "$BUILD/maze-terminal-${VER}.tar.gz" -C "$ROOT" \
  --exclude=./.git --exclude=./build --exclude=./dist-pkg --exclude='*.pro.user*' \
  --transform "s,^\.,maze-terminal-${VER}," .

cp "$PKG_SRC/PKGBUILD" "$BUILD/"
# Pin pkgver to the version we just built, in the build copy and in
# packaging/PKGBUILD too, so the source PKGBUILD never lags behind the package.
sed -i "s/^pkgver=.*/pkgver=${VER}/" "$BUILD/PKGBUILD"
if ! grep -qx "pkgver=${VER}" "$PKG_SRC/PKGBUILD"; then
    sed -i "s/^pkgver=.*/pkgver=${VER}/" "$PKG_SRC/PKGBUILD"
    info "packaging/PKGBUILD pkgver -> ${VER}"
fi

info "Running makepkg (build + tests) ..."
( cd "$BUILD" && makepkg -f --noconfirm --nodeps )

# ── aarch64: the same sources, built in an Arch Linux ARM root ───────────────
#    (MazeLinux/tools/aarch64-build.sh). Skipped until that root is set up;
#    MAZE_NO_AARCH64=1 skips it on purpose. A failure here keeps the x86_64
#    package and is reported at the end.
AARCH64_TOOL="$ROOT/../MazeLinux/tools/aarch64-build.sh"
AARCH64_FAILED=0
if [[ "${MAZE_NO_AARCH64:-0}" != 1 ]]; then
  if [[ -x "$AARCH64_TOOL" ]] && "$AARCH64_TOOL" ready; then
    "$AARCH64_TOOL" build "$BUILD" "$OUT" || AARCH64_FAILED=1
  else
    warn "aarch64 skipped — set the ARM root up once: MazeLinux/tools/aarch64-build.sh setup"
  fi
fi

PKG="$(find "$BUILD" -maxdepth 1 -name 'maze-terminal-*.pkg.tar.zst' ! -name '*-debug-*' -print -quit)"
[[ -n "$PKG" ]] || die "makepkg finished but produced no .pkg.tar.zst"
mv -f "$PKG" "$OUT/"
FINAL="$OUT/$(basename "$PKG")"
mv -f "$BUILD/maze-terminal-${VER}.tar.gz" "$OUT/"
rm -rf "$BUILD"

ok "Package ready: $FINAL"
echo "    Source:  $OUT/maze-terminal-${VER}.tar.gz"
echo
echo "  To install (this script does not do it for you):"
echo "    sudo pacman -U \"$FINAL\""
echo

if [[ "$AARCH64_FAILED" == 1 ]]; then
  warn "the aarch64 build FAILED (x86_64 package above is fine) — see the log"
  exit 1
fi
