import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";
import type { ModulePage } from "../types.ts";

const ElasticClient: ModulePage = {
  id: "ElasticClient",
  sections: { "/settings/elastic/client": HOSTNAME_WIDGET },
};

export default ElasticClient;
