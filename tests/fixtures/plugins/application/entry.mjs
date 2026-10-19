import { awaitOperation } from 'echo:plugin-sdk';

export function activate(ctx) {
  return awaitOperation(ctx.io.writeValue('lifecycle', 'activated'))
    .then(() => awaitOperation(ctx.io.readValue('lifecycle')))
    .then(value => {
      if (value !== 'activated') throw new Error('Storage did not round trip');
      return awaitOperation(ctx.player.setVolume(40));
    })
    .then(() => {
      if (ctx.player.volume !== 40) throw new Error('Playback service was not connected');
      ctx.ui.registerTitlebarAction('pause', 'Plugin integration pause', 'music', () => ctx.player.pause());
      console.info('Plugin application fixture activated');
    });
}

export function deactivate(ctx) {
  if (ctx.active) throw new Error('Deactivate received an active scope');
  console.info('Plugin application fixture deactivated');
}
