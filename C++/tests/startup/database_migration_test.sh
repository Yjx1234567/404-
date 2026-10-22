#!/usr/bin/env bash
# No database is accessed: every migration runs against the mysql stub below.
set -euo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
MIGRATION="${1:-$TEST_DIR/../../database/migrate.sh}"
MOCK_DIR="$(mktemp -d)"
cleanup() {
    rm -f -- "$MOCK_DIR/mysql" "$MOCK_DIR/calls" "$MOCK_DIR/output" \
        "$MOCK_DIR/spj_code.exists" "$MOCK_DIR/solution.exists"
    rmdir -- "$MOCK_DIR"
}
trap cleanup EXIT

cat > "$MOCK_DIR/mysql" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
printf '%s\n' "$*" >> "$MOCK_DIR/calls"
for arg in "$@"; do
    case "$arg" in
        --execute=*) stmt="${arg#--execute=}" ;;
    esac
done
case "$stmt" in
    'ALTER TABLE problems ADD COLUMN spj_code MEDIUMTEXT NULL') column='spj_code' ;;
    'ALTER TABLE problems ADD COLUMN solution MEDIUMTEXT NULL') column='solution' ;;
    *) echo 'ERROR 1064 (42000): Unexpected SQL' >&2; exit 1 ;;
esac
if [[ "${MOCK_FAIL_COLUMN:-}" == "$column" ]]; then
    # The text contains 1060, but the actual error code is not duplicate-column.
    echo 'ERROR 1049 (42000): Unknown database database_1060' >&2
    exit 7
fi
if [[ -f "$MOCK_DIR/$column.exists" ]]; then
    printf "ERROR 1060 (42S21) at line 1: Duplicate column name '%s'\n" "$column" >&2
    exit 1
fi
: > "$MOCK_DIR/$column.exists"
MOCK
chmod +x "$MOCK_DIR/mysql"
export MOCK_DIR
export PATH="$MOCK_DIR:$PATH"

fail() { printf '[FAIL] %s\n' "$1" >&2; exit 1; }

# First run adds both columns and uses the documented defaults.
unset MYSQL_USER MYSQL_DATABASE MOCK_FAIL_COLUMN
bash "$MIGRATION" > "$MOCK_DIR/output" 2>&1
[[ -f "$MOCK_DIR/spj_code.exists" && -f "$MOCK_DIR/solution.exists" ]] || fail 'Missing column on first run'
grep -F -- "--user=$(whoami) --database=oj_system" "$MOCK_DIR/calls" >/dev/null || fail 'Default connection arguments'
[[ $(grep -c '\[OK\]' "$MOCK_DIR/output") == 2 ]] || fail 'First run should apply both migrations'

# Re-running must treat only duplicate-column errors as successful skips.
bash "$MIGRATION" > "$MOCK_DIR/output" 2>&1
[[ $(grep -c '\[SKIP\]' "$MOCK_DIR/output") == 2 ]] || fail 'Repeated run is not idempotent'

# Explicit connection arguments must be passed intact as separate arguments.
MYSQL_USER='custom-user' MYSQL_DATABASE='custom_database' bash "$MIGRATION" > "$MOCK_DIR/output" 2>&1
grep -F -- '--user=custom-user --database=custom_database' "$MOCK_DIR/calls" >/dev/null || fail 'Custom connection arguments'

# A failure of the first migration must stop before the second SQL statement.
: > "$MOCK_DIR/calls"
set +e
MOCK_FAIL_COLUMN=spj_code bash "$MIGRATION" > "$MOCK_DIR/output" 2>&1
status=$?
set -e
[[ $status == 7 ]] || fail 'Migration did not retain the mysql failure exit code'
[[ $(wc -l < "$MOCK_DIR/calls") == 1 ]] || fail 'Migration continued after a failed SQL statement'
if grep -F '[INFO] 迁移完成' "$MOCK_DIR/output" >/dev/null; then fail 'Failure was reported as success'; fi

# A failure after a valid duplicate-column skip must still fail the whole run.
set +e
MOCK_FAIL_COLUMN=solution bash "$MIGRATION" > "$MOCK_DIR/output" 2>&1
status=$?
set -e
[[ $status == 7 ]] || fail 'Second-column failure was swallowed'
grep -F '[SKIP]' "$MOCK_DIR/output" >/dev/null || fail 'Existing first column should be skipped'

printf '[PASS] defaults, new columns, idempotency, custom connection, and both failure paths\n'
