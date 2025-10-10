import type { FunctionalComponent } from "preact";
import { SystemMetricsCard } from "../../components/device/SystemMetricsCard";

type DeviceMetricsPageProps = {
  path?: string;
};

export const DeviceMetricsPage: FunctionalComponent<DeviceMetricsPageProps> = () => {
  return (
    <section class="space-y-6">
      <SystemMetricsCard />
    </section>
  );
};
