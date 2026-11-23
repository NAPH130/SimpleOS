#!/bin/sh
set -eu

# 内核会一直停机等待，超时是预期行为；其他 QEMU 失败必须报错。
log=build/boot-test.log
status=0
timeout 15s qemu-system-x86_64 -cdrom build/simpleos.iso -boot d -m 128M \
    -display none -serial file:"$log" -monitor none -no-reboot -no-shutdown \
    || status=$?
if [ "$status" -ne 0 ] && [ "$status" -ne 124 ]; then
    echo "QEMU failed (exit $status)" >&2
    exit "$status"
fi
banner_top=$(printf ' ____  _                 _       ___  ____\r')
expected_logs=$(printf '[\033[32mINFO\033[0m] %s\r\n' \
    'COM1 serial console initialized.' \
    'Limine base revision accepted.' \
    'Minimal kernel ready. Halting CPU.')
if [ "$(head -n 1 "$log")" != "$banner_top" ] ||
    [ "$(tail -n 3 "$log")" != "$expected_logs" ]; then
    echo 'Expected the SimpleOS banner followed by colored startup logs:' >&2
    cat "$log" >&2
    exit 1
fi
cat "$log"
echo 'Boot test passed.'
