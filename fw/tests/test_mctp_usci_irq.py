"""Execute the actual nvme_mi.c MCTP region with CRT-free MSVC host mocks.

Run directly with Python or unittest discovery; no external packages are used.
Only temporary C++/object/executable artifacts are written. PROTSTS is a W1C
register, not ordinary RAM. SLASEL on an address ACK without STAR, early-response
holding, and master STOP timing are modeled assumptions, NOT proven hardware
readiness. The masterWrite stub records bytes/results; it does not simulate
silicon timing. DEBUG=0 executes; DEBUG=1 is compiled separately for syntax.
"""

from pathlib import Path
import re
import subprocess
import tempfile
import unittest

from test_usci_i2c_polling import toolchain


ROOT = Path(__file__).resolve().parents[2]
DRIVER = ROOT / "fw/SampleCode/StdDriver/HSUSBD_svf/nvme_mi.c"


def production_code(debug=0):
    source = DRIVER.read_text(encoding="utf-8-sig")
    region = source[source.index("#define MCU_ADDR_7BIT"):
                    source.index("/* Route the mux(es)")]
    # Only test configuration substitutions; no helper/body reimplementation.
    region, count = re.subn(r"(#define MCTP_RX_TIMEOUT_CNT\s+)0x00A00000UL",
                            r"\g<1>8UL", region)
    if count != 1 or region.count("#define NVME_UI2C0_DEBUG   0") != 1:
        raise AssertionError("Production extraction/configuration boundary changed")
    return region.replace("#define NVME_UI2C0_DEBUG   0",
                          "#define NVME_UI2C0_DEBUG   " + str(debug))


MOCK = r"""
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned long long size_t; /* x64 MSVC size_t, without CRT headers */
#define NULL 0
extern "C" __declspec(dllimport) void __stdcall ExitProcess(uint32_t);
extern "C" int printf(const char *, ...); /* DEBUG=1 compile only */
extern "C" void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst; const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; ++i) d[i] = s[i];
    return dst;
}
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
#define UI2C_CTL_PTRG 0x20U
#define UI2C_CTL_STA 0x08U
#define UI2C_CTL_STO 0x04U
#define UI2C_CTL_AA 0x02U
#define UI2C_PROTCTL_STO_Msk UI2C_CTL_STO
#define UI2C_PROTSTS_TOIF_Msk (1U << 5)
#define UI2C_PROTSTS_STARIF_Msk (1U << 8)
#define UI2C_PROTSTS_STORIF_Msk (1U << 9)
#define UI2C_PROTSTS_NACKIF_Msk (1U << 10)
#define UI2C_PROTSTS_ARBLOIF_Msk (1U << 11)
#define UI2C_PROTSTS_ERRIF_Msk (1U << 12)
#define UI2C_PROTSTS_ACKIF_Msk (1U << 13)
#define UI2C_PROTSTS_SLASEL_Msk (1U << 14)
#define UI2C_PROTSTS_SLAREAD_Msk (1U << 15)
#define UI2C_PROTIEN_TOIEN_Msk UI2C_PROTSTS_TOIF_Msk
#define UI2C_PROTIEN_STARIEN_Msk UI2C_PROTSTS_STARIF_Msk
#define UI2C_PROTIEN_STORIEN_Msk UI2C_PROTSTS_STORIF_Msk
#define UI2C_PROTIEN_NACKIEN_Msk UI2C_PROTSTS_NACKIF_Msk
#define UI2C_PROTIEN_ARBLOIEN_Msk UI2C_PROTSTS_ARBLOIF_Msk
#define UI2C_PROTIEN_ERRIEN_Msk UI2C_PROTSTS_ERRIF_Msk
#define UI2C_PROTIEN_ACKIEN_Msk UI2C_PROTSTS_ACKIF_Msk
#define S UI2C_PROTSTS_STARIF_Msk
#define O UI2C_PROTSTS_STORIF_Msk
#define A UI2C_PROTSTS_ACKIF_Msk
#define N UI2C_PROTSTS_NACKIF_Msk
#define L UI2C_PROTSTS_ARBLOIF_Msk
#define E UI2C_PROTSTS_ERRIF_Msk
#define T UI2C_PROTSTS_TOIF_Msk
#define SEL UI2C_PROTSTS_SLASEL_Msk
#define RD UI2C_PROTSTS_SLAREAD_Msk
#define CONFIG 0x5A510001U
#define I2C_CTL_SI 0x08U
#define I2C_CTL_AA 0x04U
#define I2C_CTL0_STO_Msk 0x10U
#define I2C_CTL_STO_SI (I2C_CTL0_STO_Msk | I2C_CTL_SI)
#define UI2C_GCMODE_DISABLE 0U
#define I2C_GCMODE_DISABLE 0U
#define USCI0_IRQn 1
#define I2C1_IRQn 0
static uint32_t trace[4096], ntrace, controls, clears, gets, polls, stops;
static uint32_t wmask, wr_at_clear, len_at_clear, violations, last_control;
static uint32_t primask, barriers, irq[2], pending[2], enabled[2], addrsets;
static uint32_t listen_enables, delay_calls, master_calls, short_write, stop_write;
static int32_t master_error;
static uint8_t sent_data[2][128];
static uint32_t sent_len[2], sent_addr[2], sent_bus[2];
int32_t g_I2C_i32ErrCode, g_UI2C_i32ErrCode;
static void log(uint32_t v) { if (ntrace < 4096) trace[ntrace++] = v; }
static void w1c_observe(uint32_t);
class W1C {
public:
    uint32_t bits;
    operator uint32_t() const volatile { return bits; }
    void operator=(uint32_t value) volatile {
        bits &= ~value; ++clears; wmask = value; log(1); w1c_observe(value);
    }
};
typedef struct { W1C PROTSTS; uint32_t PROTCTL, RXDAT, PROTIEN; } UI2C_T;
typedef struct { uint32_t CTL0, STATUS, DAT; } I2C_T;
static UI2C_T hardware;
static I2C_T i2c;
#define UI2C0 (&hardware)
#define I2C1 (&i2c)
static uint32_t __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask = 1; log(10); }
static void __set_PRIMASK(uint32_t m) { primask = m; log(11); }
static void __DMB(void) { ++barriers; log(2); }
static void NVIC_EnableIRQ(int n) { irq[n] = 1; ++listen_enables; log(12); }
static void NVIC_DisableIRQ(int n) { irq[n] = 0; log(13); }
static void NVIC_ClearPendingIRQ(int n) { pending[n] = 0; log(14); }
static void proto(UI2C_T *p, uint32_t m, uint32_t on) {
    if (on) { p->PROTIEN |= m; ++listen_enables; } else p->PROTIEN &= ~m;
    log(on ? 15 : 16);
}
#define UI2C_ENABLE_PROT_INT(p,m) proto(p,m,1)
#define UI2C_DISABLE_PROT_INT(p,m) proto(p,m,0)
static uint32_t status(UI2C_T *p) { ++polls; return p->PROTSTS; }
static uint32_t mock_data(UI2C_T *p) { ++gets; log(3); return p->RXDAT; }
static void control(UI2C_T *p, uint32_t c) {
    ++controls; last_control = c; log(0x100U | c);
    if ((uint32_t)p->PROTSTS & (S|O|A|N|L|E|T)) ++violations;
    p->PROTCTL = (p->PROTCTL & ~0x2EU) | c;
    if ((p->PROTCTL & ~0x2EU) != CONFIG) ++violations;
    if (c & UI2C_CTL_STO) ++stops; /* log slave reset, no fake STOP completion */
}
#define UI2C_GET_PROT_STATUS(p) status(p)
#define UI2C_GET_DATA(p) mock_data(p)
#define UI2C_SET_CONTROL_REG(p,c) control(p,c)
static void UI2C_SetSlaveAddr(UI2C_T *p, uint32_t, uint32_t a, uint32_t) {
    ++addrsets;
    if (primask != 1 || irq[1] || p->PROTIEN || a != 0x10) ++violations;
    p->PROTCTL = (p->PROTCTL & ~0x2EU); log(17);
}
static void I2C_SetSlaveAddr(I2C_T *, uint32_t, uint32_t a, uint32_t) {
    ++addrsets;
    if (primask != 1 || irq[0] || enabled[0] || a != 0x10) ++violations;
}
static void I2C_EnableInt(I2C_T *) { enabled[0] = 1; ++listen_enables; }
static void I2C_DisableInt(I2C_T *) { enabled[0] = 0; log(16); }
#define I2C_GET_STATUS(p) ((p)->STATUS)
#define I2C_GET_DATA(p) ((p)->DAT)
#define I2C_SET_CONTROL_REG(p,c) ((p)->CTL0 = (c))
#define I2C_WAIT_SI_CLEAR(p) ((void)(p))
static void CLK_SysTickDelay(uint32_t) { ++delay_calls; }
static uint32_t masterWrite(uint32_t bus, uint32_t a, uint8_t *d, uint32_t n) {
    uint32_t k = master_calls++;
    if (k < 2 && n <= 128) {
        sent_len[k] = n; sent_addr[k] = a; sent_bus[k] = bus;
        memcpy(sent_data[k], d, n);
    } else ++violations;
    if (irq[bus] || (bus ? hardware.PROTIEN : enabled[0])) ++violations;
    if (bus) {
        g_UI2C_i32ErrCode = master_error;
        if (stop_write) hardware.PROTCTL |= UI2C_CTL_STO;
    } else {
        g_I2C_i32ErrCode = master_error;
        if (stop_write) i2c.CTL0 |= I2C_CTL0_STO_Msk;
    }
    return short_write ? n - 1 : n;
}
static uint32_t UI2C_WriteMultiBytes(UI2C_T *, uint32_t a, uint8_t *d, uint32_t n) {
    return masterWrite(1,a,d,n);
}
static uint32_t I2C_WriteMultiBytes(I2C_T *, uint32_t a, uint8_t *d, uint32_t n) {
    return masterWrite(0,a,d,n);
}
"""


SUPPORT = r"""
static void w1c_observe(uint32_t) {
    wr_at_clear = s_mctpRxqWr; len_at_clear = s_mctpRxqCurLen;
}
static void reset(uint32_t bus = 1) {
    s_mctpExchangeError = 0;
    s_mctpRxqWr = s_mctpRxqRd = s_mctpRxqCurLen = s_mctpUi2cData = 0;
    s_mctpBus = (uint8_t)bus;
    s_mctpI2C1Handler = Mctp_I2C1_SlaveTRx;
    s_mctpUI2C0Handler = Mctp_UI2C0_SlaveTRx;
    hardware.PROTSTS.bits = 0; hardware.PROTCTL = CONFIG;
    hardware.PROTIEN = hardware.RXDAT = 0;
    i2c.CTL0 = i2c.STATUS = i2c.DAT = 0;
    ntrace = controls = clears = gets = polls = stops = 0;
    wmask = wr_at_clear = len_at_clear = violations = last_control = 0;
    primask = barriers = addrsets = listen_enables = delay_calls = 0;
    master_calls = short_write = stop_write = 0; master_error = 0;
    g_I2C_i32ErrCode = g_UI2C_i32ErrCode = 123;
    for (uint32_t b = 0; b < 2; ++b) {
        irq[b] = pending[b] = enabled[b] = sent_len[b] = 0;
    }
    for (uint32_t q = 0; q < MCTP_RXQ_DEPTH; ++q) {
        s_mctpRxqLen[q] = 0xDD;
        for (uint32_t j = 0; j < MCTP_RXQ_PKT_SIZE; ++j)
            s_mctpRxqPkt[q][j] = 0xCC;
    }
}
static void event(uint32_t flags, uint8_t byte = 0) {
    hardware.PROTSTS.bits = flags; hardware.RXDAT = byte;
    USCI0_IRQHandler();
}
static void address(void) { event(A|SEL,0x20); }
static void packet(uint32_t n, uint32_t final_ack_stop = 0) {
    address();
    for (uint32_t j = 0; j < n; ++j)
        event(A | ((final_ack_stop && j+1 == n) ? O : 0), (uint8_t)j);
    if (!final_ack_stop) event(O);
}
static void response(uint8_t tag, const uint8_t *payload, uint32_t n) {
    uint8_t p[129];
    p[0]=0x20; p[1]=0x0F; p[2]=(uint8_t)(5+n); p[3]=0x3B;
    p[4]=1; p[5]=1; p[6]=0; p[7]=tag;
    for (uint32_t j=0; j<n; ++j) p[8+j]=payload[j];
    p[8+n]=mctp_calc_pec(p,8+n);
    address();
    for (uint32_t j=1; j<9+n; ++j)
        event(A | (j == 8+n ? O : 0), p[j]);
}
static int queue_guard(uint32_t n) {
    for (uint32_t j = 0; j < (n < 128 ? n : 128); ++j)
        CHECK(s_mctpRxqPkt[0][j] == (uint8_t)j);
    for (uint32_t j = n; j < 128; ++j) CHECK(s_mctpRxqPkt[0][j] == 0xCC);
    for (uint32_t j = 0; j < 128; ++j) CHECK(s_mctpRxqPkt[1][j] == 0xCC);
    CHECK(s_mctpRxqLen[1] == 0xDD);
    return 0;
}
#define CALL_CHECK(x) do { int r=(x); if(r) return r; } while(0)
"""


def cases():
    result = []

    def add(name, code):
        result.append((name, code))

    add("mock/direct-W1C-snapshot", """
        reset(); hardware.PROTSTS.bits=A|O|SEL;
        CHECK((uint32_t)hardware.PROTSTS == (A|O|SEL));
        hardware.PROTSTS=S; CHECK((uint32_t)hardware.PROTSTS == (A|O|SEL));
        hardware.PROTSTS=A; CHECK((uint32_t)hardware.PROTSTS == (O|SEL));
        hardware.PROTSTS=O; CHECK((uint32_t)hardware.PROTSTS == SEL);
        CHECK(clears==3 && wmask==O);
    """)
    for final in (0, 1):
        add(f"irq/address-no-STAR/coherent-STOP{final}", f"""
            reset(); packet(4,{final});
            CHECK(s_mctpRxqWr==1 && s_mctpRxqLen[0]==4);
            CHECK(s_mctpRxqCurLen==0 && s_mctpUi2cData==0);
            CHECK(gets==5 && clears=={5 if final else 6});
            CHECK(wmask==({ 'A|O' if final else 'O' }));
            CHECK(wr_at_clear==1 && len_at_clear==0);
            CHECK(trace[ntrace-2]==1 && trace[ntrace-1]==0x122);
            CHECK(barriers==5 && stops==0 && violations==0);
            CALL_CHECK(queue_guard(4));
        """)
    for label, flags, stop in (("ACK-ERR", "A|E", 1),
                                ("ACK-TO", "A|T", 0),
                                ("ACK-ARBLO", "A|L", 0),
                                ("ARBLO-ERR", "L|E", 0),
                                ("STAR-STOP", "S|O", 0),
                                ("STAR-ACK", "S|A", 0),
                                ("ACK-NACK", "A|N", 0),
                                ("read-address", "A|SEL|RD", 0),
                                ("unselected-address", "A", 0)):
        add("irq/reject/" + label, f"""
            reset(); {'address();' if label.startswith('ACK-') or label == 'ARBLO-ERR' else ''}
            uint32_t before=gets; event({flags},0x99);
            CHECK(gets==before && s_mctpExchangeError==MCTP_ERR_SLAVE);
            CHECK(s_mctpRxqCurLen==0 && s_mctpRxqWr==0 && s_mctpUi2cData==0);
            CHECK(stops=={stop} && last_control==UI2C_CTL_PTRG);
            CHECK((hardware.PROTCTL & UI2C_CTL_AA)==0 && violations==0);
            CHECK(wmask==(({flags}) & MCTP_UI2C_EVENTS));
            CHECK(trace[ntrace-{'3' if stop else '2'}]==1);
            {'CHECK(trace[ntrace-2]==0x124);' if stop else ''}
        """)
    add("irq/no-events-and-null-handler", """
        reset(); event(SEL); CHECK(controls==0 && clears==0 && gets==0);
        s_mctpUI2C0Handler=NULL; uint32_t before=polls;
        event(A|SEL); CHECK(polls==before && controls==0 && clears==0);
        CHECK((uint32_t)hardware.PROTSTS==(A|SEL));
    """)
    add("irq/sticky-fault-drops-ongoing-packet", """
        reset(); address(); event(A,0x42); event(E);
        uint32_t before=gets; event(A|SEL,0x77); event(A|O,0x88);
        CHECK(gets==before && s_mctpRxqWr==0 && s_mctpRxqCurLen==0);
        CHECK(s_mctpExchangeError==MCTP_ERR_SLAVE && stops==1);
        CHECK((hardware.PROTCTL & UI2C_CTL_AA)==0);
        mctp_set_error(MCTP_ERR_OVERFLOW);
        CHECK(s_mctpExchangeError==MCTP_ERR_SLAVE);
    """)
    for n in (1, 128, 129, 255, 300):
        add(f"queue/length{n}/guard-index", f"""
            reset(); packet({n}); CALL_CHECK(queue_guard({n}));
            CHECK(s_mctpRxqCurLen==0);
            CHECK(s_mctpRxqWr=={1 if n <= 128 else 0});
            CHECK(s_mctpExchangeError=={'0' if n <= 128 else 'MCTP_ERR_OVERFLOW'});
            CHECK(s_mctpRxqLen[0]=={n if n <= 128 else '0xDD'});
            CHECK(violations==0);
        """)
    add("queue/full-15-overflow-preserves-unread", """
        reset(); for(uint32_t j=0;j<15;++j) packet(1);
        CHECK(s_mctpRxqWr==15 && s_mctpRxqRd==0 && barriers==30);
        address(); event(A,0xAB); event(O);
        CHECK(s_mctpExchangeError==MCTP_ERR_OVERFLOW && s_mctpRxqWr==15);
        CHECK(s_mctpRxqRd==0 && barriers==30);
        for(uint32_t j=0;j<15;++j)
            CHECK(s_mctpRxqLen[j]==1 && s_mctpRxqPkt[j][0]==0);
        CHECK(s_mctpRxqPkt[15][0]==0xCC && s_mctpRxqLen[15]==0xDD);
    """)
    add("queue/wrap-producer-consumer-DMB", """
        reset(); for(uint32_t j=0;j<20;++j) {
            mctp_rxq_append((uint8_t)j); mctp_rxq_publish();
            CHECK(s_mctpRxqPkt[s_mctpRxqRd][0]==j);
            CHECK(s_mctpRxqLen[s_mctpRxqRd]==1); mctp_rxq_release();
        }
        CHECK(s_mctpRxqWr==4 && s_mctpRxqRd==4 && barriers==60);
    """)
    add("i2c1/vector-shared-queue", """
        reset(0); i2c.STATUS=0x60; I2C1_IRQHandler();
        i2c.STATUS=0x80; i2c.DAT=0x5A; I2C1_IRQHandler();
        i2c.STATUS=0xA0; I2C1_IRQHandler();
        CHECK(s_mctpRxqWr==1 && s_mctpRxqLen[0]==1);
        CHECK(s_mctpRxqPkt[0][0]==0x5A && barriers==2);
        CHECK(i2c.CTL0==(I2C_CTL_SI|I2C_CTL_AA));
    """)
    add("crc/standard-check-vectors", """
        reset(); uint8_t v[]={'1','2','3','4','5','6','7','8','9'};
        CHECK(mctp_calc_pec(v,9)==0xF4);
        CHECK(mctp_calc_mic(v,9)==0xE3069283U);
    """)
    for fragments in (1, 2):
        add(f"rx/valid-PEC/type1-noMIC/fragments{fragments}", f"""
            reset(); uint8_t p[]={{1,0x22,0x33,0x44}}, out[6]; uint16_t n=999;
            {'response(0xC0,p,4);' if fragments == 1 else 'response(0x80,p,2); response(0x50,p+2,2);'}
            CHECK(mctp_rx_message(out,6,&n)==0 && n==4);
            for(uint32_t j=0;j<4;++j) CHECK(out[j]==p[j]);
            CHECK(s_mctpRxqRd==s_mctpRxqWr && violations==0);
        """)
    add("rx/raw-length0-through8-before-header", """
        for(uint32_t raw=0;raw<9;++raw) {
            reset(); s_mctpRxqLen[0]=(uint8_t)raw; s_mctpRxqWr=1;
            uint8_t out[4]={0xAB,0xAB,0xAB,0xAB}; uint16_t n=999;
            CHECK(mctp_rx_message(out,4,&n)==-4 && n==0);
            CHECK(out[0]==0xAB && out[3]==0xAB && s_mctpRxqRd==1);
            CHECK(barriers==2);
        }
    """)
    for label, setup, rc in (
        ("oversized-raw", "s_mctpRxqLen[0]=129; s_mctpRxqWr=1;", -3),
        ("bad-byte-count", "response(0xC0,p,2); s_mctpRxqPkt[0][1]=127;", -4),
        ("bad-PEC", "response(0xC0,p,2); s_mctpRxqPkt[0][9]^=1;", -5),
        ("missing-SOM", "response(0x40,p,2);", -6),
        ("wrong-sequence", "response(0x80,p,1); response(0x60,p+1,1);", -7),
        ("output-overflow", "response(0xC0,p,2);", -8),
        ("bad-MIC", "p[0]=0x81; response(0xC0,p,5);", -9),
    ):
        add("rx/" + label, f"""
            reset(); uint8_t p[5]={{1,2,3,4,5}}, out[8]; uint16_t n=999;
            {setup}
            CHECK(mctp_rx_message(out,{1 if rc == -8 else 8},&n)=={rc} && n==0);
            CHECK(s_mctpRxqRd==s_mctpRxqWr);
        """)
    add("rx/sticky-immediate-flush-preserves-mask", """
        for(uint32_t mask=0;mask<2;++mask) {
            reset(); primask=mask; packet(1); s_mctpExchangeError=MCTP_ERR_SLAVE;
            s_mctpRxqCurLen=3; uint8_t out[4]; uint16_t n=999;
            uint32_t before=barriers;
            CHECK(mctp_rx_message_core(out,4,&n)==MCTP_ERR_SLAVE && n==0);
            CHECK(s_mctpRxqRd==s_mctpRxqWr && s_mctpRxqCurLen==0);
            CHECK(barriers==before+1 && primask==mask && gets==2);
        }
    """)
    for bus in (0, 1):
        add(f"lifecycle/bus{bus}/prepare-listen-end/masks0-1", f"""
            for(uint32_t mask=0;mask<2;++mask) {{
                reset({bus}); primask=mask; irq[{bus}]=pending[{bus}]=1;
                hardware.PROTIEN=MCTP_UI2C_INTS; enabled[0]=1;
                s_mctpRxqWr=3; s_mctpRxqCurLen=4; s_mctpUi2cData=1;
                s_mctpExchangeError=MCTP_ERR_OVERFLOW; mctp_bus_prepare();
                CHECK(primask==mask && irq[{bus}]==0 && pending[{bus}]==0);
                CHECK({'hardware.PROTIEN==0' if bus else 'enabled[0]==0'});
                CHECK(addrsets==1 && barriers==1 && s_mctpRxqRd==3);
                CHECK(s_mctpExchangeError==0 && s_mctpRxqCurLen==0);
                CHECK(trace[0]==10 && trace[1]==16 && trace[2]==13 && trace[3]==2);
                CHECK({'s_mctpUI2C0Handler==Mctp_UI2C0_SlaveTRx' if bus else 's_mctpI2C1Handler==Mctp_I2C1_SlaveTRx'});
                hardware.PROTSTS.bits=A|SEL; s_mctpRxqCurLen=2; s_mctpUi2cData=1;
                uint32_t before=controls; mctp_bus_listen();
                CHECK(primask==mask && irq[{bus}]==1);
                CHECK((uint32_t)hardware.PROTSTS==(A|SEL));
                CHECK(s_mctpRxqCurLen==2 && s_mctpUi2cData==1 && clears==0);
                CHECK(controls==before); pending[{bus}]=1; mctp_bus_end();
                CHECK(primask==mask && irq[{bus}]==0 && pending[{bus}]==0);
                CHECK({'hardware.PROTIEN==0 && s_mctpUI2C0Handler==NULL && !(hardware.PROTCTL & UI2C_CTL_AA)' if bus else 'enabled[0]==0 && s_mctpI2C1Handler==NULL && !(i2c.CTL0 & I2C_CTL_AA)'});
                CHECK(s_mctpRxqCurLen==0 && s_mctpUi2cData==0 && barriers==2);
                CHECK(violations==0);
            }}
        """)
        add(f"lifecycle/bus{bus}/pending-STOP-not-cancelled", f"""
            for(uint32_t mask=0;mask<2;++mask) {{
                reset({bus}); primask=mask;
                {'hardware.PROTCTL|=UI2C_CTL_STO;' if bus else 'i2c.CTL0|=I2C_CTL0_STO_Msk;'}
                hardware.PROTSTS.bits=O; mctp_bus_prepare();
                CHECK(s_mctpExchangeError==MCTP_ERR_TX && primask==mask);
                CHECK(addrsets=={0 if bus else 1}); mctp_bus_listen();
                CHECK(listen_enables==0 && irq[{bus}]==0); mctp_bus_end();
                CHECK(primask==mask && controls==0 && clears==0);
                CHECK({'hardware.PROTCTL & UI2C_CTL_STO' if bus else 'i2c.CTL0 & I2C_CTL0_STO_Msk'});
                CHECK((uint32_t)hardware.PROTSTS==O);
            }}
        """)
        add(f"tx/bus{bus}/short-full-error-stop-aborts-first-fragment", f"""
            for(uint32_t mode=0;mode<3;++mode) {{
                reset({bus}); short_write=(mode==0); master_error=(mode==1 ? -2 : 0);
                stop_write=(mode==2); uint8_t p[80];
                for(uint32_t j=0;j<80;++j) p[j]=(uint8_t)j;
                CHECK(mctp_tx_message(p,80)==MCTP_ERR_TX && master_calls==1);
                CHECK(s_mctpExchangeError==MCTP_ERR_TX && controls==0 && clears==0);
                CHECK(irq[{bus}]==0 && listen_enables==0 && violations==0);
                CHECK(!(hardware.PROTCTL & UI2C_CTL_AA));
            }}
        """)
        add(f"tx/bus{bus}/two-fragments-data-order-PEC", f"""
            reset({bus}); uint8_t p[80], full[80];
            for(uint32_t j=0;j<80;++j) p[j]=(uint8_t)(j+1);
            CHECK(mctp_tx_message(p,80)==0 && master_calls==2);
            CHECK(sent_len[0]==72 && sent_len[1]==24);
            for(uint32_t k=0;k<2;++k) {{
                uint32_t n=k ? 16 : 64; CHECK(sent_bus[k]=={bus} && sent_addr[k]==0x1D);
                CHECK(sent_data[k][0]==0x0F && sent_data[k][1]==5+n);
                CHECK(sent_data[k][2]==0x21 && sent_data[k][3]==1);
                CHECK(sent_data[k][4]==0 && sent_data[k][5]==1);
                CHECK(sent_data[k][6]==(k ? 0x58 : 0x88));
                for(uint32_t j=0;j<n;++j) CHECK(sent_data[k][7+j]==p[k*64+j]);
                full[0]=0x3A; memcpy(full+1,sent_data[k],7+n);
                CHECK(mctp_calc_pec(full,8+n)==sent_data[k][7+n]);
            }}
            CHECK(controls=={bus} && clears==0 && gets==0 && listen_enables==0);
            CHECK(g_{'UI2C' if bus else 'I2C'}_i32ErrCode==0 && violations==0);
            {'CHECK(last_control==(UI2C_CTL_PTRG|UI2C_CTL_AA));' if bus else ''}
        """)
    add("common-xfer/TX-failure-propagates-no-listen-no-RX", """
        for(uint32_t bus=0;bus<2;++bus) {
            reset(bus); short_write=1; uint8_t req[16], out[8]; uint16_t n=0xBEEF;
            for(uint32_t j=0;j<16;++j) req[j]=(uint8_t)j;
            CHECK(nvme_mi_cmd_xfer(req,out,8,&n)==MCTP_ERR_TX);
            CHECK(n==0xBEEF && master_calls==1 && listen_enables==0 && gets==0);
            CHECK(irq[bus]==0 && s_mctpExchangeError==MCTP_ERR_TX);
            CHECK(s_mctpI2C1Handler==NULL || s_mctpUI2C0Handler==NULL);
            uint32_t mic=mctp_calc_mic(req,16);
            for(uint32_t j=0;j<16;++j) CHECK(sent_data[0][7+j]==req[j]);
            for(uint32_t j=0;j<4;++j) CHECK(sent_data[0][23+j]==((mic>>(8*j))&255));
            CHECK(violations==0);
        }
    """)
    return result


CASES = cases()


class TestMctpUsciIRQ(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.compiler, cls.linker, cls.kernel = toolchain()
        cls.temp = tempfile.TemporaryDirectory(prefix="mctp_usci_irq_")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.directory = Path(cls.temp.name)
        functions = "\n".join(f"static int case_{i}(void) {{\n{body}\nreturn 0;\n}}"
                              for i, (_, body) in enumerate(CASES))
        # Python selects a case by rewriting only the temporary harness main.
        # One executable per case avoids CRT command-line parsing and gives the
        # precise C++ CHECK line in the process exit code on failure.
        cls.base = MOCK + production_code() + SUPPORT + functions
        cls.base_file = cls.directory / "production_cases.cpp"
        cls.base_file.write_text(cls.base, encoding="utf-8")
        cls.base_obj = cls.directory / "production_cases.obj"
        # Export a tiny dispatcher, then link a case-specific main object.
        with cls.base_file.open("a", encoding="utf-8") as stream:
            stream.write("\nextern \"C\" int run_case(uint32_t n) { switch(n) {\n")
            for i in range(len(CASES)):
                stream.write(f"case {i}: return case_{i}();\n")
            stream.write("default: return 999; } }\n")
        cls.compile(cls.base_file, cls.base_obj)

    @classmethod
    def checked(cls, command):
        proc = subprocess.run([str(x) for x in command], capture_output=True,
                              text=True, timeout=120)
        if proc.returncode:
            raise AssertionError(f"Toolchain failed ({proc.returncode}):\n"
                                 + proc.stdout + proc.stderr)

    @classmethod
    def compile(cls, source, obj):
        cls.checked([cls.compiler, "/nologo", "/TP", "/GS-", "/Zl", "/c",
                     source, "/Fo" + str(obj)])

    def test_debug1_syntax(self):
        source = self.directory / "debug1.cpp"
        source.write_text(MOCK + production_code(1) + SUPPORT, encoding="utf-8")
        self.compile(source, self.directory / "debug1.obj")

    def test_all_five_callers_consume_tx_result(self):
        source = DRIVER.read_text(encoding="utf-8-sig")
        calls = list(re.finditer(r"\bret = mctp_tx_message\([^;]+;", source))
        self.assertEqual(len(calls), 5)
        for call in calls:
            tail = source[call.end():]
            guarded = re.match(r"\s*if\s*\(ret == 0\)\s*\{", tail)
            self.assertIsNotNone(guarded)
            start = guarded.end()
            depth, end = 1, start
            while depth:
                depth += (tail[end] == "{") - (tail[end] == "}")
                end += 1
            success = tail[start:end-1]
            self.assertEqual(success.count("mctp_bus_listen();"), 1)
            self.assertEqual(success.count("ret = mctp_rx_message("), 1)
            # No ignored TX result/retry between this call and lifecycle end.
            exchange = tail[:tail.index("mctp_bus_end();")]
            self.assertEqual(exchange.count("mctp_tx_message("), 0)
            self.assertEqual(exchange.count("mctp_rx_message("), 1)
            self.assertIn("mctp_bus_end();", tail[end:])

    def execute(self, index):
        main = self.directory / "main.cpp"
        main.write_text(
            'extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int);\n'
            'extern "C" int run_case(unsigned int);\n'
            f'extern "C" void main(void) {{ ExitProcess(run_case({index})); }}\n',
            encoding="utf-8")
        obj, exe = self.directory / "main.obj", self.directory / "case.exe"
        self.compile(main, obj)
        self.checked([self.linker, "/nologo", "/entry:main", "/subsystem:console",
                      "/nodefaultlib", "/out:" + str(exe), obj,
                      self.base_obj, self.kernel])
        proc = subprocess.run([str(exe)], capture_output=True, timeout=10)
        line = proc.returncode
        source_line = (self.base.splitlines()[line-1]
                       if 0 < line <= len(self.base.splitlines()) else "")
        self.assertEqual(line, 0, f"{CASES[index][0]}: host C++ line {line}: {source_line}")


for _index, (_name, _) in enumerate(CASES):
    def _test(self, index=_index):
        self.execute(index)
    setattr(TestMctpUsciIRQ, "test_" + f"{_index:02d}_" + re.sub(r"\W+", "_", _name), _test)


if __name__ == "__main__":
    unittest.main(verbosity=2)