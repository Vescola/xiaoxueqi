#!/usr/bin/env bash
set -u

BASE=/home/bit/charging_validation
RUN_ROOT=/home/bit/charging_validation_runs/$(date '+%Y%m%d_%H%M%S')_$$
SERVER_SRC="$RUN_ROOT/EVChargingServer"
CLIENT_SRC="$RUN_ROOT/ChargingUserClient_UserClient"
SERVER_BUILD="$RUN_ROOT/EVChargingServer_build"
CLIENT_BUILD="$RUN_ROOT/ChargingUserClient_build"
SMOKE_BUILD="$RUN_ROOT/socket_smoke_build"

SUMMARY="$BASE/summary.txt"
SERVER_BUILD_LOG="$BASE/server_build.log"
CLIENT_BUILD_LOG="$BASE/client_build.log"
SERVER_RUNTIME_LOG="$BASE/server_runtime.log"
SMOKE_BUILD_LOG="$BASE/smoke_build.log"
SMOKE_RESULT="$BASE/smoke_result.txt"
PROCESS_STATUS="$BASE/process_status.txt"

mkdir -p "$BASE"
: > "$SUMMARY"
: > "$PROCESS_STATUS"

log() {
    echo "$1" | tee -a "$SUMMARY"
}

fail() {
    log "[FAIL] $1"
    log "VALIDATION_FAIL"
    exit 1
}

pass() {
    log "[PASS] $1"
}

port_8888_listening() {
    if command -v ss >/dev/null 2>&1; then
        ss -ltn | grep -q ':8888'
    elif command -v netstat >/dev/null 2>&1; then
        netstat -ltn | grep -q ':8888'
    else
        return 1
    fi
}

log "EV Charging validation started: $(date '+%F %T')"
log "Run directory: $RUN_ROOT"

QMAKE_BIN=$(command -v qmake6 || command -v qmake || true)
if [ -z "$QMAKE_BIN" ]; then
    fail "未找到 qmake6/qmake，请先安装 Qt 6 开发环境。"
fi
pass "找到 qmake: $QMAKE_BIN"

if [ ! -f /home/bit/EVChargingServer_latest.zip ]; then
    fail "缺少 /home/bit/EVChargingServer_latest.zip"
fi
if [ ! -f /home/bit/ChargingUserClient_UserClient_latest.zip ]; then
    fail "缺少 /home/bit/ChargingUserClient_UserClient_latest.zip"
fi

mkdir -p "$SERVER_SRC" "$CLIENT_SRC" "$SERVER_BUILD" "$CLIENT_BUILD" "$SMOKE_BUILD"

python3 -m zipfile -e /home/bit/EVChargingServer_latest.zip "$SERVER_SRC" >> "$SUMMARY" 2>&1 \
    || fail "服务端源码解压失败"
python3 -m zipfile -e /home/bit/ChargingUserClient_UserClient_latest.zip "$CLIENT_SRC" >> "$SUMMARY" 2>&1 \
    || fail "用户端源码解压失败"
pass "源码解压完成"

cd "$SERVER_BUILD" || fail "进入服务端构建目录失败"
"$QMAKE_BIN" "$SERVER_SRC/EVChargingServer.pro" > "$SERVER_BUILD_LOG" 2>&1 \
    || fail "服务端 qmake 失败，查看 validate_server_build.log"
make -j"$(nproc)" >> "$SERVER_BUILD_LOG" 2>&1 \
    || fail "服务端 make 失败，查看 validate_server_build.log"
if [ ! -x "$SERVER_BUILD/EVChargingServer" ]; then
    fail "服务端可执行文件不存在"
fi
pass "服务端编译通过"

cd "$CLIENT_BUILD" || fail "进入用户端构建目录失败"
"$QMAKE_BIN" "$CLIENT_SRC/ChargingUserClient.pro" > "$CLIENT_BUILD_LOG" 2>&1 \
    || fail "用户端 qmake 失败，查看 validate_client_build.log"
make -j"$(nproc)" >> "$CLIENT_BUILD_LOG" 2>&1 \
    || fail "用户端 make 失败，查看 validate_client_build.log"
if [ ! -x "$CLIENT_BUILD/ChargingUserClient" ]; then
    fail "用户端可执行文件不存在"
fi
pass "用户端编译通过"

if port_8888_listening; then
    pass "检测到 8888 已有监听，复用当前服务端进程"
else
    nohup env QT_QPA_PLATFORM=offscreen "$SERVER_BUILD/EVChargingServer" > "$SERVER_RUNTIME_LOG" 2>&1 &
    sleep 3
fi

{
    echo PROCESS
    pgrep -af EVChargingServer || true
    echo PORT_8888
    if command -v ss >/dev/null 2>&1; then
        ss -ltnp | grep 8888 || true
    elif command -v netstat >/dev/null 2>&1; then
        netstat -ltnp | grep 8888 || true
    else
        echo "no ss/netstat"
    fi
    echo SERVER_LOG
    tail -80 "$SERVER_RUNTIME_LOG" || true
} > "$PROCESS_STATUS" 2>&1

if port_8888_listening; then
    pass "服务端已监听 8888"
elif ! command -v ss >/dev/null 2>&1 && ! command -v netstat >/dev/null 2>&1; then
    log "[WARN] 未找到 ss/netstat，跳过端口命令检查。"
else
    fail "服务端未监听 8888，查看 validate_process_status.txt"
fi

cd "$SMOKE_BUILD" || fail "进入 socket 测试构建目录失败"
"$QMAKE_BIN" /home/bit/socket_smoke.pro > "$SMOKE_BUILD_LOG" 2>&1 \
    || fail "socket 测试 qmake 失败，查看 validate_smoke_build.log"
make -j"$(nproc)" >> "$SMOKE_BUILD_LOG" 2>&1 \
    || fail "socket 测试 make 失败，查看 validate_smoke_build.log"

"$SMOKE_BUILD/socket_smoke" > "$SMOKE_RESULT" 2>&1
SMOKE_RC=$?
if [ "$SMOKE_RC" -ne 0 ]; then
    fail "socket 测试运行失败，查看 validate_smoke_result.txt"
fi
grep -q "SOCKET_OK" "$SMOKE_RESULT" || fail "socket 测试没有输出 SOCKET_OK"
pass "用户端与服务端 socket 协议验证通过"

cat "$SMOKE_RESULT" >> "$SUMMARY"
log "VALIDATION_OK"
exit 0
