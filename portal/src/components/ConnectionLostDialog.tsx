import { useEffect } from "preact/hooks";
import { useTranslation } from "../i18n/context";

type ConnectionLostDialogProps = {
  isOpen: boolean;
  onDismiss?: () => void;
};

export function ConnectionLostDialog({
  isOpen,
  onDismiss,
}: ConnectionLostDialogProps) {
  const t = useTranslation();

  useEffect(() => {
    if (!isOpen) {
      return () => undefined;
    }

    const handleEscape = (event: KeyboardEvent) => {
      if (event.key === "Escape" && onDismiss) {
        onDismiss();
      }
    };

    document.addEventListener("keydown", handleEscape);
    return () => {
      document.removeEventListener("keydown", handleEscape);
    };
  }, [isOpen, onDismiss]);

  if (!isOpen) {
    return null;
  }

  return (
    <div
      class="fixed inset-0 z-50 flex items-center justify-center p-4"
      role="dialog"
      aria-modal="true"
      aria-labelledby="connection-lost-title"
      onClick={(e) => {
        if (e.target === e.currentTarget && onDismiss) {
          onDismiss();
        }
      }}
    >
      <div
        class="absolute inset-0 bg-black/50 backdrop-blur-sm"
        aria-hidden="true"
        onClick={(e) => {
          if (onDismiss) {
            onDismiss();
          }
        }}
      />
      <div class="relative w-full max-w-md rounded-lg bg-white p-6 shadow-xl">
        <div class="flex items-start gap-4">
          <div class="flex-shrink-0">
            <svg
              class="h-6 w-6 text-rose-600"
              fill="none"
              viewBox="0 0 24 24"
              stroke="currentColor"
              aria-hidden="true"
            >
              <path
                stroke-linecap="round"
                stroke-linejoin="round"
                stroke-width="2"
                d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z"
              />
            </svg>
          </div>
          <div class="flex-1">
            <h2
              id="connection-lost-title"
              class="text-lg font-semibold text-slate-900"
            >
              {t("connection.lost.title")}
            </h2>
            <p class="mt-2 text-sm text-slate-700">
              {t("connection.lost.description")}
            </p>
            <div class="mt-4 flex items-center gap-2 text-sm text-slate-600">
              <div class="flex h-4 w-4 items-center justify-center">
                <div class="h-3 w-3 animate-spin rounded-full border-2 border-slate-300 border-t-slate-600" />
              </div>
              <span>
                {t("connection.lost.reconnecting")}
              </span>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
}
