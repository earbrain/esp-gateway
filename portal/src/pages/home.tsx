import type { FunctionalComponent } from "preact";

type HomePageProps = {
  path?: string;
  onNavigate: (path: string) => void;
};

export const HomePage: FunctionalComponent<HomePageProps> = ({
  onNavigate,
}) => (
  <section class="grid gap-6 md:grid-cols-2">
    <div class="card space-y-4">
      <div>
        <h2 class="section-title">Device Information</h2>
        <p class="muted text-sm">View device model and gateway version</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/device/info")}
      >
        View device info
      </button>
    </div>
    <div class="card space-y-4">
      <div>
        <h2 class="section-title">System Metrics</h2>
        <p class="muted text-sm">Monitor memory and system health</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/device/metrics")}
      >
        View metrics
      </button>
    </div>
    <div class="card space-y-4">
      <div>
        <h2 class="section-title">Device Logs</h2>
        <p class="muted text-sm">View real-time device logs</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/device/logs")}
      >
        View logs
      </button>
    </div>
    <div class="card space-y-4">
      <div>
        <h2 class="section-title">mDNS Configuration</h2>
        <p class="muted text-sm">Check mDNS settings and status</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/device/mdns")}
      >
        View mDNS
      </button>
    </div>
    <div class="card space-y-4">
      <div>
        <h2 class="section-title">Wi-Fi Configuration</h2>
        <p class="muted text-sm">Update Wi-Fi credentials</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/wifi")}
      >
        Configure Wi-Fi
      </button>
    </div>
  </section>
);
