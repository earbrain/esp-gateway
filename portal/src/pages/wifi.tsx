import { useCallback, useEffect, useMemo, useRef, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../hooks/useApi";
import { usePolling } from "../hooks/usePolling";
import { Toast } from "../components/Toast";
import { WifiStatusCard } from "../components/WifiStatusCard";

type WifiPageProps = {
  path?: string;
};

type WifiFormValues = {
  ssid: string;
  passphrase: string;
};

type WifiResponse = {
  restart_required?: boolean;
  sta_connect_started?: boolean;
  sta_error?: string;
};

type WifiStatus = {
  ap_active: boolean;
  sta_active: boolean;
  sta_connecting: boolean;
  sta_connected: boolean;
  sta_error: string;
  ip: string;
  disconnect_reason: number;
};

const isHexKey = (value: string) => /^[0-9a-fA-F]+$/.test(value);

export const WifiPage: FunctionalComponent<WifiPageProps> = () => {
  const [values, setValues] = useState<WifiFormValues>({
    ssid: "",
    passphrase: "",
  });
  const [validationError, setValidationError] = useState<string | null>(null);
  const [toast, setToast] = useState<{ message: string; type: "success" | "error" } | null>(null);
  const [settingsVisible, setSettingsVisible] = useState(true);
  const [showPassword, setShowPassword] = useState(false);

  const { execute, loading, error, reset } = useApi<WifiResponse>("/api/v1/wifi/credentials", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
  });

  const {
    data: status,
    loading: statusLoading,
    error: statusError,
    execute: fetchStatus,
  } = useApi<WifiStatus>("/api/v1/wifi/status", {
    method: "GET",
  });

  const { refresh: refreshStatus } = usePolling(() => fetchStatus(), {
    intervalMs: 5000,
  });

  const lastCredentialError = useRef<string | null>(null);
  const lastStatusError = useRef<string | null>(null);

  const showToast = useCallback((type: "success" | "error", message: string) => {
    setToast({ type, message });
  }, []);

  useEffect(() => {
    if (error && error !== lastCredentialError.current) {
      showToast("error", error);
      lastCredentialError.current = error;
    }
    if (!error) {
      lastCredentialError.current = null;
    }
  }, [error, showToast]);

  useEffect(() => {
    if (statusError && statusError !== lastStatusError.current) {
      showToast("error", statusError);
      lastStatusError.current = statusError;
    }
    if (!statusError) {
      lastStatusError.current = null;
    }
  }, [showToast, statusError]);

  const statusSummary = useMemo(() => {
    if (!status) {
      return statusLoading ? "Checking Wi-Fi status..." : "Status unavailable";
    }
    if (status.sta_connected) {
      return status.ip ? `Connected to ${status.ip}` : "Connected";
    }
    if (status.sta_connecting) {
      return "Connecting to network...";
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return `Connection error: ${status.sta_error}`;
    }
    return "Not connected";
  }, [status, statusLoading]);

  const apState = useMemo(() => {
    if (!status) {
      return {
        label: "Unknown",
        description: "Waiting for status update.",
        badgeClass: "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.ap_active) {
      return {
        label: "Active",
        description: "Device is broadcasting the setup access point.",
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    return {
      label: "Inactive",
      description: "Soft AP is currently disabled.",
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status]);

  const staState = useMemo(() => {
    if (!status) {
      return {
        label: statusLoading ? "Checking" : "Unknown",
        description: statusLoading
          ? "Fetching the latest station status..."
          : "Waiting for status update.",
        badgeClass: statusLoading
          ? "status-pill bg-sky-100 text-sky-600"
          : "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.sta_connected) {
      return {
        label: "Connected",
        description: status.ip
          ? `Client connected with IP ${status.ip}.`
          : "Client connected to Wi-Fi network.",
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    if (status.sta_connecting) {
      return {
        label: "Connecting",
        description: "Attempting to join the configured network...",
        badgeClass: "status-pill bg-amber-100 text-amber-700",
      };
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return {
        label: "Error",
        description: `Last error: ${status.sta_error}.`,
        badgeClass: "status-pill bg-rose-100 text-rose-700",
      };
    }
    return {
      label: "Not connected",
      description: "STA client is idle.",
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status, statusLoading]);

  const handleInputChange = (field: keyof WifiFormValues) => (event: Event) => {
    const target = event.currentTarget as HTMLInputElement | null;
    if (target) {
      setValues((prev) => ({ ...prev, [field]: target.value }));
    }
  };

  const handleSubmit = async (event: Event) => {
    event.preventDefault();

    const trimmedSsid = values.ssid.trim();
    const passphrase = values.passphrase;

    const isLengthValid = passphrase.length >= 8 && passphrase.length <= 63;
    const isHexCandidate = passphrase.length === 64 && isHexKey(passphrase);

    if (!trimmedSsid) {
      setValidationError("Please enter an SSID.");
      reset();
      return;
    }

    if (!isLengthValid && !isHexCandidate) {
      setValidationError("Password must be 8-63 characters or a 64-digit hex string.");
      reset();
      return;
    }

    setValidationError(null);

    try {
      const response = await execute({
        body: JSON.stringify({
          ssid: trimmedSsid,
          passphrase,
        }),
      });

      if (response) {
        const { restart_required, sta_connect_started, sta_error } = response;
        const messageParts: string[] = ["Saved credentials."];
        if (sta_connect_started) {
          messageParts.push("Starting STA connection...");
        } else if (typeof restart_required === "boolean" && restart_required) {
          messageParts.push("Restart is required to apply changes.");
        }
        if (sta_error) {
          messageParts.push(`Error: ${sta_error}`);
        }
        showToast(sta_error ? "error" : "success", messageParts.join(" "));
        setValues({ ssid: trimmedSsid, passphrase });
        refreshStatus();
      }
    } catch (err) {
      console.error(err);
    }
  };

  return (
    <>
      {toast && (
        <div
          class="fixed inset-x-0 top-4 z-40 flex justify-center px-4"
          role="status"
          aria-live="polite"
        >
          <Toast message={toast.message} type={toast.type} onClose={() => setToast(null)} />
        </div>
      )}
      <section class="space-y-6">
        <div class="flex justify-end">
          <button
            type="button"
            class="btn-secondary"
            onClick={() => setSettingsVisible((prev) => !prev)}
            aria-expanded={settingsVisible}
            aria-controls="wifi-settings-panel"
          >
            {settingsVisible ? "Hide Wi-Fi settings" : "Show Wi-Fi settings"}
          </button>
        </div>
        {settingsVisible && (
          <div class="card" id="wifi-settings-panel">
            <h2 class="section-title">Update Credentials</h2>
            <form class="grid gap-4" onSubmit={handleSubmit}>
              <label class="form-field">
                <span>SSID</span>
                <input
                  type="text"
                  value={values.ssid}
                  onInput={handleInputChange("ssid")}
                  placeholder="e.g. gateway-ap"
                  required
                  class="input"
                />
              </label>
              <label class="form-field">
                <span>Password</span>
                <input
                  type={showPassword ? "text" : "password"}
                  value={values.passphrase}
                  onInput={handleInputChange("passphrase")}
                  placeholder="8-63 chars or 64-digit hex"
                  minLength={8}
                  maxLength={64}
                  required
                  class="input"
                />
              </label>
              <label class="flex items-center gap-2 text-sm text-slate-600">
                <input
                  type="checkbox"
                  checked={showPassword}
                  onChange={() => setShowPassword((prev) => !prev)}
                  class="h-4 w-4"
                />
                Show password
              </label>
              <div class="flex items-center justify-end">
                <button type="submit" disabled={loading} class="btn-primary">
                  {loading ? "Saving..." : "Save"}
                </button>
              </div>
            </form>
            {validationError && <p class="error-text">{validationError}</p>}
          </div>
        )}
        <WifiStatusCard
          apState={apState}
          staState={staState}
          status={status}
          loading={statusLoading || loading}
          onRefresh={refreshStatus}
          summary={statusSummary}
        />
      </section>
    </>
  );
};
