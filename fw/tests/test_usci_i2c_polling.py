"""Execute the current production USCI I2C sequencers with bounded host mocks.

Run with Python, or unittest discovery. No CRT, vcvars, hardware, or third-party
packages are needed. Only temporary C/object/executable artifacts are generated.
The event model is an explicit test assumption, not validation of silicon timing.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "fw/Library/StdDriver/src/usci_i2c.c"


def production_code():
    source = DRIVER.read_text(encoding="utf-8-sig")
    start = source.index("/* Poll only W1C protocol events")
    boundary = re.search(r"/\*\*\s*\*\s*@brief\s+Write a byte", source[start:])
    if boundary is None:
        raise AssertionError("Cannot locate private-helper / WriteByte boundary")
    split = start + boundary.start()
    helpers, wrappers = source[start:split], source[split:]
    if "static uint32_t UI2C_XferEngine(" not in helpers:
        raise AssertionError("Expected actual private transaction engine")
    if "uint8_t UI2C_WriteByte(" not in wrappers:
        raise AssertionError("Expected actual UI2C_WriteByte")
    return helpers + wrappers


def toolchain():
    if os.name != "nt":
        raise unittest.SkipTest("This CRT-free executable harness requires Windows/MSVC")
    pf = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
    preferred = pf / (
        "Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/"
        "14.44.35207/bin/Hostx64/x64/cl.exe"
    )
    candidates = [preferred]
    on_path = shutil.which("cl.exe")
    if on_path:
        candidates.append(Path(on_path))
    candidates += sorted(
        (pf / "Microsoft Visual Studio").glob(
            "*/*/VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe"
        ), reverse=True
    )
    compiler = next((p for p in candidates if p.is_file()
                     and p.with_name("link.exe").is_file()), None)
    libraries = sorted((pf / "Windows Kits/10/Lib").glob(
        "*/um/x64/kernel32.lib"), reverse=True)
    if compiler is None or not libraries:
        raise unittest.SkipTest(
            "Install MSVC x64 Build Tools and Windows 10/11 SDK "
            "(cl.exe, adjacent link.exe, and um/x64/kernel32.lib required)"
        )
    return compiler, compiler.with_name("link.exe"), libraries[0]


MOCK = r"""
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
#define NULL 0
#define UI2C_TIMEOUT 8U
#define UI2C_ERR_FAIL (-1)
#define UI2C_ERR_TIMEOUT (-2)
typedef struct { volatile uint32_t PROTCTL, TXDAT, RXDAT; } UI2C_T;
static UI2C_T hardware, other;
#define UI2C0 (&hardware)
enum UI2C_MASTER_EVENT {
    MASTER_SEND_ADDRESS = 10, MASTER_SEND_H_WR_ADDRESS,
    MASTER_SEND_H_RD_ADDRESS, MASTER_SEND_L_ADDRESS, MASTER_SEND_DATA,
    MASTER_SEND_REPEAT_START, MASTER_READ_DATA, MASTER_STOP, MASTER_SEND_START
};
#define UI2C_CTL_PTRG 0x20U
#define UI2C_CTL_STA 0x08U
#define UI2C_CTL_STO 0x04U
#define UI2C_CTL_AA 0x02U
#define UI2C_PROTCTL_PTRG_Msk UI2C_CTL_PTRG
#define UI2C_PROTCTL_STA_Msk UI2C_CTL_STA
#define UI2C_PROTCTL_STO_Msk UI2C_CTL_STO
#define UI2C_PROTCTL_AA_Msk UI2C_CTL_AA
#define UI2C_PROTSTS_TOIF_Msk (1U << 5)
#define UI2C_PROTSTS_STARIF_Msk (1U << 8)
#define UI2C_PROTSTS_STORIF_Msk (1U << 9)
#define UI2C_PROTSTS_NACKIF_Msk (1U << 10)
#define UI2C_PROTSTS_ARBLOIF_Msk (1U << 11)
#define UI2C_PROTSTS_ERRIF_Msk (1U << 12)
#define UI2C_PROTSTS_ACKIF_Msk (1U << 13)
#define S UI2C_PROTSTS_STARIF_Msk
#define A UI2C_PROTSTS_ACKIF_Msk
#define N UI2C_PROTSTS_NACKIF_Msk
#define L UI2C_PROTSTS_ARBLOIF_Msk
#define E UI2C_PROTSTS_ERRIF_Msk
#define T UI2C_PROTSTS_TOIF_Msk
#define O UI2C_PROTSTS_STORIF_Msk
#define CONFIG 0x5A510001U
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int32_t g_UI2C_i32ErrCode;
typedef struct { uint32_t status, data; int aa; } Event;
static Event queue[64];
static uint32_t qlen, pos, flags, held, active, ready;
static uint32_t starts, stops, controls, clears, polls, gets, violations;
static uint32_t last_control, stuck, stop_delay, stopping, background;
static uint32_t inject_poll, inject_flags;
static uint32_t tx[64], ntx, ctls[64], nctl;

static void reset(void) {
    uint32_t i;
    hardware.PROTCTL = CONFIG; hardware.TXDAT = hardware.RXDAT = 0;
    other.PROTCTL = CONFIG;
    qlen = pos = flags = held = active = 0; ready = 1;
    starts = stops = controls = clears = polls = gets = violations = 0;
    last_control = stuck = stop_delay = stopping = background = ntx = nctl = 0;
    inject_poll = inject_flags = 0;
    g_UI2C_i32ErrCode = 123;
    for (i = 0; i < 64; ++i) { tx[i] = ctls[i] = 0; }
}
static void push(uint32_t status, uint32_t data, int aa) {
    queue[qlen].status = status; queue[qlen].data = data;
    queue[qlen++].aa = aa;
}
static void latch(void) {
    if (pos < qlen) {
        flags = queue[pos].status;
        hardware.RXDAT = queue[pos].data;
        held = 1;
    } else { flags = held = 0; }
}
static void complete_stop(void) {
    hardware.PROTCTL &= ~UI2C_CTL_STO;
    stopping = 0; flags |= O;
}
static uint32_t mock_status(UI2C_T *p) {
    ++polls;
    if (p != UI2C0) ++violations;
    if (stopping && !stuck) complete_stop();
    if (inject_poll && polls == inject_poll) flags |= inject_flags;
    /* Reading never consumes a snapshot or changes the event FIFO. */
    return flags | background;
}
static void mock_clear(UI2C_T *p, uint32_t mask) {
    ++clears;
    if (p != UI2C0) ++violations;
    flags &= ~mask; /* W1C: only the written ones are cleared. */
}
static void mock_control(UI2C_T *p, uint32_t value) {
    uint32_t old = p->PROTCTL;
    ++controls; last_control = value;
    if (nctl < 64) ctls[nctl++] = value;
    p->PROTCTL = (old & ~0x2EU) | value;
    if ((p->PROTCTL & ~0x2EU) != CONFIG) ++violations;
    if (value & UI2C_CTL_STO) {
        ++stops; active = held = 0; ready = 0; stopping = 1;
        if (!stuck && !stop_delay) complete_stop();
        return;
    }
    if (value & UI2C_CTL_PTRG) {
        if (flags != 0) ++violations; /* W1C must precede handshake. */
        if (active && held) {
            if (queue[pos].aa >= 0 &&
                !!(value & UI2C_CTL_AA) != queue[pos].aa) ++violations;
            if (value & UI2C_CTL_STA) ++starts;
            ++pos; latch();
        } else if (!active) ready = 1;
    }
}
static void mock_start(UI2C_T *p) {
    if (p != UI2C0 || !ready || flags || (p->PROTCTL & UI2C_CTL_STO))
        ++violations;
    p->PROTCTL = (p->PROTCTL & ~UI2C_CTL_PTRG) | UI2C_CTL_STA;
    ++starts; ready = 0; active = 1; pos = 0; latch();
}
static void mock_data(UI2C_T *p, uint32_t data) {
    p->TXDAT = data;
    if (ntx < 64) tx[ntx++] = data; else ++violations;
}
static uint32_t mock_get(UI2C_T *p) { ++gets; return p->RXDAT; }
#define UI2C_GET_PROT_STATUS(p) mock_status(p)
#define UI2C_CLR_PROT_INT_FLAG(p, m) mock_clear(p, m)
#define UI2C_SET_CONTROL_REG(p, c) mock_control(p, c)
#define UI2C_START(p) mock_start(p)
#define UI2C_STOP(p) mock_control(p, UI2C_CTL_PTRG | UI2C_CTL_STO)
#define UI2C_SET_DATA(p, d) mock_data(p, d)
#define UI2C_GET_DATA(p) mock_get(p)
"""


SUPPORT = r"""
/* Test alias only: the implementation above is unmodified UI2C_WriteByte. */
#define verifiedWriteByte UI2C_WriteByte
static uint8_t payload[3] = {0x91, 0x27, 0xE3};
static uint32_t write_call(uint32_t p, UI2C_T *dev, uint8_t addr,
                           uint8_t *data, uint32_t n) {
    if (p == 0) return UI2C_WriteMultiBytes(dev, addr, data, n);
    if (p == 1) return UI2C_WriteMultiBytesOneReg(dev, addr, 0xCD, data, n);
    return UI2C_WriteMultiBytesTwoRegs(dev, addr, 0xABCD, data, n);
}
static uint32_t read_call(uint32_t p, UI2C_T *dev, uint8_t addr,
                          uint8_t *data, uint32_t n) {
    if (p == 0) return UI2C_ReadMultiBytes(dev, addr, data, n);
    if (p == 1) return UI2C_ReadMultiBytesOneReg(dev, addr, 0xCD, data, n);
    return UI2C_ReadMultiBytesTwoRegs(dev, addr, 0xABCD, data, n);
}
static void write_queue(uint32_t p, uint32_t n) {
    uint32_t i;
    push(S, 0, 0); push(A, 0, 0);
    for (i = 0; i < p; ++i) push(A, 0, 0);
    for (i = 0; i < n; ++i) push(A, 0, -1);
}
static void read_queue(uint32_t p, uint32_t n) {
    uint32_t i;
    push(S, 0, 0);
    if (p) {
        push(A, 0, 0);
        for (i = 0; i < p; ++i) push(A, 0, 0);
        push(S, 0, 0);
    }
    /* Address ACK must set AA=0 for len1, AA=1 otherwise. */
    push(A, 0, n > 1);
    for (i = 0; i < n; ++i)
        push(i + 1 == n ? N : A, 0x61 + i,
             i + 1 == n ? -1 : (n - i - 1 > 1));
}
static int finished(uint32_t expected_starts, uint32_t expected_stops) {
    CHECK(starts == expected_starts);
    CHECK(stops == expected_stops);
    CHECK(flags == 0);
    CHECK(last_control == UI2C_CTL_PTRG);
    CHECK((hardware.PROTCTL & UI2C_CTL_STO) == 0);
    CHECK((hardware.PROTCTL & ~0x2EU) == CONFIG);
    CHECK(violations == 0);
    CHECK(polls < 80);
    return 0;
}
static int prefix_tx(uint32_t p, uint32_t read, uint32_t n) {
    uint32_t i, offset = 1;
    CHECK(tx[0] == (read && !p ? 0xA5 : 0xA4));
    if (p == 2) { CHECK(tx[offset] == 0xAB); ++offset; }
    if (p) { CHECK(tx[offset] == 0xCD); ++offset; }
    if (read) {
        if (p) { CHECK(tx[offset] == 0xA5); ++offset; }
    } else {
        for (i = 0; i < n; ++i) { CHECK(tx[offset] == payload[i]); ++offset; }
    }
    CHECK(ntx == offset);
    return 0;
}
#define CALL_CHECK(expr) do { int failure = (expr); if (failure) return failure; } while (0)
"""


def cases():
    result = []

    def add(name, code):
        result.append((name, code))

    add("mock/snapshot-W1C-PTRG", """
        reset(); push(S | A, 0, -1); push(N, 0, -1);
        UI2C_START(UI2C0);
        CHECK(mock_status(UI2C0) == (S | A));
        CHECK(mock_status(UI2C0) == (S | A)); CHECK(pos == 0);
        mock_clear(UI2C0, S); CHECK(mock_status(UI2C0) == A);
        mock_clear(UI2C0, A); CHECK(mock_status(UI2C0) == 0);
        CHECK(pos == 0); mock_control(UI2C0, UI2C_CTL_PTRG);
        CHECK(pos == 1); CHECK(mock_status(UI2C0) == N);
        CHECK(violations == 0);
    """)
    for p in range(3):
        for n in range(1, 4):
            add(f"write/prefix{p}/len{n}", f"""
                reset(); stop_delay = 1; write_queue({p}, {n});
                CHECK(write_call({p}, UI2C0, 0x52, payload, {n}) == {n});
                CHECK(g_UI2C_i32ErrCode == 0); CHECK(gets == 0);
                CALL_CHECK(prefix_tx({p}, 0, {n}));
                CALL_CHECK(finished(1, 1));
            """)
            add(f"read/prefix{p}/len{n}/canaries-AA", f"""
                uint8_t guarded[5] = {{0xD1, 0xCC, 0xCC, 0xCC, 0xD2}};
                uint32_t i;
                reset(); read_queue({p}, {n});
                CHECK(read_call({p}, UI2C0, 0x52, guarded + 1, {n}) == {n});
                CHECK(g_UI2C_i32ErrCode == 0); CHECK(gets == {n});
                CHECK(guarded[0] == 0xD1); CHECK(guarded[4] == 0xD2);
                for (i = 0; i < {n}; ++i) CHECK(guarded[i+1] == 0x61+i);
                for (i = {n}+1; i < 4; ++i) CHECK(guarded[i] == 0xCC);
                CALL_CHECK(prefix_tx({p}, 1, 0));
                CALL_CHECK(finished({2 if p else 1}, 1));
            """)
        # Inject NACK at write address, each prefix byte, and each payload byte.
        for phase in range(1, p + 5):
            acknowledged = max(0, phase - p - 2)
            queued = max(0, phase - p - 1)
            add(f"write/prefix{p}/NACK-event{phase}", f"""
                reset(); write_queue({p}, 3);
                queue[{phase}].status = N; qlen = {phase}+1;
                CHECK(write_call({p}, UI2C0, 0x52, payload, 3) == {acknowledged});
                CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL);
                CALL_CHECK(prefix_tx({min(p, phase - 1)}, 0, {queued}));
                CALL_CHECK(finished(1, 1));
            """.replace(
                # Partial two-byte prefix is AB, not the one-prefix CD.
                f"CALL_CHECK(prefix_tx({min(p, phase - 1)}, 0, {queued}));",
                (f"CHECK(ntx == {phase}); CHECK(tx[0] == 0xA4);"
                 + (" CHECK(tx[1] == 0xAB);" if p == 2 and phase > 1 else "")
                 + (f" CHECK(tx[{p}] == 0xCD);" if p and phase > p else "")
                 + "".join(f" CHECK(tx[{p+1+i}] == payload[{i}]);"
                           for i in range(queued)))
            ))
        for read in (False, True):
            call = "read_call" if read else "write_call"
            for label, dev, addr, data, length, error in (
                ("zero", "UI2C0", "0x52", "payload", 0, "0"),
                ("zero-null-data", "UI2C0", "0x52", "NULL", 0, "0"),
                ("null-data", "UI2C0", "0x52", "NULL", 1, "UI2C_ERR_FAIL"),
                ("null-device", "NULL", "0x52", "payload", 1, "UI2C_ERR_FAIL"),
                ("foreign-device", "&other", "0x52", "payload", 1, "UI2C_ERR_FAIL"),
                ("invalid-address", "UI2C0", "0x80", "payload", 1, "UI2C_ERR_FAIL"),
            ):
                add(f"{'read' if read else 'write'}/prefix{p}/{label}", f"""
                    reset(); flags = A;
                    CHECK({call}({p}, {dev}, {addr}, {data}, {length}) == 0);
                    CHECK(g_UI2C_i32ErrCode == {error});
                    CHECK(starts == 0 && stops == 0 && controls == 0);
                    CHECK(clears == 0 && polls == 0 && ntx == 0 && gets == 0);
                    CHECK(flags == A && hardware.PROTCTL == CONFIG);
                """)

    for p in range(3):
        for phase in range(1, (p + 4 if p else 2)):
            add(f"read/prefix{p}/setup-NACK-event{phase}", f"""
                uint8_t guard[3] = {{0xD1, 0xCC, 0xD2}};
                reset(); read_queue({p}, 1);
                queue[{phase}].status = N; qlen = {phase}+1;
                CHECK(read_call({p}, UI2C0, 0x52, guard+1, 1) == 0);
                CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL && gets == 0);
                CHECK(guard[0] == 0xD1 && guard[1] == 0xCC && guard[2] == 0xD2);
                CALL_CHECK(finished({2 if p and phase > p + 1 else 1}, 1));
            """)
    for label, event, error, stop in (
        ("arbitration", "L", "UI2C_ERR_FAIL", 0),
        ("arbitration-ACK", "L|A", "UI2C_ERR_FAIL", 0),
        ("arbitration-timeout-ACK", "L|T|A", "UI2C_ERR_FAIL", 0),
        ("hardware-timeout", "T", "UI2C_ERR_TIMEOUT", 1),
        ("hardware-timeout-ACK", "T|A", "UI2C_ERR_TIMEOUT", 1),
        ("bus-error", "E", "UI2C_ERR_FAIL", 1),
        ("bus-error-ACK", "E|A", "UI2C_ERR_FAIL", 1),
        ("ACK-NACK", "A|N", "UI2C_ERR_FAIL", 1),
        ("START-ACK", "S|A", "UI2C_ERR_FAIL", 1),
        ("unexpected-STOP", "O", "UI2C_ERR_FAIL", 1),
        ("unexpected-START", "S", "UI2C_ERR_FAIL", 1),
    ):
        for legacy in (False, True):
            call = "verifiedWriteByte(UI2C0, 0x52, payload[0])" if legacy else (
                "write_call(0, UI2C0, 0x52, payload, 1)")
            expected = 1 if legacy else 0
            add(f"{'WriteByte' if legacy else 'engine'}/{label}", f"""
                reset(); write_queue(0, 1); queue[2].status = {event};
                CHECK({call} == {expected});
                CHECK(g_UI2C_i32ErrCode == {error});
                CALL_CHECK(finished(1, {0 if legacy and event == 'O' else stop}));
            """)
    for legacy in (False, True):
        call = "verifiedWriteByte(UI2C0, 0x52, payload[0])" if legacy else (
            "write_call(0, UI2C0, 0x52, payload, 1)")
        label = "WriteByte" if legacy else "engine"
        add(f"{label}/success", f"""
            reset(); write_queue(0, 1); CHECK({call} == {0 if legacy else 1});
            CHECK(g_UI2C_i32ErrCode == 0);
            CALL_CHECK(prefix_tx(0, 0, 1)); CALL_CHECK(finished(1, 1));
        """)
        for phase in (1, 2):
            add(f"{label}/NACK{phase}-then-valid", f"""
                reset(); write_queue(0, 1); queue[{phase}].status = N;
                qlen = {phase}+1;
                CHECK({call} == {1 if legacy else 0});
                CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL);
                CALL_CHECK(finished(1, 1));
                qlen = 0; write_queue(0, 1);
                CHECK({call} == {0 if legacy else 1});
                CHECK(g_UI2C_i32ErrCode == 0); CALL_CHECK(finished(2, 2));
            """)
        for after_start in (False, True):
            add(f"{label}/software-timeout-{'after' if after_start else 'before'}-START-event", f"""
                reset(); {'push(S, 0, 0);' if after_start else ''}
                CHECK({call} == {1 if legacy else 0});
                CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT);
                CHECK(ntx == {1 if after_start else 0});
                CALL_CHECK(finished(1, 1));
            """)
        add(f"{label}/pending-STOP-before-START", f"""
            reset(); hardware.PROTCTL |= UI2C_CTL_STO;
            CHECK({call} == {1 if legacy else 0});
            CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT);
            CHECK(hardware.PROTCTL == (CONFIG | UI2C_CTL_STO));
            CHECK(starts == 0 && stops == 0 && controls == 0 && clears == 0);
        """)
        add(f"{label}/stuck-STOP-after-success", f"""
            reset(); stuck = 1; write_queue(0, 1);
            CHECK({call} == 1); CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT);
            CHECK(stops == 1 && starts == 1);
            CHECK(last_control == (UI2C_CTL_PTRG | UI2C_CTL_STO));
            CHECK((hardware.PROTCTL & UI2C_CTL_STO) != 0);
            CHECK((hardware.PROTCTL & ~0x2EU) == CONFIG && violations == 0);
            CHECK({call} == {1 if legacy else 0});
            CHECK(starts == 1 && stops == 1);
        """)
    for p in (1, 2):
        func = "UI2C_WriteByteOneReg" if p == 1 else "UI2C_WriteByteTwoRegs"
        args = "0xCD" if p == 1 else "0xABCD"
        for failed in (False, True):
            add(f"{func}/{'NACK' if failed else 'success'}", f"""
                reset(); write_queue({p}, 1);
                {'queue[qlen-1].status = N;' if failed else ''}
                CHECK({func}(UI2C0, 0x52, {args}, payload[0]) == {int(failed)});
                CHECK(g_UI2C_i32ErrCode == {'UI2C_ERR_FAIL' if failed else '0'});
                CALL_CHECK(finished(1, 1));
            """)
    for p, func, extra in ((0, "UI2C_ReadByte", ""),
                           (1, "UI2C_ReadByteOneReg", ", 0xCD"),
                           (2, "UI2C_ReadByteTwoRegs", ", 0xABCD")):
        for failed in (False, True):
            add(f"{func}/{'address-NACK' if failed else 'success'}", f"""
                reset(); read_queue({p}, 1);
                {'queue[qlen-2].status = N; qlen -= 1;' if failed else ''}
                CHECK({func}(UI2C0, 0x52{extra}) == {0 if failed else '0x61'});
                CHECK(g_UI2C_i32ErrCode == {'UI2C_ERR_FAIL' if failed else '0'});
                CALL_CHECK(finished({2 if p else 1}, 1));
            """)
    for label, event in (("early-NACK", "N"), ("combined-ACK-NACK", "A|N"),
                         ("arbitration-ACK", "L|A"), ("hardware-timeout", "T")):
        add(f"read/{label}/canary", f"""
            uint8_t guard[5] = {{0xD1, 0xCC, 0xCC, 0xCC, 0xD2}};
            reset(); read_queue(0, 3); queue[3].status = {event}; qlen = 4;
            CHECK(read_call(0, UI2C0, 0x52, guard+1, 3) == 1);
            CHECK(g_UI2C_i32ErrCode == {'UI2C_ERR_TIMEOUT' if event == 'T' else 'UI2C_ERR_FAIL'});
            CHECK(gets == 1 && guard[1] == 0x61);
            CHECK(guard[0] == 0xD1 && guard[2] == 0xCC && guard[3] == 0xCC && guard[4] == 0xD2);
            CALL_CHECK(finished(1, {0 if 'L' in event else 1}));
        """)
    add("read/final-ACK-is-not-NACK/len1", """
        uint8_t guard[3] = {0xD1, 0xCC, 0xD2};
        reset(); read_queue(0, 1); queue[2].status = A;
        CHECK(read_call(0, UI2C0, 0x52, guard+1, 1) == 0);
        CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL && gets == 0);
        CHECK(guard[0] == 0xD1 && guard[1] == 0xCC && guard[2] == 0xD2);
        CALL_CHECK(finished(1, 1));
    """)
    add("engine/non-event-status-does-not-progress", """
        reset(); background = 0x10;
        CHECK(write_call(0, UI2C0, 0x52, payload, 1) == 0);
        CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT && ntx == 0);
        CALL_CHECK(finished(1, 1));
    """)
    add("engine/arbitration-at-software-timeout-boundary/no-STOP", """
        reset(); inject_poll = UI2C_TIMEOUT + 1; inject_flags = L | A;
        CHECK(write_call(0, UI2C0, 0x52, payload, 1) == 0);
        CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL);
        CALL_CHECK(finished(1, 0));
    """)
    add("engine/TOIF-during-STOP/full-count-but-error", """
        reset(); write_queue(0, 1); stop_delay = 1;
        inject_poll = 5; inject_flags = T;
        CHECK(write_call(0, UI2C0, 0x52, payload, 1) == 1);
        CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT);
        CALL_CHECK(finished(1, 1));
    """)
    for p in range(3):
        for label, event in (("software-timeout", "0"), ("TOIF", "T"),
                             ("ARBLO-ACK", "L|A"), ("ERR-ACK", "E|A")):
            add(f"write/prefix{p}/{label}/partial-ACK-count", f"""
                reset(); write_queue({p}, 3);
                queue[{p+3}].status = {event}; qlen = {p+4};
                CHECK(write_call({p}, UI2C0, 0x52, payload, 3) == 1);
                CHECK(g_UI2C_i32ErrCode == {'UI2C_ERR_TIMEOUT' if event in ('0', 'T') else 'UI2C_ERR_FAIL'});
                CALL_CHECK(prefix_tx({p}, 0, 2));
                CALL_CHECK(finished(1, {0 if event == 'L|A' else 1}));
            """)
        add(f"read/prefix{p}/software-timeout/partial-RX-count", f"""
            uint8_t guard[5] = {{0xD1, 0xCC, 0xCC, 0xCC, 0xD2}};
            reset(); read_queue({p}, 3); qlen -= 2;
            CHECK(read_call({p}, UI2C0, 0x52, guard+1, 3) == 1);
            CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT && gets == 1);
            CHECK(guard[1] == 0x61 && guard[0] == 0xD1 && guard[4] == 0xD2);
            CHECK(guard[2] == 0xCC && guard[3] == 0xCC);
            CALL_CHECK(finished({2 if p else 1}, 1));
        """)
        add(f"read/prefix{p}/stuck-STOP/canary-full-count", f"""
            uint8_t guard[3] = {{0xD1, 0xCC, 0xD2}};
            reset(); stuck = 1; read_queue({p}, 1);
            CHECK(read_call({p}, UI2C0, 0x52, guard+1, 1) == 1);
            CHECK(g_UI2C_i32ErrCode == UI2C_ERR_TIMEOUT && gets == 1);
            CHECK(guard[0] == 0xD1 && guard[1] == 0x61 && guard[2] == 0xD2);
            CHECK(stops == 1 && (hardware.PROTCTL & UI2C_CTL_STO));
            CHECK(last_control == (UI2C_CTL_PTRG | UI2C_CTL_STO));
            CHECK(violations == 0);
        """)
    # These contractual probes intentionally expose missing legacy validation.
    for label, dev, addr in (("invalid-address", "UI2C0", "0x80"),
                             ("foreign-device", "&other", "0x52"),
                             ("null-device", "NULL", "0x52")):
        add(f"WriteByte/{label}/no-transfer", f"""
            reset(); write_queue(0, 1);
            CHECK(verifiedWriteByte({dev}, {addr}, payload[0]) == 1);
            CHECK(g_UI2C_i32ErrCode == UI2C_ERR_FAIL);
            CHECK(starts == 0 && stops == 0 && controls == 0 && clears == 0);
        """)
    return result


def harness(test_cases):
    dispatch = []
    for number, (name, code) in enumerate(test_cases):
        dispatch.append(f"case {number}: {{ /* {name} */\n{code}\nreturn 0; }}")
    return MOCK + production_code() + SUPPORT + r"""
extern __declspec(dllimport) void __stdcall ExitProcess(unsigned long);
extern __declspec(dllimport) char * __stdcall GetCommandLineA(void);
extern __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int);
static int run_tests(uint32_t test) {
    switch (test) {
""" + "\n".join(dispatch) + r"""
    default: return 99999;
    }
}
void main(void) {
    char *p = GetCommandLineA();
    uint32_t test = 0;
    /* Fail null-pointer probes immediately, without a modal Windows/WER dialog. */
    SetErrorMode(3U);
    /* Python passes a decimal final argument; no CRT argument parsing needed. */
    while (*p) ++p;
    while (p[-1] >= '0' && p[-1] <= '9') --p;
    while (*p >= '0' && *p <= '9') { test = test * 10 + (*p - '0'); ++p; }
    ExitProcess((unsigned long)run_tests(test));
}
"""


class TestUSCIPolling(unittest.TestCase):
    def test_production_executables(self):
        compiler, linker, kernel = toolchain()
        test_cases = cases()
        source = harness(test_cases)
        lines = source.splitlines()
        passed, failed = 0, []
        print(f"Compiler: {compiler}")
        print(f"Production source: {DRIVER}")
        with tempfile.TemporaryDirectory(prefix="usci_i2c_polling_") as directory:
            temp = Path(directory)
            cfile, obj, exe = temp / "mock.c", temp / "mock.obj", temp / "mock.exe"
            cfile.write_text(source, encoding="utf-8")
            commands = (
                [str(compiler), "/nologo", "/TC", "/GS-", "/Zl", "/Od", "/c",
                 str(cfile), "/Fo" + str(obj)],
                [str(linker), "/nologo", "/nodefaultlib", "/entry:main",
                 "/subsystem:console", "/machine:x64", "/out:" + str(exe),
                 str(obj), str(kernel)],
            )
            for command in commands:
                build = subprocess.run(command, cwd=temp, capture_output=True,
                                       text=True, timeout=120)
                if build.stdout.strip():
                    print(build.stdout.strip())
                if build.stderr.strip():
                    print(build.stderr.strip())
                self.assertEqual(build.returncode, 0, "Host harness compilation/link failed")
            for number, (name, _) in enumerate(test_cases):
                with self.subTest(case=name):
                    try:
                        run = subprocess.run([str(exe), str(number)], cwd=temp,
                                             capture_output=True, timeout=5)
                    except subprocess.TimeoutExpired:
                        failed.append(name)
                        print(f"FAIL {name}: executable exceeded 5 seconds")
                        self.fail("Unbounded host transaction")
                    if run.returncode == 0:
                        passed += 1
                        print(f"PASS {name}")
                    else:
                        failed.append(name)
                        code = run.returncode & 0xFFFFFFFF
                        detail = (f"C assertion line {code}: {lines[code-1].strip()}"
                                  if 0 < code <= len(lines)
                                  else f"Windows process exit 0x{code:08X}")
                        print(f"FAIL {name}: {detail}")
                        self.fail(detail)
        print(f"Host mock cases: {len(test_cases)}; passed: {passed}; failed: {len(failed)}")
        print("Mock results only; hardware semantics/timing are not proven.")


if __name__ == "__main__":
    unittest.main(verbosity=2)