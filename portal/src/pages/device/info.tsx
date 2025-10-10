import type { FunctionalComponent } from "preact";
import { DeviceOverviewCard } from "../../components/device/DeviceOverviewCard";

type DeviceInfoPageProps = {
  path?: string;
};

export const DeviceInfoPage: FunctionalComponent<DeviceInfoPageProps> = () => {
  return (
    <section class="space-y-6">
      <DeviceOverviewCard />
    </section>
  );
};
