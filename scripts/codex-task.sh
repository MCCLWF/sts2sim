#!/usr/bin/env bash
# Delegate one work item to Codex CLI, in its own git worktree and branch.
#
#   scripts/codex-task.sh <luna|sol> <branch> <prompt-file>
#   scripts/codex-task.sh <luna|sol> <branch> --resume [prompt-file]
#
#   luna  gpt-6-luna, reasoning xhigh: pipeline work (porting files by a template,
#         audits, tables, docs)
#   sol   gpt-6-sol, reasoning high: self-contained tools and features that need
#         judgement
#
# The worktree is created from the current HEAD (or $BASE, e.g. BASE=main) at
# ../sts2sim-wt/<branch> (slashes
# become dashes). Codex's transcript goes to <worktree>.log and its final
# message to <worktree>.md. Codex reads AGENTS.md itself; the prompt should
# name the plan file it executes. When the model reports it is at capacity the
# session is resumed after a pause (up to 6 times). --resume continues the
# branch's last session by hand. Review the branch, then merge it.
set -euo pipefail

if [[ $# -lt 3 ]]; then
  sed -n '2,17p' "$0" >&2
  exit 2
fi
TIER="$1" BRANCH="$2"
shift 2
case "$TIER" in
  luna) MODEL=gpt-6-luna EFFORT=xhigh ;;
  sol) MODEL=gpt-6-sol EFFORT=high ;;
  *) echo "tier must be luna or sol" >&2; exit 2 ;;
esac

# The main checkout (also when run from another worktree): the parent of the
# shared .git directory.
ROOT="$(dirname "$(realpath "$(git rev-parse --git-common-dir)")")"
WT_BASE="$(dirname "$ROOT")/sts2sim-wt"
WT="$WT_BASE/${BRANCH//\//-}"
LOG="$WT.log"
CONTINUE_PROMPT="上一次运行因为模型容量不足中断了。检查工作区当前状态（git status、已有改动），继续完成计划里尚未完成的任务，最后按原要求提交并回复。"

# Paths the worktree cannot derive from its own location (see AGENTS.md §9).
export STS2_SRC="${STS2_SRC:-$(dirname "$ROOT")/Slay the Spire 2/src/Core}"
export DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1

# The worktree's git metadata lives in the main repository's .git, which must
# be writable for Codex to commit.
GIT_COMMON="$(cd "$ROOT" && realpath "$(git rev-parse --git-common-dir)")"
COMMON=(-c "model=\"$MODEL\"" -c "model_reasoning_effort=\"$EFFORT\"" -c 'sandbox_mode="workspace-write"'
        -c "sandbox_workspace_write.writable_roots=[\"$HOME/.nuget\", \"$HOME/.dotnet\", \"$HOME/.local/share/NuGet\", \"$GIT_COMMON\"]")

session_id() { grep -m1 -o 'session id: [0-9a-f-]*' "$LOG" | awk '{print $3}'; }
at_capacity() { tail -5 "$LOG" | grep -q "at capacity"; }

resume() {
  local prompt="$1" id
  id="$(session_id)"
  [[ -n "$id" ]] || { echo "no session id in $LOG" >&2; return 1; }
  echo "=== resume $(date -Is)" >> "$LOG"
  (cd "$WT" && codex exec resume "${COMMON[@]}" -o "$WT.md" "$id" - <<< "$prompt") >> "$LOG" 2>&1
}

if [[ "$1" == "--resume" ]]; then
  PROMPT_TEXT="$CONTINUE_PROMPT"
  [[ $# -ge 2 ]] && PROMPT_TEXT="$(cat "$2")"
  set +e; resume "$PROMPT_TEXT"; status=$?; set -e
else
  PROMPT="$1"
  [[ -f "$PROMPT" ]] || { echo "no prompt file: $PROMPT" >&2; exit 2; }
  mkdir -p "$WT_BASE"
  [[ -d "$WT" ]] || git -C "$ROOT" worktree add -q -b "$BRANCH" "$WT" "${BASE:-HEAD}"
  set +e
  codex exec "${COMMON[@]}" -C "$WT" -o "$WT.md" - < "$PROMPT" > "$LOG" 2>&1
  status=$?
  set -e
fi

for attempt in 1 2 3 4 5 6; do
  [[ $status -ne 0 ]] && at_capacity || break
  echo "model at capacity; resuming in 3 minutes (attempt $attempt)" >&2
  sleep 180
  set +e; resume "$CONTINUE_PROMPT"; status=$?; set -e
done
echo "codex exit $status: branch $BRANCH, worktree $WT, summary $WT.md"
exit $status
