import type { FunctionalComponent } from "preact";
import { useTranslation } from "../i18n/context";

type HomePageProps = {
  path?: string;
  onNavigate: (path: string) => void;
};

export const HomePage: FunctionalComponent<HomePageProps> = ({ onNavigate }) => {
  const t = useTranslation();

  return (
    <section class="grid gap-6 md:grid-cols-2">
      <div class="card space-y-4">
        <div>
          <h2 class="section-title">{t("home.deviceInfo.title")}</h2>
          <p class="muted text-sm">{t("home.deviceInfo.description")}</p>
        </div>
        <button
          type="button"
          class="btn-primary w-full justify-center"
          onClick={() => onNavigate("/device/info")}
        >
          {t("home.deviceInfo.action")}
        </button>
      </div>
      <div class="card space-y-4">
        <div>
          <h2 class="section-title">{t("home.metrics.title")}</h2>
          <p class="muted text-sm">{t("home.metrics.description")}</p>
        </div>
        <button
          type="button"
          class="btn-primary w-full justify-center"
          onClick={() => onNavigate("/device/metrics")}
        >
          {t("home.metrics.action")}
        </button>
      </div>
      <div class="card space-y-4">
        <div>
          <h2 class="section-title">{t("home.logs.title")}</h2>
          <p class="muted text-sm">{t("home.logs.description")}</p>
        </div>
        <button
          type="button"
          class="btn-primary w-full justify-center"
          onClick={() => onNavigate("/device/logs")}
        >
          {t("home.logs.action")}
        </button>
      </div>
      <div class="card space-y-4">
        <div>
          <h2 class="section-title">{t("home.mdns.title")}</h2>
          <p class="muted text-sm">{t("home.mdns.description")}</p>
        </div>
        <button
          type="button"
          class="btn-primary w-full justify-center"
          onClick={() => onNavigate("/device/mdns")}
        >
          {t("home.mdns.action")}
        </button>
      </div>
      <div class="card space-y-4">
        <div>
          <h2 class="section-title">{t("home.wifi.title")}</h2>
          <p class="muted text-sm">{t("home.wifi.description")}</p>
        </div>
        <button
          type="button"
          class="btn-primary w-full justify-center"
          onClick={() => onNavigate("/wifi")}
        >
          {t("home.wifi.action")}
        </button>
      </div>
    </section>
  );
};
