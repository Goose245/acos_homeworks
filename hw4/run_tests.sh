#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if [ ! -x build/barbershop_server ]; then
    cmake -B build
    cmake --build build
fi

mkdir -p test_logs

run_test() {
    local cfg="$1"
    source "$cfg"

    echo "=== Тест: ${NAME} (${IP}:${PORT}, стульев=${CHAIRS}, мастеров=${BARBERS}, клиентов=${CUSTOMERS}) ==="

    local server_log="test_logs/${NAME}_server.log"
    local obs1_log="test_logs/${NAME}_obs1.log"
    local obs2_part1="test_logs/${NAME}_obs2_part1.log"
    local obs2_part2="test_logs/${NAME}_obs2_part2.log"

    ./build/barbershop_server -a "$IP" -p "$PORT" -w "$CHAIRS" > "$server_log" 2>&1 &
    local server_pid=$!
    sleep 0.3

    local barber_pids=()
    for i in $(seq 1 "$BARBERS"); do
        ./build/barbershop_barber -a "$IP" -p "$PORT" -i "$i" -m "$BARBER_MIN" -M "$BARBER_MAX" > "test_logs/${NAME}_barber_${i}.log" 2>&1 &
        barber_pids+=($!)
    done
    sleep 0.2

    ./build/barbershop_observer -a "$IP" -p "$PORT" -l "$obs1_log" > /dev/null 2>&1 &
    local obs1_pid=$!

    ./build/barbershop_observer -a "$IP" -p "$PORT" -l "$obs2_part1" > /dev/null 2>&1 &
    local obs2_pid=$!
    sleep 0.2

    if [ "$DYNAMIC_OBSERVER" -eq 1 ]; then
        ./build/barbershop_customer -a "$IP" -p "$PORT" -n "$CUSTOMERS" -s "$START_ID" -m "$CUST_MIN" -M "$CUST_MAX" > "test_logs/${NAME}_customers.log" 2>&1 &
        local cust_pid=$!

        sleep 1.2
        kill -INT "$obs2_pid" 2>/dev/null || true
        wait "$obs2_pid" 2>/dev/null || true
        echo "  Наблюдатель #2 отключен во время работы"

        sleep 1.2
        ./build/barbershop_observer -a "$IP" -p "$PORT" -l "$obs2_part2" > /dev/null 2>&1 &
        obs2_pid=$!
        echo "  Наблюдатель #2 подключен повторно"

        wait "$cust_pid"
    else
        ./build/barbershop_customer -a "$IP" -p "$PORT" -n "$CUSTOMERS" -s "$START_ID" -m "$CUST_MIN" -M "$CUST_MAX" > "test_logs/${NAME}_customers.log" 2>&1
    fi

    sleep 0.3
    kill -INT "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    wait "$obs1_pid" 2>/dev/null || true
    wait "$obs2_pid" 2>/dev/null || true
    for pid in "${barber_pids[@]}"; do
        wait "$pid" 2>/dev/null || true
    done

    grep "Остановка. Итоги:" "$server_log" || true
    echo "  Строк в логе наблюдателя #1: $(wc -l < "$obs1_log")"
    if [ "$DYNAMIC_OBSERVER" -eq 1 ]; then
        echo "  Наблюдатель #2 (до отключения): $(wc -l < "$obs2_part1") строк"
        echo "  Наблюдатель #2 (после переподключения): $(wc -l < "$obs2_part2") строк"
    fi
    echo ""
}

for cfg in data/test_*.env; do
    run_test "$cfg"
done

echo "Все тесты завершены. Логи сохранены в hw4/test_logs/"