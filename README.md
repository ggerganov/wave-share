# wave-share

A proof-of-concept for WebRTC signaling using sound. Works with all devices that have microphone + speakers. Runs in the
browser.

Nearby devices negotiate the WebRTC connection by exchanging the necessary Session Description Protocol (SDP) data via
a sequence of audio tones. Upon successful negotiation, a local WebRTC connection is established between the browsers allowing
data to be exchanged via LAN.

See it in action:

<a href="http://www.youtube.com/watch?feature=player_embedded&v=d30QDrKyQkg" target="_blank"><img src="http://img.youtube.com/vi/d30QDrKyQkg/0.jpg" alt="CG++ Data over sound" width="360" height="270" border="10" /></a>

Try it yourself: [ggerganov.github.io/wave-share.html](https://ggerganov.github.io/jekyll/update/2018/04/13/wave-share.html)
