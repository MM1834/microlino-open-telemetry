(function () {
  const providers = new Map();

  window.MOTDataProviders = {
    register(name, factory) {
      if (!name || typeof factory !== 'function') {
        throw new Error('Invalid MOT data provider registration');
      }
      providers.set(name, factory);
    },

    create(name, options) {
      const factory = providers.get(name);
      if (!factory) {
        throw new Error(`Unknown MOT data provider: ${name}`);
      }
      return factory(options || {});
    },

    list() {
      return Array.from(providers.keys());
    }
  };
})();
