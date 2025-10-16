import { useCallback, useEffect, useState } from "preact/hooks";

export type ConnectionStatus = "connected" | "disconnected" | "checking";

export type UseConnectionMonitorOptions = {
  endpoint?: string;
  intervalMs?: number;
  failureThreshold?: number;
  enabled?: boolean;
  mockMode?: boolean;
};

type HealthData = {
  status: string;
  uptime: number;
  version: string;
};

type ApiResponse<T> = {
  status: "success" | "fail" | "error";
  data: T;
  error: string | null;
};

export type UseConnectionMonitorState = {
  status: ConnectionStatus;
  consecutiveFailures: number;
  lastSuccessTime: number | null;
  shouldShowDialog: boolean;
};

export function useConnectionMonitor(
  options: UseConnectionMonitorOptions = {},
): UseConnectionMonitorState {
  const {
    endpoint = "/health",
    intervalMs = 5000,
    failureThreshold = 5,
    enabled = true,
    mockMode = false,
  } = options;

  const [status, setStatus] = useState<ConnectionStatus>("checking");
  const [consecutiveFailures, setConsecutiveFailures] = useState(0);
  const [lastSuccessTime, setLastSuccessTime] = useState<number | null>(null);
  const [shouldShowDialog, setShouldShowDialog] = useState(false);

  const checkConnection = useCallback(async () => {
    try {
      // Mock mode: always fail
      if (mockMode) {
        throw new Error("Mock failure");
      }

      const controller = new AbortController();
      const timeoutId = setTimeout(() => controller.abort(), 3000);

      const response = await fetch(endpoint, {
        method: "GET",
        signal: controller.signal,
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        throw new Error(`Health check failed: ${response.status}`);
      }

      const result = (await response.json()) as ApiResponse<HealthData>;
      if (result.status !== "success") {
        throw new Error(`Health check failed: ${result.status}`);
      }

      if (result.data.status !== "ok") {
        throw new Error("Health check returned non-ok status");
      }

      // Success
      setConsecutiveFailures(0);
      setLastSuccessTime(Date.now());
      setShouldShowDialog(false);
      setStatus("connected");
    } catch (err) {
      console.warn("Connection check failed:", err);
      setConsecutiveFailures((prev) => {
        const newCount = prev + 1;

        if (newCount >= failureThreshold) {
          setStatus("disconnected");
          setShouldShowDialog(true);
        }

        return newCount;
      });
    }
  }, [endpoint, failureThreshold, mockMode]);

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

      await checkConnection();

      if (active) {
        timer = window.setTimeout(tick, intervalMs);
      }
    };

    // Start immediately
    tick();

    return () => {
      active = false;
      if (timer !== undefined) {
        window.clearTimeout(timer);
      }
    };
  }, [checkConnection, enabled, intervalMs]);

  return {
    status,
    consecutiveFailures,
    lastSuccessTime,
    shouldShowDialog,
  };
}
