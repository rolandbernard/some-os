
COUNT=$($ARCH-someos-nm $1 | wc -l)

SYMBOLS=$($ARCH-someos-nm $1 | awk '{ print "    { .addr = 0x" $1 ", .symbol = \"" $3 "\" }," }')

echo "
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uintptr_t addr;
    const char* symbol;
} SymbolDebugInfo;

size_t __attribute__((section(\".rtdebug\"))) symbol_debug_count = $COUNT;

SymbolDebugInfo __attribute__((section(\".rtdebug\"))) symbol_debug[$COUNT] = {
$SYMBOLS
};
"

