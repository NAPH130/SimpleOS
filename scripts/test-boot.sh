#!/bin/sh
set -eu

# 内核会一直停机等待。QEMU 提前退出意味着自检未走到预期的停机路径。
scenario=${1:-boot}
build_dir=${2:-build}
case "$scenario" in
    boot)
        image=$build_dir/simpleos.iso
        log=$build_dir/boot-test.log
        ;;
    ud)
        image=$build_dir/tests/trap-ud.iso
        log=$build_dir/tests/trap-ud.log
        ;;
    *)
        echo "Unknown boot test: $scenario" >&2
        exit 2
        ;;
esac

status=0
timeout 15s qemu-system-x86_64 -cdrom "$image" -boot d -m 128M \
    -display none -serial file:"$log" -monitor none -no-reboot \
    || status=$?
if [ "$status" -ne 124 ]; then
    echo "QEMU exited before the timeout (exit $status):" >&2
    if [ -f "$log" ]; then cat "$log" >&2; fi
    exit 1
fi

if [ "$scenario" = ud ]; then
    rip=$(sed -n '1s/.*before ud2 at \(0x[0-9a-f][0-9a-f]*\).*/\1/p' "$log")
    expected_logs=$(printf '[\033[34mDEBUG\033[0m] before ud2 at %s\r\n[\033[31mERROR\033[0m] #UD Invalid opcode (vector 6), RIP=%s. Halting CPU.\r\n' \
        "$rip" "$rip")
    if [ -z "$rip" ] || [ "$(cat "$log")" != "$expected_logs" ]; then
        echo 'Expected #UD at the ud2 instruction address followed by a halt:' >&2
        cat "$log" >&2
        exit 1
    fi
    cat "$log"
    echo '#UD test passed.'
    exit 0
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
