import { useCallback, useEffect, useRef, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useTranslation } from "../../i18n/context";
import { useApi } from "../../hooks/useApi";
import type { WifiNetwork, WifiScanResponse, WifiResponse } from "../../types/wifi";
import { WifiSignalIndicator } from "./WifiSignalIndicator";
import { SecurityBadge } from "./SecurityBadge";

const LoadingSpinner: FunctionalComponent<{ label?: string }> = ({ label }) => {
  const t = useTranslation();
  return (
    <div class="flex items-center gap-2 text-sm text-slate-500" role="status" aria-live="polite">
      <span class="inline-block h-4 w-4 animate-spin rounded-full border-2 border-slate-300 border-t-sky-500"></span>
      <span>{label ?? t("common.loading")}</span>
    </div>
  );
};

type WifiNetworkListProps = {
  onError?: (message: string) => void;
  onConnectionComplete?: () => void;
};

const isHexKey = (value: string) => /^[0-9a-fA-F]+$/.test(value);

export const WifiNetworkList: FunctionalComponent<WifiNetworkListProps> = ({ onError, onConnectionComplete }) => {
  const t = useTranslation();
  const [networks, setNetworks] = useState<WifiNetwork[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [showModal, setShowModal] = useState(false);
  const [ssid, setSsid] = useState("");
  const [password, setPassword] = useState("");
  const [showPassword, setShowPassword] = useState(false);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  const {
    data: scanResult,
    error: scanError,
    execute: executeScan,
  } = useApi<WifiScanResponse>("/api/v1/wifi/scan", {
    method: "GET",
  });

  const { execute: saveCredentials, loading: isSaving, error: saveError } = useApi<WifiResponse>("/api/v1/wifi/credentials", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
  });

  const { execute: connectWifi, loading: isConnecting, error: connectError } = useApi<WifiResponse>("/api/v1/wifi/connect", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
  });

  const lastScanError = useRef<string | null>(null);
  const lastSaveError = useRef<string | null>(null);
  const lastConnectError = useRef<string | null>(null);

  const triggerScan = useCallback(async () => {
    setIsScanning(true);
    setShowModal(true);
    try {
      await executeScan({ url: "/api/v1/wifi/scan" });
    } catch (err) {
      console.error(err);
    } finally {
      setIsScanning(false);
    }
  }, [executeScan]);

  useEffect(() => {
    if (scanError && scanError !== lastScanError.current) {
      if (onError) {
        onError(scanError);
      }
      lastScanError.current = scanError;
    }
    if (!scanError) {
      lastScanError.current = null;
    }
  }, [scanError, onError]);

  useEffect(() => {
    if (saveError && saveError !== lastSaveError.current) {
      setValidationError(saveError);
      lastSaveError.current = saveError;
    }
    if (!saveError) {
      lastSaveError.current = null;
    }
  }, [saveError]);

  useEffect(() => {
    if (connectError && connectError !== lastConnectError.current) {
      setValidationError(connectError);
      lastConnectError.current = connectError;
    }
    if (!connectError) {
      lastConnectError.current = null;
    }
  }, [connectError]);

  useEffect(() => {
    if (!scanResult) {
      return;
    }

    if (scanResult.error && scanResult.error.length > 0) {
      return;
    }

    const uniqueNetworks = scanResult.networks
      .filter((network) => network.ssid.trim().length > 0)
      .reduce((acc, network) => {
        const existing = acc.find((n) => n.ssid === network.ssid);
        if (!existing || network.signal > existing.signal) {
          return [...acc.filter((n) => n.ssid !== network.ssid), network];
        }
        return acc;
      }, [] as typeof scanResult.networks);

    setNetworks(
      uniqueNetworks
        .map((network) => ({
          id: network.ssid,
          ssid: network.ssid,
          signal: Math.max(0, Math.min(100, Math.round(network.signal))),
          security: network.security,
          connected: network.connected,
          hidden: network.hidden,
          bssid: network.bssid,
          rssi: network.rssi,
        }))
        .sort((a, b) => {
          if (a.connected !== b.connected) return a.connected ? -1 : 1;
          return b.signal - a.signal;
        })
    );
  }, [scanResult]);

  const handleScan = () => {
    if (!isScanning) {
      void triggerScan();
    }
  };

  const handleNetworkSelect = (selectedSsid: string) => {
    setSsid(selectedSsid);
    setShowModal(false);
    setValidationError(null);
    setSuccessMessage(null);
  };

  const validateCredentials = useCallback(() => {
    const trimmedSsid = ssid.trim();
    const trimmedPassword = password.trim();
    const isLengthValid = trimmedPassword.length >= 8 && trimmedPassword.length <= 63;
    const isHexCandidate = trimmedPassword.length === 64 && isHexKey(trimmedPassword);

    if (!trimmedSsid) {
      setValidationError(t("wifi.config.validation.missingSsid"));
      return null;
    }

    if (trimmedSsid.length < 1 || trimmedSsid.length > 32) {
      setValidationError(t("wifi.config.validation.ssidLength"));
      return null;
    }

    if (trimmedPassword && !isLengthValid && !isHexCandidate) {
      setValidationError(t("wifi.config.validation.invalidPassword"));
      return null;
    }

    return { ssid: trimmedSsid, passphrase: trimmedPassword };
  }, [ssid, password, t]);

  const handleSubmit = useCallback(async (e: Event) => {
    e.preventDefault();

    const credentials = validateCredentials();
    if (!credentials) return;

    setValidationError(null);
    setSuccessMessage(null);

    try {
      const response = await saveCredentials({
        body: JSON.stringify(credentials),
      });

      if (response) {
        setSuccessMessage(t("wifi.config.success.saved", { ssid: credentials.ssid }));
      } else {
        setValidationError(t("wifi.config.error.saveFailed"));
      }
    } catch (err) {
      console.error(err);
      setValidationError(t("wifi.config.error.unexpected"));
    }
  }, [validateCredentials, saveCredentials, t]);

  const handleConnectSubmit = useCallback(async (e: Event) => {
    e.preventDefault();

    const credentials = validateCredentials();
    if (!credentials) return;

    setValidationError(null);
    setSuccessMessage(null);

    // First save the credentials
    try {
      const saveResponse = await saveCredentials({
        body: JSON.stringify(credentials),
      });

      if (!saveResponse) {
        setValidationError(t("wifi.config.error.saveFailed"));
        return;
      }

      // Then connect using saved credentials
      const connectResponse = await connectWifi({});

      if (connectResponse) {
        setSuccessMessage(t("wifi.config.success.connected", { ssid: credentials.ssid }));
      } else {
        setValidationError(t("wifi.config.error.connectFailed"));
      }
    } catch (err) {
      console.error(err);
      setValidationError(t("wifi.config.error.unexpected"));
    } finally {
      // Always refresh status after connection attempt (success or failure)
      if (onConnectionComplete) {
        onConnectionComplete();
      }
    }
  }, [validateCredentials, saveCredentials, connectWifi, onConnectionComplete, t]);

  return (
    <>
      <div class="card">
        <div class="mb-5">
          <h2 class="section-title !mb-1">{t("wifi.config.title")}</h2>
          <p class="text-sm text-slate-500">{t("wifi.config.description")}</p>
        </div>

        <form onSubmit={handleSubmit} class="space-y-4">
          <div class="form-field">
            <label for="ssid-input">
              <span>{t("wifi.config.networkLabel")}</span>
            </label>
            <div class="flex gap-2">
              <input
                id="ssid-input"
                type="text"
                class="input flex-1"
                value={ssid}
                onInput={(e) => {
                  setSsid((e.currentTarget as HTMLInputElement).value);
                  setValidationError(null);
                  setSuccessMessage(null);
                }}
                placeholder={t("wifi.config.networkPlaceholder")}
                maxLength={32}
                disabled={isSaving || isConnecting}
              />
              <button
                type="button"
                class="btn-secondary whitespace-nowrap"
                onClick={handleScan}
                disabled={isScanning || isSaving || isConnecting}
              >
                {isScanning ? t("wifi.config.scanning") : t("wifi.config.scan")}
              </button>
            </div>
          </div>

          <div class="form-field">
            <label for="password-input">
              <span>{t("wifi.config.passwordLabel")}</span>
            </label>
            <div class="flex gap-2">
              <input
                id="password-input"
                type={showPassword ? "text" : "password"}
                class="input flex-1"
                value={password}
                onInput={(e) => {
                  setPassword((e.currentTarget as HTMLInputElement).value);
                  setValidationError(null);
                  setSuccessMessage(null);
                }}
                placeholder={t("wifi.config.passwordPlaceholder")}
                maxLength={64}
                disabled={isSaving || isConnecting}
              />
              <div class="flex items-center px-3">
                <label class="flex items-center gap-2 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={showPassword}
                    onChange={(e) => setShowPassword((e.currentTarget as HTMLInputElement).checked)}
                    disabled={isSaving || isConnecting}
                    class="h-4 w-4 rounded border-slate-300 text-sky-600 focus:ring-2 focus:ring-sky-200 focus:ring-offset-0 disabled:opacity-50"
                  />
                  <span class="text-sm text-slate-600 whitespace-nowrap">
                    {t("wifi.config.showPassword")}
                  </span>
                </label>
              </div>
            </div>
          </div>

          {validationError && (
            <div class="rounded-2xl border border-rose-200 bg-rose-50 p-4 text-sm text-rose-700" role="alert">
              {validationError}
            </div>
          )}

          {successMessage && (
            <div class="rounded-2xl border border-emerald-200 bg-emerald-50 p-4 text-sm text-emerald-700" role="status">
              {successMessage}
            </div>
          )}

          <div class="flex gap-2">
            <button
              type="submit"
              class="btn-secondary flex-1 disabled:opacity-50 disabled:cursor-not-allowed"
              disabled={isSaving || isConnecting || !ssid.trim()}
            >
              {isSaving ? t("wifi.config.saving") : t("wifi.config.save")}
            </button>
            <button
              type="button"
              class="btn-primary flex-1 disabled:bg-sky-300 disabled:cursor-not-allowed"
              disabled={isSaving || isConnecting || !ssid.trim()}
              onClick={handleConnectSubmit}
            >
              {isConnecting ? t("wifi.config.connecting") : t("wifi.config.saveAndConnect")}
            </button>
          </div>
        </form>
      </div>

      {/* Scan Results Modal */}
      {showModal && (
        <div class="fixed inset-0 z-50 flex flex-col justify-end bg-slate-900/40 px-4 pb-4">
          <button
            type="button"
            class="flex-1"
            aria-label={t("common.closeModal")}
            onClick={() => setShowModal(false)}
            disabled={isScanning}
          />
          <div class="w-full max-h-[70vh] rounded-3xl border border-slate-200 bg-white shadow-2xl overflow-hidden flex flex-col">
            <div class="p-6 pb-4 border-b border-slate-200">
              <div class="mx-auto mb-4 h-1.5 w-12 rounded-full bg-slate-200"></div>
              <h3 class="text-lg font-semibold text-slate-900">{t("wifi.config.modal.title")}</h3>
              <p class="mt-1 text-sm text-slate-500">{t("wifi.config.modal.description")}</p>
            </div>

            <div class="flex-1 overflow-y-auto p-6 pt-4">
              {isScanning && (
                <div class="py-8 text-center">
                  <LoadingSpinner label={t("wifi.config.modal.scanning")} />
                </div>
              )}

              {!isScanning && networks.length === 0 && (
                <div class="py-8 text-center text-sm text-slate-500">
                  {t("wifi.config.modal.empty")}
                </div>
              )}

              {!isScanning && networks.length > 0 && (
                <div class="space-y-2">
                  {networks.map((network) => (
                    <button
                      key={network.id}
                      type="button"
                      class="flex w-full items-center justify-between rounded-2xl border border-transparent bg-white hover:bg-slate-50 px-4 py-3 text-left transition focus:outline-none focus:ring-2 focus:ring-sky-200"
                      onClick={() => handleNetworkSelect(network.ssid)}
                    >
                      <div class="flex flex-col gap-1">
                        <span class="text-sm font-semibold text-slate-900">{network.ssid}</span>
                        <div class="flex items-center gap-2 text-xs text-slate-500">
                          <SecurityBadge security={network.security} />
                          {network.connected && (
                            <span class="text-sky-600">{t("wifi.config.modal.connected")}</span>
                          )}
                        </div>
                      </div>
                      <WifiSignalIndicator
                        strength={network.signal}
                        label={t("wifi.config.modal.signalLabel", {
                          ssid: network.ssid,
                          rssi: network.rssi,
                        })}
                      />
                    </button>
                  ))}
                </div>
              )}
            </div>

            <div class="p-6 pt-4 border-t border-slate-200">
              <button
                type="button"
                class="btn-secondary w-full"
                onClick={() => setShowModal(false)}
              >
                {t("wifi.config.modal.close")}
              </button>
            </div>
          </div>
        </div>
      )}
    </>
  );
};
