import { useCallback, useEffect, useRef, useState } from "preact/hooks";

type ExecuteOptions = RequestInit & { url?: string };

type UseApiState<T> = {
  data: T | null;
  error: string | null;
  loading: boolean;
  execute: (override?: ExecuteOptions) => Promise<T | null>;
  reset: () => void;
};

type ApiResponseStatus = "success" | "fail" | "error";

type ApiResponse<T> = {
  status: ApiResponseStatus;
  data: T | Record<string, unknown> | null;
  error: unknown;
};

const isObject = (value: unknown): value is Record<string, unknown> =>
  typeof value === "object" && value !== null;

const isErrorObjectWithMessage = (value: unknown): value is { message: string } =>
  isObject(value) && typeof value.message === "string";

const extractErrorMessage = (
  payload: Record<string, unknown>,
  fallback: string,
): string => {
  const { error } = payload;

  if (typeof error === "string" && error.length > 0) {
    return error;
  }

  if (isErrorObjectWithMessage(error)) {
    return error.message;
  }

  if (typeof payload.message === "string" && payload.message.length > 0) {
    return payload.message;
  }

  return fallback;
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

function isEqual<T>(a: T | null, b: T | null): boolean {
  if (a === b) {
    return true;
  }
  if (!a || !b) {
    return false;
  }
  try {
    return JSON.stringify(a) === JSON.stringify(b);
  } catch {
    return false;
  }
}

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
            const json = (await response.json()) as Record<string, unknown>;
            if (isObject(json)) {
              message = extractErrorMessage(json, message);
            }
          } catch (parseErr) {
            if (parseErr instanceof Error) {
              console.debug("Failed to parse error response", parseErr);
            }
          }
          throw new Error(message);
        }

        const parsed = (await response.json()) as ApiResponse<T>;
        if (!parsed || typeof parsed.status !== "string") {
          throw new Error("Unexpected API response format");
        }

        if (parsed.status !== "success") {
          const errorValue = parsed.error;
          const message = typeof errorValue === "string"
            ? errorValue
            : isErrorObjectWithMessage(errorValue)
              ? errorValue.message
              : "Request failed";
          throw new Error(message);
        }

        const payload = parsed.data as T;
        setData((prev) => {
          if (isEqual(prev, payload)) {
            return prev;
          }
          return payload;
        });
        return payload;
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
