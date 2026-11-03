#!/usr/bin/env python3
"""Execute the actual CommandID 1 cache branch with Windows/MSVC, without CRT.

Run directly or through unittest discovery; no USB or third-party packages.
Only temporary C/object/executable artifacts are generated. sprintf is stubbed
to an empty string: failures test status/mapping, NOT error text or its bounds.
"""

from pathlib import Path
import random
import re
import subprocess
import tempfile
import unittest

from test_usci_i2c_polling import toolchain


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "fw/SampleCode/StdDriver/HSUSBD_svf/main.c"
DEFINITIONS = MAIN.with_name("g_def.h")
HOST = ROOT / "app/M460/M460.c"


def without_literals(source):
    """Blank comments/strings while retaining offsets for balanced C braces."""
    return re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                  lambda match: " " * len(match.group()), source, flags=re.S)


def production_code():
    source = MAIN.read_text(encoding="utf-8-sig")
    lexical = without_literals(source)
    matches = list(re.finditer(r'\bif\s*\(\s*cmd_id\s*==\s*1\s*\)\s*\{', lexical))
    if len(matches) != 1:
        raise AssertionError("Expected exactly one actual cmd_id == 1 branch")
    start = matches[0].start()
    opening = matches[0].end() - 1
    depth = 1
    end = opening + 1
    while end < len(lexical) and depth:
        depth += (lexical[end] == "{") - (lexical[end] == "}")
        end += 1
    if depth or not re.match(r'\s*else\s+if\s*\(\s*cmd_id\s*==\s*4\b', lexical[end:]):
        raise AssertionError("Unbalanced CommandID 1 or missing next else if cmd_id == 4")
    return source[start:end]


def constants():
    source = DEFINITIONS.read_text(encoding="utf-8-sig")
    values = {}
    for name in ("cpld_hdd_amount", "NVME_MEM_OFFSET", "NVME_READ_COUNT"):
        match = re.search(r'^\s*#define\s+' + name + r'\s+(0x[0-9a-fA-F]+|[0-9]+)\b',
                          source, re.M)
        if match is None:
            raise AssertionError("Missing literal production constant: " + name)
        values[name] = int(match[1], 0)
    if values != {"cpld_hdd_amount": 48, "NVME_MEM_OFFSET": 256, "NVME_READ_COUNT": 32}:
        raise AssertionError("Cache layout changed; review regression scenarios")
    return "\n".join(f"#define {name} {value}U" for name, value in values.items())


# Explicit independent expectations: (name, count0, count1, present, wire, bank, local).
# bank=-1 means unavailable. CLI conversion is checked separately in host source.
TARGETED = (
    ("CLI28/14+14/bank1-local13", 14, 14, 1, 27, 1, 13),
    ("wire28/14+14/unavailable", 14, 14, 1, 28, -1, 0),
    ("CLI28/24+5/bank1-local3", 24, 5, 1, 27, 1, 3),
    ("wire28/24+5/bank1-local4/29slots", 24, 5, 1, 28, 1, 4),
    ("bank0-lastslot23/base992/end1023", 24, 24, 1, 23, 0, 23),
    ("bank1-lastslot23/base992/end1023", 24, 24, 1, 47, 1, 23),
    ("bank0-zero/bank1-first", 0, 24, 1, 0, 1, 0),
    ("bank0-zero/bank1-last", 0, 24, 1, 23, 1, 23),
    ("absent-bank1/bank0-usable", 24, 24, 0, 23, 0, 23),
    ("absent-bank1/reject-selected-bank1", 24, 24, 0, 24, -1, 0),
    ("absent-bank1/bank0-zero", 0, 24, 0, 0, -1, 0),
    ("invalid-bank1/bank0-usable", 24, 255, 1, 23, 0, 23),
)


SUPPORT = r"""
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#define NULL 0
#define CHECK(expr) do { if (!(expr)) return __LINE__; } while (0)
#define CALL(expr) do { int failure = (expr); if (failure) return failure; } while (0)
static volatile uint8_t bmc_report[1024], bmc_report1[1024], response_buff[1024];
static volatile uint32_t g_u32clpd_lost;
static uint8_t data_cnt, status;
static uint32_t i, scenarios, successes, failures;
static uint32_t current0, current1, current_present, current_index;
/* Status/mapping only: no claim about production sprintf error text safety. */
static int sprintf(char *destination, const char *format, ...) {
    (void)format; destination[0] = '\0'; return 0;
}
"""


RUNNER = r"""
extern __declspec(dllimport) void __stdcall ExitProcess(unsigned long);
extern __declspec(dllimport) char * __stdcall GetCommandLineA(void);
extern __declspec(dllimport) unsigned int __stdcall SetErrorMode(unsigned int);
extern __declspec(dllimport) void * __stdcall GetStdHandle(unsigned long);
extern __declspec(dllimport) int __stdcall WriteFile(void *, const void *,
                                                  unsigned long, unsigned long *, void *);
static void print(const char *text) {
    unsigned long length = 0, written;
    while (text[length]) ++length;
    WriteFile(GetStdHandle((unsigned long)-11), text, length, &written, NULL);
}
static void number(uint32_t value) {
    char text[11]; uint32_t n = 10;
    text[n] = 0;
    do { text[--n] = (char)('0' + value % 10); value /= 10; } while (value);
    print(text + n);
}
static void seed_caches(void) {
    uint32_t bank, slot, byte;
    for (byte = 0; byte < 1024; ++byte) {
        bmc_report[byte] = 0xD0; bmc_report1[byte] = 0xD1;
    }
    for (bank = 0; bank < 2; ++bank)
        for (slot = 0; slot < 24; ++slot)
            for (byte = 0; byte < 32; ++byte) {
                if (bank) bmc_report1[256 + slot * 32 + byte] = markers[bank][slot][byte];
                else bmc_report[256 + slot * 32 + byte] = markers[bank][slot][byte];
            }
}
static int verify(uint32_t c0, uint32_t c1, uint32_t present, uint32_t index,
                  int bank, uint32_t local) {
    uint32_t byte;
    current0 = c0; current1 = c1; current_present = present; current_index = index;
    bmc_report[cpld_hdd_amount] = (uint8_t)c0;
    bmc_report1[cpld_hdd_amount] = (uint8_t)c1;
    g_u32clpd_lost = present ? 0U : 1U;
    for (byte = 0; byte < 1024; ++byte) response_buff[byte] = 0xA5;
    data_cnt = 0; status = 1;
    command_one((uint8_t)index);
    CHECK(data_cnt == (bank < 0 ? 0 : 32));
    CHECK(status == (bank < 0 ? 1 : 0));
    for (byte = 0; byte < 1024; ++byte) {
        uint8_t expected = 0xA5;
        if (bank >= 0 && byte >= 4 && byte < 36)
            expected = markers[bank][local][byte - 4];
        CHECK(response_buff[byte] == expected);
    }
    ++scenarios;
    if (bank < 0) ++failures; else ++successes;
    return 0;
}
static int enumerate_pair(uint32_t c0, uint32_t c1, uint32_t present) {
    int banks[256]; uint32_t locals[256], index, length = 0, slot;
    /* Independent oracle: concatenate the valid per-bank slot inventory.
     * Invalid bank0 invalidates the split; invalid/absent bank1 adds no slots. */
    for (index = 0; index < 256; ++index) { banks[index] = -1; locals[index] = 0; }
    if (c0 <= 24) {
        for (slot = 0; slot < c0; ++slot) { banks[length] = 0; locals[length++] = slot; }
        if (present && c1 <= 24)
            for (slot = 0; slot < c1; ++slot) { banks[length] = 1; locals[length++] = slot; }
    }
    for (index = 0; index < 256; ++index)
        CALL(verify(c0, c1, present, index, banks[index], locals[index]));
    return 0;
}
static int run(uint32_t group) {
    uint32_t c0, c1, present, a, b;
    static const uint32_t invalid[] = {0xFF, 25, 255};
    seed_caches();
    if (group == 0) {
        for (c0 = 0; c0 <= 24; ++c0)
            for (c1 = 0; c1 <= 24; ++c1)
                for (present = 0; present <= 1; ++present)
                    CALL(enumerate_pair(c0, c1, present));
    } else if (group == 1 || group == 2) {
        /* 0xFF and 255 intentionally named separately as requested probes. */
        for (a = 0; a < 3; ++a)
            for (b = 0; b <= 24; ++b)
                for (present = 0; present <= 1; ++present)
                    CALL(enumerate_pair(group == 1 ? invalid[a] : b,
                                        group == 1 ? b : invalid[a], present));
    } else if (group == 3) {
        for (a = 0; a < 3; ++a)
            for (b = 0; b < 3; ++b)
                for (present = 0; present <= 1; ++present)
                    CALL(enumerate_pair(invalid[a], invalid[b], present));
    } else if (group == 4) {
        TARGETED_CALLS
    } else return 99999;
    return 0;
}
void main(void) {
    char *p = GetCommandLineA(); uint32_t group = 0; int result;
    SetErrorMode(3U);
    while (*p) ++p;
    while (p[-1] >= '0' && p[-1] <= '9') --p;
    while (*p >= '0' && *p <= '9') { group = group * 10 + (*p - '0'); ++p; }
    result = run(group);
    print("scenarios="); number(scenarios);
    print(" success="); number(successes); print(" unavailable="); number(failures);
    if (result) {
        print(" FAIL count0="); number(current0); print(" count1="); number(current1);
        print(" present="); number(current_present); print(" wire="); number(current_index);
        print(" assertion-line="); number((uint32_t)result);
    }
    print("\n"); ExitProcess((unsigned long)result);
}
"""


def harness():
    rng = random.Random(0x476CA5)
    banks = []
    for bank in range(2):
        slots = []
        for slot in range(24):
            marker = [rng.randrange(256) for _ in range(32)]
            marker[:3] = [0xB0 + bank, slot, 32]
            slots.append("{" + ",".join(map(str, marker)) + "}")
        banks.append("{" + ",\n".join(slots) + "}")
    markers = "\nstatic const uint8_t markers[2][24][32] = {" + ",\n".join(banks) + "};\n"
    calls = "\n".join("CALL(verify(" + ", ".join(map(str, case[1:])) + "));"
                      for case in TARGETED)
    return (SUPPORT + constants() + markers
            + "\nstatic void command_one(uint8_t nvme_index) {\nuint8_t cmd_id = 1;\n"
            + production_code() + "\n}\n" + RUNNER.replace("TARGETED_CALLS", calls))


class TestNvmeSlotCache(unittest.TestCase):
    def test_host_cli_28_is_wire_27(self):
        source = without_literals(HOST.read_text(encoding="utf-8-sig"))
        self.assertIsNotNone(re.search(r'\bint\s+nvme_index\s*=\s*atoi\(argv\[2\]\)', source),
                             "Host must parse the CLI slot from argv[2]")
        self.assertIsNotNone(re.search(
            r'usbd_multi_nvme_mi_cmd\s*\(\s*\(unsigned char\)count\s*,\s*'
            r'\(unsigned char\)\(nvme_index\s*-\s*1\)', source),
            "Host must pass one-based CLI slot minus one to the wire command")
        print("Host source check: CLI slot 28 is one-based, transmitted wire index 27.", flush=True)

    def test_production_executable(self):
        compiler, linker, kernel = toolchain()
        source = harness()
        lines = source.splitlines()
        groups = (("all valid counts/index0..255/presence", 320000),
                  ("invalid bank0/ff,25,255", 38400),
                  ("invalid bank1/ff,25,255", 38400),
                  ("both banks invalid/ff,25,255", 4608),
                  ("explicit boundaries and CLI28", len(TARGETED)))
        passed = 0
        print(f"Compiler: {compiler}\nActual branch source: {MAIN}", flush=True)
        with tempfile.TemporaryDirectory(prefix="nvme_slot_cache_") as directory:
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
                self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            for group, (name, count) in enumerate(groups):
                with self.subTest(group=name):
                    result = subprocess.run([str(exe), str(group)], cwd=temp,
                                            capture_output=True, text=True, timeout=120)
                    print(f"{name}: {result.stdout.strip()}", flush=True)
                    code = result.returncode & 0xFFFFFFFF
                    detail = (f"C assertion: {lines[code - 1].strip()}"
                              if 0 < code <= len(lines) else f"exit=0x{code:08X}")
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr + detail)
                    match = re.fullmatch(r'scenarios=(\d+) success=(\d+) unavailable=(\d+)\s*',
                                         result.stdout)
                    self.assertIsNotNone(match, "Missing actual executable scenario counts")
                    actual, success, unavailable = map(int, match.groups())
                    self.assertEqual(actual, count)
                    self.assertEqual(success + unavailable, actual)
                    passed += actual
            for case in TARGETED:
                print("Boundary probe: " + case[0], flush=True)
        total = sum(count for _, count in groups)
        print(f"Cache scenarios: {total}; passed: {passed}; failed: {total - passed}", flush=True)
        print("All 32 payload bytes and response canaries checked; error text and live USB not covered.",
              flush=True)


if __name__ == "__main__":
    unittest.main(verbosity=2)