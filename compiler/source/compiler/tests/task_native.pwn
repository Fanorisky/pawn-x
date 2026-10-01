// Native path for `task`: the synthesised registration lowers to the pawn-x
// runtime name Timer_Set (bound to the host SetTimer by <timers>). With a real
// `native Timer_Set` it must emit a valid sysreq (correct native id) -> a
// well-formed .amx the disassembler can read. Guards the uREAD-before-ffcall bug
// that made the timer native get no sysreq id and corrupted the natives table
// (invisible to a stub-stock test, since a stock takes ffcall's plain-call branch).
native Timer_Set(const func[], interval, bool:repeat);

task Ticker[1000]() { new n; n++; }

main() { print("boot"); }
