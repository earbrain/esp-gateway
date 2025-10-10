import type { FunctionalComponent } from "preact";
import { WifiSignalIndicator } from "./WifiSignalIndicator";

type WifiNetwork = {
  id: string;
  ssid: string;
  signal: number;
  security: string;
  connected: boolean;
  hidden: boolean;
  bssid: string;
  rssi: number;
};

type WifiConnectModalProps = {
  network: WifiNetwork | null;
  open: boolean;
  password: string;
  showPassword: boolean;
  loading: boolean;
  validationError: string | null;
  awaitingResult: boolean;
  notice: { type: "info" | "error"; message: string } | null;
  onClose: () => void;
  onConnect: () => void;
  onPasswordChange: (value: string) => void;
  onToggleShowPassword: () => void;
};

export const WifiConnectModal: FunctionalComponent<WifiConnectModalProps> = ({
  network,
  open,
  password,
  showPassword,
  loading,
  awaitingResult,
  notice,
  validationError,
  onClose,
  onConnect,
  onPasswordChange,
  onToggleShowPassword,
}) => {
  if (!open || !network) {
    return null;
  }

  const requiresPassword = network.security.trim().toLowerCase() !== "open";

  return (
    <div class="fixed inset-0 z-50 flex flex-col justify-end bg-slate-900/40 px-4 pb-4">
      <button
        type="button"
        class="flex-1"
        aria-label="Close modal"
        onClick={() => {
          if (!awaitingResult) {
            onClose();
          }
        }}
        disabled={awaitingResult}
      ></button>
      <div class="w-full rounded-3xl border border-slate-200 bg-white p-6 shadow-2xl">
        <div class="mx-auto mb-4 h-1.5 w-12 rounded-full bg-slate-200"></div>
        <div class="flex items-start justify-between gap-4">
          <div>
            <span class="text-xs font-medium text-slate-500">
              {requiresPassword ? network.security : "No password required"}
            </span>
            <h3 class="mt-1 text-lg font-semibold text-slate-900">{network.ssid}</h3>
          </div>
          <WifiSignalIndicator strength={network.signal} />
        </div>
        {requiresPassword && (
          <div class="mt-5">
            <label class="form-field">
              <span>Password</span>
              <div class="relative">
                <input
                  type={showPassword ? "text" : "password"}
                  value={password}
                  onInput={(event) =>
                    onPasswordChange((event.currentTarget as HTMLInputElement).value)
                  }
                  placeholder="8-63 characters or 64-digit hex"
                  minLength={8}
                  maxLength={64}
                  required
                  class="input pr-20"
                  disabled={awaitingResult}
                />
                <button
                  type="button"
                  class="absolute inset-y-0 right-0 flex items-center px-3 text-sm font-medium text-sky-600"
                  onClick={onToggleShowPassword}
                  disabled={awaitingResult}
                >
                  {showPassword ? "Hide" : "Show"}
                </button>
              </div>
            </label>
          </div>
        )}
        {validationError && (
          <p class="mt-4 text-sm font-medium text-rose-600" role="alert">
            {validationError}
          </p>
        )}
        <div class="mt-6 flex items-center justify-between">
          <span class="text-sm text-slate-500">
            {awaitingResult ? "Connecting to the selected network..." : "Will connect once credentials are saved."}
          </span>
          <button
            type="button"
            class={`btn-secondary${awaitingResult ? " opacity-60" : ""}`}
            onClick={() => {
              if (!awaitingResult) {
                onClose();
              }
            }}
            disabled={awaitingResult}
          >
            {awaitingResult ? "Please wait..." : "Cancel"}
          </button>
        </div>
        <button
          type="button"
          class="btn-primary mt-6 w-full disabled:bg-sky-300 disabled:cursor-not-allowed disabled:opacity-80"
          onClick={onConnect}
          disabled={loading || awaitingResult}
        >
          {loading ? "Saving..." : awaitingResult ? "Connecting..." : "Connect"}
        </button>
        {notice && (
          <div
            class={`mt-6 rounded-2xl border p-4 text-sm ${
              notice.type === "error"
                ? "border-rose-200 bg-rose-50 text-rose-700"
                : "border-sky-200 bg-sky-50 text-sky-700"
            }`}
            role={notice.type === "error" ? "alert" : "status"}
            aria-live="polite"
          >
            {notice.message}
          </div>
        )}
      </div>
    </div>
  );
};
