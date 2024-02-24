# @thorvg/lottie-player

A Lottie Player which uses [ThorVG](https://github.com/thorvg/thorvg) as a renderer, provides a web component for easily embedding and playing [Lottie](https://airbnb.io/lottie) animations.

# Installation

- Import from CDN
```html
<script src="https://unpkg.com/@thorvg/lottie-player@latest/dist/lottie-player.js"></script>
```

- Install from [NPM](https://www.npmjs.com/package/@thorvg/lottie-player)
```sh
npm install @thorvg/lottie-player
```

# Usage
### With HTML (Basic Usage)

Once you import from CDN, you can access `<lottie-player/>`

```html
<lottie-player 
  autoPlay 
  loop
  mode="normal"
  src="https://lottie.host/6d7dd6e2-ab92-4e98-826a-2f8430768886/NGnHQ6brWA.json"
  style="width: 500px; height: 500px;"
>
</lottie-player>
```

### With NPM (JS/TS)

Import the library and please follow Basic Usage, you can use library on any NPM-based project such as React, Vue and Svelte.

```ts
import '@thorvg/lottie-player';
```

### With ReactJS + TypeScript

Add `declarations.d.ts` on the root of project and make sure following declaration.

```js
declare namespace JSX {
  interface IntrinsicElements {
    "lottie-player": any;
  }
}
```

Then you will be able to use this as same as above
```js
import '@thorvg/lottie-player';

<lottie-player 
  autoPlay 
  loop
  mode="normal"
  src="https://lottie.host/6d7dd6e2-ab92-4e98-826a-2f8430768886/NGnHQ6brWA.json"
  style="width: 500px; height: 500px;"
>
</lottie-player>
```

### With SSR Framework
We should be careful when using on SSR frameworks such as NextJS, NuxtJS and Svelte, as it means the library must to be rendered on browser/client side.

- NextJS
```ts
import { useEffect } from "react";

export default function Home() {
  // ...

  useEffect(() => {
    import("@thorvg/lottie-player");
  });

  // ...
}
```

- NuxtJS
```html
<template>
  {/* ... */}
</template>

<script>
  export default {
    mounted() {
      import("@thorvg/lottie-player");
    }
  }
</script>
```

- Svelte
```html
<script>
  import { onMount } from 'svelte';

  onMount(async () => {
    await import('@thorvg/lottie-player');
  });
</script>
```

# API
## Properties

| Property | Description | Type | Default Value | Required |
| --- | --- | --- | --- | --- |
| src | A graphic resource to play. It could be an internal/external URL or JSON string for Lottie. | string | undefined | Y |
| speed | Animation speed (for Lottie) | number | 1 | N |
| autoplay | When set to true, automatically plays the animation on loading it (for Lottie) | boolean | false | N |
| count | Number of times to loop the animation | number | undefined | N |
| loop | When set to true, loops the animation. The count property defines the number of times to loop the animation. Setting the count property to 0 and setting the loop to true, loops the animation indefinitely. | boolean | false | N |
| direction | Direction of the animation. Set to 1 to play the animation forward or set to -1 to play it backward. | number (1 or -1) | 1 | N |
| mode | Play mode. Setting the mode to PlayMode.Bounce plays the animation in an indefinite cycle, forwards and then backwards. | PlayMode | PlayMode.Normal | N |
| intermission | Duration (in milliseconds) to pause before playing each cycle in a looped animation. Set this parameter to 0 (no pause) or any positive number. | number | 1 | N |

## Events

You can adapt the event with the following code example

```jsx
const player = document.querySelector('lottie-player');

player.addEventListener('load', () => {
  // TODO: implements
});
```

| Name | Description |
| --- | --- |
| load | A graphic resource is loaded |
| error | An animation data can’t be parsed. |
| ready | Animation data is loaded and player is ready |
| play | Animation starts playing |
| pause | Animation is paused |
| stop | Animation is stopped |
| freeze | Animation is paused due to player being invisible |
| loop | An animation loop is completed |
| complete | Animation is complete (all loops completed) |
| frame | A new frame is entered |


## Methods

**Method** : `load(src: string | object)`

**Purpose** : Load and play Lottie animation

**Parameters**
| Name | Type | Description
| --- | --- | --- |
| src | `string` or `object` | URL, or a JSON string or object representing a Lottie animation to play.

**Return Type** : `Promise<void>`

---

**Method** : `play()`

**Purpose** : Play loaded animation

**Return Type** : `void`

---

**Method** : `pause()`

**Purpose** : Pause playing animation

**Return Type** : `void`

---

**Method** : `stop()`

**Purpose** : Stop current animation

**Return Type** : `void`

---

**Method** : `seek(frame: number)`

**Purpose** : Move to a given frame

**Parameters**
| Name | Type | Description
| --- | --- | --- |
| src | `number` | The frame number to move, shouldn't be less than 0 and greater than `totalFrame`

**Return Type** : `void`

> [!TIP]
> You can easily check total frame of animation, use `player.totalFrame`

---

**Method** : `destory()`

**Purpose** : Destroy animation and lottie-player element

**Return Type** : `void`

---

**Method** : `setLooping(value: boolean)`

**Purpose** : Enable animation loop

**Parameters**
| Name | Type | Description
| --- | --- | --- |
| value | `boolean` | `true` enables looping, while `false` disables looping.

**Return Type** : `void`

---

**Method** : `setDirection(value: number)`

**Purpose** : Set animation direction


**Parameters**
| Name | Type | Description
| --- | --- | --- |
| value | `number` | Direction values. `1` to play the animation forward, `-1` to play it backward.

**Return Type** : `void`

---

**Method** : `setSpeed(value: number)`

**Purpose** : Set speed of animation


**Parameters**
| Name | Type | Description
| --- | --- | --- |
| value | `number` | Playback speed. The value must be any positive integer.

**Return Type** : `void`

---

**Method** : `setBgColor(value: string)`

**Purpose** : Set a backgroud color

**Parameters**
| Name | Type | Description
| --- | --- | --- |
| value | `string` | Color values. Hex(#fff) or string('red') to set background color

**Return Type** : `void`

---

**Method** : `save(target: ExportableType)`

**Purpose** : Export current animation to other file type

**Parameters**
| Name | Type | Description
| --- | --- | --- |
| value | `ExportableType` | File type to export

**Return Type** : `void`


---

**Method** : `getVersion()`

**Purpose** : Return current ThorVG version

**Return Type** : `LibraryVersion`

# Examples

Please check these examples in various environments.

- [VanillaJS Usage](https://codesandbox.io/p/sandbox/thorvg-lottieplayer-vanillajs-t737qm)
- [React Usage](https://codesandbox.io/p/devbox/thorvg-lottieplayer-react-hkkrq3)
- [VueJS Usage](https://codesandbox.io/p/devbox/thorvg-lottieplayer-vue-758h3l)
- [Svelte Usage](https://codesandbox.io/p/devbox/thorvg-lottieplayer-svelte-xd4zp6)
