#!/bin/bash
exec 1> >(cat >/dev/null)
python3 -c "import os; import sys; print('isatty_1=', os.isatty(1), file=sys.stderr)"
