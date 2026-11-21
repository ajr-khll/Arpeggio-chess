# Shared helpers for scripts that drive the Mac Studio. Source after match.conf.
# Expects: HOST, REMOTE_DIR, and cwd = chess-ai repo root.

ssh_mac() { ssh -o ConnectTimeout=10 "$HOST" "export PATH=/opt/homebrew/bin:\$PATH; $*"; }

# resolve "latest" -> ./VERSION, validate X.Y.Z
resolve_version() {
  local v=$1
  [[ $v == latest ]] && v=$(cat VERSION)
  [[ $v =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo "bad version: $1" >&2; return 1; }
  echo "$v"
}

# ensure_mac_engine VERSION [force]
# Builds $REMOTE_DIR/engines/Arpeggio-V/ on the Mac from Engines/Arpeggio-V/src.tar.gz,
# so the Mac binary for a version number is built from the same source as the laptop's.
ensure_mac_engine() {
  local v=$1 force=${2:-0}
  ssh_mac "mkdir -p $REMOTE_DIR/{engines,build,openings,matches}"
  if [[ $force == 1 ]] || ! ssh_mac "test -x $REMOTE_DIR/engines/Arpeggio-$v/Arpeggio-$v"; then
    local snap="Engines/Arpeggio-$v/src.tar.gz"
    [[ -f $snap ]] || { echo "no $snap - can't build $v on the Mac (only versions built after snapshots were added have one)" >&2; return 1; }
    echo ">> building Arpeggio $v on $HOST"
    scp -q "$snap" "$HOST:$REMOTE_DIR/build/$v.tar.gz"
    ssh_mac "set -e; cd $REMOTE_DIR/build && rm -rf $v && mkdir $v && tar xzf $v.tar.gz -C $v \
      && { make -s -C $v -j\$(sysctl -n hw.ncpu) as-version V=$v 2>&1 | grep -E 'error' || true; } \
      && test -x $v/Engines/Arpeggio-$v/Arpeggio-$v \
      && rm -rf ../engines/Arpeggio-$v && mv $v/Engines/Arpeggio-$v ../engines/"
  fi
}
