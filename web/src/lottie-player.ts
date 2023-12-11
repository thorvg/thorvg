// @ts-ignore: WASM Glue code doesn't have type & Only available on build progress
import Module from '../dist/thorvg-wasm';
import { ExportableType, LibraryVersion, LottiePlayerModel, PlayerEvent, PlayerState } from './lottie-player.model';

let _tvg: any;
(async () => {  
  const module = await Module();
  _tvg = new module.TvgLottieAnimation();
})();

@customElement('lottie-player')
export class LottiePlayer extends LitElement {
  public render (): TemplateResult {
    return html`
      <h1>Hello ThorVG!</h1>
    `
  }
}
