import { useEffect, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";

type DevicePageProps = {
  path?: string;
};

type DeviceInfo = {
  model: string;
  firmware_version: string;
  build_time: string;
  idf_version: string;
};

export const DevicePage: FunctionalComponent<DevicePageProps> = () => {
  const [info, setInfo] = useState<DeviceInfo | null>(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();

    const fetchDeviceInfo = async () => {
      setLoading(true);
      try {
        const response = await fetch("/api/v1/device-info", {
          signal: controller.signal,
        });
        if (!response.ok) {
          throw new Error(`request failed: ${response.status}`);
        }
        const json: DeviceInfo = await response.json();
        setInfo(json);
        setError(null);
      } catch (err) {
        if ((err as DOMException).name === "AbortError") {
          return;
        }
        console.error(err);
        setError("Failed to fetch device information.");
      } finally {
        setLoading(false);
      }
    };

    fetchDeviceInfo();
    return () => controller.abort();
  }, []);

  return (
    <section class="space-y-6">
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
