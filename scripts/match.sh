#!/usr/bin/env bash
# Run Arpeggio engine matches with cutechess-cli on the Mac Studio (via tailscale ssh).
#
# Engine versions on the Mac are rebuilt from Engines/Arpeggio-X.Y.Z/src.tar.gz
# (written by every `make`), so version numbers match the laptop exactly.
#
# The match runs detached on the Mac, so closing the laptop or losing the
# connection doesn't stop it. Starting a match follows its live output; Ctrl-C
# only stops following. Results are copied back when it finishes while you're
# following, or later with `fetch`.
#
# Usage: scripts/match.sh [options] ENGINE ENGINE [ENGINE...]
#        scripts/match.sh list             matches on the Mac, running or finished
#        scripts/match.sh attach [TAG]     follow a match again (default: newest)
#        scripts/match.sh fetch  [TAG]     copy its PGN and log to ./matches/ (works mid-match too)
#        scripts/match.sh stop   [TAG]     stop a running match
#
# ENGINE:
#   1.0.8          an Arpeggio version
#   latest         the version in ./VERSION
#   sf | sf:1500   Stockfish (full strength, or UCI_Elo limited; min 1320)
#
# Options:
#   -n GAMES       games per pairing                    (default from match.conf)
#   -t TC          time control, e.g. 10+0.1, 40/60, 5+0
#   -s SECONDS     fixed time per move instead of -t, e.g. 0.5
#   -c N           concurrent games
#   -b BOOK        opening book (.pgn/.epd): Mac openings/ name or local path; "none"
#   -p PLIES       max book plies
#   -g             gauntlet: first engine plays each of the others
#   -r             force rebuild of Arpeggio versions on the Mac
#   -H HOST        ssh host
#   -- ARGS...     extra raw cutechess-cli args (e.g. -- -sprt elo0=0 elo1=10 alpha=0.05 beta=0.05)
#
# Examples:
#   scripts/match.sh latest 1.0.7
#   scripts/match.sh -n 400 -t 5+0.05 -c 12 1.0.8 1.0.6
#   scripts/match.sh -g -n 50 latest 1.0.5 1.0.6 1.0.7 sf:1500
#   scripts/match.sh -b test-positions/endgames.epd -n 26 1.0.8 1.0.7
#
# PGNs and logs are copied back to ./matches/.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
source "$ROOT/scripts/match.conf"

MOVETIME="" REBUILD=0 EXTRA=()
usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }

# --- detached runs: helpers and subcommands -----------------------------------
MDIR="$REMOTE_DIR/matches"

newest_tag() {
  local t
  t=$(ssh_mac "ls -t $MDIR 2>/dev/null | grep '\.run\.sh$' | head -1")
  [[ -n $t ]] || { echo "no matches found on $HOST" >&2; exit 1; }
  t=$(basename "$t"); echo "${t%.run.sh}"
}

is_running() { ssh_mac "test -f $MDIR/$1.pid && kill -0 \$(cat $MDIR/$1.pid) 2>/dev/null"; }

fetch() {
  local tag=$1
  mkdir -p matches
  scp -q "$HOST:$MDIR/$tag.pgn" "$HOST:$MDIR/$tag.log" matches/ 2>/dev/null \
    && echo ">> saved matches/$tag.pgn and .log" || echo ">> nothing to copy yet for $tag"
  if is_running "$tag"; then
    echo ">> $tag is still running on $HOST (scripts/match.sh attach | fetch | stop)"
    return 0
  fi
  return "$(ssh_mac "cat $MDIR/$tag.done 2>/dev/null || echo 1")"
}

follow() {
  local tag=$1
  echo ">> following $tag on $HOST. Ctrl-C stops following; the match keeps running."
  ssh -t -o ConnectTimeout=10 "$HOST" "cd $MDIR || exit 1
    for i in 1 2 3 4 5 6 7 8 9 10; do [ -f $tag.pid ] && [ -f $tag.log ] && break; sleep 0.5; done
    tail -n +1 -f $tag.log & t=\$!
    while kill -0 \$(cat $tag.pid 2>/dev/null) 2>/dev/null; do sleep 2; done
    sleep 1; kill \$t 2>/dev/null" || true
  trap '' INT
  fetch "$tag"
}

case ${1:-} in
  list)
    source "$ROOT/scripts/remote.sh"
    ssh_mac "cd $MDIR 2>/dev/null || exit 0; for f in \$(ls -t | grep '\.run\.sh$'); do t=\${f%.run.sh}
      if [ -f \$t.pid ] && kill -0 \$(cat \$t.pid) 2>/dev/null; then s=running; elif [ \"\$(cat \$t.done 2>/dev/null)\" = 143 ]; then s=stopped; elif [ -f \$t.done ]; then s=\"finished (exit \$(cat \$t.done))\"; else s=\"died (no exit code)\"; fi
      printf '%-60s %4s games  %s\n' \$t \$(grep -c '^\[Result' \$t.pgn 2>/dev/null || echo 0) \"\$s\"; done"
    exit 0 ;;
  attach|fetch|stop)
    source "$ROOT/scripts/remote.sh"
    sub=$1; tag=${2:-$(newest_tag)}
    case $sub in
      attach) follow "$tag"; exit $? ;;
      fetch)  fetch "$tag"; exit $? ;;
      stop)
        if is_running "$tag"; then
          ssh_mac "p=\$(cat $MDIR/$tag.pid); pkill -TERM -P \$p; kill \$p 2>/dev/null; echo 143 > $MDIR/$tag.done"
          echo ">> stopped $tag"
        else
          echo ">> $tag is not running"
        fi
        fetch "$tag" || true; exit 0 ;;
    esac ;;
esac

while getopts "n:t:s:c:b:p:grH:h" opt; do
  case $opt in
    n) GAMES=$OPTARG ;;   t) TC=$OPTARG ;;        s) MOVETIME=$OPTARG ;;
    c) CONCURRENCY=$OPTARG ;; b) BOOK=$OPTARG ;;  p) BOOK_PLIES=$OPTARG ;;
    g) TOURNAMENT=gauntlet ;; r) REBUILD=1 ;;     H) HOST=$OPTARG ;;
    h) usage ;; *) usage 1 ;;
  esac
done
shift $((OPTIND - 1))
ENGINES=()
while [[ $# -gt 0 && $1 != -- ]]; do ENGINES+=("$1"); shift; done
[[ ${1:-} == -- ]] && { shift; EXTRA=("$@"); }
[[ ${#ENGINES[@]} -ge 2 ]] || { echo "need at least two engines" >&2; usage 1; }

source "$ROOT/scripts/remote.sh"
ssh_mac "mkdir -p $REMOTE_DIR/{engines,build,openings,matches}"

# --- make sure every Arpeggio version exists on the Mac -----------------------
ENGINE_ARGS=() NAMES=()
for spec in "${ENGINES[@]}"; do
  case $spec in
    sf|stockfish)
      ENGINE_ARGS+=(-engine name=Stockfish cmd=stockfish); NAMES+=(sf) ;;
    sf:*|stockfish:*)
      elo=${spec#*:}
      ENGINE_ARGS+=(-engine "name=Stockfish-$elo" cmd=stockfish option.UCI_LimitStrength=true "option.UCI_Elo=$elo")
      NAMES+=("sf$elo") ;;
    *)
      v=$(resolve_version "$spec")
      ensure_mac_engine "$v" "$REBUILD"
      ENGINE_ARGS+=(-engine "name=Arpeggio-$v" "cmd=./Arpeggio-$v" "dir=engines/Arpeggio-$v" arg=uci)
      NAMES+=("$v") ;;
  esac
done

# --- openings ------------------------------------------------------------------
BOOK_ARGS=()
if [[ $BOOK != none ]]; then
  if [[ -f $BOOK ]]; then
    scp -q "$BOOK" "$HOST:$REMOTE_DIR/openings/"
    BOOK=$(basename "$BOOK")
  fi
  fmt=${BOOK##*.}
  BOOK_ARGS=(-openings "file=openings/$BOOK" "format=$fmt" order=random "plies=$BOOK_PLIES" -repeat)
fi

# --- run ------------------------------------------------------------------------
ROUNDS=$(( (GAMES + 1) / 2 ))
if [[ -n $MOVETIME ]]; then TIME_ARGS=("st=$MOVETIME"); else TIME_ARGS=("tc=$TC"); fi
TAG="$(date +%Y%m%d-%H%M%S)_$(IFS=-; echo "${NAMES[*]}")"

CMD=(cutechess-cli
  "${ENGINE_ARGS[@]}"
  -each proto=uci "${TIME_ARGS[@]}" timemargin=200
  -tournament "$TOURNAMENT" -games 2 -rounds "$ROUNDS"
  -concurrency "$CONCURRENCY"
  "${BOOK_ARGS[@]}"
  -draw movenumber=40 movecount=8 score=10
  -resign movecount=5 score=1000
  -maxmoves 200
  -recover
  -ratinginterval 10 -outcomeinterval 0
  -pgnout "matches/$TAG.pgn"
  "${EXTRA[@]}")

echo ">> $HOST: ${CMD[*]}"

# Write a run script on the Mac and start it detached (nohup, caffeinate keeps the Mac awake),
# so it no longer depends on this ssh session.
ssh_mac "cat > $MDIR/$TAG.run.sh" <<EOF
echo \$\$ > matches/$TAG.pid
$(printf '%q ' "${CMD[@]}") > matches/$TAG.log 2>&1
echo \$? > matches/$TAG.done
EOF
ssh_mac "cd $REMOTE_DIR && nohup caffeinate -i sh matches/$TAG.run.sh </dev/null >/dev/null 2>&1 &"
echo ">> started $TAG on $HOST (detached)"

follow "$TAG"
