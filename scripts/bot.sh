#!/usr/bin/env bash
# Manage the lichess bot running 24/7 on the Mac Studio (launchd LaunchAgent).
#
# Usage: scripts/bot.sh COMMAND
#   deploy [VERSION|latest]  build that Arpeggio version on the Mac (same source as the laptop),
#                            point lichess-bot at it and restart. Default: latest
#   push-config              copy ~/Projects/lichess-bot/config.yml to the Mac (keeps the deployed engine) and restart
#   start | stop | restart   control the launchd service
#   status                   service state + which engine version is deployed
#   logs [-f]                tail the bot log (-f to follow)
#
# The Mac copy lives in ~/lichess-bot; the service is ~/Library/LaunchAgents/$LABEL.plist.

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
source "$ROOT/scripts/match.conf"
source "$ROOT/scripts/remote.sh"

BOT_DIR=lichess-bot                     # relative to the Mac's $HOME
LOCAL_BOT="$HOME/Projects/lichess-bot"
LABEL=com.arpeggio.lichess-bot
PLIST="Library/LaunchAgents/$LABEL.plist"
DOMAIN='gui/$(id -u)'

write_plist() {
  ssh_mac "mkdir -p ~/Library/LaunchAgents ~/$BOT_DIR/logs; cat > ~/$PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>Label</key><string>$LABEL</string>
  <key>WorkingDirectory</key><string>/Users/$(ssh_mac whoami)/$BOT_DIR</string>
  <key>ProgramArguments</key>
  <array>
    <string>venv/bin/python</string>
    <string>lichess-bot.py</string>
  </array>
  <key>EnvironmentVariables</key>
  <dict>
    <key>PATH</key><string>/opt/homebrew/bin:/usr/bin:/bin</string>
    <key>PYTHONUNBUFFERED</key><string>1</string>
    <key>COLUMNS</key><string>160</string>
  </dict>
  <key>RunAtLoad</key><true/>
  <key>KeepAlive</key><true/>
  <key>ThrottleInterval</key><integer>60</integer>
  <key>ProcessType</key><string>Interactive</string>
  <key>StandardOutPath</key><string>/Users/$(ssh_mac whoami)/$BOT_DIR/logs/stdout.log</string>
  <key>StandardErrorPath</key><string>/Users/$(ssh_mac whoami)/$BOT_DIR/logs/stdout.log</string>
</dict>
</plist>
EOF
}

svc_start()   { write_plist; ssh_mac "launchctl bootstrap $DOMAIN ~/$PLIST 2>/dev/null || launchctl kickstart $DOMAIN/$LABEL"; echo ">> started"; }
svc_stop()    { ssh_mac "launchctl bootout $DOMAIN/$LABEL 2>/dev/null || true"; echo ">> stopped"; }
svc_restart() { svc_stop; sleep 2; svc_start; }

cmd=${1:-status}; shift || true
case $cmd in
  deploy)
    v=$(resolve_version "${1:-latest}")
    ensure_mac_engine "$v"
    ssh_mac "set -e; cd ~/$BOT_DIR/engines && find . -maxdepth 1 -iname 'arpeggio-*' -exec rm -rf {} + \
      && cp ~/$REMOTE_DIR/engines/Arpeggio-$v/{Arpeggio-$v,arpeggio-$v.sh} . \
      && sed -i '' -E '/^engine:/,/^[^ #]/ s|^(  name: )\"[^\"]*\"|\1\"arpeggio-$v.sh\"|' ../config.yml \
      && grep -m1 '^  name:' ../config.yml"
    echo ">> deployed Arpeggio $v"
    svc_restart ;;
  push-config)
    name=$(ssh_mac "grep -m1 '^  name:' ~/$BOT_DIR/config.yml")
    scp -q "$LOCAL_BOT/config.yml" "$HOST:$BOT_DIR/config.yml"
    ssh_mac "chmod 600 ~/$BOT_DIR/config.yml; sed -i '' -E '/^engine:/,/^[^ #]/ s|^  name: .*|$name|' ~/$BOT_DIR/config.yml"
    echo ">> config pushed (engine kept: $name)"
    svc_restart ;;
  start)   svc_start ;;
  stop)    svc_stop ;;
  restart) svc_restart ;;
  status)
    ssh_mac "launchctl print $DOMAIN/$LABEL 2>/dev/null | grep -E '^[[:space:]]+(state|pid|last exit code|runs) ' || echo 'not loaded'; \
      echo; grep -m1 '^  name:' ~/$BOT_DIR/config.yml" ;;
  logs)
    ssh_mac "tail -n 50 ${1:+-f} ~/$BOT_DIR/logs/stdout.log" ;;
  -h|--help|help) sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//' ;;
  *) echo "unknown command: $cmd" >&2; exit 1 ;;
esac
