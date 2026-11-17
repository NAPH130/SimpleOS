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
if ! grep -Fq '[SimpleOS] Hello, kernel!' "$log"; then
    echo 'Kernel greeting was not found in serial output:' >&2
    cat "$log" >&2
    exit 1
fi
cat "$log"
echo 'Boot test passed.'
