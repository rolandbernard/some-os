
SYMBOLS=$($ARCH-someos-nm $1 | awk '{ print "     { .addr = 0x" $1 ", .symbol = \"" $3 "\" }," }' | sort)
SYMBOLS_COUNT=$($ARCH-someos-nm -n $1 | wc -l)

LINES=$($ARCH-someos-objdump -WL $1 | awk '/x$/ {print "     { .addr = " $3 ", .file = \"" $1 "\", .line = " $2 " }," }' | sort)
LINES_COUNT=$($ARCH-someos-objdump -WL $1 | awk '/x$/' | wc -l)

echo "
#include \"error/debuginfo.h\"

size_t __attribute__((section(\".rtdebug\"))) symbol_debug_count = $SYMBOLS_COUNT;
SymbolDebugInfo __attribute__((section(\".rtdebug\"))) symbol_debug[$SYMBOLS_COUNT] = {
$SYMBOLS
};

size_t __attribute__((section(\".rtdebug\"))) line_debug_count = $LINES_COUNT;
LineDebugInfo __attribute__((section(\".rtdebug\"))) line_debug[$LINES_COUNT] = {
$LINES
};
"

