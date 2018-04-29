# wave-share

A proof-of-concept for WebRTC signaling using sound. Works with all devices that have microphone + speakers. Runs in the
browser.

Nearby devices negotiate the WebRTC connection by exchanging the necessary Session Description Protocol (SDP) data via
a sequence of audio tones. Upon successful negotiation, a local WebRTC connection is established between the browsers allowing
data to be exchanged via LAN.

See it in action: (add video link here)

Try it yourself: [ggerganov.github.io/wave-share.html](https://ggerganov.github.io/jekyll/update/2018/04/13/wave-share.html)
