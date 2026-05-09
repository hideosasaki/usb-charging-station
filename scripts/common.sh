# Shared helpers for scripts/upload-via-pi.sh and serial-via-pi.sh.
# Sourced, not executed.

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$PROJECT_DIR/.env" ] && set -a && . "$PROJECT_DIR/.env" && set +a
: "${PI_HOST:?PI_HOST not set; create .env from .env.example or export PI_HOST=...}"
