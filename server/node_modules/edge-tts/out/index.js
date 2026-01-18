// index.ts
import {Buffer} from "node:buffer";
import fs from "node:fs";
import {WebSocket} from "ws";
async function getVoices() {
  const resp = await fetch(voiceListUrl);
  return await resp.json();
}
var uuid = function() {
  return crypto.randomUUID().replaceAll("-", "");
};
function tts(text, options = {}) {
  const { voice = "en-GB-SoniaNeural", volume = "+0%", rate = "+0%", pitch = "+0Hz" } = options;
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`${webSocketURL}&ConnectionId=${uuid()}`, {
      host: "speech.platform.bing.com",
      origin: "chrome-extension://jdiccldimpdaibmpdkjnbmckianbfold",
      headers: { "User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/103.0.5060.66 Safari/537.36 Edg/103.0.1264.44" }
    });
    const audioData = [];
    ws.on("message", (rawData, isBinary) => {
      if (!isBinary) {
        const data2 = rawData.toString("utf8");
        if (data2.includes("turn.end")) {
          resolve(Buffer.concat(audioData));
          ws.close();
        }
        return;
      }
      const data = rawData;
      const separator = "Path:audio\r\n";
      const content = data.subarray(data.indexOf(separator) + separator.length);
      audioData.push(content);
    });
    ws.on("error", reject);
    const speechConfig = JSON.stringify({ context: { synthesis: { audio: {
      metadataoptions: { sentenceBoundaryEnabled: false, wordBoundaryEnabled: false },
      outputFormat: "audio-24khz-48kbitrate-mono-mp3"
    } } } });
    const configMessage = `X-Timestamp:${Date()}\r\nContent-Type:application/json; charset=utf-8\r\nPath:speech.config\r\n\r\n${speechConfig}`;
    ws.on("open", () => ws.send(configMessage, { compress: true }, (configError) => {
      if (configError)
        reject(configError);
      const ssmlMessage = `X-RequestId:${uuid()}\r\nContent-Type:application/ssml+xml\r\n` + `X-Timestamp:${Date()}Z\r\nPath:ssml\r\n\r\n` + `<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='en-US'>` + `<voice name='${voice}'><prosody pitch='${pitch}' rate='${rate}' volume='${volume}'>` + `${text}</prosody></voice></speak>`;
      ws.send(ssmlMessage, { compress: true }, (ssmlError) => {
        if (ssmlError)
          reject(ssmlError);
      });
    }));
  });
}
async function ttsSave(text, file, options = {}) {
  fs.writeFileSync(file, await tts(text, options));
}
var baseUrl = `speech.platform.bing.com/consumer/speech/synthesize/readaloud`;
var token = "6A5AA1D4EAFF4E9FB37E23D68491D6F4";
var webSocketURL = `wss://${baseUrl}/edge/v1?TrustedClientToken=${token}`;
var voiceListUrl = `https://${baseUrl}/voices/list?trustedclienttoken=${token}`;
var Personalities = ["Approachable", "Authentic", "Authority", "Bright", "Caring", "Casual", "Cheerful", "Clear", "Comfort", "Confident", "Considerate", "Conversational", "Cute", "Expressive", "Friendly", "Honest", "Humorous", "Lively", "Passion", "Pleasant", "Positive", "Professional", "Rational", "Reliable", "Sincere", "Sunshine", "Warm"];
var Categories = ["Novel", "Cartoon", "Conversation", "Copilot", "Dialect", "General", "News", "Novel", "Sports"];
export {
  ttsSave,
  tts,
  getVoices,
  Personalities,
  Categories
};

//# debugId=EA7BA9497FADA67464756e2164756e21
