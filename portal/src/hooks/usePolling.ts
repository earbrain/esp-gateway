import { useCallback, useEffect, useRef } from "preact/hooks";

export type UsePollingOptions = {
  intervalMs?: number;
  enabled?: boolean;
  immediate?: boolean;
};

type PollingCallback = () => void | Promise<unknown>;

export function usePolling(
  callback: PollingCallback,
  options: UsePollingOptions = {},
): { refresh: () => Promise<void> } {
  const { intervalMs = 3000, enabled = true, immediate = true } = options;
  const savedCallback = useRef<PollingCallback>(() => undefined);

  useEffect(() => {
    savedCallback.current = callback;
  }, [callback]);

  const refresh = useCallback(async () => {
    await savedCallback.current();
  }, []);

  useEffect(() => {
    if (!enabled || typeof window === "undefined") {
      return () => undefined;
    }

    let active = true;
    let timer: number | undefined;

    const tick = async () => {
      if (!active) {
        return;
      }

      try {
        await savedCallback.current();
      } finally {
        if (active) {
          timer = window.setTimeout(tick, intervalMs);
        }
      }
    };

    if (immediate) {
      tick();
    } else {
      timer = window.setTimeout(tick, intervalMs);
    }

    return () => {
      active = false;
      if (timer !== undefined) {
        window.clearTimeout(timer);
      }
    };
  }, [enabled, immediate, intervalMs]);

  return { refresh };
}
