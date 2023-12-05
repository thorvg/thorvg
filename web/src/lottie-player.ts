import { LitElement, type TemplateResult, html } from 'lit'
import { customElement } from 'lit/decorators.js'

@customElement('lottie-player')
export class LottiePlayer extends LitElement {
  public render (): TemplateResult {
    return html`
      <h1>Hello ThorVG!</h1>
    `
  }
}
