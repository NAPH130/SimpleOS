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
    ud|gp|pf|de|ss|df|ring3)
        image=$build_dir/tests/trap-$scenario.iso
        log=$build_dir/tests/trap-$scenario.log
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

if [ "$scenario" != boot ]; then
    rip=unknown
    case "$scenario" in
        ud)
            exception='#UD'
            rip=$(sed -n '1s/.*before ud2 at \(0x[0-9a-f][0-9a-f]*\).*/\1/p' "$log")
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before ud2 at %s\r\n[\033[31mERROR\033[0m] #UD Invalid opcode (vector 6), RIP=%s. Halting CPU.\r\n' \
                "$rip" "$rip")
            ;;
        gp)
            exception='#GP'
            fields=$(sed -n '1s/.*before mov ss at \(0x[0-9a-f][0-9a-f]*\), expected error=\(0x[1-9a-f][0-9a-f]*\).*/\1 \2/p' "$log")
            rip=${fields%% *}
            error=${fields#* }
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before mov ss at %s, expected error=%s\r\n[\033[31mERROR\033[0m] #GP General protection fault (vector 13), error=%s, RIP=%s. Halting CPU.\r\n' \
                "$rip" "$error" "$error" "$rip")
            ;;
        pf)
            exception='#PF'
            rip=$(sed -n '1s/.*before unmapped read at \(0x[0-9a-f][0-9a-f]*\), address=0x1000.*/\1/p' "$log")
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before unmapped read at %s, address=0x1000\r\n[\033[31mERROR\033[0m] #PF Page fault (vector 14), error=0x0, RIP=%s, CR2=0x1000. Halting CPU.\r\n' \
                "$rip" "$rip")
            ;;
        de)
            exception='#DE'
            rip=$(sed -n '2s/.*RIP=\(0x[0-9a-f][0-9a-f]*\).*/\1/p' "$log")
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before integer divide by zero\r\n[\033[31mERROR\033[0m] #DE Divide error (vector 0), error=0x0, RIP=%s. Halting CPU.\r\n' \
                "$rip")
            ;;
        ss)
            exception='#SS'
            error=$(sed -n '1s/.*expected error=\(0x[1-9a-f][0-9a-f]*\).*/\1/p' "$log")
            rip=$(sed -n '2s/.*RIP=\(0x[0-9a-f][0-9a-f]*\).*/\1/p' "$log")
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before absent stack segment, expected error=%s\r\n[\033[31mERROR\033[0m] #SS Stack segment fault (vector 12), error=%s, RIP=%s. Halting CPU.\r\n' \
                "$error" "$error" "$rip")
            ;;
        df)
            exception='#DF'
            expected_logs=$(printf '[\033[34mDEBUG\033[0m] before general protection fault with absent gate\r\n[\033[31mERROR\033[0m] #DF Double fault (vector 8), error=0x0, IST=yes. Halting CPU.\r\n')
            ;;
        ring3)
            exception='Ring 3 #UD'
            fields=$(sed -n '1s/.*before entering Ring 3, expected RIP=\(0x[0-9a-f][0-9a-f]*\), RSP=\(0x[0-9a-f][0-9a-f]*\).*/\1 \2/p' "$log")
            rip=${fields%% *}
            rsp=${fields#* }
            expected_logs=$(printf '[\033[%sm%s\033[0m] %s\r\n' \
                34 DEBUG "before entering Ring 3, expected RIP=$rip, RSP=$rsp" \
                34 DEBUG "Ring 3 trap: CS=0x2b, SS=0x33, RSP=$rsp, on RSP0 stack=yes." \
                31 ERROR "#UD Invalid opcode (vector 6), RIP=$rip. Halting CPU.")
            ;;
    esac
    if [ -z "$rip" ] || [ "$(cat "$log")" != "$expected_logs" ]; then
        echo "Expected $exception diagnostics matching the test input followed by a halt:" >&2
        cat "$log" >&2
        exit 1
    fi
    cat "$log"
    echo "$exception test passed."
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
