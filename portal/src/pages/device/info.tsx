import { useApi } from "../../hooks/useApi";
import type { FunctionalComponent } from "preact";
import { useEffect } from "preact/hooks";

type DeviceInfoPageProps = {
  path?: string;
};

type DeviceDetail = {
  model: string;
  firmware_version: string;
  build_time: string;
  idf_version: string;
};

export const DeviceInfoPage: FunctionalComponent<DeviceInfoPageProps> = () => {
  const { data: info, loading, error, execute } = useApi<DeviceDetail>("/api/v1/device");

  useEffect(() => {
    execute();
  }, [execute]);

  return (
    <section class="space-y-6">
      <div class="card">
        <h2 class="section-title">Device Information</h2>
        {loading && <p class="muted">Loading...</p>}
        {error && <p class="error-text">{error}</p>}
        {info && (
          <dl class="grid gap-3 text-sm text-slate-700">
            <div class="info-row">
              <dt>Model</dt>
              <dd>{info.model}</dd>
            </div>
            <div class="info-row">
              <dt>Firmware</dt>
              <dd>{info.firmware_version}</dd>
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
    </section>
  );
};
