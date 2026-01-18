# edge-tts
`edge-tts` is a allows you to use Microsoft Edge's online text-to-speech service from within your TypeScript code.

Based on [edge-tts](https://github.com/rany2/edge-tts/) for python.

## Usage
```js
import { tts, ttsSave } from 'edge-tts'

// buffer of mp3 data
const audioBuffer = await tts('hello world')

// or if you just want a file
await ttsSave('hello world', './tts.mp3')

// customize voice
await tts('hello world', {
  voice: 'es-VE-PaolaNeural', // can be any Voice.Name, Voice.ShortName, or Voice.FriendlyName
  volume: '+50%', // 50% louder
  rate: '-50%', // 50% the speed
  pitch: '-50Hz' // 50Hz lower
})
```

Find voices to use for the `voice` param using `getVoices`
```js
import { getVoices, tts } from 'edge-tts'

// get list of available voices
const voices = await getVoices()

// get voices done by native english speakers
const englishVoices = voices.filter(v => v.Locale === 'en-US')

// select a random english voice
const voice = englishVoices[Math.floor(Math.random() * englishVoices.length)]

// can also use voice.ShortName, or voice.FriendlyName
const audioBuffer = await tts('hello world', { voice: voice.Name })
```
