import { useCallback, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { Toast } from "../components/Toast";
import { WifiStatusCard } from "../components/WifiStatusCard";
import { WifiNetworkList } from "../components/wifi/WifiNetworkList";

type WifiPageProps = {
  path?: string;
};

export const WifiPage: FunctionalComponent<WifiPageProps> = () => {
  const [toast, setToast] = useState<{ message: string; type: "success" | "error" } | null>(null);
  const [statusRefresh, setStatusRefresh] = useState(0);

  const handleError = useCallback((message: string) => {
    setToast({ type: "error", message });
  }, []);

  const handleConnectionComplete = useCallback(() => {
    setStatusRefresh((prev) => prev + 1);
  }, []);

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
        <WifiStatusCard refresh={statusRefresh} />
        <WifiNetworkList onError={handleError} onConnectionComplete={handleConnectionComplete} />
      </section>
    </>
  );
};
