import { useEffect, useState } from "preact/hooks";

type DeviceInfo = {
  model: string;
  firmware_version: string;
  build_time: string;
  idf_version: string;
};

export function App() {
  const [info, setInfo] = useState<DeviceInfo | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    const controller = new AbortController();

    const fetchDeviceInfo = async () => {
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
        setError("Failed to load device information.");
      }
    };

    fetchDeviceInfo();
    return () => controller.abort();
  }, []);

  return (
    <main class="app">
      <h1>ESP Gateway</h1>
      {error && <p class="error">{error}</p>}
      {info ? (
        <dl class="device-info">
          <div>
            <dt>Model</dt>
            <dd>{info.model}</dd>
          </div>
          <div>
            <dt>Firmware</dt>
            <dd>{info.firmware_version}</dd>
          </div>
          <div>
            <dt>Build Time</dt>
            <dd>{info.build_time}</dd>
          </div>
          <div>
            <dt>ESP-IDF</dt>
            <dd>{info.idf_version}</dd>
          </div>
        </dl>
      ) : (
        !error && <p class="loading">Loading…</p>
      )}
    </main>
  );
}
