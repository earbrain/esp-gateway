import { useState, useCallback, useEffect, useRef } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../../hooks/useApi";
import type { WifiNetwork, WifiResponse } from "../../types/wifi";
import { WifiSignalIndicator } from "./WifiSignalIndicator";
import { SecurityBadge } from "./SecurityBadge";
import { WifiConnectModal } from "./WifiConnectModal";

type WifiNetworkCardProps = {
  network: WifiNetwork;
  onConnectionStatusChange?: (networkId: string, connected: boolean) => void;
  onPasswordSaved?: (networkId: string, password: string) => void;
};

const isHexKey = (value: string) => /^[0-9a-fA-F]+$/.test(value);

export const WifiNetworkCard: FunctionalComponent<WifiNetworkCardProps> = ({
  network,
  onConnectionStatusChange,
  onPasswordSaved,
}) => {
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [password, setPassword] = useState(network.lastPassphrase || "");
  const [showPassword, setShowPassword] = useState(false);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [isPending, setIsPending] = useState(false);
  const [isFailed, setIsFailed] = useState(false);
  const [isAwaitingResult, setIsAwaitingResult] = useState(false);
  const [notice, setNotice] = useState<{ type: "info" | "error"; message: string } | null>(null);

  const { execute, loading, error, reset } = useApi<WifiResponse>("/api/v1/wifi/credentials", {
    method: "POST",
    headers: {
      "Content-Type": "application/json",
    },
  });

  const lastError = useRef<string | null>(null);

  useEffect(() => {
    if (error && error !== lastError.current) {
      setNotice({ type: "error", message: error });
      setIsAwaitingResult(false);
      setIsPending(false);
      setIsFailed(true);
      lastError.current = error;
    }
    if (!error) {
      lastError.current = null;
    }
  }, [error]);

  const handleCardClick = useCallback(() => {
    setIsModalOpen(true);
    setShowPassword(false);
    setValidationError(null);
    setIsFailed(false);
    setNotice(null);
    setIsAwaitingResult(false);
    setPassword(network.lastPassphrase || "");
  }, [network.lastPassphrase]);

  const handleModalClose = useCallback(() => {
    setIsModalOpen(false);
    setPassword("");
    setValidationError(null);
    setShowPassword(false);
    setNotice(null);
    setIsAwaitingResult(false);
    setIsPending(false);
    reset();
  }, [reset]);

  const handleConnect = useCallback(async () => {
    const trimmedPassword = password.trim();
    const requiresPassword = network.security.trim().toLowerCase() !== "open";
    const isLengthValid = trimmedPassword.length >= 8 && trimmedPassword.length <= 63;
    const isHexCandidate = trimmedPassword.length === 64 && isHexKey(trimmedPassword);

    if (requiresPassword && !trimmedPassword) {
      setValidationError("Please enter the password.");
      reset();
      return;
    }

    if (requiresPassword && !isLengthValid && !isHexCandidate) {
      setValidationError("Enter 8-63 characters or a 64-digit hex string.");
      reset();
      return;
    }

    setValidationError(null);
    setIsFailed(false);
    setIsPending(true);
    setIsAwaitingResult(true);
    setNotice({
      type: "info",
      message: `Attempting to connect to ${network.ssid}...`,
    });

    try {
      const response = await execute({
        body: JSON.stringify({
          ssid: network.ssid.trim(),
          passphrase: requiresPassword ? trimmedPassword : "",
        }),
      });

      if (response) {
        const { restart_required, sta_connect_started, sta_error } = response;

        if (onPasswordSaved && requiresPassword) {
          onPasswordSaved(network.id, trimmedPassword);
        }

        if (sta_error) {
          setNotice({
            type: "error",
            message: `Failed to connect (${sta_error}). Please review the credentials and try again.`,
          });
          setIsAwaitingResult(false);
          setIsPending(false);
          setIsFailed(true);
          return;
        }

        const shouldAwaitResult = Boolean(sta_connect_started);
        if (shouldAwaitResult) {
          setNotice({
            type: "info",
            message: `Connecting to ${network.ssid}...`,
          });
        } else if (typeof restart_required === "boolean" && restart_required) {
          setNotice({
            type: "info",
            message: "Credentials saved. Device restart is required to finish applying the changes.",
          });
        } else {
          setNotice({
            type: "info",
            message: "Credentials saved. Waiting for status update...",
          });
        }

        setIsAwaitingResult(shouldAwaitResult);

        if (!shouldAwaitResult) {
          setIsPending(false);
        }
      } else {
        setIsAwaitingResult(false);
        setIsPending(false);
        setNotice({
          type: "error",
          message: "Failed to submit credentials. Please try again.",
        });
      }
    } catch (err) {
      console.error(err);
      setIsPending(false);
      setIsAwaitingResult(false);
      setNotice({
        type: "error",
        message: "An unexpected error occurred while saving credentials. Please try again.",
      });
    }
  }, [network, password, execute, onPasswordSaved, reset]);

  useEffect(() => {
    if (network.connected && isPending) {
      setIsPending(false);
      setIsAwaitingResult(false);
      setIsFailed(false);
      setNotice(null);
      if (isModalOpen) {
        setIsModalOpen(false);
        setPassword("");
        setValidationError(null);
        setShowPassword(false);
        reset();
      }
      if (onConnectionStatusChange) {
        onConnectionStatusChange(network.id, true);
      }
    }
  }, [network.connected, isPending, isModalOpen, reset, network.id, onConnectionStatusChange]);

  const statusLabel = isPending
    ? "Connecting..."
    : isFailed
      ? "Not connected"
      : network.connected
        ? "Connected"
        : undefined;

  const highlightClass = isModalOpen || network.connected
    ? "border-sky-200 bg-sky-50"
    : "border-transparent bg-white hover:bg-slate-50";
  const pendingClass = isPending ? "ring-2 ring-amber-300" : "";

  return (
    <>
      <button
        type="button"
        class={`flex w-full items-center justify-between rounded-2xl border px-4 py-3 text-left transition focus:outline-none focus:ring-2 focus:ring-sky-200 ${highlightClass} ${pendingClass}`}
        onClick={handleCardClick}
      >
        <div class="flex flex-col gap-1">
          <span class="text-sm font-semibold text-slate-900">{network.ssid}</span>
          <div class="flex items-center gap-2 text-xs text-slate-500">
            <SecurityBadge security={network.security} />
            {statusLabel && (
              <span
                class={`flex items-center gap-1 ${isPending ? "text-amber-600" : isFailed ? "text-rose-600" : "text-sky-600"}`}
              >
                {isPending && (
                  <span
                    class="inline-block h-3.5 w-3.5 animate-spin rounded-full border border-current border-t-transparent"
                    aria-hidden="true"
                  ></span>
                )}
                {statusLabel}
              </span>
            )}
          </div>
        </div>
        <WifiSignalIndicator
          strength={network.signal}
          label={`Signal strength for ${network.ssid}: ${network.rssi} dBm`}
        />
      </button>

      <WifiConnectModal
        network={network}
        open={isModalOpen}
        password={password}
        showPassword={showPassword}
        loading={loading}
        awaitingResult={isAwaitingResult}
        notice={notice}
        validationError={validationError}
        onClose={handleModalClose}
        onConnect={handleConnect}
        onPasswordChange={setPassword}
        onToggleShowPassword={() => setShowPassword((prev) => !prev)}
      />
    </>
  );
};
