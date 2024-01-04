/*
 * Copyright (c) 2023 - 2024 the ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

import { html, PropertyValueMap, LitElement, type TemplateResult } from 'lit';
import { customElement, property } from 'lit/decorators.js';

// @ts-ignore: WASM Glue code doesn't have type & Only available on build progress
import Module from '../dist/thorvg-wasm';
import { THORVG_VERSION } from './version';

type LottieJson = Map<PropertyKey, any>;
type TvgModule = any;

let _tvg: TvgModule;
let _module: any;
(async () => {  
  _module = await Module();
  _tvg = new _module.TvgLottieAnimation();
})();

// Define library version
export interface LibraryVersion {
  THORVG_VERSION: string
}

// Define file type which can be exported
export enum ExportableType {
  GIF = 'gif',
  TVG = 'tvg',
}

// Define mime type which player can load
export enum MimeType {
  JSON = 'json',
  JPG = 'jpg',
  PNG = 'png',
  SVG = 'svg',
  TVG = 'tvg',
}

// Define valid player states
export enum PlayerState {
  Destroyed = "destroyed", // Player is destroyed by `destroy()` method
  Error = "error", // An error occurred
  Loading = "loading", // Player is loading
  Paused = "paused", // Player is paused
  Playing = "playing", // Player is playing
  Stopped = "stopped",  // Player is stopped
  Frozen = "frozen", // Player is paused due to player being invisible
}

// Define play modes
export enum PlayMode {
  Bounce = "bounce",
  Normal = "normal",
}

// Define player events
export enum PlayerEvent {
  Complete = "complete",
  Destroyed = "destroyed",
  Error = "error",
  Frame = "frame",
  Freeze = "freeze",
  Load = "load",
  Loop = "loop",
  Pause = "pause",
  Play = "play",
  Ready = "ready",
  Stop = "stop",
}

const _parseLottieFromURL = async (url: string): Promise<LottieJson> => {
  if (typeof url !== "string") {
    throw new Error(`The url value must be a string`);
  }

  try {
    const srcUrl: URL = new URL(url);
    const result = await fetch(srcUrl.toString());
    const json = await result.json();

    return json;
  } catch (err) {
    throw new Error(
      `An error occurred while trying to load the Lottie file from URL`
    );
  }
}

const _parseImageFromURL = async (url: string): Promise<ArrayBuffer> => {
  const response = await fetch(url);
  return response.arrayBuffer();
}

const _parseJSON = async (data: string): Promise<string> => {
  try {
    data = JSON.parse(data);
  } catch (err) {
    const json = await _parseLottieFromURL(data as string);
    data = JSON.stringify(json);
  }

  return data;
}

const _parseSrc = async (src: string | object | ArrayBuffer, mimeType: MimeType): Promise<Uint8Array> => {
  const encoder = new TextEncoder();
  let data = src;

  switch (typeof data) {
    case 'object':
      if (data instanceof ArrayBuffer) {
        return new Uint8Array(data);
      }

      data = JSON.stringify(data);
      return encoder.encode(data);
    case 'string':
      if (mimeType === MimeType.JSON) {
        data = await _parseJSON(data);
        return encoder.encode(data);
      }

      const buffer = await _parseImageFromURL(data);
      return new Uint8Array(buffer);
    default:
      throw new Error('Invalid src type');
  }
}

const _wait = (timeToDelay: number) => {
  return new Promise((resolve) => setTimeout(resolve, timeToDelay))
};

const _observerCallback = (entries: IntersectionObserverEntry[]) => {
  const entry = entries[0];
  const target = entry.target as LottiePlayer;
  
  if (entry.isIntersecting) {
    if (target.currentState === PlayerState.Frozen) {
      target.play();
    }
  } else if (target.currentState === PlayerState.Playing) {
    target.freeze();
    target.dispatchEvent(new CustomEvent(PlayerEvent.Freeze));
  }
}

@customElement('lottie-player')
export class LottiePlayer extends LitElement {
  /**
  * LottieFiles JSON data or URL to JSON.
  * @since 1.0
  */
  @property({ type: String })
  public src?: string;

  /**
  * File mime type.
  * @since 1.0
  */
  @property({ type: MimeType })
  public mimeType: MimeType = MimeType.JSON;

  /**
   * Animation speed.
   * @since 1.0
   */
  @property({ type: Number })
  public speed: number = 1.0;

  /**
   * Autoplay animation on load.
   * @since 1.0
   */
  @property({ type: Boolean })
  public autoPlay: boolean = false;

  /**
   * Number of times to loop animation.
   * @since 1.0
   */
  @property({ type: Number })
  public count?: number;

  /**
   * Whether to loop animation.
   * @since 1.0
   */
  @property({ type: Boolean })
  public loop: boolean = false;

  /**
   * Direction of animation.
   * @since 1.0
   */
  @property({ type: Number })
  public direction: number = 1;

  /**
   * Play mode.
   * @since 1.0
   */
  @property()
  public mode: PlayMode = PlayMode.Normal;

  /**
   * Intermission
   * @since 1.0
   */
  @property()
  public intermission: number = 1;

  /**
   * total frame of current animation (readonly)
   * @since 1.0
   */
  @property({ type: Number })
  public totalFrame: number = 0;

  /**
   * current frame of current animation (readonly)
   * @since 1.0
   */
  @property({ type: Number })
  public currentFrame: number = 0;

  /**
   * Player state
   * @since 1.0
   */
  @property({ type: Number })
  public currentState: PlayerState = PlayerState.Loading;

  /**
   * original size of the animation (readonly)
   * @since 1.0
   */
  @property({ type: Float32Array })
  public get size(): Float32Array {
    return Float32Array.from(this._TVG?.size() || [0, 0]);
  }

  private _TVG?: TvgModule;
  private _canvas?: HTMLCanvasElement;
  private _imageData?: ImageData;
  private _beginTime: number = Date.now();
  private _counter: number = 1;
  private _timer?: ReturnType<typeof setInterval>;
  private _observer?: IntersectionObserver;

  constructor() {
    super();
    this._init();
  }

  private async _init(): Promise<void> {
    if (!_tvg) {
      // throw new Error('ThorVG has not loaded');
      return;
    }

    this._TVG = _tvg;
  }

  private _delayedLoad(): void {
    if (!_tvg || !this._timer) {
      return;
    }

    clearInterval(this._timer);
    this._timer = undefined;

    this._TVG = _tvg;

    if (this.src) {
      this.load(this.src, this.mimeType);
    }
  }

  protected firstUpdated(_changedProperties: PropertyValueMap<any> | Map<PropertyKey, unknown>): void {
    this._canvas = this.shadowRoot!.querySelector('#thorvg-canvas') as HTMLCanvasElement;
    this._canvas.width = this._canvas.offsetWidth;
    this._canvas.height = this._canvas.offsetHeight;

    this._observer = new IntersectionObserver(_observerCallback);
    this._observer.observe(this);

    if (this.src) {
      if (this._TVG) {
        this.load(this.src, this.mimeType);
      } else {
        this._timer = setInterval(this._delayedLoad.bind(this), 100);
      }
    }
  }

  protected createRenderRoot(): HTMLElement | DocumentFragment {
    this.style.display = 'block';
    return super.createRenderRoot();
  }

  private async _animLoop(){
    if (!this._TVG) {
      return;
    }

    if (await this._update()) {
      this._render();
      window.requestAnimationFrame(this._animLoop.bind(this));
    }
  }

  private _loadBytes(data: Uint8Array, rPath: string = ''): void {
    const isLoaded = this._TVG.load(data, this.mimeType, this._canvas!.width, this._canvas!.height, rPath);
    if (!isLoaded) {
      throw new Error('Unable to load an image. Error: ', this._TVG.error());
    }

    this._render();
    this.dispatchEvent(new CustomEvent(PlayerEvent.Load));
    
    if (this.autoPlay) {
      this.play();
    }
  }

  private _flush(): void {
    const context = this._canvas!.getContext('2d');
    context!.putImageData(this._imageData!, 0, 0);
  }

  private _render(): void {
    this._TVG.resize(this._canvas!.width, this._canvas!.height);
    const isUpdated = this._TVG.update();

    if (!isUpdated) {
      return;
    }

    const buffer = this._TVG.render();
    const clampedBuffer = Uint8ClampedArray.from(buffer);
    if (clampedBuffer.length < 1) {
      return;
    }

    this._imageData = new ImageData(clampedBuffer, this._canvas!.width, this._canvas!.height);
    this._flush();
  }

  private async _update(): Promise<boolean> {
    if (this.currentState !== PlayerState.Playing) {
      return false;
    }

    const duration = this._TVG.duration();
    const currentTime = Date.now() / 1000;
    this.currentFrame = (currentTime - this._beginTime) / duration * this.totalFrame * this.speed;
    if (this.direction === -1) {
      this.currentFrame = this.totalFrame - this.currentFrame;
    }

    if (
      (this.direction === 1 && this.currentFrame >= this.totalFrame) ||
      (this.direction === -1 && this.currentFrame <= 0)
    ) {
      const totalCount = this.count ? this.mode === PlayMode.Bounce ? this.count * 2 : this.count : 0;
      if (this.loop || (totalCount && this._counter < totalCount)) {
        if (this.mode === PlayMode.Bounce) {
          this.direction = this.direction === 1 ? -1 : 1;
          this.currentFrame = this.direction === 1 ? 0 : this.totalFrame;
        }

        if (this.count) {
          this._counter += 1;
        }

        await _wait(this.intermission);
        this.play();
        return true;
      }

      this.dispatchEvent(new CustomEvent(PlayerEvent.Complete));
      this.currentState = PlayerState.Stopped;
    }

    this.dispatchEvent(new CustomEvent(PlayerEvent.Frame, {
      detail: {
        frame: this.currentFrame,
      },
    }));
    return this._TVG.frame(this.currentFrame);
  }

  private _frame(curFrame: number): void {
    this.pause();
    this.currentFrame = curFrame;
    this._TVG.frame(curFrame);
  }

  /**
   * Configure and load
   * @since 1.0
   */
  public async load(src: string | object, mimeType: MimeType = MimeType.JSON): Promise<void> {
    try {
      const bytes = await _parseSrc(src, mimeType);
      this.dispatchEvent(new CustomEvent(PlayerEvent.Ready));

      this.mimeType = mimeType;
      this._loadBytes(bytes);
    } catch (err) {
      this.currentState = PlayerState.Error;
      this.dispatchEvent(new CustomEvent(PlayerEvent.Error));
    }
  }

  /**
   * Start playing animation.
   * @since 1.0
   */
  public play(): void {
    if (this.mimeType !== MimeType.JSON) {
      return;
    }

    this.totalFrame = this._TVG.totalFrame();
    if (this.totalFrame < 1) {
      return;
    }

    this._beginTime = Date.now() / 1000;
    if (this.currentState == PlayerState.Playing) {
      return;
    }

    this.currentState = PlayerState.Playing;
    window.requestAnimationFrame(this._animLoop.bind(this));
  }

  /**
   * Pause animation.
   * @since 1.0
   */
  public pause(): void {
    this.currentState = PlayerState.Paused;
    this.dispatchEvent(new CustomEvent(PlayerEvent.Pause));
  }

  /**
   * Stop animation.
   * @since 1.0
   */
  public stop(): void {
    this.currentState = PlayerState.Stopped;
    this.currentFrame = 0;
    this._counter = 1;
    this._TVG.frame(0);

    this.dispatchEvent(new CustomEvent(PlayerEvent.Stop));
  }

  /**
   * Freeze animation.
   * @since 1.0
   */
  public freeze(): void {
    this.currentState = PlayerState.Frozen;
    this.dispatchEvent(new CustomEvent(PlayerEvent.Freeze));
  }

  /**
   * Seek to a given frame
   * @param frame Frame number to move
   * @since 1.0
   */
  public async seek(frame: number): Promise<void> {
    this._frame(frame);
    await this._update();
    this._render();
  }

  /**
   * Adjust the canvas size.
   * @param width The width to resize
   * @param height The height to resize
   * @since 1.0
   */
  public resize(width: number, height: number) {
    this._canvas!.width = width;
    this._canvas!.height = height;

    if (this.currentState !== PlayerState.Playing) {
      this._render();
    }
  }

  /**
   * Destroy animation and lottie-player element.
   * @since 1.0
   */
  public destroy(): void {
    if (!this._TVG) {
      return;
    }

    this._TVG = null;
    this.currentState = PlayerState.Destroyed;

    if (this._observer) {
      this._observer.disconnect();
      this._observer = undefined;
    }
    
    this.dispatchEvent(new CustomEvent(PlayerEvent.Destroyed));
    this.remove();
  }

  /**
   * Sets the repeating of the animation.
   * @param value Whether to enable repeating. Boolean true enables repeating.
   * @since 1.0
   */
  public setLooping(value: boolean): void {
    if (!this._TVG) {
      return;
    }

    this.loop = value;
  }

  /**
   * Animation play direction.
   * @param value Direction values. (1: forward, -1: backward)
   * @since 1.0
   */
  public setDirection(value: number): void {
    if (!this._TVG) {
      return;
    }

    this.direction = value;
  }

  /**
   * Set animation play speed.
   * @param value Playback speed. (any positive number)
   * @since 1.0
   */
  public setSpeed(value: number): void {
    if (!this._TVG) {
      return;
    }

    this.speed = value;
  }

  /**
   * Set a background color. (default: 0x00000000)
   * @param value Hex(#fff) or string(red) of background color
   * @since 1.0
   */
  public setBgColor(value: string): void {
    if (!this._TVG) {
      return;
    }

    this._canvas!.style.backgroundColor = value;
  }

  /**
   * Save current animation to other file type
   * @param target File type to export
   * @since 1.0
   */
  public async save(target: ExportableType): Promise<void> {
    if (!this._TVG || !this.src) {
      return;
    }

    const isExported = this._TVG.save(target);
    if (!isExported) {
      throw new Error('Unable to save. Error: ', this._TVG.error());
    }

    const fileName = `output.${target}`;
    const data = _module.FS.readFile(fileName);

    if (target === ExportableType.GIF && data.length < 6) {
      throw new Error(
        `Unable to save the GIF data. The generated file size is invalid.`
      );
    }

    if (target === ExportableType.TVG && data.length < 33) {
      throw new Error(
        `Unable to save the TVG data. The generated file size is invalid.`
      );
    }

    const blob = new Blob([data], {type: 'application/octet-stream'});
    const link = document.createElement("a");
    link.setAttribute('href', URL.createObjectURL(blob));
    link.setAttribute('download', fileName);
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
  }

  /**
   * Return thorvg version
   * @since 1.0
   */
  public getVersion(): LibraryVersion {
    return {
      THORVG_VERSION,
    };
  }

  public render(): TemplateResult {
    return html`
      <canvas id="thorvg-canvas" style="width: 100%; height: 100%;" />
    `;
  }
}
