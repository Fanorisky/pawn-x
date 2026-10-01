// A ptask name long enough that "@ptd_<name>" (5-char prefix) would overflow
// the mangling buffer at sNAMEMAX: must be rejected (warning 200 + skip), not
// silently registered under a truncated name (dead timer / stack write).
ptask AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA[1000](playerid) { new n = playerid; n++; }
main() { print("x"); }
