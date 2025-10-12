import { useEffect } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../../hooks/useApi";
import type { DeviceDetail } from "../../types/device";

export const DeviceOverviewCard: FunctionalComponent = () => {
  const { data: info, loading, error, execute } = useApi<DeviceDetail>("/api/v1/device");

  useEffect(() => {
    execute();
  }, [execute]);

  return (
    <div class="card">
      <h2 class="section-title">Device Overview</h2>
      {loading && <p class="muted">Loading...</p>}
      {error && <p class="error-text">{error}</p>}
      {info && (
        <dl class="grid gap-3 text-sm text-slate-700">
          <div class="info-row">
            <dt>Model</dt>
            <dd>{info.model}</dd>
          </div>
          <div class="info-row">
            <dt>Gateway Version</dt>
            <dd>{info.gateway_version}</dd>
          </div>
          <div class="info-row">
            <dt>Build Time</dt>
            <dd>{info.build_time}</dd>
          </div>
          <div class="info-row">
            <dt>ESP-IDF</dt>
            <dd>{info.idf_version}</dd>
          </div>
        </dl>
      )}
    </div>
  );
};
