import Router, { route, type RouterOnChangeArgs } from "preact-router";
import { useEffect, useRef, useState } from "preact/hooks";

import { BreadcrumbSection, type PageMeta } from "./components/BreadcrumbSection";
import { HomePage } from "./pages/home";
import { DevicePage } from "./pages/device";
import { WifiPage } from "./pages/wifi";

type NavItem = {
  path: string;
  label: string;
};

const navItems: NavItem[] = [
  { path: "/", label: "Home" },
  { path: "/device", label: "Device" },
  { path: "/wifi", label: "Wi-Fi" },
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
  "/device": {
    showHeader: true,
    title: "Device",
    description: "View device details",
    breadcrumbs: [{ label: "Home", path: "/" }, { label: "Device" }],
  },
  "/wifi": {
    showHeader: true,
    title: "Wi-Fi",
    description: "Update Wi-Fi credentials",
    breadcrumbs: [{ label: "Home", path: "/" }, { label: "Wi-Fi" }],
  },
};

const defaultMeta: PageMeta = {
  showHeader: false,
  title: "",
  description: "",
  breadcrumbs: [],
};

export function App() {
  const [currentUrl, setCurrentUrl] = useState<string>(() =>
    typeof window !== "undefined" ? window.location.pathname : "/",
  );
  const [menuOpen, setMenuOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement | null>(null);

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
      <header class="bg-white/95 shadow-sm">
        <div class="mx-auto flex w-full max-w-4xl items-center justify-between px-4 py-4">
          <h1 class="text-xl font-semibold text-slate-900">ESP Gateway</h1>
          <nav class="relative" ref={menuRef}>
            <button
              type="button"
              class="menu-toggle"
              aria-haspopup="true"
              aria-expanded={menuOpen}
              aria-label="Open navigation"
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
                    class={`menu-item${
                      normalizedUrl === item.path ? " menu-item-active" : ""
                    }`}
                    onClick={(event) => {
                      event.stopPropagation();
                      navigate(item.path);
                    }}
                  >
                    {item.label}
                  </button>
                ))}
              </div>
            )}
          </nav>
        </div>
      </header>
      <main class="mx-auto w-full max-w-4xl space-y-6 px-4 py-8">
        <BreadcrumbSection meta={pageMeta} onNavigate={navigate} />
        <Router onChange={handleRouteChange}>
          <HomePage path="/" onNavigate={navigate} />
          <DevicePage path="/device" />
          <WifiPage path="/wifi" />
          <HomePage default onNavigate={navigate} />
        </Router>
      </main>
    </div>
  );
}
