"""Bound simultaneous downloads per verified credential without persistent identity data."""
from contextlib import contextmanager
import threading


class DownloadLimits:
    def __init__(self, per_account=2):
        if type(per_account) is not int or not 1 <= per_account <= 12:
            raise ValueError("Invalid account download limit")
        self.per_account = per_account
        self._active = {}
        self._lock = threading.Lock()

    @contextmanager
    def lease(self, account_key):
        if not isinstance(account_key, bytes) or len(account_key) != 32:
            raise ValueError("A verified account digest is required")
        admitted = False
        with self._lock:
            count = self._active.get(account_key, 0)
            if count < self.per_account:
                self._active[account_key] = count + 1
                admitted = True
        try:
            yield admitted
        finally:
            if admitted:
                with self._lock:
                    count = self._active[account_key] - 1
                    if count:
                        self._active[account_key] = count
                    else:
                        del self._active[account_key]
