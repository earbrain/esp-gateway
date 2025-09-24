#pragma once

namespace earbrain {

class Gateway {
public:
  Gateway();
  ~Gateway();

  void start();
  void stop();

  const char *version() const { return "0.0.0"; }
};

} // namespace earbrain
