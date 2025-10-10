import type { FunctionalComponent } from "preact";
import { MdnsCard } from "../../components/device/MdnsCard";

type DeviceMdnsPageProps = {
  path?: string;
};

export const DeviceMdnsPage: FunctionalComponent<DeviceMdnsPageProps> = () => {
  return (
    <section class="space-y-6">
      <MdnsCard />
    </section>
  );
};
