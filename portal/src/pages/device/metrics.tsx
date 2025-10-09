import { useApi } from "../../hooks/useApi";
import { usePolling } from "../../hooks/usePolling";
import type { FunctionalComponent } from "preact";
import { useEffect } from "preact/hooks";

type DeviceMetricsPageProps = {
  path?: string;
};

type Metrics = {
  heap_total: number;
  heap_free: number;
  heap_used: number;
  heap_min_free: number;
  heap_largest_free_block: number;
  timestamp_ms: number;
};

export const DeviceMetricsPage: FunctionalComponent<DeviceMetricsPageProps> = () => {
  const {
    data: metrics,
    loading: metricsLoading,
    error: metricsError,
    execute: fetchMetrics,
  } = useApi<Metrics>("/api/v1/metrics");

  const { refresh: refreshMetrics } = usePolling(() => fetchMetrics(), {
    intervalMs: 5000,
    immediate: false,
  });

  const formatBytes = (value: number): string => {
    if (!Number.isFinite(value) || value < 0) {
      return "-";
    }
    if (value < 1024) {
      return `${value} B`;
    }
    if (value < 1024 * 1024) {
      return `${(value / 1024).toFixed(1)} KiB`;
    }
    return `${(value / (1024 * 1024)).toFixed(2)} MiB`;
  };

  const heapUsagePercent =
    metrics && metrics.heap_total > 0
      ? Math.round((metrics.heap_used / metrics.heap_total) * 100)
      : 0;
  const showMetricsLoading = metricsLoading && !metrics;

  useEffect(() => {
    fetchMetrics();
  }, [fetchMetrics]);

  return (
    <section class="space-y-6">
      <div class="card space-y-4">
        <div class="flex items-center justify-between gap-4">
          <h2 class="section-title">System Metrics</h2>
          <button
            type="button"
            class="btn-secondary"
            onClick={() => {
              void refreshMetrics();
            }}
            disabled={metricsLoading}
          >
            Refresh
          </button>
        </div>
        {showMetricsLoading && <p class="muted">Loading...</p>}
        {metricsError && <p class="error-text">{metricsError}</p>}
        {metrics && (
          <div class="space-y-5 text-sm text-slate-700">
            <div>
              <h3 class="text-xs font-semibold uppercase tracking-wide text-slate-500">Heap</h3>
              <dl class="mt-2 grid gap-3">
                <div class="info-row">
                  <dt>Total Heap</dt>
                  <dd>{formatBytes(metrics.heap_total)}</dd>
                </div>
                <div class="info-row">
                  <dt>Heap Used</dt>
                  <dd>
                    {formatBytes(metrics.heap_used)}{" "}
                    <span class="muted text-xs">({heapUsagePercent}%)</span>
                  </dd>
                </div>
                <div class="info-row">
                  <dt>Heap Free</dt>
                  <dd>{formatBytes(metrics.heap_free)}</dd>
                </div>
                <div class="info-row">
                  <dt>Minimum Free (Watermark)</dt>
                  <dd>{formatBytes(metrics.heap_min_free)}</dd>
                </div>
                <div class="info-row">
                  <dt>Largest Free Block</dt>
                  <dd>{formatBytes(metrics.heap_largest_free_block)}</dd>
                </div>
              </dl>
            </div>
          </div>
        )}
      </div>
    </section>
  );
};
