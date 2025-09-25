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
  const [ssid, setSsid] = useState("");
  const [passphrase, setPassphrase] = useState("");
  const [submitError, setSubmitError] = useState<string | null>(null);
  const [submitSuccess, setSubmitSuccess] = useState<string | null>(null);
  const [submitting, setSubmitting] = useState(false);

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

  const handleSubmit = async (event: Event) => {
    event.preventDefault();
    if (submitting) {
      return;
    }
    setSubmitError(null);
    setSubmitSuccess(null);

    if (!ssid.trim()) {
      setSubmitError("Please enter an SSID.");
      return;
    }
    if (passphrase.length < 8 || passphrase.length > 63) {
      setSubmitError("Password must be 8-63 characters.");
      return;
    }

    setSubmitting(true);
    try {
      const response = await fetch("/api/v1/wifi/credentials", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          ssid: ssid.trim(),
          passphrase,
        }),
      });

      if (!response.ok) {
        const json = await response.json().catch(() => null);
        if (json && typeof json.message === "string") {
          throw new Error(json.message);
        }
        throw new Error(`request failed: ${response.status}`);
      }

      const json: { restart_required?: boolean } = await response.json();
      if (json.restart_required) {
        setSubmitSuccess("Saved credentials. Restart is required.");
      } else {
        setSubmitSuccess("Saved credentials.");
      }
    } catch (err) {
      console.error(err);
      setSubmitError(
        err instanceof Error ? err.message : "Failed to save credentials.",
      );
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <main class="w-full max-w-md rounded-xl border border-black/10 bg-white p-8 shadow-sm">
      <h1 class="mb-6 text-center text-2xl font-semibold text-slate-900">
        ESP Gateway
      </h1>

      {error && (
        <p class="mb-6 text-center text-sm font-medium text-red-600">{error}</p>
      )}

      {info ? (
        <dl class="grid gap-3 text-sm text-slate-700">
          <div class="flex items-center justify-between gap-4">
            <dt class="font-semibold text-slate-600">Model</dt>
            <dd>{info.model}</dd>
          </div>
          <div class="flex items-center justify-between gap-4">
            <dt class="font-semibold text-slate-600">Firmware</dt>
            <dd>{info.firmware_version}</dd>
          </div>
          <div class="flex items-center justify-between gap-4">
            <dt class="font-semibold text-slate-600">Build Time</dt>
            <dd>{info.build_time}</dd>
          </div>
          <div class="flex items-center justify-between gap-4">
            <dt class="font-semibold text-slate-600">ESP-IDF</dt>
            <dd>{info.idf_version}</dd>
          </div>
        </dl>
      ) : (
        !error && (
          <p class="mb-6 text-center text-sm text-slate-500">Loading…</p>
        )
      )}

      <section class="mt-8 border-t border-slate-200 pt-6">
        <h2 class="mb-4 text-lg font-semibold text-slate-900">Configure Wi-Fi</h2>
        <form class="grid gap-4" onSubmit={handleSubmit}>
          <label class="flex flex-col gap-2 text-sm font-medium text-slate-700">
            <span>SSID</span>
            <input
              type="text"
              value={ssid}
              onInput={(event) => {
                const target = event.currentTarget;
                if (target instanceof HTMLInputElement) {
                  setSsid(target.value);
                }
              }}
              placeholder="e.g. gateway-ap"
              required
              class="rounded-lg border border-slate-300 px-3 py-2 text-base shadow-sm transition focus:border-blue-500 focus:outline-none focus:ring-2 focus:ring-blue-200"
            />
          </label>
          <label class="flex flex-col gap-2 text-sm font-medium text-slate-700">
            <span>Password</span>
            <input
              type="password"
              value={passphrase}
              onInput={(event) => {
                const target = event.currentTarget;
                if (target instanceof HTMLInputElement) {
                  setPassphrase(target.value);
                }
              }}
              placeholder="8-63 characters"
              minLength={8}
              maxLength={63}
              required
              class="rounded-lg border border-slate-300 px-3 py-2 text-base shadow-sm transition focus:border-blue-500 focus:outline-none focus:ring-2 focus:ring-blue-200"
            />
          </label>
          <button
            type="submit"
            disabled={submitting}
            class="ml-auto inline-flex items-center justify-center rounded-full bg-blue-600 px-6 py-2 text-sm font-semibold text-white transition hover:bg-blue-700 disabled:cursor-not-allowed disabled:bg-blue-400"
          >
            {submitting ? "Saving…" : "Save"}
          </button>
        </form>
        {submitError && (
          <p class="mt-4 text-sm font-medium text-red-600">{submitError}</p>
        )}
        {submitSuccess && (
          <p class="mt-4 text-sm font-medium text-emerald-600">
            {submitSuccess}
          </p>
        )}
      </section>
    </main>
  );
}
