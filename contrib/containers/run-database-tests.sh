#!/bin/sh
set -eu

if [ "$(id -u)" -ne 0 ] || [ "$#" -ne 2 ]; then
    echo "Usage: run-database-tests UID GID (as container root)" >&2
    exit 2
fi

uid=$1
gid=$2
case "$uid:$gid" in
    *[!0-9:]*|:*|*:) echo "UID and GID must be numeric" >&2; exit 2 ;;
esac
if [ "$uid" -eq 0 ]; then
    echo "The build/test UID must not be root" >&2
    exit 2
fi

pgbin=$(pg_config --bindir)
work=$(mktemp -d /tmp/fd-databases.XXXXXXXX)
pg_started=0
mysql_pid=
mysql_ready=0
cleanup() {
    result=0
    if [ "$pg_started" -eq 1 ]; then
        runuser -u postgres -- "$pgbin/pg_ctl" -D "$work/pgdata" -m fast -w stop || result=1
    fi
    if [ -n "$mysql_pid" ]; then
        if [ "$mysql_ready" -eq 1 ]; then
            mariadb-admin --no-defaults --socket="$work/mysql.sock" --user=root shutdown || result=1
        elif kill -0 "$mysql_pid" 2>/dev/null; then
            kill "$mysql_pid" || result=1
        fi
        wait "$mysql_pid" || result=1
    fi
    return "$result"
}
trap cleanup EXIT

chmod 755 "$work"
mkdir "$work/pgdata" "$work/pgsocket" "$work/mysql"
chown postgres:postgres "$work/pgdata" "$work/pgsocket"
chown mysql:mysql "$work/mysql" "$work"
runuser -u postgres -- "$pgbin/initdb" -D "$work/pgdata" --auth=trust --no-instructions
runuser -u postgres -- "$pgbin/pg_ctl" -D "$work/pgdata" -l "$work/pgdata/server.log" \
    -o "-c listen_addresses='' -c unix_socket_directories=$work/pgsocket" -w start
pg_started=1
export PGHOST="$work/pgsocket" PGUSER=postgres PGDATABASE=postgres
createdb fd_accounting
psql -X -v ON_ERROR_STOP=1 -d fd_accounting -c 'CREATE TABLE incoming_test (
  recorded_on timestamp with time zone NOT NULL,
  "Accounting-Record-Type" integer, "Session-Id" bytea,
  "Accounting-Record-Number" integer, "Route-Record1" bytea,
  "Route-Record2" bytea, "Route-Record3" bytea, "Route-Record4" bytea)'

mariadb-install-db --user=mysql --datadir="$work/mysql" \
    --auth-root-authentication-method=normal --skip-test-db
runuser -u mysql -- mariadbd --no-defaults --datadir="$work/mysql" \
    --socket="$work/mysql.sock" --pid-file="$work/mysql.pid" \
    --skip-networking --log-error="$work/mysql.log" &
mysql_pid=$!
ready=0
for attempt in $(seq 1 60); do
    if mariadb-admin --no-defaults --socket="$work/mysql.sock" --user=root ping >/dev/null 2>&1; then
        ready=1
        break
    fi
    if ! kill -0 "$mysql_pid" 2>/dev/null; then
        cat "$work/mysql.log" >&2
        exit 1
    fi
    sleep 1
done
if [ "$ready" -ne 1 ]; then
    echo "MariaDB did not become ready" >&2
    cat "$work/mysql.log" >&2
    exit 1
fi
mysql_ready=1
mariadb --no-defaults --socket="$work/mysql.sock" --user=root <<'SQL'
CREATE DATABASE fd_diameap;
CREATE DATABASE fd_sip;
CREATE USER 'fd_test'@'localhost' IDENTIFIED BY '';
GRANT SELECT ON fd_diameap.* TO 'fd_test'@'localhost';
GRANT SELECT, UPDATE ON fd_sip.* TO 'fd_test'@'localhost';
SQL
mariadb --no-defaults --socket="$work/mysql.sock" --user=root fd_diameap < /src/extensions/app_diameap/diameap.sql
mariadb --no-defaults --socket="$work/mysql.sock" --user=root fd_diameap < /src/tests/test_diameap_db.sql
mariadb --no-defaults --socket="$work/mysql.sock" --user=root fd_sip < /src/doc/app_sip.sql
mariadb --no-defaults --socket="$work/mysql.sock" --user=root fd_sip < /src/tests/test_sip_db.sql

export FD_TEST_MYSQL_SOCKET="$work/mysql.sock" FD_TEST_MYSQL_DATABASE=fd_sip \
    FD_TEST_MYSQL_USER=fd_test HOME=/tmp
setpriv --reuid="$uid" --regid="$gid" --clear-groups sh -c '
    set -eu
    cmake -S /src -B /build -DALL_EXTENSIONS=ON -DTEST_APP_ACCT=ON \
      -DTEST_APP_ACCT_CONNINFO:STRING="dbname=fd_accounting" -DTEST_DIAMEAP_MYSQL=ON \
      -DTEST_DIAMEAP_EXCHANGE=ON \
      -DTEST_APP_SIP_MYSQL=ON -DTEST_APP_SIP_DISPATCH=ON
    cmake --build /build --parallel 2
    ctest --test-dir /build --output-on-failure
'
