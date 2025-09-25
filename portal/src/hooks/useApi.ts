import { useCallback, useEffect, useRef, useState } from "preact/hooks";

type ExecuteOptions = RequestInit & { url?: string };

type UseApiState<T> = {
  data: T | null;
  error: string | null;
  loading: boolean;
  execute: (override?: ExecuteOptions) => Promise<T | null>;
  reset: () => void;
};

const mergeHeaders = (
  base: HeadersInit | undefined,
  override: HeadersInit | undefined,
): HeadersInit | undefined => {
  if (!base) {
    return override;
  }
  if (!override) {
    return base;
  }

  const merged = new Headers(base);
  new Headers(override).forEach((value, key) => {
    merged.set(key, value);
  });
  return merged;
};

export function useApi<T>(url: string, init?: RequestInit): UseApiState<T> {
  const [data, setData] = useState<T | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState<boolean>(false);
  const controllerRef = useRef<AbortController | null>(null);

  const execute = useCallback(
    async (override?: ExecuteOptions) => {
      controllerRef.current?.abort();
      const controller = new AbortController();
      controllerRef.current = controller;

      const requestUrl = override?.url ?? url;
      const mergedHeaders = mergeHeaders(init?.headers, override?.headers);
      const requestInit: RequestInit = {
        ...init,
        ...override,
        headers: mergedHeaders,
        signal: controller.signal,
      };

      setLoading(true);
      setError(null);

      try {
        const response = await fetch(requestUrl, requestInit);
        if (!response.ok) {
          let message = `request failed: ${response.status}`;
          try {
            const json = await response.json();
            if (json && typeof json.message === "string") {
              message = json.message;
            }
          } catch (parseErr) {
            if (parseErr instanceof Error) {
              console.debug("Failed to parse error response", parseErr);
            }
          }
          throw new Error(message);
        }

        const parsed = (await response.json()) as T;
        setData(parsed);
        return parsed;
      } catch (err) {
        if ((err as DOMException).name === "AbortError") {
          return null;
        }
        console.error(err);
        setError(err instanceof Error ? err.message : "Request failed");
        setData(null);
        return null;
      } finally {
        setLoading(false);
      }
    },
    [init, url],
  );

  useEffect(() => () => controllerRef.current?.abort(), []);

  const reset = useCallback(() => {
    controllerRef.current?.abort();
    setData(null);
    setError(null);
    setLoading(false);
  }, []);

  return { data, error, loading, execute, reset };
}
