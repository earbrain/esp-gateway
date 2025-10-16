import Router, { route, type RouterOnChangeArgs } from "preact-router";
import { useEffect, useRef, useState } from "preact/hooks";

import { BreadcrumbSection, type PageMeta } from "./components/BreadcrumbSection";
import { ConnectionLostDialog } from "./components/ConnectionLostDialog";
import { LanguageSelector } from "./components/LanguageSelector";
import { useConnectionMonitor } from "./hooks/useConnectionMonitor";
import { useTranslation } from "./i18n/context";
import type { TranslationKey } from "./i18n/translations";
import { HomePage } from "./pages/home";
import { DeviceInfoPage } from "./pages/device/info";
import { DeviceMetricsPage } from "./pages/device/metrics";
import { DeviceLogsPage } from "./pages/device/logs";
import { DeviceMdnsPage } from "./pages/device/mdns";
import { WifiPage } from "./pages/wifi";

type NavItem = {
  path: string;
  labelKey: TranslationKey;
};

const navItems: NavItem[] = [
  { path: "/", labelKey: "nav.home" },
  { path: "/device/info", labelKey: "nav.deviceInfo" },
  { path: "/device/metrics", labelKey: "nav.deviceMetrics" },
  { path: "/device/logs", labelKey: "nav.deviceLogs" },
  { path: "/device/mdns", labelKey: "nav.deviceMdns" },
  { path: "/wifi", labelKey: "nav.wifi" },
];

const normalizeUrl = (url: string) => {
  if (!url) {
    return "/";
  }
  const [path] = url.split("?");
  if (path.length > 1 && path.endsWith("/")) {
    return path.slice(0, -1);
  }
  return path || "/";
};

const pageMetaMap: Record<string, PageMeta> = {
  "/device/info": {
    showHeader: true,
    titleKey: "page.deviceInfo.title",
    descriptionKey: "page.deviceInfo.description",
    breadcrumbs: [
      { labelKey: "nav.home", path: "/" },
      { labelKey: "page.deviceInfo.breadcrumb" },
    ],
  },
  "/device/metrics": {
    showHeader: true,
    titleKey: "page.metrics.title",
    descriptionKey: "page.metrics.description",
    breadcrumbs: [
      { labelKey: "nav.home", path: "/" },
      { labelKey: "page.metrics.breadcrumb" },
    ],
  },
  "/device/logs": {
    showHeader: true,
    titleKey: "page.logs.title",
    descriptionKey: "page.logs.description",
    breadcrumbs: [
      { labelKey: "nav.home", path: "/" },
      { labelKey: "page.logs.breadcrumb" },
    ],
  },
  "/device/mdns": {
    showHeader: true,
    titleKey: "page.mdns.title",
    descriptionKey: "page.mdns.description",
    breadcrumbs: [
      { labelKey: "nav.home", path: "/" },
      { labelKey: "page.mdns.breadcrumb" },
    ],
  },
  "/wifi": {
    showHeader: true,
    titleKey: "page.wifi.title",
    descriptionKey: "page.wifi.description",
    breadcrumbs: [
      { labelKey: "nav.home", path: "/" },
      { labelKey: "page.wifi.breadcrumb" },
    ],
  },
};

const defaultMeta: PageMeta = {
  showHeader: false,
  titleKey: "app.title",
  descriptionKey: "app.title",
  breadcrumbs: [],
};

export function App() {
  const t = useTranslation();
  const [currentUrl, setCurrentUrl] = useState<string>(() =>
    typeof window !== "undefined" ? window.location.pathname : "/",
  );
  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement | null>(null);
  const [userDismissed, setUserDismissed] = useState(false);

  // Check for mock mode via query parameter
  const mockMode = typeof window !== "undefined"
    ? new URLSearchParams(window.location.search).has("mock")
    : false;

  // Monitor connection to gateway
  const { status, consecutiveFailures, shouldShowDialog } = useConnectionMonitor({
    endpoint: "/health",
    intervalMs: 10000,
    failureThreshold: 2, // Show dialog after 2 consecutive failures (20 seconds)
    enabled: true,
    mockMode,
  });

  // Reset userDismissed when connection is restored
  useEffect(() => {
    if (status === "connected") {
      setUserDismissed(false);
    }
  }, [status]);

  useEffect(() => {
    const handleClick = (event: MouseEvent) => {
      if (
        menuRef.current &&
        event.target instanceof Node &&
        !menuRef.current.contains(event.target)
      ) {
        setMenuOpen(false);
      }
    };

    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        setMenuOpen(false);
      }
    };

    document.addEventListener("click", handleClick);
    document.addEventListener("keydown", handleKeyDown);

    return () => {
      document.removeEventListener("click", handleClick);
      document.removeEventListener("keydown", handleKeyDown);
    };
  }, []);

  const handleRouteChange = (event: RouterOnChangeArgs) => {
    setCurrentUrl(normalizeUrl(event.url ?? "/"));
    setMenuOpen(false);
  };

  const navigate = (path: string) => {
    route(path);
    setMenuOpen(false);
  };

  const normalizedUrl = normalizeUrl(currentUrl);
  const pageMeta = pageMetaMap[normalizedUrl] ?? defaultMeta;

  return (
    <div class="min-h-screen bg-slate-100 text-slate-900">
      <ConnectionLostDialog
        isOpen={shouldShowDialog && !userDismissed}
        onDismiss={() => setUserDismissed(true)}
      />
      <header class="bg-white/95 shadow-sm">
        <div class="mx-auto flex w-full max-w-4xl items-center justify-between gap-4 px-4 py-4">
          <h1 class="text-xl font-semibold text-slate-900">{t("app.title")}</h1>
          <div class="flex items-center gap-3">
            <LanguageSelector />
            <nav class="relative" ref={menuRef}>
              <button
                type="button"
                class="menu-toggle"
                aria-haspopup="true"
                aria-expanded={menuOpen}
                aria-label={t("app.menu.toggle")}
                onClick={(event) => {
                  event.stopPropagation();
                  setMenuOpen((prev) => !prev);
                }}
              >
                <span class="menu-icon" aria-hidden="true">
                  <span></span>
                  <span></span>
                  <span></span>
                </span>
              </button>
              {menuOpen && (
                <div class="menu-sheet" role="menu">
                  {navItems.map((item) => (
                    <button
                      key={item.path}
                      type="button"
                      role="menuitem"
                      class={`menu-item${normalizedUrl === item.path ? " menu-item-active" : ""}`}
                      onClick={(event) => {
                        event.stopPropagation();
                        navigate(item.path);
                      }}
                    >
                      {t(item.labelKey)}
                    </button>
                  ))}
                </div>
              )}
            </nav>
          </div>
        </div>
      </header>
      <main class="mx-auto w-full max-w-4xl space-y-6 px-4 py-8">
        <BreadcrumbSection meta={pageMeta} onNavigate={navigate} />
        <Router onChange={handleRouteChange}>
          <HomePage path="/" onNavigate={navigate} />
          <DeviceInfoPage path="/device/info" />
          <DeviceMetricsPage path="/device/metrics" />
          <DeviceLogsPage path="/device/logs" />
          <DeviceMdnsPage path="/device/mdns" />
          <WifiPage path="/wifi" />
          <HomePage default onNavigate={navigate} />
        </Router>
      </main>
    </div>
  );
}
