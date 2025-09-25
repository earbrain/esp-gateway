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
        <h2 class="section-title">Device Overview</h2>
        <p class="muted text-sm">Check device details</p>
      </div>
      <button
        type="button"
        class="btn-primary w-full justify-center"
        onClick={() => onNavigate("/device")}
      >
        View device details
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
