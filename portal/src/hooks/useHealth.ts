import { useEffect } from "preact/hooks";
import { useApi } from "./useApi";
import type { HealthStatus } from "../types/device";

export function useHealth() {
  const { data, error, loading, execute } = useApi<HealthStatus>("/health");

  useEffect(() => {
    execute();
  }, [execute]);

  return { health: data, error, loading };
}
