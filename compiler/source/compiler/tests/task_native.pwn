// Native path for `task`: with a real `native SetTimer`, the synthesised
// registration must emit a valid sysreq (correct native id) -> a well-formed
// .amx the disassembler can read. Guards the uREAD-before-ffcall bug that made
// SetTimer get no sysreq id and corrupted the natives table (invisible to the
// stub-stock test, since a stock takes ffcall's plain-call branch).
native SetTimer(const func[], interval, bool:repeat);

task Ticker[1000]() { new n; n++; }

main() { print("boot"); }
