#!/bin/bash

# Clean experiment runner for Part C
# Uses namespaces + perf + CSV output

set -e

if [[ $EUID -ne 0 ]]; then
    echo "Please run with sudo"
    exit 1
fi

LISTEN_PORT=8080
RUN_SECONDS=5

SIZES_LIST=(1024 4096 16384 65536)
WORKER_LIST=(1 2 4 8)
IMPL_LIST=("A1" "A2" "A3")

SERVER_ADDR="10.0.0.1"
CLIENT_ADDR="10.0.0.2"

RESULT_FILE="MT25087_Result.csv"

SERVER_NS="srv"
CLIENT_NS="cli"

BINARIES=()

# ---------------- Helpers ----------------

allow_perf() {
    echo -1 > /proc/sys/kernel/perf_event_paranoid 2>/dev/null || true
}

destroy_env() {
    ip netns del $SERVER_NS 2>/dev/null || true
    ip netns del $CLIENT_NS 2>/dev/null || true
    pkill -f perf 2>/dev/null || true
    rm -f *_server *_client perf.log server.log client.log
}

trap destroy_env EXIT

build_all() {

    echo "[Build] compiling sources..."

    for v in "${IMPL_LIST[@]}"; do
        gcc -O2 MT25087_Part_A_${v}_server.c -o ${v}_server -lpthread
        gcc -O2 MT25087_Part_A_${v}_client.c -o ${v}_client
        BINARIES+=("${v}_server" "${v}_client")
    done
}

setup_network() {

    destroy_env

    ip netns add $SERVER_NS
    ip netns add $CLIENT_NS

    ip link add s_veth type veth peer name c_veth

    ip link set s_veth netns $SERVER_NS
    ip link set c_veth netns $CLIENT_NS

    ip netns exec $SERVER_NS ip addr add ${SERVER_ADDR}/24 dev s_veth
    ip netns exec $SERVER_NS ip link set s_veth up
    ip netns exec $SERVER_NS ip link set lo up

    ip netns exec $CLIENT_NS ip addr add ${CLIENT_ADDR}/24 dev c_veth
    ip netns exec $CLIENT_NS ip link set c_veth up
    ip netns exec $CLIENT_NS ip link set lo up
}

csv_header() {
    echo "Impl,Size,Workers,ThroughputGbps,LatencyUs,Cycles,L1Miss,LLCMiss,ContextSwitch" > $RESULT_FILE
}

extract_perf() {

    local key=$1
    awk -F, "/$key/ {gsub(/,/,\"\", \$1); print \$1; exit}" perf.log 2>/dev/null || echo 0
}

run_single() {

    local impl=$1
    local bytes=$2
    local workers=$3

    echo "[Run] $impl size=$bytes workers=$workers"

    ip netns exec $SERVER_NS perf stat \
        -e cycles,L1-dcache-load-misses,LLC-load-misses,context-switches \
        -x, -o perf.log \
        ./${impl}_server $LISTEN_PORT $bytes $workers \
        > server.log 2>&1 &

    PERF_PID=$!
    sleep 1

    ip netns exec $CLIENT_NS timeout $((RUN_SECONDS+5)) \
        ./${impl}_client $SERVER_ADDR $LISTEN_PORT $bytes $RUN_SECONDS \
        > client.log 2>&1 || true

    ip netns exec $SERVER_NS pkill -INT -f ${impl}_server 2>/dev/null || true
    wait $PERF_PID 2>/dev/null || true

    total=$(awk '/Total bytes received/ {print $NF}' client.log | tail -1)
    [[ -z "$total" ]] && total=0

    msgs=$(awk -v t="$total" -v s="$bytes" 'BEGIN{ if(s>0) print int(t/s); else print 0 }')

    tput=$(awk -v t="$total" -v d="$RUN_SECONDS" \
        'BEGIN{printf "%.6f",(t*8)/(d*1e9)}')

    if [[ "$msgs" -gt 0 ]]; then
        lat=$(awk -v d="$RUN_SECONDS" -v m="$msgs" \
            'BEGIN{printf "%.3f",(d*1e6)/m}')
    else
        lat=0
    fi

    cyc=$(extract_perf cycles)
    l1=$(extract_perf L1-dcache-load-misses)
    llc=$(extract_perf LLC-load-misses)
    ctx=$(extract_perf context-switches)

    echo "$impl,$bytes,$workers,$tput,$lat,$cyc,$l1,$llc,$ctx" >> $RESULT_FILE

    sleep 0.5
}

# ---------------- Main ----------------

allow_perf
build_all
setup_network
csv_header

for impl in "${IMPL_LIST[@]}"; do
    for sz in "${SIZES_LIST[@]}"; do
        for wk in "${WORKER_LIST[@]}"; do
            run_single $impl $sz $wk
        done
    done
done

echo
echo "Finished. Results stored in $RESULT_FILE"
