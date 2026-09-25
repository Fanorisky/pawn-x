#include <open.mp>
#include <async_omp>

/* Native async/await validated on a REAL open.mp host (experiment 012 phase 3).
 * Coroutines suspend on ACTUAL open.mp timers via the async_omp host adapter --
 * no synthetic pump. OnGameModeInit starts them and returns immediately; the
 * server's tick loop fires the timers, which resume the coroutines in place. */

/* --- 1) sequential real-timer awaits ------------------------------------- */
async Countdown()
{
    new base = 1000;                       // scalar local survives across the awaits
    print("[async] countdown: start");
    await Async_Ms(200);
    print("[async] countdown: tick @200ms");
    await Async_Ms(200);
    printf("[async] countdown: tick @400ms (base still %d)", base);
    print("[async] countdown: DONE");
    return 0;
}

/* --- 2) combinator fed by TWO real timers -------------------------------- */
new g_gate;
forward FeedGate(val);
public FeedGate(val) { Async_GateFeed(g_gate, val); }

async WaitBoth()
{
    g_gate = Async_All(2);                 // resume once BOTH timers have fired
    SetTimerEx("FeedGate", 150, false, "i", 11);
    SetTimerEx("FeedGate", 350, false, "i", 22);
    new total = await Async_Wait(g_gate);  // parks until both -> total = 11+22
    printf("[async] combinator: both timers fired, total=%d", total);
    return total;
}

/* --- 3) fault channel over a real timer ---------------------------------- */
new g_failTok;
forward FailIt();
public FailIt() { Async_ResumeError(g_failTok, 404); }

async MayFail()
{
    g_failTok = Async_Self();
    SetTimerEx("FailIt", 250, false, "");
    new v = await 0;                       // resumed as an ERROR by the timer
    if (Async_Failed())
        printf("[async] fault: caught err=%d (recovered)", Async_Error());
    else
        printf("[async] fault: ok v=%d", v);
    return 0;
}

/* --- 4) ARRAY local surviving a real-timer await ------------------------- */
async ArrayAcross()
{
    new buf[8];
    for (new i = 0; i < 8; i++) buf[i] = i * i;   // fill BEFORE the await (loop has no await)
    await Async_Ms(300);                           // suspend on a real timer
    new sum = 0;
    for (new j = 0; j < 8; j++) sum += buf[j];      // buf lifted into B -> survived the suspend
    printf("[async] array: buf survived the timer, sum=%d", sum);   // 0+1+4+9+16+25+36+49 = 140
    return sum;
}

/* --- lifetime probe: after everything completes, the arena is back to 0 --- */
forward Baseline();
public Baseline() { printf("[async] lifetime: active coroutines now = %d (expect 0)", Async_ActiveCount()); }

public OnGameModeInit()
{
    print(">>> async host-adapter test: starting coroutines");
    Async_Start(Countdown);
    Async_Start(WaitBoth);
    Async_Start(MayFail);
    Async_Start(ArrayAcross);
    printf(">>> started; %d coroutines parked on REAL timers; control returned", Async_ActiveCount());
    SetTimer("Baseline", 700, false);      // after all awaits (<=400ms) have resolved
    return 1;
}

main() {}
