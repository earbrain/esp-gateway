import type { FunctionalComponent } from "preact";
import { DeviceLogsCard } from "../../components/device/DeviceLogsCard";

type DeviceLogsPageProps = {
  path?: string;
};

export const DeviceLogsPage: FunctionalComponent<DeviceLogsPageProps> = () => {
  return (
    <section class="space-y-6">
      <DeviceLogsCard />
    </section>
  );
};
