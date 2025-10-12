import { useEffect } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useTranslation } from "../i18n/context";

export type ToastProps = {
  message: string;
  type: "success" | "error";
  onClose: () => void;
  durationMs?: number;
};

export const Toast: FunctionalComponent<ToastProps> = ({
  message,
  type,
  onClose,
  durationMs = 3000,
}) => {
  const t = useTranslation();
  useEffect(() => {
    if (!message) {
      return;
    }
    const timer = window.setTimeout(() => {
      onClose();
    }, durationMs);
    return () => window.clearTimeout(timer);
  }, [message, durationMs, onClose]);

  const baseClasses =
    "flex items-center gap-3 rounded-full border px-4 py-2 shadow-lg backdrop-blur-md";
  const paletteClasses =
    type === "success"
      ? "bg-emerald-50/90 border-emerald-200 text-emerald-900"
      : "bg-rose-50/90 border-rose-200 text-rose-900";

  return (
    <div class={`${baseClasses} ${paletteClasses}`} role="alert">
      <span class="text-sm font-medium">{message}</span>
      <button
        type="button"
        class="rounded-full px-2 text-lg font-bold text-inherit transition hover:bg-white/40"
        onClick={onClose}
        aria-label={t("common.dismiss")}
      >
        ×
      </button>
    </div>
  );
};
