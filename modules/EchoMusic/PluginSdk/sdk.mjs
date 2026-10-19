// This adapter consumes the typed operation; it owns no business state.
export function awaitOperation(operation) {
  return new Promise((resolve, reject) => {
    if (!operation) {
      reject(new Error('Unavailable operation'));
      return;
    }
    const finish = () => {
      if (operation.state < 2) return;
      operation.completed.disconnect(finish);
      if (operation.state === 2) resolve(operation.result);
      else {
        const error = new Error(operation.errorMessage);
        error.code = operation.errorCode;
        reject(error);
      }
    };
    operation.completed.connect(finish);
    finish();
  });
}

// Runtime stays dynamically typed; the author SDK maps each service method to
// its own JSON argument/result contract without passing provider Qt objects.
export function pluginService(context, pluginId, serviceId, versionRange) {
  return Object.freeze({
    call(method, args) {
      return awaitOperation(
        context.plugins.call(pluginId, serviceId, versionRange, method, args || {}),
      );
    },
  });
}
