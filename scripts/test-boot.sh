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
expected_logs=$(printf '[\033[%sm%s\033[0m] %s\r\n' \
    32 INFO 'COM1 serial console initialized.' \
    32 INFO 'Limine base revision accepted.' \
    34 DEBUG 'before int3' \
    34 DEBUG 'breakpoint handled' \
    34 DEBUG 'after int3' \
    32 INFO 'Minimal kernel ready. Halting CPU.')
if [ "$(head -n 1 "$log")" != "$banner_top" ] ||
    [ "$(tail -n 6 "$log")" != "$expected_logs" ]; then
    echo 'Expected the SimpleOS banner, startup logs, and successful int3 round trip:' >&2
    cat "$log" >&2
    exit 1
fi
cat "$log"
echo 'Boot test passed.'
