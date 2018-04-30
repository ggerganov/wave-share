# wave-share

A proof-of-concept for WebRTC signaling using sound. Works with all devices that have microphone + speakers. Runs in the
browser.

Nearby devices negotiate the WebRTC connection by exchanging the necessary Session Description Protocol (SDP) data via
a sequence of audio tones. Upon successful negotiation, a local WebRTC connection is established between the browsers allowing
data to be exchanged via LAN.

See it in action:

<a href="http://www.youtube.com/watch?feature=player_embedded&v=d30QDrKyQkg" target="_blank"><img src="http://img.youtube.com/vi/d30QDrKyQkg/0.jpg" alt="CG++ Data over sound" width="360" height="270" border="10" /></a>

Try it yourself: [ggerganov.github.io/wave-share.html](https://ggerganov.github.io/jekyll/update/2018/04/13/wave-share.html)

## How it works

The [WebRTC](https://en.wikipedia.org/wiki/WebRTC) technology allows two browsers running on different devices to connect with each other and exchange data. There is no need to install plugins or download applications. To initiate the connection, the peers need to exchange contact information (ip address, network ports, session id, etc.). This process is called "signaling". The WebRTC specification does not define any standard about signaling. The contact exchange can be achieved by any protocol or technology.

In this project the signaling is performed via sound. The signaling sequence looks like this:

  - Peer A broadcasts an offer for a WebRTC connection by encoding the session data into the output audio
  - Nearby peer(s) capture the sound emitted by peer A and decode the WebRTC session data
  - Peer B, who wants to establish connection with peer A, responds with an audio answer. The answer has peer B's contact information encoded in it. Additionally, peer B starts trying to connect to peer A
  - Peer A receives the answer from peer B, decodes the transmitted contact data and allows peer B to connect
  - Connection is established
  
In contrast to most WebRTC applications, the described signaling sequence does not involve a signaling server. Therefore, an application using sound signaling can be served by a static web page. The only requirement is to have control over the audio output/capture devices.

An obvious limitation (feature) of the current approach is that only nearby devices (e.g. within the same room) can establish connection with each other.

## Build

## Known problems / stuff to improve
