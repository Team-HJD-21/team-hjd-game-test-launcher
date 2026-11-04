"""Local-only registry retention/deletion. JSON output contains counts only."""
import argparse
import json
from pathlib import Path
import sys
from cloudflare_auth import AuditUnavailable, CloudflareAccess

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', required=True, type=Path)
    parser.add_argument('--apply', action='store_true', help='Default is preview without record changes')
    parser.add_argument('--delete-email-stdin', action='store_true', help='Read one exact email from stdin; never put it in the process command line')
    args = parser.parse_args()
    selected = None
    if args.delete_email_stdin:
        selected = sys.stdin.readline(512).rstrip('\r\n')
        if not selected or len(selected) > 254:
            parser.error('One bounded identity is required on stdin')
    try:
        result = CloudflareAccess(args.root.absolute()).maintain_users(apply=args.apply, remove_email=selected)
        print(json.dumps(result, ensure_ascii=False))
        return 0
    except (AuditUnavailable, OSError, ValueError):
        print('Registry maintenance failed; existing records preserved.', file=sys.stderr)
        return 1

if __name__ == '__main__':
    raise SystemExit(main())
