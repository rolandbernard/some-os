
COUNT=$($ARCH-someos-nm $1 | wc -l)

SYMBOLS=$($ARCH-someos-nm $1 | sort | awk '{ print "    { .addr = 0x" $1 ", .symbol = \"" $3 "\" }," }')

echo "
#include \"error/debuginfo.h\"

size_t __attribute__((section(\".rtdebug\"))) symbol_debug_count = $COUNT;

SymbolDebugInfo __attribute__((section(\".rtdebug\"))) symbol_debug[$COUNT] = {
$SYMBOLS
};
"

